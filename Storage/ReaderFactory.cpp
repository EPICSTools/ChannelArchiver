// DataReader.cpp

// Tools
#include "MsgLogger.h"
#include "Filename.h"
// Storage
#include "DataFile.h"
#include "DataReader.h"
#include "AverageReader.h"
#include "PlotReader.h"
#include "LinearReader.h"

#undef DEBUG_DATAREADER

const char *ReaderFactory::toString(How how, double delta)
{
    static char buf[100];
    switch (how)
    {
        case Raw:
            return "Raw Data";
        case Plotbin:
            sprintf(buf, "Plot-Binning, %g sec bins", delta);
            return buf;
        case Average:
            sprintf(buf, "Averaging every %g seconds", delta);
            return buf;
        case Linear:
            sprintf(buf, "Linear Interpolation every %g seconds", delta);
            return buf;
        default:
            return "<unknown>";
    };  
}

DataReader *ReaderFactory::create(Index &index, How how, double delta)
{
    if (how == Raw  ||  delta <= 0.0)
        return new RawDataReader(index);
    else if (how == Plotbin)
        return new PlotReader(index, delta);
    else if (how == Average)
        return new AverageReader(index, delta);
    else
        return new LinearReader(index, delta);
    return 0;
}


