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
    fi
}

ArchiveExport index -l  | grep -v "# Generated by ArchiveExport "  >test/names
compare "names" "Name List"

ArchiveExport index -i  | grep -v "# Generated by ArchiveExport "  >test/name_info
compare "names" "Name List with info"

ArchiveExport index BoolPV  | grep -v "# Generated by ArchiveExport "  >test/bool
compare "bool" "Bool PV Dump"

ArchiveExport index janet -t  | grep -v "# Generated by ArchiveExport "  >test/janet
compare "janet" "Double PV Dump"

ArchiveExport index janet -t -s "03/23/2004 10:49:20"  | grep -v "# Generated by ArchiveExport "  >test/janet_start
compare "janet_start" "Double PV Dump with 'start'"

ArchiveExport index janet -t -e "03/23/2004 10:48:37"  | grep -v "# Generated by ArchiveExport "  >test/janet_end
compare "janet_end" "Double PV Dump with 'end'"

ArchiveExport index alan -t  | grep -v "# Generated by ArchiveExport "  >test/alan
compare "alan" "Empty Array PV Dump"

ArchiveExport index ExampleArray -s "03/05/2004 19:50:35" -text  | grep -v "# Generated by ArchiveExport " >test/array
compare "array" "Array PV Dump"
