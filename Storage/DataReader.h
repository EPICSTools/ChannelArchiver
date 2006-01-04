// -*- c++ -*-

#ifndef __DATA_READER_H__
#define __DATA_READER_H__

// Tools
#include "stdString.h"
// Storage
#include "CtrlInfo.h"
#include "RawValue.h"
#include "Index.h"
#include "RTree.h"

/// \addtogroup Storage
/// @{

/// Reads data from storage.
///
/// The data reader interfaces to the Index/DataFile
/// and returns a stream of RawValue values.
class DataReader
{
public:
    virtual ~DataReader();
    
    /// Locate data.
    ///
    /// Positions reader on given channel and start time.
    ///
    /// Specifically: If a value with the exact start time exists,
    /// it will be returned. Otherwise the value just before the start time
    /// is returned, so that the user can then decide if and how that value
    /// might extrapolate onto the start time.
    ///
    /// @param channel_name: Name of the channel
    /// @param start: start time or 0 for first value
    /// @param error_info: may be set to error information
    ///
    /// @return Returns pointer to first value, or 0 if the channel
    ///         was found but there was no data.
    ///
    /// @exception GenericException on error, including file access issues
    ///            or the fact that a channel doesn't exist.
    virtual const RawValue::Data *find(const stdString &channel_name,
                                       const epicsTime *start) = 0;

    /// Obtain the next value.
    ///
    /// After a successful call to find() for the first value,
    /// this routine returns the following values with each call.
    /// When no more data is available, 0 is returned.
    /// Any calls to next() after either find() or next() return 0
    /// to indicate the end of the available data will result in
    /// exceptions.
    ///
    /// @pre find()
    ///
    /// @return Returns next value or 0.
    ///
    /// @exception GenericException on error: File access,
    ///            called after reaching the end of data.
    virtual const RawValue::Data *next() = 0;

    /// Name of the channel, i.e. the one passed to find()
    stdString channel_name;
    
    /// The dbr_time_xxx type
    virtual DbrType getType() const = 0;
    
    /// array size
    virtual DbrCount getCount() const = 0;
    
    /// The meta information for the channel
    virtual const CtrlInfo &getInfo() const = 0;

    /// next() updates this if dbr_type/count changed.

    /// Returns whether the type changed or not AND!!
    /// resets the flag!!
    ///
    virtual bool changedType() = 0;

    /// next() updates this if ctrl_info changed.

    /// Returns whether the ctrl_info  changed or not
    /// AND(!) resets the flag!!
    ///
    virtual bool changedInfo() = 0;
};

/// @}

#endif
