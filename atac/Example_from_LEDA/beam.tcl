
source /home/kasemir/Epics/extensions/src/ChannelArchiver/atac/atacTools.tcl
package require BLT
namespace import blt::*

#Hack to overcome missing blt installation
#set blt_library "[pwd]/O.solaris/blt2.4"

#####################################################
#Configure this:
#
set archiveName "/archives/aptdvl_archives/master_arch/chan_arch/freq_directory"
#set archiveName "/u1/archives/LEDA/dir"
# set archiveName "d:/testarchive/dir"
set archiveName "/mnt/cdrom/dir"
set logName "log"
set DCName      RFQ_BMDCTor_D_Avg
set dutyName    rp_params_duty
set daysAgo     1

#####################################################
# Determine day of interest,
# generate start/end times for that range
#
set yesterdaySecs [ expr [ clock seconds ] - 60*60*24*$daysAgo ]
stamp2values [ secs2stamp $yesterdaySecs ] year0 month0 day0 hour minute second

set hour0   0
set minute0 0
set month1  $month0
set day1    $day0
set year1   $year0
set hour1   23
set minute1 59

#################################
# Parse args.
#
# Command line args override GUI:
# Program will run on args & exit.
#
proc usage {} {
        global argv0
        puts "USAGE: $argv0 startTime endTime"
        puts "       times as \"YYYY/MM/DD hh:mm:ss\" in 24h format"

        exit 1
}

set argc [ llength $argv ]
if { $argc > 0 } {
	if { $argc != 2 } { usage }

	set startStamp  [ lindex $argv 0 ]        
	set endStamp    [ lindex $argv 1 ]        
	stamp2values $startStamp year0 month0 day0 hour0 minute0 second0
	stamp2values $endStamp   year1 month1 day1 hour1 minute1 second1
}

# GUI ##########################################################################

label .archivelabel -text "Archive:"
entry .archiveentry -textvariable archiveName -width 40
grid .archivelabel -row 0 -column 0
grid .archiveentry -row 0 -column 1

label .loglabel -text "Log:"
entry .logentry -textvariable logName -width 40
grid .loglabel -row 1 -column 0
grid .logentry -row 1 -column 1

label .startlabel -text "Start:"
frame .start
entry .start.month -width 2 -textvariable month0
label .start.ml -text "/"
entry .start.day -width 2 -textvariable day0
label .start.dl -text "/"
entry .start.year -width 4 -textvariable year0
label .start.yl -text ", "
entry .start.hour -width 2 -textvariable hour0
label .start.hl -text ":"
entry .start.minute -width 2 -textvariable minute0

grid .start.month .start.ml .start.day .start.dl .start.year \
	.start.yl .start.hour .start.hl .start.minute
grid .startlabel -row 2 -column 0
grid .start      -row 2 -column 1 -sticky w

label .endlabel -text "End:"
frame .end
entry .end.month -width 2 -textvariable month1
label .end.ml -text "/"
entry .end.day -width 2 -textvariable day1
label .end.dl -text "/"
entry .end.year -width 4 -textvariable year1
label .end.yl -text ", "
entry .end.hour -width 2 -textvariable hour1
label .end.hl -text ":"
entry .end.minute -width 2 -textvariable minute1 
grid .end.month .end.ml .end.day .end.dl .end.year \
        .end.yl .end.hour .end.hl .end.minute
grid .endlabel -row 3 -column 0
grid .end      -row 3 -column 1 -sticky w

button .analyzeButton -text "Analyze" -command analyze
grid .analyzeButton  -row 4 -column 0

frame .progress
label .progress.label -text "Progress:"
scale .progress.bar -orient horizontal -from 0 -to 100 -variable progress \
	-showvalue false -state disabled
grid .progress.label .progress.bar
grid .progress -row 4 -column 1 -sticky w

entry .info -width 50 -state disabled -textvariable info
grid .info -row 5 -columnspan 2 -sticky w

proc showInfo { txt } {
	global info
	set info $txt
	update
}

toplevel .periods
graph .periods.graph
.periods.graph configure -width 700 -height 500
pack .periods.graph

toplevel .currents
createAtacGraph .currents.graph
.currents.graph configure -width 700 -height 500
pack .currents.graph

