// ArchiveDataTool ----------

// System
#include <stdio.h>
// Base
#include <epicsVersion.h>
// Tools
#include <AutoPtr.h>
#include <ArchiverConfig.h>
#include <ArgParser.h>
#include <BenchTimer.h>
#include <epicsTimeHelper.h>
// Storage
#include <DirectoryFile.h>
#include <DataFile.h>
#include <DataReader.h>
#include <DataWriter.h>

int verbose;

// TODO: export/import: ASCII or XML
// TODO: delete/rename channel

void show_hash_info(const stdString &index_name)
{
    IndexFile index(3);
    if (!index.open(index_name))
    {
        fprintf(stderr, "Cannot open index '%s'\n",
                index_name.c_str());
        return;
    }
    index.showStats(stdout);
    index.close();
}

void list_names(const stdString &index_name)
{
    IndexFile index(3);
    IndexFile::NameIterator names;
    epicsTime stime, etime, t0, t1;
    stdString start, end;
    bool ok, first =true;
    if (!index.open(index_name))
    {
        fprintf(stderr, "Cannot open index '%s'\n",
                index_name.c_str());
        return;
    }
    for (ok = index.getFirstChannel(names);
         ok;
         ok = index.getNextChannel(names))
    {
        AutoPtr<RTree> tree(index.getTree(names.getName()));
        if (!tree)
        {
            fprintf(stderr, "Cannot get tree for channel '%s'\n",
                    names.getName().c_str());
            continue;
        }
        tree->getInterval(stime, etime);
        printf("Channel '%s' (M=%d): %s - %s\n",
               names.getName().c_str(),
               tree->getM(),
               epicsTimeTxt(stime, start),
               epicsTimeTxt(etime, end));
        if (first)
        {
            t0 = stime;
            t1 = etime;
            first = false;
        }
        else
        {
            if (t0 > stime)
                t0 = stime;
            if (t1 < etime)
                t1 = etime;
        }
    }
    index.close();
    printf("Overall time range: %s - %s\n",
           epicsTimeTxt(t0, start), epicsTimeTxt(t1, end));
}

DataHeader *get_dataheader(const stdString &dir, const stdString &file,
                           FileOffset offset)
{
    DataFile *datafile = DataFile::reference(dir, file, false);
    if (!datafile)
        return 0;
    DataHeader *header = datafile->getHeader(offset);
    datafile->release(); // ref'ed by header
    return header; // might be NULL
}

// Try to determine the period and a good guess
// for the number of samples for a channel.
void determine_period_and_samples(IndexFile &index,
                                  const stdString &channel,
                                  double &period, size_t &num_samples)
{
    period      = 1.0; // initial guess
    num_samples = 10;
    // Whatever fails only means that there is no data,
    // so the initial guess remains OK.
    // Otherwise we peek into the last datablock.
    AutoPtr<RTree> tree(index.getTree(channel));
    if (!tree)
        return;
    RTree::Node node(tree->getM(), false);
    RTree::Datablock block;
    int i;
    if (!tree->getLastDatablock(node, i, block))
        return;
    DataHeader *header;
    if (!(header = get_dataheader(index.getDirectory(),
                                  block.data_filename, block.data_offset)))
        return;
    period = header->data.period;
    num_samples = header->data.num_samples;
    if (verbose > 2)
        printf("Last source buffer: Period %g, %lu samples.\n",
               period, (unsigned long) num_samples);
    delete header;
}

