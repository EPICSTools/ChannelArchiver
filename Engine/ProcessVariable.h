#ifndef PROCESSVARIABLE_H_
#define PROCESSVARIABLE_H_

// Base
#include <cadef.h>
// Tool
#include <Guard.h>
// Storage
#include <RawValue.h>
#include <CtrlInfo.h>
// local
#include "Named.h"
#include "ProcessVariableContext.h"
#include "ProcessVariableListener.h"

/**\ingroup Engine
 *  One process variable.
 *
 *  Handles the connect/disconnect,
 *  also requests the full information (DBR_CTRL_...)
 *  and from then on get/monitor will only
 *  get the DBR_TIME_...
 *  <p>
 *  Locking: The ProcessVariable and the ProcessVariableContext
 *  call into the ChannelAccess client library.
 *  The CA client library also invokes callbacks in the 
 *  ProcessVariable. To avoid deadlocks between ProcessVariable
 *  and CA semaphores, the ProcessVariable unlocks itself
 *  before calling any CA library.
 *  This means, however, that one should not otherwise modify
 *  the ProcessVariable while it is dealing with the CA client
 *  library. To help enforce this, the add/removeListener calls
 *  are only allowed while the ProcessVariable is not running.
 */
class ProcessVariable : public NamedBase, public Guardable
{
public:
    /** Create a ProcessVariable with given name. */
    ProcessVariable(ProcessVariableContext &ctx, const char *name);
    
    /** Destructor. */
    virtual ~ProcessVariable();

    /** @see Guardable */
    OrderedMutex &getMutex();
    
    /** Possible states of a ProcessVariable. */
    enum State
    {
        /** Not initialized */
        INIT,
        /** Not connected, but trying to connect. */
        DISCONNECTED,
        /** Received CA connection callback, getting control info. */
        GETTING_INFO,
        /** Fully connected. */
        CONNECTED
    };
    
    /** @return Returns the current state. */
    State getState(Guard &guard) const;

    /** @return Returns true if the current state is CONNECTED. */
    bool isConnected(Guard &guard) const;

    /** @return Returns the current state. */
    const char *getStateStr(Guard &guard) const;
    
    /** @return Returns the ChannelAccess state. */
    const char *getCAStateStr(Guard &guard) const;
    
    /** Get the DBR_TIME_... type of this PV.
     * 
     *  Only valid when getState() was CONNECTED
     */
    DbrType getDbrType(Guard &guard) const;
    
    /** Get the array size of this PV.
     * 
     *  Only valid when getState() was CONNECTED
     */
    DbrCount getDbrCount(Guard &guard) const;
    
    /** @return Returns the control information. */    
    const CtrlInfo &getCtrlInfo(Guard &guard) const;

    /** Add a ProcessVariableStateListener. */
    void addStateListener(Guard &guard, ProcessVariableStateListener *listener);

    /** Remove a ProcessVariableStateListener. */
    void removeStateListener(Guard &guard,
                             ProcessVariableStateListener *listener);

    /** Add a ProcessVariableValueListener. */
    void addValueListener(Guard &guard, ProcessVariableValueListener *listener);

    /** Remove a ProcessVariableValueListener. */
    void removeValueListener(Guard &guard,
                             ProcessVariableValueListener *listener);

    /** Add a ProcessVariableListener. */
    void addListener(Guard &guard, ProcessVariableListener *listener);

    /** Remove a ProcessVariableListener. */
    void removeListener(Guard &guard, ProcessVariableListener *listener);

    /** Start the connection mechanism.
     *  @see stop()
     *  @see isRunning()
     */
    void start(Guard &guard);
    
    /** @return Returns true if start() has been called but not stop().
     *  @see start()     */
    bool isRunning(Guard &guard);

    /** Perform a single 'get'.
     *
     *  Value gets delivered to listeners.
     */
    void getValue(Guard &guard);

    /** Subscribe for value updates.
     *
     *  Values get delivered to listeners.
     */
    void subscribe(Guard &guard);

    /** @return Returns 'true' if we are subscribed. */
    bool isSubscribed(Guard &guard) const;

    /** Unsubscribe, no more updates. */
    void unsubscribe(Guard &guard);
    
    /** Disconnect.
     *  @see #start()
     */
    void stop(Guard &guard);
    
private:
    OrderedMutex                       mutex;
    ProcessVariableContext             &ctx;
    State                              state;
    stdList<ProcessVariableStateListener *> state_listeners;
    stdList<ProcessVariableValueListener *> value_listeners;
    chid                               id;
    evid                               ev_id;
    DbrType                            dbr_type;
    DbrCount                           dbr_count;
    CtrlInfo                           ctrl_info;
    size_t                             outstanding_gets;
    bool                               subscribed;
 
    // Channel Access callbacks   
    static void connection_handler(struct connection_handler_args arg);
    static void control_callback(struct event_handler_args arg);
    static void value_callback(struct event_handler_args);
    
    bool setup_ctrl_info(Guard &guard, DbrType type, const void *dbr_ctrl_xx);
    void firePvConnected(Guard &guard);
    void firePvDisconnected(Guard &guard);
    void firePvValue(Guard &guard, const RawValue::Data *value);
};

inline bool ProcessVariable::isConnected(Guard &guard) const
{
    return getState(guard) == CONNECTED;
}

inline DbrType ProcessVariable::getDbrType(Guard &guard) const
{
    return dbr_type;
}
    
inline DbrCount ProcessVariable::getDbrCount(Guard &guard) const
{
    return dbr_count;
}

inline const CtrlInfo &ProcessVariable::getCtrlInfo(Guard &guard) const
{
    return ctrl_info;
}

inline void ProcessVariable::addListener(Guard &guard,
                                         ProcessVariableListener *listener)
{
    addStateListener(guard, listener);
    addValueListener(guard, listener);
}

inline void ProcessVariable::removeListener(Guard &guard,
                                            ProcessVariableListener *listener)
{
    removeValueListener(guard, listener);
    removeStateListener(guard, listener);
}

inline bool ProcessVariable::isSubscribed(Guard &guard) const
{
    return subscribed;
}

#endif /*PROCESSVARIABLE_H_*/
