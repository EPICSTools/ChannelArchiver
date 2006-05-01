// Tools
#include <MsgLogger.h>
// Engine
#include "ArchiveChannel.h"
#include "GroupInfo.h"
#include "SampleMechanismMonitored.h"
#include "SampleMechanismGet.h"
#include "SampleMechanismMonitoredGet.h"

#define DEBUG_ARCHIVE_CHANNEL
 
ArchiveChannel::ArchiveChannel(EngineConfig &config,
                               ProcessVariableContext &ctx,
                               ScanList &scan_list,
                               const char *channel_name,
                               double scan_period, bool monitor)
    : NamedBase(channel_name), 
      mutex("ArchiveChannel", 30), 
      scan_period(scan_period),
      monitor(monitor),
      currently_disabling(false),
      disable_count(0)
{
    sample_mechanism = createSampleMechanism(config, ctx, scan_list);
    LOG_ASSERT(sample_mechanism);
}

ArchiveChannel::~ArchiveChannel()
{
    if (sample_mechanism)
    {
        try
        {
            Guard guard(__FILE__, __LINE__, *sample_mechanism);
            sample_mechanism->removeStateListener(guard, this);
            if (canDisable(guard))  
                sample_mechanism->removeValueListener(guard, this);   
        }
        catch (GenericException &e)
        {
            LOG_MSG("ArchiveChannel '%s': %s\n", getName().c_str(), e.what());
        }
        catch (...)
        {
            LOG_MSG("ArchiveChannel '%s': exception in desctructor\n",
                     getName().c_str());
        }
    } 
    sample_mechanism = 0;
}

OrderedMutex &ArchiveChannel::getMutex()
{
    return mutex;
}

void ArchiveChannel::configure(EngineConfig &config,
                               ProcessVariableContext &ctx,
                               ScanList &scan_list,
                               double scan_period, bool monitor)
{
    LOG_MSG("ArchiveChannel '%s' reconfig...\n",
            getName().c_str());
    LOG_ASSERT(sample_mechanism);
    // Check args, stop old sample mechanism.
    Guard guard(__FILE__, __LINE__, *this);
    {
        Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);
        LOG_ASSERT(!sample_mechanism->isRunning(sample_guard));
        // If anybody wants to monitor, use monitor
        if (monitor)
            this->monitor = true;
        // Is scan_period initialized?
        if (this->scan_period <= 0.0)
            this->scan_period = scan_period;
        else if (this->scan_period > scan_period) // minimize
            this->scan_period = scan_period;
    
        sample_mechanism->removeStateListener(sample_guard, this);
        if (canDisable(guard))  
            sample_mechanism->removeValueListener(sample_guard, this);
    }
    // Replace with new one
    sample_mechanism = createSampleMechanism(config, ctx, scan_list);
    LOG_ASSERT(sample_mechanism);
    if (canDisable(guard))
    {
        Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);
        sample_mechanism->addValueListener(sample_guard, this);
    }
}

SampleMechanism *ArchiveChannel::createSampleMechanism(
    EngineConfig &config, ProcessVariableContext &ctx, ScanList &scan_list)
{
    AutoPtr<SampleMechanism> new_mechanism;
    // Determine new mechanism
    if (monitor)
        new_mechanism = new SampleMechanismMonitored(config, ctx,
                                                     getName().c_str(),
                                                     scan_period);
    else if (scan_period >= config.getGetThreshold())
        new_mechanism = new SampleMechanismGet(config, ctx, scan_list,
                                               getName().c_str(),
                                                scan_period);
    else
        new_mechanism = new SampleMechanismMonitoredGet(config, ctx,
                                                        getName().c_str(),
                                                         scan_period);
    LOG_ASSERT(new_mechanism);
    Guard guard(__FILE__, __LINE__, *new_mechanism); // Lock: Channel, SampleMech.          
    new_mechanism->addStateListener(guard, this);
    return new_mechanism.release();
}