void copy(const stdString &index_name, const stdString &copy_name,
          int RTreeM, const epicsTime *start, const epicsTime *end)
{
    IndexFile               index(RTreeM), new_index(RTreeM);
    IndexFile::NameIterator names;
    bool                    channel_ok;
    const RawValue::Data   *value;
    size_t                  channel_count = 0, value_count = 0, back_count = 0;
    size_t                  count, back;
    double                  period = 1.0;
    size_t                  num_samples = 4096;    
    BenchTimer              timer;
    stdString               dir1, dir2;
    Filename::getDirname(index_name, dir1);
    Filename::getDirname(copy_name, dir2);
    if (dir1 == dir2)
    {
        printf("You have to assert that the new index (%s)\n"
               "is in a  directory different from the old index\n"
               "(%s)\n", copy_name.c_str(), index_name.c_str());
        return;
    }
    if (! (index.open(index_name) && new_index.open(copy_name, false)))
        return;
    if (verbose)
        printf("Copying values from '%s' to '%s'\n",
               index_name.c_str(), copy_name.c_str());
    RawDataReader reader(index);
    for (channel_ok = index.getFirstChannel(names);
         channel_ok;  channel_ok = index.getNextChannel(names))
    {
        const stdString &channel_name = names.getName();
        if (verbose > 1)
        {
            printf("Channel '%s': ", channel_name.c_str());
            if (verbose > 3)
                printf("\n");
            fflush(stdout);
        }
        DataWriter *writer = 0;
        count = back = 0;
        determine_period_and_samples(index, channel_name, period, num_samples);
        value = reader.find(channel_name, start);
        while (value && start && RawValue::getTime(value) < *start)
        {
            if (verbose > 2)
                printf("Skipping sample before start time\n");
            value = reader.next();
        }
        for (/**/; value; value = reader.next())
        {
            if (end  &&  RawValue::getTime(value) >= *end)
                break;
            if (!writer || reader.changedType() || reader.changedInfo())
            {
                delete writer;
                writer = new DataWriter(
                    new_index, names.getName(),
                    reader.getInfo(), reader.getType(), reader.getCount(),
                    period, num_samples);
                if (!writer)
                {
                    printf("Cannot create DataWriter\n");
                    return;
                }
            }
            if (RawValue::getTime(value) < writer->getLastStamp())
            {
                ++back;
                if (verbose > 2)
                    printf("Skipping %lu back-in-time values\r",
                           (unsigned long) back);
                continue;
            }
            DataWriter::DWA add_status = writer->add(value);
            if (add_status == DataWriter::DWA_Error)
            {
                printf("DataWriter::add failed\n");
                break;
            }
            else if (add_status == DataWriter::DWA_Back)
            {
                printf("DataWriter::add still claims back-in-time\n");
                continue;
            }
            ++count;
            if (verbose > 3 && (count % 1000 == 0))
                printf("Copied %lu values\r", (unsigned long) count);
                
        }
        delete writer;
        if (verbose > 1)
        {
            if (back)
                printf("%lu values, %lu back-in-time                       \n",
                       (unsigned long) count, (unsigned long) back);
            else
                printf("%lu values                                         \n",
                       (unsigned long) count);
        }
        ++channel_count;
        value_count += count;
        back_count += back;
    }
    new_index.close();
    index.close();
    timer.stop();
    if (verbose)
    {
        printf("Total: %lu channels, %lu values\n",
               (unsigned long) channel_count, (unsigned long) value_count);
        printf("Skipped %lu back-in-time values\n",
               (unsigned long) back_count);
        printf("Runtime: %s\n", timer.toString().c_str());
    }
}

void convert_dir_index(int RTreeM,
                       const stdString &dir_name, const stdString &index_name)
{
    DirectoryFile dir;
    if (!dir.open(dir_name))
        return;
	
    DirectoryFileIterator channels = dir.findFirst();
    IndexFile index(RTreeM);
    if (verbose)
        printf("Opened directory file '%s'\n", dir_name.c_str());
    if (!index.open(index_name, false))
        return;
    if (verbose)
        printf("Created index '%s'\n", index_name.c_str());
    for (/**/;  channels.isValid();  channels.next())
    {
        if (verbose)
            printf("Channel '%s':\n", channels.entry.data.name);
        if (! Filename::isValid(channels.entry.data.first_file))
        {
            if (verbose)
                printf("No values\n");
            continue;
        }
        AutoPtr<RTree> tree(index.addChannel(channels.entry.data.name));
        if (!tree)
        {
            fprintf(stderr, "Cannot add channel '%s' to index '%s'\n",
                    channels.entry.data.name, index_name.c_str());
            continue;
        }   
        DataFile *datafile =
            DataFile::reference(dir.getDirname(),
                                channels.entry.data.first_file, false);
        AutoPtr<DataHeader> header(
            datafile->getHeader(channels.entry.data.first_offset));
        datafile->release();
        while (header && header->isValid())
        {
            if (verbose)
            {
                stdString start, end;
                epicsTime2string(header->data.begin_time, start);
                epicsTime2string(header->data.end_time, end);    
                printf("'%s' @ 0x0%lX: %s - %s\n",
                       header->datafile->getBasename().c_str(),
                       header->offset,
                       start.c_str(),
                       end.c_str());
            }
            if (!tree->insertDatablock(
                    header->data.begin_time, header->data.end_time,
                    header->offset,  header->datafile->getBasename()))
            {
                fprintf(stderr, "insertDatablock failed for channel '%s'\n",
                        channels.entry.data.name);
                break;
            }
            header->read_next();
        }
    }
    DataFile::close_all();
    index.close();
}