update


#####################################################
#
# "Main" routine
#
proc analyze {} {
	global archiveName logName
	global year0 month0 day0 hour0 minute0
	global year1 month1 day1 hour1 minute1
	global DCName dutyName
	global progress

	# Initialize...
	showInfo "Initializing..."
	set startStamp [ values2stamp $year0 $month0 $day0 $hour0 $minute0  0 ]
	set endStamp   [ values2stamp $year1 $month1 $day1 $hour1 $minute1 59 ]
	set startSecs  [ stamp2secs $startStamp ]
	set timeSpanInSecs [ expr [ stamp2secs $endStamp ] - $startSecs ]

	# Prohibit user input from now on
	.analyzeButton configure -state disabled
	set progress 0
	set oldProgress 0
	update

	set log [ open $logName w ]
	puts $log "# Analyse $month0/$day0/$year0 - $month1/$day1/$year1"
	puts $log "#"
	puts $log "# time\t$DCName\t$dutyName\tDutyFactor\tPeakCurrent\tdT/s\tT(total)\tCharge(total)\tAvg.Curr\tBeam On\tBeam On/Off\tOn Time\tOn Charge"
	puts $log "# -------------------------------------------------------------"

	set DutyFactor 0
	set PeakCurrent 0
	set prevTime 0
	set prevDuty 0
	set prevCurrent 0
	set TotalTime 0
	set TotalCharge 0
	set beamWasOn false
	set onTime 0
	set onCharge 0
	set AverageCurrent 0
	set Duration 0

	# Arrays for graph
	set timeArray {}
	set PeakCurrentArray {}
	set AvgCurrentArray {}

	# Translate combination of "beam was on"/"beam is on" into "change" string
	set OnOffChange(falsefalse)	""
	set OnOffChange(truetrue)	""
	set OnOffChange(falsetrue)	"On"
	set OnOffChange(truefalse)	"Off"

	# list of { Time AverageCurrent }
	set beamOnStatistics {}

	# Main loop...
	set time $startStamp
	set channelNameList [ list $DCName $dutyName ]
	set goOn [ openSpreadsheetContext $archiveName $time $channelNameList context values stati ]
	showInfo "Calculating..."
	while { $goOn  && ($time < $endStamp) } {
		set DCPV   [ lindex $values 0 ]
		set dutyPV [ lindex $values 1 ]

		set line "$time"
		foreach value $values {
			if { $value == {} } {
				append line "\t#N/A"
			} else {
				append line [ format "\t%7.1f" $value ]
			}
		}

		set secTime [ stamp2secs $time ]
		lappend timeArray $secTime
		# Keep duty factor while duty PV is off
		if { $dutyPV != {} } { set DutyFactor [ expr $dutyPV / 100 ] }
		append line [ format "\t%5.1f" $DutyFactor ]

		# Have new and valid DC value for PeakCurrent?
		if { $DCPV != {} } {
			if { $DCPV >= 5  &&  $DCPV <= 115 } {
				set PeakCurrent $DCPV
			} else {
				set PeakCurrent 0
			}
		}
		append line [ format "\t%5.1f" $PeakCurrent ]
		lappend PeakCurrentArray $PeakCurrent

		# Delta T in seconds
		if { $prevTime > 0 } {
			set dT [ expr $secTime - $prevTime ]
		} else {
			set dT 0
		}
		set prevTime $secTime
		append line [ format "\t%5.1f" $dT ]

		# Update progress indicator
		set p [ expr int(100*($secTime - $startSecs) / $timeSpanInSecs) ]
		if { $p != $oldProgress } {
			set progress $p
			set oldProgress $p
			update
		}

		# Accumulate total time
		set TotalTime [ expr $TotalTime + $dT ]
		append line [ format "\t%10.0f" $TotalTime ]

		# Accumulate total charge
		# [charge] = mA*s
		set charge [ expr $prevDuty * $prevCurrent * $dT ]
		set TotalCharge [ expr $TotalCharge + $charge ]
		append line [ format "\t%10.0f" $TotalCharge ]
		set prevDuty $DutyFactor
		set prevCurrent $PeakCurrent

		# Average Current
		if { $TotalTime > 0 } {
			set AverageCurrent [ expr $TotalCharge / $TotalTime ]
		} else {
			set AverageCurrent 0
		}
		append line [ format "\t%5.0f" $AverageCurrent ]
		lappend AvgCurrentArray $AverageCurrent

		# Is Beam On?
		if { $PeakCurrent > 5 } {
			set BeamOn	true
		} else {
			set BeamOn	false
		}
		append line "\t$BeamOn"

		# Beam switched On or Off?
		set beamChange $OnOffChange($beamWasOn$BeamOn)
		append line "\t$beamChange"
		set beamWasOn $BeamOn

		# Accumulate time and charge for this "on" period
		if { ( $BeamOn && $beamChange=="" ) || $beamChange == "Off" } {
			set onTime [ expr $onTime + $dT ]
			set onCharge [ expr $onCharge + $charge ]
		} else {
			set onTime 0
			set onCharge 0
		}
		append line [ format "\t%5.1f" $onTime ]
		append line [ format "\t%5.0f" $onCharge ]

		# Extra column for final time in min, avg. charge for this "on" period
		if { $beamChange == "Off" } {
			lappend beamOnStatistics [ list [ expr $onTime/60] [ expr $onCharge/$onTime ] ]
		}

		puts $log $line
		set goOn [ nextSpeadsheetContext context time values stati ]
	}
	set progress 0
	showInfo "Creating plots..."

	# Duration: Total time considered so far in hours
	set Duration [ expr $TotalTime / 60 / 60 ]

	# TotalCharge: Convert from mA * sec to mA * h
	set TotalCharge [ expr $TotalCharge / 60 / 60 ]

	puts $log "\n"
	puts $log "Beam Statistics:"
	puts $log "================"
	puts $log "Period/min\tAvg. current/mA"
	foreach info [ lsort -real -index 0 $beamOnStatistics ] {
		puts $log [ format "%5.1f\t%5.1f" [ lindex $info 0 ] [ lindex $info 1 ] ]
	}

	# Graph: Beam On Periods
	set g .periods.graph
	$g configure -title "\"Beam On\" Periods\nfor $month0/$day0/$year0"
	$g xaxis configure -title "Run Time (min)\n\nData used: $startStamp - $endStamp"
	$g yaxis configure -title "DC Current (mA)"
	$g grid configure -hide no -dashes { 2 2 }
	if { [ $g element exists data ] } { $g element delete data }
	$g element create data -data [ join $beamOnStatistics ] \
		-label "" -linewidth 0 -pixels 5
	#Blt_Crosshairs $g
	#Blt_ZoomStack $g
	#Blt_ActiveLegend $g

	# Graph: Currents over time
	set g .currents.graph
	set title "LEDA Performance\n\n"
	append title "Avg. Current: [ format "%5.1f" $AverageCurrent] mA,\n"
	append title "Total Charge: [ format "%5.1f" $TotalCharge] mA*h,\n"
	append title "Total Time:  [ format "%5.1f" $Duration] h"
	$g configure -title $title
	$g xaxis configure -title "Time\n\nData used: $startStamp - $endStamp"
	$g yaxis configure -title "Beam Current (mA)"

	if { [ $g element exists peak ] } { $g element delete peak }
	if { [ $g element exists average ] } { $g element delete average }
	$g element create peak -xdata $timeArray -ydata $PeakCurrentArray \
		-label "Peak Current" -color blue -symbol none
	$g element create average -xdata $timeArray -ydata $AvgCurrentArray \
		-label "Average Current" -color red -symbol none

	# Snapshots:
	# There seems to be a timing problem here:
	# When saveGraph is called too soon,
	# the snapshotted graph is empty...
	update
	showInfo "Creating plot snapshots..."
	update
	saveGraph GIF .periods.graph  periods.gif
	saveGraph GIF .currents.graph current.gif

	###################################################
	#####################################################
	# cleanup
	#
	closeSpeadsheetContext context

	showInfo "Done."
	.analyzeButton configure -state normal
}


# If there were command-line args,
# run once and exit
if { $argc > 0 } {
	analyze
	exit 0
}