void ArchiveChannel::addToGroup(Guard &group_guard, GroupInfo *group,
                                Guard &channel_guard, bool disabling)
{
    channel_guard.check(__FILE__, __LINE__, mutex);
    LOG_ASSERT(!isRunning(channel_guard));    
    stdList<GroupInfo *>::iterator i;
    // Add to the group list
    bool add = true;
    for (i=groups.begin(); i!=groups.end(); ++i)
    {
        if (*i == group)
        {
            add = false;
            break;
        }
    }
    if (add)
    {
        groups.push_back(group);
        // Is the group disabled?      
        if (! group->isEnabled(group_guard))
        {  
            LOG_MSG("Channel '%s': added to disabled group '%s'\n",
                    getName().c_str(), group->getName().c_str());
            disable(channel_guard, epicsTime::getCurrent());
        }
    }       
    // Add to the 'disable' groups?
    if (disabling)
    {
        // Is this the first time we become 'disabling',
        // i.e. not diabling, yet?
        // --> monitor values!
        if (! canDisable(channel_guard))
        {
            Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);
            sample_mechanism->addValueListener(sample_guard, this);
        }
        // Add, but only once.
        add = true;
        for (i=disable_groups.begin(); i!=disable_groups.end(); ++i)
        {
            if (*i == group)
            {
                add = false;
                break;
            }
        }
        if (add)
        {
            disable_groups.push_back(group);
            // Is the channel already 'disabling'?
            // Then disable that new group right away.
            if (currently_disabling)
            {
                epicsTime when = epicsTime::getCurrent();
                LOG_MSG("Channel '%s' disables '%s' right away\n",
                        getName().c_str(), group->getName().c_str());  
                group->disable(group_guard, this, when);
            }
        }
    }
}

void ArchiveChannel::start(Guard &guard)
{
    guard.check(__FILE__, __LINE__, mutex);
    LOG_ASSERT(!isRunning(guard));
    Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);
    sample_mechanism->start(sample_guard);
}
      
bool ArchiveChannel::isRunning(Guard &guard)
{
    guard.check(__FILE__, __LINE__, mutex);
    LOG_ASSERT(sample_mechanism);
    Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);
    return sample_mechanism->isRunning(sample_guard);
}

bool ArchiveChannel::isConnected(Guard &guard) const
{
    Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);
    return sample_mechanism->getPVState(sample_guard)
                                 == ProcessVariable::CONNECTED;
}
     
// Called by group to disable channel.
// Doesn't mean that this channel itself can disable,
// that's handled in pvValue() callback!
void ArchiveChannel::disable(Guard &guard, const epicsTime &when)
{
    guard.check(__FILE__, __LINE__, mutex);
    ++disable_count;
    if (disable_count > groups.size())
    {
        LOG_MSG("ERROR: Channel '%s' disabled %zu times?\n",
                getName().c_str(), disable_count);
        return;
    }
    if (isDisabled(guard))
    {
        LOG_MSG("Channel '%s' disabled\n", getName().c_str());  
        Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);    
        sample_mechanism->disable(sample_guard, when);
    }
#if 0
    // TODO:
    // In case we're asked to disconnect _and_ this channel
    // doesn't need to stay connected because it disables other channels,
    // stop CA
    if (theEngine->disconnectOnDisable(engine_guard) && groups_to_disable.empty())
        stop(engine_guard, guard);
#endif
}
 
// Called by group to re-enable channel.
void ArchiveChannel::enable(Guard &guard, const epicsTime &when)
{
    guard.check(__FILE__, __LINE__, mutex);
    if (disable_count <= 0)
    {
        LOG_MSG("ERROR: Channel '%s' enabled while not disabled?\n",
                getName().c_str());
        return;
    }
    --disable_count;
    if (!isDisabled(guard))
    {
        LOG_MSG("Channel '%s' enabled\n", getName().c_str());   
        Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);    
        sample_mechanism->enable(sample_guard, when);
    }
#if 0
    // TODO:
    // In case we're asked to disconnect _and_ this channel
    // doesn't need to stay connected because it disables other channels,
    // stop CA
    if (theEngine->disconnectOnDisable(engine_guard) && groups_to_disable.empty())
        stop(engine_guard, guard);
#endif
}    
          
stdString ArchiveChannel::getSampleInfo(Guard &guard)
{
    Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);    
    return sample_mechanism->getInfo(sample_guard);
}
     
void ArchiveChannel::stop(Guard &guard)
{
    guard.check(__FILE__, __LINE__, mutex);
    LOG_ASSERT(isRunning(guard));
    Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);
    GuardRelease release(__FILE__, __LINE__, guard); // Avoid deadlock with CA callbacks    
    sample_mechanism->stop(sample_guard);
}

unsigned long  ArchiveChannel::write(Guard &guard, Index &index)
{
    guard.check(__FILE__, __LINE__, mutex);
    Guard sample_guard(__FILE__, __LINE__, *sample_mechanism);    
    return sample_mechanism->write(sample_guard, index);
}