void convert_index_dir(const stdString &index_name, const stdString &dir_name)
{
    IndexFile::NameIterator names;
    IndexFile index(50);
    RTree::Datablock block;
    stdString start_file, end_file;
    FileOffset start_offset, end_offset;
    epicsTime start_time, end_time;
    bool ok;
    if (!index.open(index_name.c_str()))
        return;
    DirectoryFile dir;
    if (!dir.open(dir_name, true))
    {
        fprintf(stderr, "Cannot create  '%s'\n", dir_name.c_str());
        return;
    }
    for (ok=index.getFirstChannel(names); ok; ok=index.getNextChannel(names))
    {
        if (verbose)
            printf("Channel '%s':\n", names.getName().c_str());
        AutoPtr<const RTree> tree(index.getTree(names.getName()));
        if (!tree)
        {
            printf("No RTree for channel '%s'\n", names.getName().c_str());
            continue;
        }
        RTree::Node node(tree->getM(), false);
        int i;
        if (!tree->getFirstDatablock(node, i, block))
        {
            printf("No first datablock for channel '%s'\n",
                   names.getName().c_str());
            continue;
        }
        start_file   = block.data_filename;
        start_offset = block.data_offset;
        if (!tree->getLastDatablock(node, i, block))
        {
            printf("No last datablock for channel '%s'\n",
                   names.getName().c_str());
            continue;
        }
        end_file   = block.data_filename;
        end_offset = block.data_offset;
        // Check by reading first+last buffer & getting times
        bool times_ok = false;
        DataHeader *header;
        header = get_dataheader(index.getDirectory(), start_file, start_offset);
        if (header)
        {
            start_time = header->data.begin_time;
            delete header;
            header = get_dataheader(index.getDirectory(),end_file, end_offset);
            if (header)
            {
                end_time = header->data.end_time;
                delete header;
                times_ok = true;
            }
        }
        if (times_ok)
        {
            stdString start, end;
            epicsTime2string(start_time, start);
            epicsTime2string(end_time, end);
            if (verbose)
                printf("%s - %s\n",
                       start.c_str(), end.c_str());
            DirectoryFileIterator dfi;
            dfi = dir.find(names.getName());
            if (!dfi.isValid())
                dfi = dir.add(names.getName());
            dfi.entry.setFirst(start_file, start_offset);
            dfi.entry.setLast(end_file, end_offset);
            dfi.save();
        }
    }
    index.close();
}

unsigned long dump_datablocks_for_channel(IndexFile &index,
                                          const stdString &channel_name,
                                          unsigned long &direct_count,
                                          unsigned long &chained_count)
{
    direct_count = chained_count = 0;
    AutoPtr<RTree> tree(index.getTree(channel_name));
    if (! tree)
        return 0;
    RTree::Datablock block;
    RTree::Node node(tree->getM(), true);
    stdString start, end;
    int idx;
    bool ok;
    if (verbose > 2)
        printf("Datablocks for channel '%s': ", channel_name.c_str());
    for (ok = tree->getFirstDatablock(node, idx, block);
         ok;
         ok = tree->getNextDatablock(node, idx, block))
    {
        ++direct_count;
        if (verbose > 2)
            printf("'%s' @ 0x%lX: %s - %s\n",
                   block.data_filename.c_str(), block.data_offset,
                   epicsTimeTxt(node.record[idx].start, start),
                   epicsTimeTxt(node.record[idx].end, end));
        while (tree->getNextChainedBlock(block))
        {
            ++chained_count;
            if (verbose > 2)
                printf("---  '%s' @ 0x%lX\n",
                       block.data_filename.c_str(), block.data_offset);
        }
    }
    return direct_count + chained_count;
}

