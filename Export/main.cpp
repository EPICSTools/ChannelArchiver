// Export/main.cpp

// Base
#include <epicsVersion.h>
// Tools
#include <AutoPtr.h>
#include <BinaryTree.h>
#include <RegularExpression.h>
#include <epicsTimeHelper.h>
#include <ArgParser.h>
// Storage
#include <SpreadsheetReader.h>
#include <ListIndex.h>

int precision;
RawValue::NumberFormat format = RawValue::DEFAULT;
bool verbose;

// "visitor" for BinaryTree of channel names
static void add_name2vector(const stdString &name, void *arg)
{
    stdVector<stdString> *names = (stdVector<stdString> *)arg;
    if (verbose)
        printf("%s\n", name.c_str());
    names->push_back(name);
}

void get_names_for_pattern(Index &index,
                           stdVector<stdString> &names,
                           const stdString &pattern)
{
    if (verbose)
        printf("Expanding pattern '%s'\n", pattern.c_str());
    RegularExpression *regex = 0;
    if (pattern.length() > 0)
    {
        regex = RegularExpression::reference(pattern.c_str());
        if (!regex)
        {
            fprintf(stderr, "Cannot allocate regular expression\n");
            return;
        }
    }
    Index::NameIterator name_iter;
    if (!index.getFirstChannel(name_iter))
        return; // No names
    // Put all names in binary tree
    BinaryTree<stdString> channels;
    do
    {
        if (regex && !regex->doesMatch(name_iter.getName()))
            continue; // skip what doesn't match regex
        channels.add(name_iter.getName());
    }
    while (index.getNextChannel(name_iter));
    if (regex)
        regex->release();
    // Sorted dump of names
    channels.traverse(add_name2vector, (void *)&names);
}

bool list_channels(Index &index, stdVector<stdString> names, bool info)
{
    epicsTime start, end;
    stdString s, e;
    size_t i;
    for (i=0; i<names.size(); ++i)
    {
        if (info)
        {
            stdString directory;
            AutoPtr<RTree> tree(index.getTree(names[i], directory));
            if (!tree)
                continue;
            tree->getInterval(start, end);
            printf("%s\t%s\t%s\n", names[i].c_str(),
                   epicsTimeTxt(start, s), epicsTimeTxt(end, e));
        }
        else
            printf("%s\n", names[i].c_str());
    }
    return true;
}

