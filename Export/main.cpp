// Export/main.cpp

// Base
#include <epicsVersion.h>
// Tools
#include <BinaryTree.h>
#include <RegularExpression.h>
#include <epicsTimeHelper.h>
#include <ArgParser.h>
// Storage
#include <SpreadsheetReader.h>

bool verbose;

// "visitor" for BinaryTree of channel names
static void add_name2vector(const stdString &name, void *arg)
{
    stdVector<stdString> *names = (stdVector<stdString> *)arg;
    if (verbose)
        printf("%s\n", name.c_str());
    names->push_back(name);
}

void get_names_for_pattern(IndexFile &index,
                           stdVector<stdString> &names,
                           const stdString &pattern)
{
    if (verbose)
        printf("Expanding pattern '%s'\n", pattern.c_str());
    RegularExpression *regex =
        RegularExpression::reference(pattern.c_str());
    if (!regex)
    {
        fprintf(stderr, "Cannot allocate regular expression\n");
        return;
    }     
    IndexFile::NameIterator name_iter;
    if (!index.getFirstChannel(name_iter))
    {
        fprintf(stderr, "Cannot get channel name iterator\n");
        return;
    }
    // Put all names in binary tree
 	BinaryTree<stdString> channels;
    do
    {
        if (!regex->doesMatch(name_iter.getName()))
            continue; // skip what doesn't match regex
        channels.add(name_iter.getName());
    }
    while (index.getNextChannel(name_iter));
    regex->release();
    // Sorted dump of names
    channels.traverse(add_name2vector, (void *)&names);
}

// Used by list_channels & name_printer
class ChannelInfo
{
public:
    stdString name;
    epicsTime start, end;

    bool operator < (const ChannelInfo &rhs)
    { return name < rhs.name; }

    bool operator == (const ChannelInfo &rhs)
    { return name == rhs.name; }
};

// "visitor" for BinaryTree of channel names
static void name_printer(const ChannelInfo &info, void *arg)
{
    bool show_info = (bool) arg;
    stdString s, e;
    if (show_info)
        printf("%s\t%s\t%s\n", info.name.c_str(),
               epicsTimeTxt(info.start, s), epicsTimeTxt(info.end, e));
    else
        printf("%s\n", info.name.c_str());
}

bool list_channels(IndexFile &index, const stdString &pattern,
                   bool show_info)
{
    RegularExpression *regex = 0;
    if (pattern.length() > 0)
        regex = RegularExpression::reference(pattern.c_str());
    IndexFile::NameIterator name_iter;
    if (!index.getFirstChannel(name_iter))
    {
        fprintf(stderr, "Cannot get channel name iterator\n");
        return false;
    }
    // Put all names in binary tree
 	BinaryTree<ChannelInfo> channels;
    ChannelInfo info;
    do
    {
        if (regex && !regex->doesMatch(name_iter.getName()))
            continue; // skip what doesn't match regex
        info.name = name_iter.getName();
        if (show_info)
        {
            RTree *tree = index.getTree(info.name);
            if (tree)
            {
                tree->getInterval(info.start, info.end);
                delete tree;
            }
        }
        channels.add(info);
    }
    while (index.getNextChannel(name_iter));
    if (regex)
        regex->release();
    // Sorted dump of names
    channels.traverse(name_printer, (void *)show_info);
    return true;
}

