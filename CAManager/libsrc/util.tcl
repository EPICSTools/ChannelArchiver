# This is to get the CVS-revision-code into the source...
set Revision ""
set Date ""
set Author ""
set CVS(Revision,Util) "$Revision$"
set CVS(Date,Util) "$Date$"
set CVS(Author,Util) "$Author$"

regsub ": (.*) \\$" $CVS(Revision,Util) "\\1" CVS(Revision,Util)
regsub ": (.*) \\$" $CVS(Date,Util) "\\1" CVS(Date,Util)
regsub ": (.*) \\$" $CVS(Author,Util) "\\1" CVS(Author,Util)

if {[info exists env(DEBUG)]} {
  set ::debug 1
} else {
  set ::debug 0
}

set yesno {NO YES}
set truefalse {FALSE TRUE}

set ::tl_cnt 0
set ::_port 4610
set ::dontCheckAtAll 0
set ::checkBgMan 1
set ::checkInt 10
set ::bgCheckInt 10
set ::bgUpdateInt 10

set ::multiVersion 1
set ::socks {}

if [info exists tk_version] {
  set funame xxx
} else {
  set funame bgerror
}

proc $funame {args} {
  global errorInfo
  foreach l [split $errorInfo "\n"] {
    Puts "$l" error
  }
  if {$::debug} exit
}

proc Exit {{code 0}} {
  if {[llength $::socks] > 0} {
    puts stderr "waiting for open connections to close..."
  }
  set ::nextLoop 0
  while {[llength $::socks] > 0} {
    after 1000 {incr ::nextLoop}
    update
    vwait ::nextLoop
    if {$::nextLoop >= 10} {break}
  }
  if {[llength $::socks] > 0} {
    puts stderr "connections still open - terminating anyway!"
  }
  exit $code
}

proc Puts {s {color normal}} {
  set color $::colormap($color)
  set now "[clock format [clock seconds] -format %Y/%m/%d\ %H:%M:%S]: "
  if {[info exists ::args(log)] && ("$::args(log)" != "")} {
    set w [open $::args(log) a+]
    puts $w "$now$s"
    close $w
  }

  puts stdout "$now$s"
  if {$color == "no"} return
  if {$color != "normal"} {
    set s "<font color=$color>$s</font>"
  }
  set ::_msg [lrange [linsert $::_msg 0 "$now$s"] 0 99]
}

proc checkArchivers {args} {
  trace vdelete ArchiversSet w {after 1 checkArchivers}
  after [expr 1000 * $::bgCheckInt] checkArchivers
  foreach i [camMisc::arcIdx] {
    if {![info exists ::_run($i)]} {set ::_run($i) -}
    if {"[camMisc::arcGet $i host]" != "$::_host"} continue
#    if {![regexp "^since" $::_run($i)]} {set ::_run($i) -}
    trace variable ::_run($i) w checkstate
    camComm::CheckRunning $i ::_run($i)
  }
}

proc checkBgManager {fd h} {
  gets $fd line
  if [regexp "Server: Channel Archiver bgManager (.*)@(.*):(\[0-9\]*)" $line \
	  all user host port] {
    set ::reply($h) "$user@$host:$port"
  }
  if {"$line" == "</html>"} {
    close $fd
    incr ::open -1
    if {$::open == 0} {set ::pipi 1}
  }
}

proc cfbgmTimeout {fd h} {
  catch {close $fd}
}

