lappend auto_path [file join $::env(DAQROOT) TclLibs]

package provide MDPPSCPSROSoftTrigger 1.0

package require Actions
package require DefaultActions

package require RunstateMachine
package require ring

namespace eval MDPPSCPSROSoftTrigger {
  variable pipe {}
  variable parser [::Actions %AUTO%]
  set ::DefaultActions::name "MDPP-16/32 SCP SRO Software Trigger"

	variable trigCh
	variable windowStart
	variable windowWidth
  
  proc register {tc ws ww} {
		variable trigCh
		variable windowStart
		variable windowWidth

    set stateMachine [RunstateMachineSingleton %AUTO%]
    $stateMachine addCalloutBundle MDPPSCPSROSoftTrigger
    $stateMachine destroy

		set trigCh $tc
		set windowStart $ws
		set windowWidth $ww
  }

  proc enter {from to} {
  }

  proc leave {from to} {
    if {($from eq "Halted") && ($to eq "Active")} {
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
		variable trigCh
		variable windowStart
		variable windowWidth

		# on the first time running, kill of old processes, and launch a new one.
		if {$pipe eq {}} {
#			set cmdpath "/user/n4vault/mdpp32_streaming_readout"
			set cmdpath "/genie/mdpp32_streaming_readout/package"
			set inring  n4vault
			set outring n4vault_softtrig

			killOldProvider $outring

			set cmd [file join $cmdpath MDPPSCPSROSoftTrigger]
			set pipe [open "| $cmd  tcp://localhost/$inring tcp://localhost/$outring $trigCh $windowStart $windowWidth |& cat" r]

			chan configure $pipe -blocking 0
			chan configure $pipe -buffering line
			chan event $pipe readable [list $parser onReadable $pipe]
		}
	}

  namespace export enter leave attach
}