bool dump_spreadsheet(IndexFile &index,
                      stdVector<stdString> names,
                      epicsTime *start, epicsTime *end,
                      double interpolate,
                      bool status_text,
                      stdString output_name,
                      bool gnuplot, bool image)
{
    SpreadsheetReader sheet(index, interpolate);
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

    FILE *gnufile = 0;
    if (gnuplot || image)
    {
        stdString cmd_name = output_name;
        cmd_name += ".plt";
        gnufile = fopen(cmd_name.c_str(), "wt");
        if (!gnufile)
            fprintf(stderr, "Cannot create %s\n",
                    cmd_name.c_str());       
    }
    if (gnufile)
    {
        fprintf(gnufile, "# GNUPlot command file for data in\n");
        fprintf(gnufile, "# %s\n", output_name.c_str());
        fprintf(gnufile, "# Generated by ArchiveExport " ARCH_VERSION_TXT "\n");
        fprintf(gnufile, "\n");
        fprintf(gnufile, "set data style steps\n");   
        fprintf(gnufile, "set timefmt \"%%d/%%m/%%Y %%H:%%M:%%S\"\n");	
        fprintf(gnufile, "set xdata time\n");
        fprintf(gnufile, "set xlabel \"Time\"\n");	
        fprintf(gnufile, "set format x \"%%d/%%m/%%Y\\n%%H:%%M:%%S\"\n");
        fprintf(gnufile, "set ylabel \"Data\"\n");
        fprintf(gnufile, "set grid\n");
        if (image)
        {
            fprintf(gnufile, "set terminal png small color "
                    "xfffcce x000040 xd1cfad\n");
            fprintf(gnufile, "set output '%s.png'\n",
                    output_name.c_str());
        } 
    }
    fprintf(f, "# Generated by ArchiveExport " ARCH_VERSION_TXT "\n");
    fprintf(f, "\n");
    fprintf(f, "# Time                       ");
    for (i=0; i<sheet.getNum(); ++i)
    {
        fprintf(f, "\t%s [%s]", sheet.getName(i).c_str(),
               sheet.getCtrlInfo(i).getUnits());
        if (status_text)
            fprintf(f, "\t");
        if (gnufile)
        {
            if (i==0)
                fprintf(gnufile, "plot '%s' using 1:%d title '%s [%s]'",
                        output_name.c_str(), 3+i,
                        sheet.getName(i).c_str(),
                        sheet.getCtrlInfo(i).getUnits());
            else
                fprintf(gnufile, ", '%s' using 1:%d title '%s [%s]'",
                        output_name.c_str(), 3+i,
                        sheet.getName(i).c_str(),
                        sheet.getCtrlInfo(i).getUnits());
        }
    }
    fprintf(f, "\n");
    if (gnufile)
    {
        fprintf(gnufile, "\n");
        fclose(gnufile);
    }
    while (ok)
    {
        if (end && sheet.getTime() >= *end)
            break;
        epicsTime2string(sheet.getTime(), time);
        fprintf(f, "%s", time.c_str());
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
                        value, &sheet.getCtrlInfo(i));
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
    CmdArgFlag   do_list    (parser, "list", "List all channels");
    CmdArgFlag   do_info    (parser, "info", "Time-range info on channels");
    CmdArgString start_time (parser, "start", "<time>",
                             "Format: \"mm/dd/yyyy[ hh:mm:ss[.nano-secs]]\"");
    CmdArgString end_time   (parser, "end", "<time>", "(exclusive)");
    CmdArgFlag   status_text(parser, "text",
                             "Include text column for status/severity");
    CmdArgString pattern    (parser, "match", "<reg. exp.>",
                             "Channel name pattern");
    CmdArgDouble interpol   (parser,
                             "interpolate", "<seconds>", "interpolate values");
    CmdArgString output     (parser,
                             "output", "<file>", "output to file");
    CmdArgFlag   GNUPlot    (parser,
                             "gnuplot", "generate GNUPlot command file");
    CmdArgFlag   image      (parser,
                             "Gnuplot", "generate GNUPlot output for Image");
#if 0
    CmdArgInt    reduce     (parser,
                             "reduce", "<# of buckets>", "reduce data to # buckets");
    CmdArgFlag   pipe       (parser,
                             "pipe", "run GNUPlot via pipe");
    CmdArgFlag   MLSheet    (parser,
                             "MLSheet", "generate spreadsheet for Matlab");
    CmdArgFlag   Matlab     (parser,
                             "Matlab", "generate Matlab command-file output");
#endif
    if (! parser.parse())
        return -1;
    if (parser.getArguments().size() < 1)
    {
        parser.usage();
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
    // Open index
    IndexFile index;
    if (!index.open(index_name.c_str()))
    {
        fprintf(stderr, "Cannot open index '%s'\n",
                index_name.c_str());
        return -1;
    }
    if (verbose)
        printf("Opened index '%s'\n", index_name.c_str());    
    if (names.size() == 0 && pattern.get().length() > 0)
        get_names_for_pattern(index, names, pattern);
    if (do_info)
        result = list_channels(index, pattern, true);
    if (do_list)
        result = list_channels(index, pattern, false);
    if (names.size() > 0)
        result = dump_spreadsheet(index, names, start, end,
                                  interpol, status_text,
                                  output, GNUPlot, image) ? 0 : -1;
    index.close();
 
    return result;
}