unsigned long dump_datablocks(const stdString &index_name,
                              const stdString &channel_name)
{
    unsigned long direct_count, chained_count;
    IndexFile index(3);
    if (!index.open(index_name))
        return 0;
    dump_datablocks_for_channel(index, channel_name,
                                direct_count, chained_count);
    index.close();
    printf("%ld data blocks, %ld hidden blocks, %ld total\n",
           direct_count, chained_count,
           direct_count + chained_count);
    return direct_count + chained_count;
}

unsigned long dump_all_datablocks(const stdString &index_name)
{
    unsigned long total = 0, direct = 0, chained = 0, channels = 0;
    IndexFile index(3);
    bool ok;
    if (!index.open(index_name))
        return 0;
    IndexFile::NameIterator names;
    for (ok = index.getFirstChannel(names);
         ok;  ok = index.getNextChannel(names))
    {
        ++channels;
        unsigned long direct_count, chained_count;
        total += dump_datablocks_for_channel(index, names.getName(),
                                             direct_count, chained_count);
        direct += direct_count;
        chained += chained_count;
    }
    index.close();
    printf("Total: %ld channels, %ld datablocks\n", channels, total);
    printf("       %ld visible datablocks, %ld hidden\n", direct, chained);
    return total;
}

void dot_index(const stdString &index_name, const stdString channel_name,
               const stdString &dot_name)
{
    IndexFile index(3);
    if (!index.open(index_name))
    {
        fprintf(stderr, "Cannot open index '%s'\n",
                index_name.c_str());
        return;
    }
    AutoPtr<RTree> tree(index.getTree(channel_name));
    if (!tree)
    {
        fprintf(stderr, "Cannot find '%s' in index '%s'\n",
                channel_name.c_str(), index_name.c_str());
        return;
    }
    tree->makeDot(dot_name.c_str());
    index.close();
}

bool seek_time(const stdString &index_name,
               const stdString &channel_name,
               const stdString &start_txt)
{
    epicsTime start;
    if (!string2epicsTime(start_txt, start))
    {
        fprintf(stderr, "Cannot convert '%s' to time stamp\n",
                start_txt.c_str());
        return false;
    }
    IndexFile index(3);
    if (!index.open(index_name))
        return false;
    AutoPtr<RTree> tree(index.getTree(channel_name));
    if (tree)
    {
        RTree::Datablock block;
        RTree::Node node(tree->getM(), true);
        int idx;
        if (tree->searchDatablock(start, node, idx, block))
        {
            stdString s, e;
            printf("Found block %s - %s\n",
                   epicsTimeTxt(node.record[idx].start, s),
                   epicsTimeTxt(node.record[idx].end, e));
        }
        else
            printf("Nothing found\n");
    }
    else
        fprintf(stderr, "Cannot find channel '%s'\n", channel_name.c_str());
    index.close();
    return true;
}

bool check (const stdString &index_name)
{
    IndexFile index(3);
    if (!index.open(index_name))
        return false;
    bool ok = index.check(verbose);
    index.close();
    return ok;
}

