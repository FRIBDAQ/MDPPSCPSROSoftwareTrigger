vmusb create vme
#vmusb config vme -bufferlength 8k
vmusb config vme -bufferlength 64
vmusb config vme -scalera nimi2 -readscalers true -incremental false
#vmusb config vme -bufferlength evtcount -eventsperbuffer 1
#vmusb config vme -forcescalerdump true

set tfintdiff         [list 67 20 77 20  20 20 20 20]
set pz                [list 2040 2048 0xFFFF 0xFFFF  0xFFFF 0xFFFF 0xFFFF 0xFFFF \
                            0xFFFF 0xFFFF 0xFFFF 0xFFFF  1840 0xFFFF 0xFFFF 0xFFFF \
                            0xFFFF 0xFFFF 0xFFFF 0xFFFF  0xFFFF 0xFFFF 0xFFFF 0xFFFF \
                            0xFFFF 0xFFFF 0xFFFF 0xFFFF  0xFFFF 0xFFFF 0xFFFF 0xFFFF]
set gain              [list 1000 1000 1000 100  200 200 1000 1000]
set threshold         [list 2050 2050 1000 2000  1400 900 1600 5000 \
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
# outputformat 4 REQUIRED FOR SCP SRO Software Trigger #####
mdpp32scp config scp -tfintdiff $tfintdiff \
                     -outputformat 4 \
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

set adcChannels(scp) [list scp.000 scp.001 scp.002 scp.003 scp.004 scp.005 scp.006 scp.007 \
                           scp.008 scp.009 scp.010 scp.011 scp.012 scp.013 scp.014 scp.015 \
                           scp.016 scp.017 scp.018 scp.019 scp.020 scp.021 scp.022 scp.023 \
                           scp.024 scp.025 scp.026 scp.027 scp.028 scp.029 scp.030 scp.031 \
                           scp.032 scp.033 scp.034 scp.035 scp.036 scp.037 scp.038 scp.039 \
                           scp.040 scp.041 scp.042 scp.043 scp.044 scp.045 scp.046 scp.047 \
                           scp.048 scp.049 scp.050 scp.051 scp.052 scp.053 scp.054 scp.055 \
                           scp.056 scp.057 scp.058 scp.059 scp.060 scp.061 scp.062 scp.063 \
                           scp.064 scp.065 scp.066 scp.067 scp.068 scp.069 scp.070 scp.071 \
                           scp.072 scp.073 scp.074 scp.075 scp.076 scp.077 scp.078 scp.079 \
                           scp.080 scp.081 scp.082 scp.083 scp.084 scp.085 scp.086 scp.087 \
                           scp.088 scp.089 scp.090 scp.091 scp.092 scp.093 scp.094 scp.095 \
                           scp.096 scp.097 scp.098 scp.099 scp.100 scp.101 scp.102 scp.103 \
                           scp.104 scp.105 scp.106 scp.107 scp.108 scp.109 scp.110 scp.111 \
                           scp.112 scp.113 scp.114 scp.115 scp.116 scp.117 scp.118 scp.119 \
                           scp.120 scp.121 scp.122 scp.123 scp.124 scp.125 scp.126 scp.127]
                           # 32-63: rollover count | 24.41ps timestamp for 0-31
                           # 64-95: timestamp from window start or 0
                           # 96-127: vme scaler 1 for 0-31
