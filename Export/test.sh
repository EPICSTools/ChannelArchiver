#!/bin/sh

# Use the tool from this source tree,
# not one that happens to be in the PATH.
EXPORT=O.$EPICS_HOST_ARCH/ArchiveExport

function check()
{
    output=test/$1
    previous=test/$1.OK
    info=$2
    diff -q $output $previous
    if [ $? -eq 0 ]
    then
        echo "OK : $info"
    else
        echo "FAILED : $info. Check diff $output $previous"
        exit 1
    fi
}

echo ""
echo "Export Test"
echo "***********"

INDEX=../DemoData/index 
DEL=""

$EXPORT $INDEX -l >test/list
check list "Channel list."
DEL="$DEL list"

$EXPORT $INDEX -i >test/info
check info "Channel info."
DEL="$DEL info"

$EXPORT $INDEX -i  -m '(fre)|(ja)' >test/pattern
check pattern "Channel info with match pattern."
DEL="$DEL pattern"

$EXPORT $INDEX fred >test/fred
check fred "Full single channel dump."
DEL="$DEL fred"

$EXPORT $INDEX fred janet >test/fred_janet
check fred_janet "Full dual channel dump."
DEL="$DEL fred_janet"

$EXPORT $INDEX -s '03/23/2004 10:48:39' -e '03/23/2004 10:49:10' fred janet >test/fred_janet_piece
check fred_janet_piece "Time range of dual channel dump."
DEL="$DEL fred_janet_piece"

$EXPORT $INDEX -s '03/05/2004 10:48:39' -e '03/23/2004 10:48:00' fred ExampleArray >test/fred_ExampleArray
check fred_ExampleArray "Time range with array."
DEL="$DEL fred_ExampleArray"

$EXPORT $INDEX -o test/raw -gnuplot fred janet
check raw "GNUPlot data file."
DEL="$DEL raw raw.plt"

$EXPORT $INDEX -o test/lin -gnuplot fred janet -linear 1.0
check lin "GNUPlot data file for 'linear'."
DEL="$DEL lin lin.plt"

$EXPORT $INDEX -o test/avg -gnuplot fred janet -average 5.0
check avg "GNUPlot data file for 'average'."
DEL="$DEL avg avg.plt"

# Round to millisecs because otherwise we get
# differing results on Linux and MacOSX (1 nanosec!!)
$EXPORT $INDEX -o test/pb -gnuplot fred janet -plotbin 5.0 -millisecs
check pb "GNUPlot data file for 'plotbin'."
DEL="$DEL pb pb.plt"

gnuplot test/combined.plt

$EXPORT $INDEX DTL_HPRF:Tnk1:T DTL_HPRF:Tnk2:T -o test/dtl    -gnuplot 
check dtl "GNUPlot data file for DTL data."
DEL="$DEL dtl dtl.plt"

$EXPORT $INDEX DTL_HPRF:Tnk1:T DTL_HPRF:Tnk2:T -o test/dtl_pb -gnuplot -plotbin 3600
check dtl_pb "GNUPlot data file for DTL data 'plotbin'."
DEL="$DEL dtl_pb dtl_pb.plt"

gnuplot test/plotbin.plt

echo "View these for plot output: /tmp/combined.png /tmp/plotbin.png"

cd test
rm $DEL

