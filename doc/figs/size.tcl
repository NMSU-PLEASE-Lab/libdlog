#!/usr/bin/tclsh

set bucket 5000
set num 30
array set hist {}
for {set i 0} {$i < $num} {incr i} {
   set hist($i) 0
}
while {[gets stdin line]>=0} {
   #puts $line
   regexp {.*size=([0-9]+)} $line dummy size
   #puts $size
   set i [expr $size/$bucket]
   if {$i >= $num} {set i [expr $num-1]}
   incr hist($i)
}
for {set i 0} {$i < $num} {incr i} {
   puts "[expr $i*$bucket] $hist($i)"
}

exit 0

