set here [file dirname [file normalize [info script]]]
lappend auto_path [file join $::env(DAQROOT) TclLibs]
lappend auto_path $here

package require MDPPSCPSROSoftTrigger

MDPPSCPSROSoftTrigger::register 12 15000 30000 ;# Trigger Ch: 12, Window: [-15, 15] us