bool dump_gnuplot(Index &index,
                  stdVector<stdString> names,
                  epicsTime *start, epicsTime *end,
                  ReaderFactory::How how, double delta,
                  stdString output_name,
                  bool image)
{
    stdVector<stdString> units;
    size_t i, e;
    stdString time, stat, val;
    const RawValue::Data *value;
    bool prev_line_was_data;
    bool is_array = false;
    
    FILE *f = fopen(output_name.c_str(), "wt");
    if (! f)
    {
        fprintf(stderr, "Cannot open %s\n",
                output_name.c_str());
        return false;
    }
    fprintf(f, "# Generated by ArchiveExport " ARCH_VERSION_TXT "\n");
    fprintf(f, "# Method: %s\n", ReaderFactory::toString(how, delta));
    for (i=0; i<names.size(); ++i)
    {
        fprintf(f, "# Channel '%s'\n", names[i].c_str());
        prev_line_was_data = false;
        DataReader *reader = ReaderFactory::create(index, how, delta);
        if (!reader)
        {
                fprintf(stderr, "Cannot create reader for '%s'\n",
                        names[i].c_str());
                return false;
        }
        value = reader->find(names[i], start);
        if (value)
        {
            if (reader->getCount() > 1)
                is_array = true;
            if (is_array  &&  i > 0)
            {
                fprintf(stderr,
                        "Your channel list contains arrays, "
                        "and you can only export a single array channel "
                        " by itself\n");
                return false;
            }
            units.push_back(reader->getInfo().getUnits());
        }
        else
            units.push_back("-no data-");
        if (!reader->channel_found)
            fprintf(stderr, "Warning: channel '%s' not in archive\n",
                    names[i].c_str());
        while (value)
        {
            if (end && RawValue::getTime(value) >= *end)
                break;
            epicsTime2string(RawValue::getTime(value), time);
            RawValue::getStatus(value, stat);            
            if (RawValue::isInfo(value))
            {   // Add one(!) empty line for break in data
                if (prev_line_was_data)
                    fprintf(f, "\n");
                fprintf(f, "# %s %s\n", time.c_str(), stat.c_str());
                prev_line_was_data = false;
            }
            else
            {
                if (reader->getCount() <= 1)
                {
                    RawValue::getValueString(
                        val, reader->getType(), reader->getCount(),
                        value, &reader->getInfo(), format, precision); 
                    fprintf(f, "%s\t%s\t%s\n",
                            time.c_str(), val.c_str(), stat.c_str());
                }
                else
                {
                    double dbl;
                    for (e=0; e<reader->getCount(); ++e)
                    {
                        RawValue::getDouble(reader->getType(),
                                            reader->getCount(),
                                            value, dbl, e);
                        fprintf(f, "%s\t%d\t%g\n", time.c_str(), e, dbl);
                    }
                    fprintf(f, "\n");
                }
                prev_line_was_data = true;
            }
            value = reader->next();
        }
        fprintf(f, "\n\n"); // GNUPlot index separator
        delete reader;
    }
    fclose(f);
    // Generate command file for GNUPlot
    stdString cmd_name = output_name;
    cmd_name += ".plt";
    if (!(f = fopen(cmd_name.c_str(), "wt")))
        fprintf(stderr, "Cannot create %s\n",
                cmd_name.c_str());
    fprintf(f, "# GNUPlot command file for data in\n");
    fprintf(f, "# the file '%s'\n", output_name.c_str());
    fprintf(f, "# Generated by ArchiveExport " ARCH_VERSION_TXT "\n");
    fprintf(f, "\n");
    fprintf(f, "set data style steps\n");   
    fprintf(f, "set timefmt \"%%m/%%d/%%Y %%H:%%M:%%S\"\n");    
    fprintf(f, "set xdata time\n");
    fprintf(f, "set xlabel \"Time\"\n");
    if (is_array)
        fprintf(f, "set format x \"%%m/%%d/%%Y %%H:%%M:%%S\"\n");
    else
        fprintf(f, "set format x \"%%m/%%d/%%Y\\n%%H:%%M:%%S\"\n");
    fprintf(f, "set ylabel \"Data\"\n");
    fprintf(f, "set grid\n");
    fprintf(f, "# X axis tick control:\n");
    fprintf(f, "# One tick per day:\n");
    fprintf(f, "# set xtics 86400\n");
    fprintf(f, "# Two tick per day:\n");
    fprintf(f, "# set xtics 43200\n");
    fprintf(f, "# One tick per hour:\n");
    fprintf(f, "# set xtics 3600\n");
    fprintf(f, "set mxtics 2\n");
    bool second_y_axis = names.size() == 2;
    if (second_y_axis)
    {
        fprintf(f, "# When using 2 channels:\n");
        fprintf(f, "set ylabel '%s'\n", names[0].c_str());
        fprintf(f, "set y2label '%s'\n", names[1].c_str());
        fprintf(f, "set y2tics\n");
    }    
    if (image)
    {
        fprintf(f, "set terminal png small color "
                "xfffcce x000040 xd1cfad\n");
        fprintf(f, "set output '%s.png'\n",
                output_name.c_str());
    }
    if (is_array)
    {
        fprintf(f, "set view 60,260,1\n");
        fprintf(f, "#set contour\n");
        fprintf(f, "set surface\n");
        fprintf(f, "#set hidden3d\n");
        fprintf(f, "splot '%s' using 1:3:4",
                output_name.c_str());
        fprintf(f, " title '%s' with lines\n",
                names[0].c_str());
    }
    else
    {
        fprintf(f, "plot '%s' index 0 using 1:3 title '%s [%s]'",
                output_name.c_str(),
                names[0].c_str(), units[0].c_str());
        for (i=1; i<names.size(); ++i)
            fprintf(f, ", '%s' index %d using 1:3 %s title '%s [%s]'",
                    output_name.c_str(), i,
                    ((second_y_axis && i==1) ? "axes x1y2" : ""),
                    names[i].c_str(), units[i].c_str());
    }
    fprintf(f, "\n");
    fclose(f);
    return true;
}