// ArchiveChannel is StateListener to SampleMechanism (==PV)
void ArchiveChannel::pvConnected(Guard &pv_guard, ProcessVariable &pv,
                                 const epicsTime &when)
{
#ifdef DEBUG_ARCHIVE_CHANNEL
    LOG_MSG("ArchiveChannel '%s' is connected\n", getName().c_str());
#endif
    GuardRelease release(__FILE__, __LINE__, pv_guard);
    {
        Guard guard(__FILE__, __LINE__, *this); // Lock order: only Channel
        // Notify groups
        stdList<GroupInfo *>::iterator gi;
        for (gi = groups.begin(); gi != groups.end(); ++gi)
        {
            GroupInfo *g = *gi;
            GuardRelease release(__FILE__, __LINE__, guard);
            {
                Guard group_guard(__FILE__, __LINE__, *g); // Lock Order: only Group
                g->incConnected(group_guard, *this);
            }
        }
    }
}

// ArchiveChannel is StateListener to SampleMechanism (==PV)
void ArchiveChannel::pvDisconnected(Guard &pv_guard, ProcessVariable &pv,
                    const epicsTime &when)
{
#ifdef DEBUG_ARCHIVE_CHANNEL
    LOG_MSG("ArchiveChannel '%s' is disconnected\n", getName().c_str());
#endif
    GuardRelease pv_release(__FILE__, __LINE__, pv_guard);
    {
        Guard guard(__FILE__, __LINE__, *this); // Lock order: Only Channel.
        // Notify groups
        stdList<GroupInfo *>::iterator gi;
        for (gi = groups.begin(); gi != groups.end(); ++gi)
        {
            GroupInfo *g = *gi;
            GuardRelease release(__FILE__, __LINE__, guard);
            {
                Guard group_guard(__FILE__, __LINE__, *g); // Lock order: Only Group.
                g->decConnected(group_guard, *this);
            }
        }
    }
}

// ArchiveChannel is ValueListener to SampleMechanism (==PV) _IF_ disabling
void ArchiveChannel::pvValue(Guard &pv_guard, ProcessVariable &pv,
                             const RawValue::Data *data)
{
    bool should_disable = RawValue::isAboveZero(pv.getDbrType(pv_guard), data);
    GuardRelease pv_release(__FILE__, __LINE__, pv_guard);
    {
        Guard guard(__FILE__, __LINE__, *this); // Lock order: Only Channel
        if (!canDisable(guard))
        {
            LOG_MSG("ArchiveChannel '%s' got value for disable test "
                    "but not configured to disable\n",
                    getName().c_str());
            return;
        }        
        if (should_disable)
        {
            //LOG_MSG("ArchiveChannel '%s' got disabling value\n",
            //        getName().c_str());
            if (currently_disabling) // Was and still is disabling
                return;
            // Wasn't, but is now disabling.
            LOG_MSG("ArchiveChannel '%s' disables its groups\n",
                    getName().c_str());
            currently_disabling = true;
            // Notify groups
            stdList<GroupInfo *>::iterator gi;
            epicsTime when = RawValue::getTime(data);
            for (gi = disable_groups.begin(); gi != disable_groups.end(); ++gi)
            {
                GroupInfo *g = *gi;
                GuardRelease release(__FILE__, __LINE__, guard);
                {
                    Guard group_guard(__FILE__, __LINE__, *g);  // Lock order: Only Group.
                    g->disable(group_guard, this, when);
                }
            }
        }
        else
        {
            //LOG_MSG("ArchiveChannel '%s' got enabling value\n",
            //        getName().c_str());
            if (! currently_disabling) // Wasn't and isn't disabling.
                return;
            // Re-enable groups.
            LOG_MSG("ArchiveChannel '%s' enables its groups\n",
                    getName().c_str());
            currently_disabling = false;
            // Notify groups
            stdList<GroupInfo *>::iterator gi;
            epicsTime when = RawValue::getTime(data);
            for (gi = disable_groups.begin(); gi != disable_groups.end(); ++gi)
            {
                GroupInfo *g = *gi;
                GuardRelease release(__FILE__, __LINE__, guard);
                {
                    Guard group_guard(__FILE__, __LINE__, *g);  // Lock order: Only Group.
                    g->enable(group_guard, this, when);
                }
            }
        }
    }
}

void ArchiveChannel::addToFUX(Guard &guard, class FUX::Element *doc)
{
    FUX::Element *channel = new FUX::Element(doc, "channel");
    new FUX::Element(channel, "name", getName());
    sample_mechanism->addToFUX(guard, channel);
    if (canDisable(guard))
        new FUX::Element(channel, "disable");
}


