lappend auto_path [file join $::env(DAQROOT) TclLibs]
set here [file dirname [file normalize [info script]]]

package provide MDPPSCPSROSoftTrigger 1.0

package require Actions
package require DefaultActions

package require RunstateMachine
package require ring

namespace eval MDPPSCPSROSoftTrigger {
  variable pipe {}
  variable oldring {}
  variable parser [::Actions %AUTO%]
  set ::DefaultActions::name "MDPP-16/32 SCP SRO Software Trigger"

  proc register {} {

    set stateMachine [RunstateMachineSingleton %AUTO%]
    $stateMachine addCalloutBundle MDPPSCPSROSoftTrigger
    $stateMachine destroy
  }

  proc enter {from to} {
  }

  proc leave {from to} {
		variable pipe

    if {($from eq "Halted") && ($to eq "Active")} {
	    if {$pipe ne {}} {
		    chan event $pipe readable [list]
		    set pipe {}
		    if ($oldring ne {}} {
			    killOldProvider $oldring
		    }
	    }

      startSimulator
    }

  }

  proc attach {state} {
  }

  proc killOldProvider {ringName} {
		if {$ringName in [ringbuffer list]} {
			set usage [ringbuffer usage $ringName]
			set producerPid [lindex $usage 3]

			if {$producerPid != -1} {
				ReadoutGUIPanel::Log MDPPSCPSROSoftTrigger warning "Killing off old producing process with PID=$producerPid"			
				exec kill $producerPid
			}
		}
  }

  proc startSimulator {} {
		variable pipe
		variable parser
		variable oldring

		# on the first time running, kill of old processes, and launch a new one.
		if {$pipe eq {}} {
			set cmdpath $::here
			source MDPPSCPSROSoftTriggerSettings.tcl

			killOldProvider $outring

			set cmd [file join $cmdpath MDPPSCPSROSoftTrigger]
			set pipe [open "| $cmd  tcp://localhost/$inring tcp://localhost/$outring $trigCh $windowStart $windowWidth |& cat" r]

			chan configure $pipe -blocking 0
			chan configure $pipe -buffering line
			chan event $pipe readable [list $parser onReadable $pipe]

			set oldring $outring
		}
	}

  namespace export enter leave attach
}