proc checkForBgManager {} {
  global tcl_platform
  if {$::checkBgMan == 0} return
  set hosts {}
  foreach arc [camMisc::arcIdx] {
    lappend hosts [camMisc::arcGet $arc host]
  }
  set ::open 0
  foreach h [lrmdups $hosts] {
    if [catch {set sock [socket $h $::_port]}] {
      continue
    }
    set ::reply($h) {}
    incr ::open
    puts $sock "GET / HTTP/1.1\n"
    flush $sock
    fconfigure $sock -blocking 1
    fileevent $sock readable "checkBgManager $sock $h"
    after 3000 "cfbgmTimeout $sock $h"
  }
  if {$::open > 0} { vwait ::pipi }
  foreach h [lrmdups $hosts] {
    set act none
    if {![info exists ::reply($h)]} {
      set msg "No CAbgManager running for $tcl_platform(user)@$h:$::_port!"
      set act start
    } elseif {[llength $::reply($h)] == 0} {
      set msg "CAbgManager $tcl_platform(user)@$h:$::_port didn't reply!"
      set act config
    } elseif {"$::reply($h)" != "$tcl_platform(user)@$h:$::_port"} {
      set msg "CAbgManager $tcl_platform(user)@$h:$::_port answered $::reply($h)!"
      set act config
    } else {
      set msg "CAbgManager $tcl_platform(user)@$h:$::_port OK!"
    }
    switch $act {
      start {
	if {[camGUI::MessageBox warning "Warning!" "CAbgManager not running!" "$msg!" "" {"Continue without" "Start"} .] == "Start"} {
	  if {[regexp "Windows" $tcl_platform(os)]} {
	    set tclext .tcl
	  } else {
	    set tclext ""
	  }
	  exec CAbgManager$tclext &
	}
      }
      config {
	tk_dialog .w "CAbgManager: invalid response" "$msg\nPlease reconfigure!" warning 0 "Ok"
      }
    }
  }
}

proc checkstate {arr ind op} {
  if {"$::_run($ind)" == "NO"} {
    trace vdelete ::_run($ind) w checkstate
    if {("[camMisc::arcGet $ind start]" == "NO")} return
    if {("[camMisc::arcGet $ind start]" != "timerange")} {
      catch {after cancel $::sched($ind,start,job)}
      set ::sched($ind,start,job) [after 100 "runArchiver $ind"]
    } else {
      # timerange
      lassign [duetime $ind] Starttime Stoptime
      set starttime [expr ( $Starttime - [clock seconds] ) * 1000]
      set stoptime [expr ( $Stoptime - [clock seconds] ) * 1000]
      if {$stoptime <= 0} return
      if {$starttime < 0} {set starttime 0}
      if {[info exists ::sched($ind,start,time)] && ($::sched($ind,start,time) == $Starttime)} return
      Puts "starting \"[camMisc::arcGet $ind descr]\" @ [httpd::time $Starttime]" schedule
      set ::sched($ind,start,time) $Starttime
      set ::sched($ind,start,job) [after $starttime "runArchiver $ind"]
    }
  } elseif [regexp "^(BLOCKED|since)" $::_run($ind)] {
    trace vdelete ::_run($ind) w checkstate
  }
}

# returns start- and stop-time, when an archiver is SUPPOSED to run!
proc duetime {i {run X} {timespec X}} {
  if {$run == "X"} {set run [camMisc::arcGet $i start]}
  if {$timespec == "X"} {set timespec [camMisc::arcGet $i timespec]}
  set now [clock seconds]
  switch $run {
    minute {
      lassign $timespec st ev
      set delta [expr $ev * 60]
      set m [expr 1[clock format $now -format %M] - 100]
      set fh [clock scan [clock format $now -format "%m/%d/%Y %H:00"]]
      set starttime [expr ( ( $m / $ev ) * $ev + $st ) * 60 + $fh]
    }
    hour {
      lassign $timespec time ev
      set delta [expr $ev * 3600]
      set h [expr 1[clock format $now -format %H] - 100]
      set starttime [expr (( $h / $ev ) * $ev) * 3600 + [clock scan $time]]
    }
    day {
      lassign $timespec time ev
      set delta [expr 86400 * $ev]
      set day0 [expr ($now / $delta) * $delta]
      set starttime [clock scan [clock format $day0 -format "%m/%d/%Y $time"]]
    }
    week {
      lassign $timespec day time ev
      set starttime [clock scan $time]
      set dd [lsearch $::weekdays $day]
      set wd [clock format $now -format %w]
      incr starttime [expr ( $dd - $wd ) * 86400]
      set delta [expr $ev * 7 * 86400]
    }
    month {
      lassign $timespec day time ev
      if { ( $day > 28 ) || ( $day < 1 ) } { return {-1 -1} }
      set td [expr 1[clock format $now -format %d] - 100]
      set tm [expr 1[clock format $now -format %m] - 101]
      set ty [clock format $now -format %Y]
      if {$tm == 0} { set tm 12; incr ty -1 }
      while {[set d1 [clock scan "$tm/$day/$ty $time"]] < $now} {
	set starttime $d1
	incr tm
	if {$tm == 13} { incr ty; set tm 1 }
      }
      set delta [expr $d1 - $starttime]
    }
    timerange {
      set starttime [clock scan "[lindex $timespec 0] [lindex $timespec 1]"]
      set delta [clock scan "[lindex $timespec 3] [lindex $timespec 4]"]
      incr delta -$starttime
    }
    always {
      set starttime 0
      set delta 0
    }
    NO {
      set starttime 0
      set delta 0
    }
    default {
      Puts "invalid schedule-mode" error
      return {-1 -1}
    }
  }

  if {($run != "timerange") && ($delta != 0)} {
    while {$starttime > $now} { incr starttime -$delta }
    while {[expr $starttime + $delta] <= $now} {incr starttime $delta}
  }
  return [list $starttime [expr $starttime + $delta]]
}

