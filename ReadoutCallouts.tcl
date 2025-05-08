set here [file dirname [file normalize [info script]]]
lappend auto_path [file join $::env(DAQROOT) TclLibs]
lappend auto_path $here

package require MDPPSCPSROSoftTrigger

MDPPSCPSROSoftTrigger::register 
