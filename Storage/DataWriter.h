// -*- c++ -*-

// Tools
#include "stdString.h"
// Storage
#include "CtrlInfo.h"
#include "RawValue.h"
// Index
#include "IndexFile.h"

/// \ingroup Storage Writes data to storage.

/// The data writer interfaces between a series of
/// RawValue values and the Index/DataFile.
///
class DataWriter
{
public:
    /// Create a writer for the given index.
    /// \param channel_name: Name of the channel
    /// \param ctrl_info: The meta information for the channel
    /// \param dbr_type: the dbr_time_xxx type
    /// \param dbr_count: array size
    /// \param num_samples: estimated number of samples
    ///                     (helps w/ buffer allocation)
    DataWriter(IndexFile &index,
               const stdString &channel_name,
               const CtrlInfo &ctrl_info,
               DbrType dbr_type,
               DbrCount dbr_count,
               double period,
               size_t num_samples);

    /// Destructor
    ~DataWriter();

    /// Add a value, returns true for OK.
    bool add(const RawValue::Data *data);

    /// Data file size limit.
    static FileOffset file_size_limit;
    
private:
    IndexFile &index;
    RTree     *tree;
    const stdString &channel_name;
    const CtrlInfo &ctrl_info;
    DbrType dbr_type;
    DbrCount dbr_count;
    double period;
    size_t raw_value_size;

    void makeDataFileName(int serial, stdString &name);
    DataFile *createNewDataFile();

    // Sets next_buffer_size to at least 'start',
    // so that buffers get bigger and bigger up to
    // some limit.
    void calc_next_buffer_size(size_t start);
    size_t next_buffer_size;

    class DataHeader *header;
    size_t available;
    MemoryBuffer<dbr_time_string> cvt_buffer;

    bool addNewHeader(bool new_ctrl_info);
};