if {$::debug} {
  set ::weekdays {Sunday Monday Tuesday Wednesday Thursday Friday Saturday}
  set xamples {
    {minute "0 1"}
    {minute "12 12"}
    {minute "12 15"}
    {hour "00:27:00 1"}
    {hour "06:00:00 8"}
    {day "14:30:00 1"}
    {day "02:00:00 2"}
    {day "23:59:59 3"}
    {week "Sunday 12:00:00 1"}
    {week "Monday 12:00:00 2"}
    {week "Tuesday 12:00:00 3"}
    {week "Wednesday 12:00:00 4"}
    {week "Thursday 12:00:00 5"}
    {week "Friday 12:00:00 6"}
    {week "Saturday 12:00:00 7"}
    {month "1 12:00:00"}
    {month "10 12:00:00"}
    {month "20 12:00:00"}
    {month "28 12:00:00"}
    {timerange "10/28/2001 12:00:00 - 10/28/2001 13:00:00"}
  }
  foreach sample $xamples {
    lassign $sample run timespec
    lassign [duetime 0 $run $timespec] starttime stoptime
    if {$starttime < 0} {
      set a "invalid"
    } else {
      set a "[clock format $starttime] .. [clock format $stoptime]"
    }
    puts [format "%-25s %s" "$sample:" $a]
  }
}

set cfg_ts 0
proc readCFG {} {
  after 1000 readCFG
  if {$camMisc::force_cfg_file} {
    set ts [file mtime $camMisc::cfg_file]
  } else {
    set ts [registry get "$camMisc::reg_stem" "ts"]
  }
  if {$ts > $::cfg_ts} {
    Puts "(re-)readCFG"
    foreach i [array names ::sched *,job] {
      regsub "job\$" $i "time" t
      after cancel $::sched($i)
      array unset ::sched $i
      array unset ::sched $t
    }
    camMisc::init
    set ::cfg_ts $ts
    foreach i [camMisc::arcIdx] {
      if {"[camMisc::arcGet $i host]" != "$::_host"} continue
      if {"[camMisc::arcGet $i start]" == "NO"} continue
      scheduleStop $i
    }
  }
  update
}

proc EPuts {i lvl msg} {
  if {![info exists ::wasError($i)] || ($::wasError($i) != $lvl)} {
    Puts "$msg" error
  }
  set ::wasError($i) $lvl
}

set ::mutex 0
proc semTake {} {
  while {$::mutex} {
    after 100 {set ::pipi 1}
    vwait ::pipi
  }
  set ::mutex 1
}

proc semGive {} {
  set ::mutex 0
}

