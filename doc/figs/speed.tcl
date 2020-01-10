#!/usr/bin/tclsh

set bucket 100
set num 30
array set hist {}
for {set i 0} {$i < $num} {incr i} {
   set hist($i) 0
}
while {[gets stdin line]>=0} {
   #puts $line
   regexp {.*size=([0-9]+).*xferDuration=([0-9]+),([0-9]+)} $line dummy size sec nsec
   #puts "$size $sec $nsec"
   set nsec [expr ($nsec + $sec*1000000000)]
   set speed [expr double($size)/$nsec*1000]
   set i [expr round($speed/$bucket)]
   #puts "$size $sec $nsec $speed $i"
   if {$i >= $num} {set i [expr $num-1]}
   incr hist($i)
}
for {set i 0} {$i < $num} {incr i} {
   puts "[expr $i*$bucket] $hist($i)"
}

exit 0

