// GNUPlotExporter.cpp

#ifdef WIN32
#pragma warning (disable: 4786)
#endif

#include "../ArchiverConfig.h"
#include "ArchiveException.h"
#include "GNUPlotExporter.h"
#include "Filename.h"
#include <fstream>

const char *GNUPlotExporter::imageExtension()
{   return ".png"; }

static void printTime(FILE *f, const osiTime &time)
{
    int year, month, day, hour, min, sec;
    unsigned long nano;
    osiTime2vals (time, year, month, day, hour, min, sec, nano);
    fprintf(f, "%02d/%02d/%04d %02d:%02d:%02d.%09ld",
            month, day, year, hour, min, sec, nano);
}

static void printTimeAndValue(FILE *f, const osiTime &time,
                              const ValueI &value)
{
    int prec;

    if (value.getCtrlInfo() &&
        value.getCtrlInfo()->getPrecision() > 0)
        prec = value.getCtrlInfo()->getPrecision();
    else
        prec = 0;
    
    ::printTime(f, time);
    if (value.getCount() > 0)
    {
        for (size_t ai=0; ai<value.getCount(); ++ai)
        {
            if (prec > 0)
                fprintf(f, "\t%.*f\n", prec, value.getDouble(ai));
            else
                fprintf(f, "\t%g\n", value.getDouble(ai));
        }
    }
    else
    {
        stdString txt;
        value.getStatus(txt);
        if (prec > 0)
            fprintf(f, "\t%.*f\t%s\n",
                    prec, value.getDouble(), txt.c_str());
        else
            fprintf(f, "\t%g\t%s\n",
                    value.getDouble(), txt.c_str());
    }
}

GNUPlotExporter::GNUPlotExporter(Archive &archive, const stdString &filename)
        : SpreadSheetExporter (archive.getI(), filename)
{
    _make_image = false;
    _use_pipe = false;
    if (filename.empty())
        throwDetailedArchiveException(Invalid, "empty filename");
}           

GNUPlotExporter::GNUPlotExporter(ArchiveI *archive, const stdString &filename)
        : SpreadSheetExporter (archive, filename)
{
    _make_image = false;
    _use_pipe = false;
    if (filename.empty())
        throwDetailedArchiveException(Invalid, "empty filename");
}           