bool dump_spreadsheet(Index &index,
                      stdVector<stdString> names,
                      epicsTime *start, epicsTime *end,
                      bool raw_time,
                      bool status_text,
                      ReaderFactory::How how, double delta,
                      stdString output_name)
{
    SpreadsheetReader sheet(index, how, delta);
    bool ok = sheet.find(names, start);
    size_t i;
    stdString time, stat, val;
    const RawValue::Data *value;
    FILE *f = stdout;
    if (output_name.length() > 0)
        f = fopen(output_name.c_str(), "wt");
    if (! f)
    {
        fprintf(stderr, "Cannot open %s\n",
                output_name.c_str());
        return false;
    }
    for (i=0; i<sheet.getNum(); ++i)
        if (!sheet.getChannelFound(i))
            fprintf(stderr, "Warning: channel '%s' not in archive\n",
                    names[i].c_str());
    fprintf(f, "# Generated by ArchiveExport " ARCH_VERSION_TXT "\n");
    fprintf(f, "# Method: %s\n", ReaderFactory::toString(how, delta));
    fprintf(f, "\n");
    fprintf(f, "# Time                       ");
    if (raw_time)
        fprintf(f, "\tsecs");
    for (i=0; i<sheet.getNum(); ++i)
    {
        fprintf(f, "\t%s [%s]", sheet.getName(i).c_str(),
               sheet.getCtrlInfo(i).getUnits());
        if (status_text)
            fprintf(f, "\t");
    }
    fprintf(f, "\n");
    while (ok)
    {
        if (end && sheet.getTime() >= *end)
            break;
        fprintf(f, "%s", epicsTimeTxt(sheet.getTime(), time));
        if (raw_time)
        {
            epicsTimeStamp stamp = sheet.getTime();
            fprintf(f, "\t%lu", (unsigned long)stamp.secPastEpoch);
        }
        for (i=0; i<sheet.getNum(); ++i)
        {
            value = sheet.getValue(i);
            if (value)
            {
                RawValue::getStatus(value, stat);
                if (RawValue::isInfo(value))
                {
                    fprintf(f, "\t#N/A");
                    if (status_text)
                        fprintf(f, "\t%s", stat.c_str());
                }
                else
                {
                    RawValue::getValueString(
                        val, sheet.getType(i), sheet.getCount(i),
                        value, &sheet.getCtrlInfo(i), format, precision);
                    fprintf(f, "\t%s", val.c_str());
                    if (status_text)
                        fprintf(f, "\t%s", stat.c_str());
                }
            }
            else
            {
                fprintf(f, "\t#N/A");
                if (status_text)
                    fprintf(f, "\t");
            }
        }
        fprintf(f, "\n");
        ok = sheet.next();
    }
    if (f != stdout)
        fclose(f);
    return true;
}