int main(int argc, const char *argv[])
{
    initEpicsTimeHelper();

    CmdArgParser parser(argc, argv);
    parser.setHeader("Archive Data Tool version " ARCH_VERSION_TXT ", "
                     EPICS_VERSION_STRING
                      ", built " __DATE__ ", " __TIME__ "\n\n"
                     );
    parser.setArgumentsInfo("<index-file>");
    CmdArgFlag help          (parser, "help", "Show help");
    CmdArgInt verbosity      (parser, "verbose", "<level>", "Show more info");
    CmdArgFlag list_index    (parser, "list", "List channel name info");
    CmdArgString copy_index  (parser, "copy", "<new index>", "Copy channels");
    CmdArgString start_time  (parser, "start", "<time>",
                              "Format: \"mm/dd/yyyy[ hh:mm:ss[.nano-secs]]\"");
    CmdArgString end_time    (parser, "end", "<time>", "(exclusive)");
    CmdArgDouble file_limit  (parser, "file_limit", "<MB>", "File Size Limit");
    CmdArgString dir2index   (parser, "dir2index", "<dir. file>",
                              "Convert old directory file to index");
    CmdArgString index2dir   (parser, "index2dir", "<dir. file>",
                              "Convert index to old directory file");
    CmdArgInt RTreeM         (parser, "M", "<1-100>", "RTree M value");
    CmdArgFlag dump_blocks   (parser, "blocks", "List channel's data blocks");
    CmdArgFlag all_blocks    (parser, "Blocks", "List all data blocks");
    CmdArgString dotindex    (parser, "dotindex", "<dot filename>",
                              "Dump contents of RTree index into dot file");
    CmdArgString channel_name(parser, "channel", "<name>", "Channel name");
    CmdArgFlag hashinfo      (parser, "hashinfo", "Show Hash table info");
    CmdArgString seek_test   (parser, "seek", "<time>", "Perform seek test");
    CmdArgFlag test          (parser, "test", "Perform some consistency tests");
    // Defaults
    RTreeM.set(50);
    file_limit.set(100.0);
    // Get Arguments
    if (! parser.parse())
        return -1;
    if (help   ||   parser.getArguments().size() != 1)
    {
        parser.usage();
        return -1;
    }
    // Consistency checks
    if ((dump_blocks ||
         dotindex.get().length() > 0  ||
         seek_test.get().length() > 0)
        && channel_name.get().length() <= 0)
    {
        fprintf(stderr,
                "Options 'blocks' and 'dotindex' require 'channel'.\n");
        return -1;
    }
    verbose = verbosity;
    stdString index_name = parser.getArgument(0);
    DataWriter::file_size_limit = (unsigned long)(file_limit*1024*1024);
    if (file_limit < 10.0)
        fprintf(stderr, "file_limit values under 10.0 MB are not useful\n");
    // Start/end time
    epicsTime *start = 0, *end = 0;
    stdString txt;
	if (start_time.get().length() > 0)
    {
        start = new epicsTime;
        string2epicsTime(start_time.get(), *start);
        if (verbose)
            printf("Using start time %s\n", epicsTimeTxt(*start, txt));
    }
	if (end_time.get().length() > 0)
    {
        end = new epicsTime();
        string2epicsTime(end_time.get(), *end);
        if (verbose)
            printf("Using end time   %s\n", epicsTimeTxt(*end, txt));
    }
    // What's requested?
    if (list_index)
        list_names(index_name);
    else if (copy_index.get().length() > 0)
    {
        copy(index_name, copy_index, RTreeM, start, end);
        return 0;
    }
    else if (hashinfo)
        show_hash_info(index_name);
    else if (dir2index.get().length() > 0)
    {
        convert_dir_index(RTreeM, dir2index, index_name);
        return 0;
    }
    else if (index2dir.get().length() > 0)
    {
        convert_index_dir(index_name, index2dir);
        return 0;
    }
    else if (all_blocks)
    {
        dump_all_datablocks(index_name);
        return 0;
    }
    else if (dump_blocks)
    {
        dump_datablocks(index_name, channel_name);
        return 0;
    }
    else if (dotindex.get().length() > 0)
    {
        dot_index(index_name, channel_name, dotindex);
        return 0;
    }
    else if (seek_test.get().length() > 0)
    {
        seek_time(index_name, channel_name, seek_test);
        return 0;
    }
    else if (test)
    {
        return check(index_name) ? 0 : -1;
    }
    else
    {
        parser.usage();
        return -1;
    }
    
    return 0;
}