proc runArchiver {i {forceRun 0}} {

  # Each archiver has a root-directory ROOT
  # ArchiveEngine runs in ROOT
  #     ArchiveEngine -p ... -l x/y/z/log cfg a/b/c/directory
  # files/dirs:
  #            /ROOT/CVS/
  #                  cfg/
  #                  config-file
  #                  a/b/c/directory
  #                  x/y/z/log
  #                  archive_active.lck
  #                  BLOCKED
  #
  # parameters:
  #             cfg	/ROOT/config-file
  #		log	x/y/z/log
  #		archive	a/b/c/directory
  #		descr	BlaBlaBla
  #		port	4711

  array unset ::sched $i,start
  set now [clock seconds]
  foreach attr {port descr cfg cfgc start multi timespec} {
    set $attr [camMisc::arcGet $i $attr]
  }
  foreach attr {log archive} {
    set $attr [clock format $now -format [camMisc::arcGet $i $attr]]
  }

  set ROOT "[file dirname $cfg]"

  semTake

  if {![file exists $cfg]} {
    if {![info exists ::wasError($i)] || ($::wasError($i) != 1)} {
      Puts "Config file of \"$descr\" doesn't exist!" error
    }
    set ::wasError($i) 1
    semGive
    return
  }

  # Inspect the first 20 lines of the previous log-file for possible
  # problems.
  if {[file exists [file join $ROOT runlog]]} {
    set rh [open [file join $ROOT runlog] r]
    for {set j 0} {$j < 20} {incr j} {
      if {[eof $rh]} break
      gets $rh line
      switch -regexp -- $line {
	"Found an existing lock file" {
	  EPuts $i 2 "\"$descr\" terminated because it found an existing lock file!"
	}
	"bind failed for HTTPServer::create" {
	  EPuts $i 3 "\"$descr\" terminated because it couldn't bind to the required port!"
	}
	"Config file '.*': cannot open" {
	  EPuts $i 4 "\"$descr\" terminated because it couldn't open the config-file!"
	}
      }
    }
    close $rh
  }

  # The lockfile may still exist, because the previous archiver still needs
  # time to flush it's buffers and close the archive.
  # We'll try again later.
  if {![info exists ::tryAgain($i)]} {set ::tryAgain($i) 1}
  if {[file exists [file join $ROOT archive_active.lck]]} {
    incr ::tryAgain($i)
    if {($::tryAgain($i) > 6)} {
      EPuts $i 5 "Lockfile for \"$descr\" exists - Archiver already/still running! (or terminated abnormally?)"
      semGive
      return
    } else {
      Puts "Lockfile for \"$descr\" exists - $::tryAgain($i). try in $::bgCheckInt seconds"
      semGive
      return
    }
  }
  set ::tryAgain($i) 1
  
  lassign [duetime $i] starttime stoptime

  set toggle 0
  while {[regexp "%(\[1-9\]\[0-9\]*\)" $archive all mod]} {
    set toggle 1
    set div 0
    switch $start {
      minute  {set div 1; set active [expr [clock format $now -format %s -gmt 1] / 60]}
      hour    {set div 1; set active [expr [clock format $now -format %s -gmt 1] / 3600]}
      day     {set div 1; set active [expr [clock format $now -format %s -gmt 1] / 86400]}
      week    {set div 2; set active [expr [clock format $now -format %s -gmt 1] / (7*86400)]}
      month   {set div 2; set active [expr [clock format $now -format %Y -gmt 1] * 12 + [clock format $now -format %m -gmt 1]]}
    }
    if {$div > 0} { set div [lindex $timespec $div] } else { set div 1 }
    set active [expr ( $active / $div ) % $mod]
    regsub -all " *%$mod" $archive $active archive
  }

  # if it's a toggle-archive, delete the one to overwrite
#  set adir [file dirname $archive]
  if {$toggle && ("$::lastArc($i)" != "$archive")} {
#     if {[file exists $ROOT/$archive]} {
#       file mkdir $ROOT/last/$adir
#       if {[file exists $ROOT/last/$adir]} {
# 	file delete -force $ROOT/last/$adir
#       }
#       file rename $ROOT/$adir $ROOT/last/$adir
#    }
    set files [glob -nocomplain $ROOT/[file dirname $archive]/*]
    if {[llength $files] > 0} {
#      Puts "deleting $files"
      eval file delete -force $files
    }
  }
  set ::lastArc($i) $archive

  if {!$forceRun && [file exists [file join $ROOT BLOCKED]]} {
    if { ![info exists ::wasError($i)] || ($::wasError($i) != 3) } {
      Puts "start of \"$descr\" blocked!" error
      set ::wasError($i) 3
    }
    semGive
    return
  }
  set ::wasError($i) 0

  # create all directories that are necessary
  file mkdir "$ROOT/[file dirname $log]"
  file mkdir "$ROOT/[file dirname $archive]"
  if {!$cfgc} {file mkdir "$ROOT/cfg"} else {file delete -force $ROOT/cfg}

  # Save the (eventually) modified cfg-files in ROOT/cfg/
  if {!$cfgc} {
    set cfgfiles {}
    set X 1
    foreach f [glob -nocomplain $ROOT/cfg/*] {
      if {$X} {Puts "Saving config-files of \"$descr\""}
      set X 0
      set f [file tail $f]
      if {![file exists $ROOT/$f] ||
	  ([file mtime $ROOT/cfg/$f] > [file mtime $ROOT/$f])} {
	file copy -force $ROOT/cfg/$f $ROOT/$f
	lappend cfgfiles $f
      } else {
	Puts "Conflict copying \"$f\" - $ROOT/$f is newer!" error
      }
    }
  }

  # Start the Archiver
  set cfg "[file tail $cfg]"
  cd $ROOT
  Puts "start \"$descr\" (in $ROOT/[file dirname $archive])" command
  if {$cfgc} {
#    exec strace -oTRACE ArchiveEngine -p $port -description "$descr" -log $log -nocfg $cfg $archive >&runlog &
    exec ArchiveEngine -p $port -description "$descr" -log $log -nocfg $cfg $archive >&runlog &
  } else {
#    exec strace -oTRACE ArchiveEngine -p $port -description "$descr" -log $log $cfg $archive >&runlog &
    exec ArchiveEngine -p $port -description "$descr" -log $log $cfg $archive >&runlog &
  }

  semGive

  if {!$forceRun} {scheduleStop $i}

  # Check the changed cfg-files into cvs.
  if {!$cfgc} {
    if {[file isdir $ROOT/CVS]} {
      set ee [split [read_file $ROOT/CVS/Entries]]
      foreach f $cfgfiles {
	cd $ROOT
	if {[lsearch -regex $ee "^/[file tail $f]/"] < 0} {
	  exec cvs add -m "automatic add by CAManager" [file tail $f]
	}
	exec cvs commit -m "automatic commit by CAManager" [file tail $f]
      }
    }
  }

  # Now edit the master-files accordingly
  set master_dir $ROOT/[file tail $archive]
  set master_txt "master_version=$::multiVersion"

  if {[file exists $master_dir] && ("[file dirname $archive]" != ".")} { 
    regsub -all "\r" [read_file $master_dir] "" master_txt
  }
  set md [split $master_txt "\n"]
  if {$toggle} {
    set j [lsearch -regex $md "^(.* )?$ROOT/$archive"]
    if {$j > 0} {
      if {[regexp "^\#" [lindex $md [expr $j - 1]]]} {incr j -1}
      set md [lrange $md 0 [expr $j - 1]]
    }
  }

  if {"[file dirname $archive]" != "."} {
    if {"[lindex $md 0]" == "master_version=$::multiVersion"} {
      if {[lsearch -regex $md "^(.* )?$ROOT/$archive"] < 0} {
	set ts ""
	if {$::multiVersion == 2} {
	  if {$starttime == 0} {
	    lassign [getTimes $ROOT/$archive] starttime stoptime
	    if {"$starttime" != "0"} {
	      set starttime [clock scan $starttime]
	      set stoptime [clock scan $stoptime]
	    }
	  }
	  set ts "[clock format $starttime -format %m/%d/%Y\ %H:%M:%S] [clock format $stoptime -format %m/%d/%Y\ %H:%M:%S] "
	  set md [linsert $md 1 "$ts$ROOT/$archive"]
	} else {
	  set md [linsert $md 1 "$ROOT/$archive"]
	}
	write_file $master_dir [join $md "\n"]
      }
    }
  }
  if {"$multi" != ""} {
    after 20000 "updateMultiArchive \"$multi\""
  }
}

proc scheduleStop {i} {
  set now [clock seconds]
  lassign [duetime $i] starttime stoptime
  if {$starttime > $now} return
  if {$stoptime < $now} return

  camMisc::arcSet $i stoptime $stoptime
  if {[info exists ::sched($i,stop,time)] && ($::sched($i,stop,time) == $stoptime)} return
  if {[info exists ::sched($i,stop,job)]} {after cancel $::sched($i,stop,job)}
  Puts "stopping \"[camMisc::arcGet $i descr]\" @ [httpd::time $stoptime]" schedule
  set ::sched($i,stop,job) [after [expr ( $stoptime - $now ) * 1000] "stopArchiver $i"]
  set ::sched($i,stop,time) $stoptime
}

proc stopArchiver {i {forceStop 0}} {
  array unset ::sched $i,stop,*
  if {!$forceStop && [file exists [file dirname [camMisc::arcGet $i cfg]]/BLOCKED]} {
    Puts "stop of \"[camMisc::arcGet $i descr]\" blocked" error
    return
  }
  Puts "stop \"[camMisc::arcGet $i descr]\"" command
  if {![catch {set sock [socket [camMisc::arcGet $i host] [camMisc::arcGet $i port]]}]} {
    fconfigure $sock -blocking 0
    fileevent $sock readable [list justread $sock]
    lappend ::socks $sock
    puts $sock "GET /stop HTTP/1.0"
    puts $sock ""
    flush $sock
#    puts stderr "                   sent stop-request($sock)"
  }
}

proc justread {sock} {
#  puts stderr "                   consuming data($sock)"
  set data [read $sock]
  flush $sock
  if {[eof $sock]} {
#    puts stderr "                   closing $sock"
    close $sock
    if {[set i [lsearch -exact $::socks $sock]] >= 0} { set ::socks [lreplace $::socks $i $i] }
  }
  update
}

proc updateMultiArchives {} {
  set A {}
  foreach i [camMisc::arcIdx] {
    if {[camMisc::arcGet $i multi] != ""} {
      lappend A [camMisc::arcGet $i multi]
    }
  }
  foreach a [luniq $A] {
    updateMultiArchive $a
  }
}

proc getTimes {arc i} {
  if {![catch {
    for_file line "|ArchiveManager -info \"$arc\"" {
      regexp "First.*sample.*: (\[^\\.\]*)" $line all starttime
      regexp "Last.*sample.*: (\[^\\.\]*)" $line all stoptime
    }
  }]} {
    if {[info exists ::_run($i)] && [regexp "^since" $::_run($i)]} {
      set stoptime [clock format [expr [clock seconds] + 365*86400] -format "%m/%d/%Y %H:%M:%S"]
    }
    return [list $starttime $stoptime]
  } else {
    return 0
  }
}

set ::n 1
proc updateMultiArchive {arch} {
  set myN $::n
  incr ::n
  set wh [open $arch.$myN w]
  puts $wh "master_version=$::multiVersion"
  foreach i [camMisc::arcIdx] {
    if {[camMisc::arcGet $i multi] == $arch} {
      set archive "[file dirname [camMisc::arcGet $i cfg]]/[file tail [camMisc::arcGet $i archive]]"
      puts $wh "\# from $archive"
      if {![file exists $archive]} {
	puts $wh "\#  nothing..."
	continue
      }
      if {[file dirname [camMisc::arcGet $i archive]] == "."} {
	set ts ""
	if {$::multiVersion == 2} {
	  lassign [getTimes $archive $i] starttime stoptime
	  if {"$starttime" != "0"} {
	    set ts "$starttime $stoptime "
	  } else {
	    continue
	  }
	}
	puts $wh "$ts$archive"
      } else {
	regsub -all "\n\n" xxx "\n" f
	puts $wh [string trim [join [lrange [split [read_file $archive] "\n"] 1 end] "\n"]]
      }
    }
  }
  close $wh
  file rename -force $arch.$myN $arch
}
