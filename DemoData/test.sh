#!/bin/sh

function compare()
{
    name=$1
    info=$2
    diff -q test/${name}.OK test/${name}
    if [ $? -eq 0 ]
    then
        echo "OK : $info"
    else
        echo "FAILED : $info. Check test/${name}.OK against test/${name}"
	exit 1
    fi
}

# Use the tools built in here, not what's installed in the PATH
EXPORT=../Export/O.$EPICS_HOST_ARCH/ArchiveExport
DATA=../DataTool/O.$EPICS_HOST_ARCH/ArchiveDataTool
INDEX=../IndexTool/O.$EPICS_HOST_ARCH/ArchiveIndexTool

echo ""
echo "Tests of ArchiveExport on DemoData"
echo "**********************************"
echo ""

$EXPORT index -l  | grep -v "# Generated by ArchiveExport "  >test/names
compare "names" "Name List"

$EXPORT index -i  | grep -v "# Generated by ArchiveExport "  >test/name_info
compare "names" "Name List with info"

$EXPORT index BoolPV  | grep -v "# Generated by ArchiveExport "  >test/bool
compare "bool" "Bool PV Dump"

$EXPORT index janet -t  | grep -v "# Generated by ArchiveExport "  >test/janet
compare "janet" "Double PV Dump"

$EXPORT index janet -t -s "03/23/2004 10:49:20"  | grep -v "# Generated by ArchiveExport "  >test/janet_start
compare "janet_start" "Double PV Dump with 'start'"

$EXPORT index janet -t -e "03/23/2004 10:48:37"  | grep -v "# Generated by ArchiveExport "  >test/janet_end
compare "janet_end" "Double PV Dump with 'end'"

$EXPORT index alan -t  | grep -v "# Generated by ArchiveExport "  >test/alan
compare "alan" "Empty Array PV Dump"

$EXPORT index ExampleArray -s "03/05/2004 19:50:35" -text  | grep -v "# Generated by ArchiveExport " >test/array
compare "array" "Array PV Dump"

$EXPORT index  DTL_HPRF:Tnk1:T DTL_HPRF:Tnk2:T DTL_HPRF:Tnk3:T DTL_HPRF:Tnk4:T  DTL_HPRF:Tnk5:T DTL_HPRF:Tnk6:T -plotbin 3600 -o test/tnk_temps -gnu
mv test/tnk_temps test/tnk_temps.tmp
grep  -v "# Generated by ArchiveExport " test/tnk_temps.tmp >test/tnk_temps
compare "tnk_temps" "Plot-Binned Tank Data"

$EXPORT index  DTL_HPRF:Tnk2:T -s "03/24/2004" -e "03/27/2004"              | grep -v "# Generated by ArchiveExport " >test/raw
compare "raw" "Tank Data"

$EXPORT index  DTL_HPRF:Tnk2:T -s "03/24/2004" -e "03/27/2004" -linear 600  | grep -v "# Generated by ArchiveExport " >test/lin60
compare "lin60" "Linear Interpolation Data"

echo ""
echo "Tests of ArchiveIndexTool on DemoData"
echo "*************************************"
echo ""
cd test
if [ -f new_index ]
then
	rm new_index
fi
ArchiveIndexTool -M 10 -reindex -verbose 10 ../index new_index
cd ..
echo "Comparing data exported from that copy"
$EXPORT test/new_index janet -t  | grep -v "# Generated by ArchiveExport "  >test/janet
compare "janet" "Double PV Dump"

echo ""
echo "Tests of ArchiveDataTool on DemoData"
echo "************************************"
echo ""

$EXPORT index -l  | grep -v "# Generated by ArchiveExport "  >test/names
# DataTool Tests
$DATA index  -blocks -ch janet -v 10 >test/janet.blocks
compare "janet.blocks" "Data Block Dump"

echo "Creating copy of demo archive...."
rm -f test/copy_index test/copy_data
$DATA index -copy test/copy_index -base copy_data

echo "Comparing data exported from that copy"
$EXPORT test/copy_index janet -t  | grep -v "# Generated by ArchiveExport "  >test/janet
compare "janet" "Double PV Dump"

$EXPORT test/copy_index  DTL_HPRF:Tnk1:T DTL_HPRF:Tnk2:T DTL_HPRF:Tnk3:T DTL_HPRF:Tnk4:T  DTL_HPRF:Tnk5:T DTL_HPRF:Tnk6:T -plotbin 3600 -o test/tnk_temps -gnu
mv test/tnk_temps test/tnk_temps.tmp
grep  -v "# Generated by ArchiveExport " test/tnk_temps.tmp >test/tnk_temps
compare "tnk_temps" "Plot-Binned Tank Data"
