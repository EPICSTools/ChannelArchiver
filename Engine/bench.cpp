/*

Machine          hdparm -t     hdparm -T        bench            bench -read
------------------------------------------------------------------------------
500MHz Laptop    12.36 MB/sec  129.29 MB/sec    ~45000 vals/sec  200k vals/sec
bogart (800Mhz, RAID)                           ~80307 vals/sec  236k vals/sec

hdparm -t /dev/hda: Timing buffered disk reads,
hdparm -T /dev/hda: Timing buffer-cache reads.

Older results with LibIO:
blitz, 800 Mhz NT4.0             20000
ioc94, 266 Mhz RedHat 6.1         7740
gopher1, 500Mhz PIII, RH 6.1     14477
gopher0, 2x333Mhz, RAID5          8600

*/

// Tools
#include <ArgParser.h>
// Storage
#include <archiver_index.h>
#include <TreeDataWriter.h>
#include <DataWriter.h>
#include <DataReader.h>
#include <DirectoryFile.h>
#include <OldDataWriter.h>
#include <OldDataReader.h>
#include <DataFile.h>

bool write_samples(const stdString &index_name,
                   const stdString &channel_name,
                   size_t samples)
{
    IndexFile index;
    CtrlInfo info;

    if (!index.open(index_name.c_str(), false))
    {
        fprintf(stderr, "Cannot create index '%s'\n",
                index_name.c_str());
        return false;
    }
    info.setNumeric (2, "socks",
                     0.0, 10.0,
                     0.0, 1.0, 9.0, 10.0);
    DbrType dbr_type = DBR_TIME_DOUBLE;
    DbrCount dbr_count = 1;
    DataWriter * writer =
        new DataWriter(index,
                       channel_name, info,
                       dbr_type, dbr_count, 2.0,
                       samples);
    dbr_time_double *data =  RawValue::allocate(dbr_type, dbr_count, 1);
    data->status = 0;
    data->severity = 0;
    size_t i;
    for (i=0; i<samples; ++i)
    {
        data->value = (double) i;
        RawValue::setTime(data, epicsTime::getCurrent());
        if (!writer->add(data))
        {
            fprintf(stderr, "Write error with value %d/%d\n",
                    i, samples);
            break;
        }   
    }
    RawValue::free(data);
    delete writer;
    DataFile::close_all();
    index.close();
    return true;
}

bool tree_write_samples(const stdString &index_name,
                   const stdString &channel_name,
                   size_t samples)
{
    archiver_Index index;
    CtrlInfo info;

    if (!index.open(index_name.c_str(), false))
    {
        fprintf(stderr, "Cannot create index '%s'\n",
                index_name.c_str());
        return false;
    }
    info.setNumeric (2, "socks",
                     0.0, 10.0,
                     0.0, 1.0, 9.0, 10.0);
    DbrType dbr_type = DBR_TIME_DOUBLE;
    DbrCount dbr_count = 1;
    TreeDataWriter * writer =
        new TreeDataWriter(index,
                       channel_name, info,
                       dbr_type, dbr_count, 2.0,
                       samples);
    dbr_time_double *data =  RawValue::allocate(dbr_type, dbr_count, 1);
    data->status = 0;
    data->severity = 0;
    size_t i;
    for (i=0; i<samples; ++i)
    {
        data->value = (double) i;
        RawValue::setTime(data, epicsTime::getCurrent());
        if (!writer->add(data))
        {
            fprintf(stderr, "Write error with value %d/%d\n",
                    i, samples);
            break;
        }   
    }
    RawValue::free(data);
    delete writer;
    DataFile::close_all();
    index.close();
    return true;
}

bool old_write_samples(const stdString &index_name,
                   const stdString &channel_name,
                   size_t samples)
{
    DirectoryFile index;
    CtrlInfo info;

    if (!index.open(index_name, true))
    {
        fprintf(stderr, "Cannot create dir. file '%s'\n",
                index_name.c_str());
        return false;
    }
    info.setNumeric (2, "socks",
                     0.0, 10.0,
                     0.0, 1.0, 9.0, 10.0);
    DbrType dbr_type = DBR_TIME_DOUBLE;
    DbrCount dbr_count = 1;
    OldDataWriter * writer =
        new OldDataWriter(index,
                       channel_name, info,
                       dbr_type, dbr_count, 2.0,
                       samples);
    dbr_time_double *data =  RawValue::allocate(dbr_type, dbr_count, 1);
    data->status = 0;
    data->severity = 0;
    size_t i;
    for (i=0; i<samples; ++i)
    {
        data->value = (double) i;
        RawValue::setTime(data, epicsTime::getCurrent());
        if (!writer->add(data))
        {
            fprintf(stderr, "Write error with value %d/%d\n",
                    i, samples);
            break;
        }   
    }
    RawValue::free(data);
    delete writer;
    DataFile::close_all();
    return true;
}

size_t read_samples(const stdString &index_name,
                    const stdString &channel_name)
{
    IndexFile index;

    if (!index.open(index_name.c_str()))
    {
        fprintf(stderr, "Cannot open index '%s'\n",
                index_name.c_str());
        return 0;
    }
    size_t samples = 0;
    DataReader *reader = new RawDataReader(index);
    const RawValue::Data *data = reader->find(channel_name, 0, 0);
    while (data)
    {
        ++samples;
        data = reader->next();    
    }
    delete reader;
    DataFile::close_all();
    index.close();
    return samples;
}

size_t old_read_samples(const stdString &index_name,
                    const stdString &channel_name)
{
    DirectoryFile index;

    if (!index.open(index_name))
    {
        fprintf(stderr, "Cannot open dir. file '%s'\n",
                index_name.c_str());
        return 0;
    }
    size_t samples = 0;
    OldDataReader *reader = new OldDataReader(index);
    const RawValue::Data *data = reader->find(channel_name, 0);
    while (data)
    {
        ++samples;
        data = reader->next();    
    }
    delete reader;
    DataFile::close_all();
    return samples;
}

int main (int argc, const char *argv[])
{
    CmdArgParser parser(argc, argv);
    parser.setHeader("Archive Benchmark\n");
    parser.setArgumentsInfo("<index>");
    CmdArgFlag old(parser, "old", "Use old directory file");
    CmdArgInt samples(parser, "samples", "<count>",
                      "Number of samples to write");
    CmdArgString channel_name(parser, "channel", "<name>",
                      "Channel Name");
    CmdArgFlag do_read(parser, "read",
                       "Perform read test");
    
    // defaults
    samples.set(100000);
    channel_name.set("fred");

    if (parser.parse() == false)
        return -1;
    if (parser.getArguments().size() != 1)
    {
        parser.usage();
        return -1;
    }
    stdString index_name = parser.getArgument(0);
    size_t count;
    epicsTime start, stop;
    if (do_read)
    {
        start = epicsTime::getCurrent ();
        if (old)
            count = old_read_samples(index_name, channel_name.get());
        else
            count = read_samples(index_name, channel_name.get());
        stop = epicsTime::getCurrent ();
    }
    else
    {
        count = samples.get();
        start = epicsTime::getCurrent ();
        if (old)
            tree_write_samples(index_name, channel_name.get(), count);
        else
            write_samples(index_name, channel_name.get(), count);
        stop = epicsTime::getCurrent ();
    }
    
    double secs = stop - start;
    printf("%d values in %g seconds: %g vals/sec\n",
           count, secs,
           (double)count / secs);
    return 0;
}