int main(int argc, const char *argv[])
{
    int result = 0;
    initEpicsTimeHelper();
    CmdArgParser parser(argc, argv);
    parser.setHeader("Archive Export version " ARCH_VERSION_TXT ", "
                     EPICS_VERSION_STRING
                     ", built " __DATE__ ", " __TIME__ "\n\n");
    parser.setArgumentsInfo("<index file> {channel}");
    CmdArgFlag   be_verbose (parser, "verbose", "Verbose mode");
    CmdArgString pattern    (parser, "match", "<reg. exp.>",
                             "Channel name pattern");
    CmdArgFlag   do_list    (parser, "list", "List all channels");
    CmdArgFlag   do_info    (parser, "info", "Time-range info on channels");
    CmdArgString start_time (parser, "start", "<time>",
                             "Format: \"mm/dd/yyyy[ hh:mm:ss[.nano-secs]]\"");
    CmdArgString end_time   (parser, "end", "<time>", "(exclusive)");
    CmdArgFlag   status_text(parser, "text",
                             "Include text column for status/severity");
    CmdArgString output     (parser,
                             "output", "<file>", "Output to file");
    CmdArgDouble plotbin    (parser,
                             "plotbin", "<seconds>",
                             "Bin the raw data for plotting");
    CmdArgDouble average    (parser,
                             "average", "<seconds>", "average values");
    CmdArgDouble linear     (parser,
                             "linear", "<seconds>",
                             "Interpolate values linearly");
    CmdArgString format_txt (parser,
                             "format", "<decimal|engineering|exponential>",
                             "Use specific format for numbers");
    CmdArgInt    prec       (parser,
                             "precision", "<int>", "Precision of numbers");
    CmdArgFlag   GNUPlot    (parser,
                             "gnuplot", "Generate GNUPlot command file");
    CmdArgFlag   image      (parser,
                             "Gnuplot", "Generate GNUPlot output for Image");
    CmdArgFlag   raw_time   (parser, "raw_time",
                             "Include columns for EPICS time stamp");
    // defaults
    prec.set(-1);
    if (! parser.parse())
        return -1;
    if (parser.getArguments().size() < 1)
    {
        parser.usage();
        return -1;
    }
    precision = prec;
    if (!strncmp(format_txt.get().c_str(), "d", 1))
        format = RawValue::DECIMAL;
    else if (!strncmp(format_txt.get().c_str(), "en", 2))
        format = RawValue::ENGINEERING;
    else if (!strncmp(format_txt.get().c_str(), "ex", 2))
        format = RawValue::EXPONENTIAL;
    else if (format_txt.get().length() > 0)
    {
        fprintf(stderr, "Unknown format string '%s'\n", format_txt.get().c_str());
        return -1;
    }   
    verbose = be_verbose;
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
    // Index name
    stdString index_name = parser.getArgument(0);
    // Channel names
    stdVector<stdString> names;
    if (parser.getArguments().size() > 1)
    {
        if (! pattern.get().empty())
        {
            fputs("Pattern from '-m' switch is ignored\n"
                  "since a list of channels was also provided.\n", stderr);
        }
        // first argument was directory file name, skip that:
        for (size_t i=1; i<parser.getArguments().size(); ++i)
            names.push_back(parser.getArgument(i));
    }
    if ((GNUPlot || image) && output.get().length() == 0)
    {

        fprintf(stderr, "The -gnuplot/Gnuplot options require "
                "an -output file\n");
        return -1;    
    }
    // How?
    ReaderFactory::How how = ReaderFactory::Raw;
    double delta = 0.0;
    if (double(plotbin) > 0.0)
    {
        how = ReaderFactory::Plotbin;
        delta = double(plotbin);
    }
    else if (double(average) > 0.0)
    {
        how = ReaderFactory::Average;
        delta = double(average);
    }
    else if (double(linear) > 0.0)
    {
        how = ReaderFactory::Linear;
        delta = double(linear);
    }
    // Open index
    //IndexFile index(50);
    ListIndex index;
    if (!index.open(index_name.c_str()))
    {
        fprintf(stderr, "Cannot open index '%s'\n",
                index_name.c_str());
        return -1;
    }
    if (verbose)
        printf("Opened index '%s'\n", index_name.c_str());
    if (do_info  &&  names.size()<=0  &&  pattern.get().length()<=0)
        do_list.set(); // otherwise it'd be a NOP
    if (names.size() <= 0 &&
        (do_list  ||  pattern.get().length() > 0))
        get_names_for_pattern(index, names, pattern);
    if (do_info)
        result = list_channels(index, names, true);
    else if (do_list)
        result = list_channels(index, names, false);
    else if (names.size() > 0)
    {
        if (GNUPlot || image)
            result = dump_gnuplot(index, names, start, end,
                                  how, delta, output, image) ? 0 : -1;
        else
            result = dump_spreadsheet(index, names, start, end,
                                      raw_time, status_text, how, delta,
                                      output) ? 0 : -1;
    }
    index.close();
    if (end)
        delete end;
    if (start)
        delete start;
 
    return result;
}

