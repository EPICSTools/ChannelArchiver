
// Tools
#include "MsgLogger.h"
#include "Filename.h"
#include "epicsTimeHelper.h"
// Storage
#include "DataFile.h"
#include "TreeDataWriter.h"

// TODO: Switch to new data file after
// time limit or file size limit

static stdString makeDataFileName()
{
    int year, month, day, hour, min, sec;
    unsigned long nano;
    char buffer[80];
    
    epicsTime now = epicsTime::getCurrent();    
    //    if (getSecsPerFile() == SECS_PER_MONTH)
    //{
    epicsTime2vals(now, year, month, day, hour, min, sec, nano);
    sprintf(buffer, "%04d%02d%02d", year, month, day);
    return stdString(buffer);
    //}
    //now = roundTimeDown(now, _secs_per_file);
    //epicsTime2vals(now, year, month, day, hour, min, sec, nano);
    //sprintf(buffer, "%04d%02d%02d-%02d%02d%02d", year, month, day, hour, min, sec);
    //return stdString(buffer);
}

TreeDataWriter::TreeDataWriter(archiver_Index &index,
                       const stdString &channel_name,
                       const CtrlInfo &ctrl_info,
                       DbrType dbr_type,
                       DbrCount dbr_count,
                       double period,
                       size_t num_samples)
        : index(index), channel_name(channel_name),
          priority(0),
          ctrl_info(ctrl_info), dbr_type(dbr_type),
          dbr_count(dbr_count), period(period), header(0), available(0)
{
    DataFile *datafile;

    // Size of next buffer should at least hold num_samples
    calc_next_buffer_size(num_samples);
    raw_value_size = RawValue::getSize(dbr_type, dbr_count);

    // Find or add appropriate data buffer
    archiver_Unit au;
    if (!index.getLatestAU(channel_name.c_str(), &au))
    {   // - There is no datafile, no buffer
        // Create data file
        stdString data_file_name = makeDataFileName();
        datafile = DataFile::reference(index.getDirectory(),
                                       data_file_name, true);
        if (! datafile)
        {
             LOG_MSG ("TreeDataWriter(%s): Cannot create data file '%s'\n",
                 channel_name.c_str(), data_file_name.c_str());
             return;
        }        
        // add CtrlInfo
        FileOffset ctrl_info_offset;
        if (!datafile->addCtrlInfo(ctrl_info, ctrl_info_offset))
        {
             LOG_MSG ("TreeDataWriter(%s): Cannot add CtrlInfo to file '%s'\n",
                 channel_name.c_str(), data_file_name.c_str());
             datafile->release();
             return;
        }        
        // add first header
        header = datafile->addHeader(dbr_type, dbr_count,
                                     period, next_buffer_size);
        datafile->release(); // now ref'ed by header
        if (!header)
        {
            LOG_MSG ("TreeDataWriter(%s): Cannot add Header to data file '%s'\n",
                    channel_name.c_str(), data_file_name.c_str());
           return;
        }    
        header->data.ctrl_info_offset = ctrl_info_offset;
        // Upp the buffer size
        calc_next_buffer_size(next_buffer_size);
        // Will add to index when we release the buffer
    }
    else
    {   // - There is a data file and buffer
        stdString data_file_name = au.getKey().getPath();
        FileOffset offset = au.getKey().getOffset();
        datafile = DataFile::reference(index.getDirectory(),
                                       data_file_name, true);
        if (!datafile)
        {
            LOG_MSG("TreeDataWriter(%s) cannot open data file %s\n",
                    channel_name.c_str(), data_file_name.c_str());
            return;
        } 
        header = datafile->getHeader(offset);
        datafile->release(); // now ref'ed by header
        if (!header)
        {
            LOG_MSG("TreeDataWriter(%s): cannot get header %s @ 0x%lX\n",
                    channel_name.c_str(),
                    data_file_name.c_str(), offset);
            return;
        }
        // See if anything has changed
        CtrlInfo prev_ctrl_info;
        if (!prev_ctrl_info.read(header->datafile,
                                 header->data.ctrl_info_offset))
        {
            LOG_MSG("TreeDataWriter(%s): header in %s @ 0x%lX"
                    " has bad CtrlInfo\n",
                    channel_name.c_str(),
                    header->datafile->getBasename().c_str(),
                    header->offset);
            delete header;
            header = 0;
            return;            
        }
        if (prev_ctrl_info != ctrl_info)
        {   // Add new header because info has changed
            if (!addNewHeader(true))
                return;
        }
        else if (header->data.dbr_type != dbr_type  ||
                 header->data.dbr_count != dbr_count)
        {   // Add new header because type has changed
            if (!addNewHeader(false))
                return;
        }
        else
        {   // All fine, just check if we're already in bigger league
            size_t capacity = header->capacity();
            if (capacity > num_samples)
                calc_next_buffer_size(capacity);
        }
    }   
    available = header->available();
}
    
