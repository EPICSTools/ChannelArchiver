// -*- c++ -*-
// kasemir@lanl.gov

// Tools
#include <ToolsConfig.h>
// XML-RPC
#include <xmlrpc.h>
#include <xmlrpc_client.h>
// Storage
#include <CtrlInfo.h>
#include <RawValue.h>
// Local
#include "DataServer.h"

/// \ingroup DataServer
/// Interface class to the Archiver's network data server.
///
/// This class wraps the underlying XML-RPC calls, so it would
/// allow replacement of XML-RPC by something else.
/// It does not, however, go full length to implement e.g.
/// a DataReader class as defined in the Storage library.
class ArchiveDataClient
{
public:
    /// Severity info as returned by server's archiver.info method.
    class SeverityInfo
    {
    public:
        int       num;       ///< Code for this severity
        stdString text;      ///< String for this severity
        bool      has_value; ///< Use or ignore values w/ this severity?
        bool      txt_stat;  ///< Use status as text or count?
    };

    /// Archive info as returned by server's archiver.archives method.
    class ArchiveInfo
    {
    public:
        int       key;
        stdString name;
        stdString path;
    };

    /// Name info as returned by server's archiver.names method.
    class NameInfo
    {
    public:
        stdString name;
        epicsTime start, end;
    };

    /// Constructor
    ArchiveDataClient(const char *URL);

    /// Destructor
    ~ArchiveDataClient();

    /// Interface to data server's archiver.info method.
    bool getInfo(int &version, stdString &description,
                 stdVector<stdString> &how_strings,
                 stdVector<stdString> &stat_strings,
                 stdVector<SeverityInfo> &sevr_infos);

    /// Interface to data server's archiver.archives method.
    bool getArchives(stdVector<ArchiveInfo> &archives);

    /// Interface to data server's archiver.names method.
    bool getNames(int key, const stdString &pattern,
                  stdVector<NameInfo> &names);

    /// getValues will use this type of callback.

    /// Return true if you want to continue, otherwise false.
    typedef bool (*value_callback)(void *arg,
                                   const CtrlInfo &info,
                                   DbrType type, DbrCount count,
                                   const RawValue::Data *value);
    
    /// Interface to data server's archiver.values method.

    /// In order to avoid yet another copy of the data,
    /// each sample is fed to a callback routine.
    ///
    bool getValues(int key, stdVector<stdString> &names,
                   const epicsTime &start, const epicsTime &end,
                   int count, int how,
                   value_callback callback, void *callback_arg);
    
private:
    const char *URL;
    xmlrpc_env env;
    bool log_fault(); // on fault, logs fault & returns true
    bool decode_channel(xmlrpc_value *channel,
                        value_callback callback, void *callback_arg);
    bool decode_meta(xmlrpc_value *meta, CtrlInfo &ctrlinfo);
    bool decode_data(xmlrpc_int32 type, xmlrpc_int32 count,
                     xmlrpc_value *data_array, CtrlInfo &ctrlinfo,
                     value_callback callback, void *callback_arg);
};