void GNUPlotExporter::exportChannelList(
    const stdVector<stdString> &channel_names)
{
    size_t num = channel_names.size();
    char info[300];
    if (num > _max_channel_count)
    {
        sprintf(info,
                "You tried to export %d channels.\n"
                "For performance reason you are limited "
                "to export %d channels at once",
                num, _max_channel_count);
        throwDetailedArchiveException(Invalid, info);
        return;
    }

    stdString directory, data_name, script_name, image_name;
    Filename::getDirname  (_filename, directory);
    data_name = _filename;
    script_name = _filename;
    script_name += ".plt";
    image_name = _filename;
    image_name += imageExtension();
    
    FILE *f;
    f = fopen(data_name.c_str(), "wt");
    if (! f)
        throwDetailedArchiveException(WriteError, data_name);
    
    fprintf(f, "# GNUPlot Exporter V " VERSION_TXT "\n");
    fprintf(f, "#\n");
    
    stdVector<stdString> plotted_channels;
    stdString channel_desc;
    Archive         archive(_archive);
    ChannelIterator channel(archive);
    ValueIterator   value(archive);
    osiTime time;
    stdString txt;
    for (size_t i=0; i<num; ++i)
    {
        if (! archive.findChannelByName(channel_names[i], channel))
        {
            archive.detach();
            sprintf(info, "Cannot find channel '%s' in archive",
                    channel_names[i].c_str());
            throwDetailedArchiveException (ReadError, info);
            return;
        }
        if (! channel->getValueBeforeTime(_start, value) &&
            ! channel->getValueAfterTime(_start, value))
            continue; // nothing in time range
        
        if (value->getCount() > 1)
        {
            _is_array = true;
            if (num > 1)
            {
                archive.detach();
                sprintf(info,
                        "Array channels like '%s' can only be exported "
                        "on their own, "
                        "not together with another channel",
                        channel_names[i].c_str());
                throwDetailedArchiveException(Invalid, info);
            }
        }
        
        // Header: Channel name [units]
        if (value->getCtrlInfo() &&
            value->getCtrlInfo()->getUnits())
        {
            channel_desc = channel->getName();
            channel_desc += " [";
            channel_desc += value->getCtrlInfo()->getUnits();
            channel_desc += "]";
        }
        else
            channel_desc = channel->getName();
        fprintf(f, "# %s\n", channel_desc.c_str());
        
        // Dump values for this channel. We really want to see
        // something for _start and _end, patching the original
        // time stamps to get there.
        bool have_anything = false;
        bool last_was_data = false;
        while (value)
        {
            time = value->getTime();
            if (isValidTime(_start) && time < _start) // start hack
            {
                fprintf(f, "# extrapolated onto start time from ");
                ::printTime(f, time);
                fprintf(f, "\n");
                time = _start;
            }
            if (isValidTime(_end) && time > _end)
                break;
            if (value->isInfo())
            {
                // Show as comment & empty line -> "gap" in GNUplot graph.
                // But only one empty line, multiples separate channels!
                fprintf(f, "# ");
                ::printTime(f, time);
                value->getStatus(txt);
                fprintf(f, "\t%s\n", txt.c_str());
                if (last_was_data)
                    fprintf(f, "\n");
                last_was_data = false;
            }
            else
            {
                have_anything = true;
                ++_data_count;
                printTimeAndValue(f, time, *value);
                last_was_data = true;
            }
            ++value;
        }
        // time == stamp of last value.
        // If _start was in the future, time==_start.
        if (last_was_data && isValidTime(_end)) // end hack
        {
            fprintf(f, "# extrapolated onto end time from ");
            ::printTime(f, time);
            fprintf(f, "\n");
            --value;
            if (value)
            {
                osiTime now = osiTime::getCurrent();
                if (now > time) // extrapolate until "now"
                    time = now;
                if (_end < time) // but not beyond "_end"
                    time = _end;
                printTimeAndValue(f, time, *value);
            }
        }
        
        if (have_anything)
            plotted_channels.push_back(channel_desc);
        // Gap separates channels
        fprintf(f, "\n\n\n");
    }
    archive.detach();
    fclose(f);

    // Generate script
    if (_use_pipe)
    {
#ifdef WIN32
        f = _popen(GNUPLOT_PIPE, "w");
#else
        f = popen(GNUPLOT_PIPE, "w");
#endif
        if (!f)
            throwDetailedArchiveException(OpenError, GNUPLOT_PIPE);
    }
    else
    {
        fopen(script_name.c_str(), "wt");
        if (!f)
            throwDetailedArchiveException(WriteError, script_name);
    }

    fprintf(f, "# GNUPlot Exporter V " VERSION_TXT "\n");
    fprintf(f, "#\n\n");
    if (!directory.empty())
    {
        fprintf(f, "# Change to the data directory:\n");
        fprintf(f, "cd '%s'\n", directory.c_str());
    }
    if (_make_image)
    {
        fprintf(f, "# Create image:\n");
        fprintf(f, "set terminal png small color\n");
        fprintf(f, "set output '%s'\n", image_name.c_str());
    }

    fprintf(f, "# x-axis is time axis:\n");
    fprintf(f, "set xdata time\n");
    fprintf(f, "set timefmt \"%%m/%%d/%%Y %%H:%%M:%%S\"\n");
    if (_is_array)
        fprintf(f, "set format x \"%%m/%%d/%%Y, %%H:%%M:%%S\"\n");
    else
        fprintf(f, "set format x \"%%m/%%d/%%Y\\n%%H:%%M:%%S\"\n");
    fprintf(f, "\n");
#if 0
    // No clue what to do. Sometimes the default works nicely,
    // sometimes it's a mess. But I haven't found the perfect
    // formula for placing the ticks, either, so I leave it with
    // the default and blame it all on GNUPlot...
    fprintf(f, "# Set labels/ticks on x-axis for every xxx seconds:\n");
    double secs = (double(_end) - double(_start)) / 5;
    if (secs > 0.0)
        fprintf(f, "set xtics %g\n", secs);
#endif
    fprintf(f, "set grid\n");
    fprintf(f, "\n");
    fprintf(f, "set key title \"Channels:\"\n");
    fprintf(f, "set key box\n");

    num = plotted_channels.size();
    if (num == 2)
    {
        fprintf(f, "# When using 2 channels:\n");
        fprintf(f, "set ylabel '%s'\n", plotted_channels[0].c_str());
        fprintf(f, "set y2label '%s'\n", plotted_channels[1].c_str());
        fprintf(f, "set y2tics\n");
    }
    
    if (_is_array)
    {
        fprintf(f, "set view 60,260,1\n");
        fprintf(f, "#set contour\n");
        fprintf(f, "set surface\n");
        if (_data_count < 50)
            fprintf(f, "set hidden3d\n");
        else
            fprintf(f, "#set hidden3d\n");
        fprintf(f, "splot '%s' using 1:3:4",
                data_name.c_str());
        fprintf(f, " title '%s' with lines\n",
                plotted_channels[0].c_str());
    }
    else
    {
        fprintf(f, "plot ");
        for (size_t i=0; i<num; ++i)
        {
            fprintf(f, "'%s' index %d using 1:3",
                    data_name.c_str(), i);
            if (num==2 && i==1)
                fprintf(f, " axes x1y2");
            fprintf(f, " title '%s' with steps",
                    plotted_channels[i].c_str());
            if (i < num-1)
                fprintf(f, ", ");
        }
        fprintf(f, "\n");
    }
    if (_use_pipe)
    {
#ifdef WIN32
        fflush(f);
        _pclose(f);
#else
        pclose(f);
#endif
    }
    else
        fclose(f);
}