TreeDataWriter::~TreeDataWriter()
{
    // Update index
    if (header && index.isInstanceValid())
    {   
        header->data.dir_offset = 0; // no longer used
        header->write();
        archiver_Unit au(
            key_Object(header->datafile->getBasename().c_str(), header->offset),
            interval(header->data.begin_time, header->data.end_time),
            priority);
        if (!index.addAU(channel_name.c_str(), au))
        {
            LOG_MSG("Cannot add %s @ 0x%lX to index\n",
                    header->datafile->getBasename().c_str(), header->offset);
        }   
        delete header;
    }
}

bool TreeDataWriter::add(const RawValue::Data *data)
{
    if (!header) // In here, we should always have a header
        return false;
    if (available <= 0) // though it might be full
    {
        if (!addNewHeader(false))
            return false;
        available = header->available();
    }
    // Add the value
    available -= 1;
    FileOffset offset = header->offset
        + sizeof(DataHeader::DataHeaderData)
        + header->data.curr_offset;
    RawValue::write(dbr_type, dbr_count,
                    raw_value_size, data,
                    cvt_buffer,
                    header->datafile, offset);
    // Update the header
    header->data.curr_offset += raw_value_size;
    header->data.num_samples += 1;
    header->data.buf_free    -= raw_value_size;
    epicsTime time = RawValue::getTime(data);
    if (header->data.num_samples == 1) // first entry?
        header->data.begin_time = time;
    header->data.end_time = time;
    // Note: we didn't write the header nor the dfi,
    // that'll happen when we close the TreeDataWriter!
    return true;
}

void TreeDataWriter::calc_next_buffer_size(size_t start)
{
    // We want the next power of 2:
    int log2 = 0;
    while (start > 0)
    {
        start >>= 1;
        ++log2;
    }
    if (log2 < 6) // minumum: 2^6 == 64
        log2 = 6;
    if (log2 > 12) // maximum: 2^12 = 4096
        log2 = 12;
    next_buffer_size = 1 << log2;
}

// Helper: Assuming that we have
// a valid header, we add a new one,
// because the existing one is full or
// has the wrong ctrl_info
//
// Will write ctrl_info out if new_ctrl_info is set,
// otherwise the new header points to the existin ctrl_info
bool TreeDataWriter::addNewHeader(bool new_ctrl_info)
{
    LOG_ASSERT(header != 0);
    FileOffset ctrl_info_offset;
    if (new_ctrl_info)
    {
        if (!header->datafile->addCtrlInfo(ctrl_info, ctrl_info_offset))
        {
            LOG_MSG ("TreeDataWriter(%s): Cannot add CtrlInfo to file '%s'\n",
                     channel_name.c_str(),
                     header->datafile->getBasename().c_str());
            return false;
        }        
    }
    else // use existing one
        ctrl_info_offset = header->data.ctrl_info_offset;
        
    DataHeader *new_header =
        header->datafile->addHeader(dbr_type, dbr_count, period,
                                    next_buffer_size);
    if (!new_header)
        return false;
    new_header->data.ctrl_info_offset = ctrl_info_offset;
    // Link old header to new one
    header->set_next(new_header->datafile->getBasename(),
                     new_header->offset);
    header->write();
    // back from new to old
    new_header->set_prev(header->datafile->getBasename(),
                         header->offset);        
    // Update index entry for the old header
    if (index.isInstanceValid())
    {   
        archiver_Unit au(
            key_Object(header->datafile->getBasename().c_str(), header->offset),
            interval(header->data.begin_time, header->data.end_time),
            priority);
        if (!index.addAU(channel_name.c_str(), au))
        {
            LOG_MSG("Cannot add %s @ 0x%lX to index\n",
                    header->datafile->getBasename().c_str(), header->offset);
        }   
    }        
    // Switch to new header
    delete header;
    header = new_header;
    // Upp the buffer size
    calc_next_buffer_size(next_buffer_size);
    // new header will be added to index when it's closed
    return true;
}
    