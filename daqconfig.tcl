vmusb create vme
#vmusb config vme -bufferlength 8k
vmusb config vme -bufferlength 64
vmusb config vme -scalera nimi2 -readscalers true -incremental false
#vmusb config vme -bufferlength evtcount -eventsperbuffer 1
#vmusb config vme -forcescalerdump true

set tfintdiff         [list 67 20 77 20  20 20 20 20]
set pz                [list 2040 0xFFFF 0xFFFF 0xFFFF  0xFFFF 0xFFFF 0xFFFF 0xFFFF \
                            0xFFFF 0xFFFF 0xFFFF 0xFFFF  1840 0xFFFF 0xFFFF 0xFFFF \
                            0xFFFF 0xFFFF 0xFFFF 0xFFFF  0xFFFF 0xFFFF 0xFFFF 0xFFFF \
                            0xFFFF 0xFFFF 0xFFFF 0xFFFF  0xFFFF 0xFFFF 0xFFFF 0xFFFF]
set gain              [list 1000 1000 1000 100  200 200 1000 1000]
set threshold         [list 2050 0xFFFF 1000 2000  1400 900 1600 5000 \
                            1400 0xFFFF 0xFFFF 0xFFFF  1000 0xFFFF 0xFFFF 0xFFFF \
                            0xFFFF 0xFFFF 0xFFFF 0xFFFF  0xFFFF 0xFFFF 0xFFFF 0xFFFF \
                            0xFFFF 0xFFFF 0xFFFF 500  0xFFFF 0xFFFF 0xFFFF 500]
set shapingtime       [list 140 140 140 160  200 200 200 200]
set blr               [list 2 2 2 2  2 2 2 2]
set signalrisetime    [list 16 0 0 124  16 16 16 16]
set resettime         [list 0 1000 0 0  0 0 0 0]
set windowStart        16380;#0x3F80
set windowWidth         10000
set firstHit            0
set testPulser          0
set pulserAmplitude     400
set triggerSource       0x400
set triggerOutput       0x400

### REQUIRED FOR SCP SRO Software Trigger #####
set MDPPSCPSROSoftTrigger 1
set spectcl 0
if {[info globals SpecTclHome] ne ""} {
	set spectcl 1
}
### REQUIRED FOR SCP SRO Software Trigger #####

mdpp32scp create scp -base 0x22220000 -id 0 -ipl 1 -vector 0 -irqsource event -irqeventthreshold 1
mdpp32scp config scp -tfintdiff $tfintdiff \
                     -outputformat 4 \   ;### REQUIRED FOR SCP SRO Software Trigger #####
                     -pz $pz \
                     -gain $gain \
                     -threshold $threshold \
                     -shapingtime $shapingtime \
                     -blr $blr \
                     -signalrisetime $signalrisetime \
                     -windowstart $windowStart \
                     -windowwidth $windowWidth \
                     -firsthit $firstHit \
                     -testpulser $testPulser \
                     -resettime $resettime \
                     -pulseramplitude $pulserAmplitude \
                     -triggersource $triggerSource \
                     -triggeroutput $triggerOutput \
                     -resettime $resettime \
                     -printregisters 1

sis3820 create sisscaler 0x38000000
sis3820 config sisscaler -timestamp on -outputmode clock2x10Mhz

stack create events
### REQUIRED FOR SCP SRO Software Trigger #####
if {! $spectcl} {
	stack config events -trigger interrupt -stack 2 -ipl 1 -vector 0xff00 -modules [list vme scp] -delay 0
} else {
	stack config events -trigger interrupt -stack 2 -ipl 1 -vector 0xff00 -modules [list scp] -delay 0
}
### REQUIRED FOR SCP SRO Software Trigger #####

set adcChannels(scp) [list scp.00 scp.01 scp.02 scp.03 scp.04 scp.05 scp.06 scp.07 \
                           scp.08 scp.09 scp.10 scp.11 scp.12 scp.13 scp.14 scp.15 \
                           scp.16 scp.17 scp.18 scp.19 scp.20 scp.21 scp.22 scp.23 \
                           scp.24 scp.25 scp.26 scp.27 scp.28 scp.29 scp.30 scp.31 \
                           scp.32 scp.33 scp.34 scp.35]
                           # 32: rollover count
                           # 33: 24.41ps timestamp
													 # 34: vme scaler 1
													 # 35: vme scaler 2
