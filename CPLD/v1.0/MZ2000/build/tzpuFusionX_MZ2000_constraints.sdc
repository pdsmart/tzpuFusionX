## Generated SDC file "tzpuFusionX.out.sdc"

## Copyright (C) 1991-2013 Altera Corporation
## Your use of Altera Corporation's design tools, logic functions 
## and other software and tools, and its AMPP partner logic 
## functions, and any output files from any of the foregoing 
## (including device programming or simulation files), and any 
## associated documentation or information are expressly subject 
## to the terms and conditions of the Altera Program License 
## Subscription Agreement, Altera MegaCore Function License 
## Agreement, or other applicable license agreement, including, 
## without limitation, that your use is for the sole purpose of 
## programming logic devices manufactured by Altera and sold by 
## Altera or its authorized distributors.  Please refer to the 
## applicable agreement for further details.


## VENDOR  "Altera"
## PROGRAM "Quartus II"
## VERSION "Version 13.0.1 Build 232 06/12/2013 Service Pack 1 SJ Web Edition"

## DATE    "Fri Jun 26 22:10:05 2020"

##
## DEVICE  "EPM7160STC100-10"
##


#**************************************************************
# Time Information
#**************************************************************

set_time_format -unit ns -decimal_places 3



#**************************************************************
# Create Clock
#**************************************************************

# Standard mainboard clock. If using tzpuFusionX on a different host then set to the host frequency.
create_clock -name {Z80_CLK} -period 250.000 -waveform { 0.000 125.000 } [get_ports { Z80_CLK }]

# For 50MHz crystal.
create_clock -name {CLK_50M} -period 20.000 -waveform { 0.000 10.000 }   [ get_ports { CLK_50M }]

# For SPI CSn
#create_clock -name {VSOM_SPI_CSn} -period 200.000 -waveform { 160.000 40.000 }   [ get_ports { VSOM_SPI_CSn }]

# For SPI CLK
create_clock -name {VSOM_SPI_CLK} -period 14.000 -waveform { 0.000 7.000 }   [ get_ports { VSOM_SPI_CLK }]

# For basic board with oscillator.
#create_clock -name {CTLCLK} -period 20.000 -waveform { 0.000 10.000 }   [ get_ports { CTLCLK }]
#create_clock -name {cpld512:cpldl512Toplevel|CTLCLKi} -period 280.000 -waveform { 0.000 140.000 } [ get_keepers {cpld512:cpldl512Toplevel|CTLCLKi} ]
##create_clock -name {Z80_CLK} -period 50.000 -waveform { 0.000 25.000 } [get_ports { CTLCLK }]


#**************************************************************
# Create Generated Clock
#**************************************************************



#**************************************************************
# Set Clock Latency
#**************************************************************



#**************************************************************
# Set Clock Uncertainty
#**************************************************************

#derive_clock_uncertainty



#**************************************************************
# Set Input Delay
#**************************************************************

#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {CTL_MBSEL}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {CTL_BUSRQn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {CTL_WAITn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {SYS_BUSRQn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {SYS_WAITn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_ADDR[*]}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_HI_ADDR[*]}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {VZ80_ADDR[*]}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_BUSACKn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_DATA[*]}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_HALTn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_IORQn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_M1n}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_MREQn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_RESETn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_RFSHn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_WRn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {Z80_RDn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {R_IN}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {G_IN}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {B_IN}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {COLR_IN}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {CSYNC_IN}]
##set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {CVIDEO_IN}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {HSYNC_IN}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {VSYNC_IN}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  1.000 [get_ports {VZ80_DATA[*]}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_MREQn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_IORQn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_WRn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_RDn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_M1n}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_BUSACKn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_INTn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_NMIn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_WAITn}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VWAITn_A21_V_CSYNC}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_A20_RFSHn_V_HSYNC}]
#set_input_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_A19_HALTn_V_VSYNC}]

#**************************************************************
# Set Output Delay
#**************************************************************

#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {CTL_BUSACKn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {CTL_HALTn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {CTL_M1n}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {CTL_RFSHn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {RAM_CSn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {RAM_CS2n}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {RAM_OEn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {RAM_WEn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {SVCREQn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {SYS_BUSACKn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_BUSRQn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_DATA[*]}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_ADDR[*]}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_HI_ADDR[*]}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_RA_ADDR[*]}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_WAITn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_MREQn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_CLK}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_ADDR[*]}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_DATA[*]}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_CLK}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_MREQn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_IORQn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_RDn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_WRn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_M1n}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VIDEO_RDn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VIDEO_WRn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_A18_INTn_V_R}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_BUSRQn_V_G}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_A16_WAITn_V_B}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_A17_NMIn_V_COLR}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VWAITn_A21_V_CSYNC}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_A20_RFSHn_V_HSYNC}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {VZ80_A19_HALTn_V_VSYNC}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_HALTn}]
#set_output_delay -add_delay  -clock [get_clocks {SYSCLK}]  5.000 [get_ports {Z80_RFSHn}]

# For K64F
#set_output_delay -add_delay  -clock [get_clocks {CTLCLK}]  5.000 [get_ports {Z80_CLK}]

# For basic board with oscillator.
#set_output_delay -add_delay  -clock [get_clocks {cpld512:cpldl512Toplevel|CTLCLKi}]  5.000 [get_ports {Z80_CLK}]

#**************************************************************
# Set Max Delay
#**************************************************************
#set_max_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_HALTn} 30.000
#set_max_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_IORQn} 30.000
#set_max_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_M1n}   30.000
#set_max_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_RDn}   30.000
#set_max_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_WRn}   30.000
#set_max_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_RFSHn} 30.000
#set_max_delay -from [get_ports {VZ80_A19_HALTn_V_VSYNC}]        -to {Z80_HALTn} 30.000
#set_max_delay -from [get_ports {VZ80_IORQn}]                    -to {Z80_IORQn} 30.000
#set_max_delay -from [get_ports {VZ80_MREQn}]                    -to {Z80_IORQn} 30.000
#set_max_delay -from [get_ports {VZ80_M1n}]                      -to {Z80_M1n}   30.000
#set_max_delay -from [get_ports {VZ80_RDn}]                      -to {Z80_RDn}   30.000
#set_max_delay -from [get_ports {VZ80_WRn}]                      -to {Z80_WRn}   30.000
#set_max_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_HALTn} 40.000
#set_max_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_RFSHn} 40.000
#set_max_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_IORQn} 40.000
#set_max_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_M1n}   30.000
#set_max_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_RDn}   30.000
#set_max_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_WRn}   30.000
#set_max_delay -from [get_ports {VZ80_A20_RFSHn_V_HSYNC}]        -to {Z80_RFSHn} 30.000
#set_max_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_HALTn} 30.000
#set_max_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_IORQn} 30.000
#set_max_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_M1n}   30.000
#set_max_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_RDn}   30.000
#set_max_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_WRn}   30.000
#set_max_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_RFSHn} 30.000
#set_max_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_HALTn}]   45.000
#set_max_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_IORQn}]   30.000
#set_max_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_M1n}]     30.000
#set_max_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_RDn}]     30.000
#set_max_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_RFSHn}]   45.000
#set_max_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_WRn}]     30.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_HALTn}]   45.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_IORQn}]   50.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_M1n}]     40.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_RDn}]     40.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_WRn}]     40.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_RFSHn}]   45.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_HALTn}]   60.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_IORQn}]   45.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_M1n}]     40.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_RDn}]     40.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_WRn}]     40.000
#set_max_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_RFSHn}]   60.000

#**************************************************************
# Set Min Delay
#**************************************************************
#set_min_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_HALTn} 1.000
#set_min_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_IORQn} 1.000
#set_min_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_M1n}   1.000
#set_min_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_RDn}   1.000
#set_min_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_WRn}   1.000
#set_min_delay -from [get_ports {CTL_BUSRQn}]                    -to {Z80_RFSHn} 1.000
#set_min_delay -from [get_ports {VZ80_A19_HALTn_V_VSYNC}]        -to {Z80_HALTn} 1.000
#set_min_delay -from [get_ports {VZ80_IORQn}]                    -to {Z80_IORQn} 1.000
#set_min_delay -from [get_ports {VZ80_MREQn}]                    -to {Z80_IORQn} 1.000
#set_min_delay -from [get_ports {VZ80_M1n}]                      -to {Z80_M1n}   1.000
#set_min_delay -from [get_ports {VZ80_RDn}]                      -to {Z80_RDn}   1.000
#set_min_delay -from [get_ports {VZ80_WRn}]                      -to {Z80_WRn}   1.000
#set_min_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_HALTn} 1.000
#set_min_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_RFSHn} 1.000
#set_min_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_IORQn} 1.000
#set_min_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_M1n}   1.000
#set_min_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_RDn}   1.000
#set_min_delay -from [get_ports {VZ80_BUSACKn}]                  -to {Z80_WRn}   1.000
#set_min_delay -from [get_ports {VZ80_A20_RFSHn_V_HSYNC}]        -to {Z80_RFSHn} 1.000
#set_min_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_HALTn} 1.000
#set_min_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_IORQn} 1.000
#set_min_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_M1n}   1.000
#set_min_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_RDn}   1.000
#set_min_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_WRn}   1.000
#set_min_delay -from [get_ports {Z80_BUSACKn}]                   -to {Z80_RFSHn} 1.000
#set_min_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_HALTn}] 1.000
#set_min_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_IORQn}] 1.000
#set_min_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_M1n}]   1.000
#set_min_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_RDn}]   1.000
#set_min_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_RFSHn}] 1.000
#set_min_delay -from {Z80_BUSACKn}                               -to [get_ports {Z80_WRn}]   1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_HALTn}] 1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_IORQn}] 1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_M1n}]   1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_RDn}]   1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_WRn}]   1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]} -to [get_ports {Z80_RFSHn}] 1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_HALTn}] 1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_IORQn}] 1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_M1n}]   1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_RDn}]   1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_WRn}]   1.000
#set_min_delay -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}  -to [get_ports {Z80_RFSHn}] 1.000



#**************************************************************
# Set Clock Groups
#**************************************************************



#**************************************************************
# Set False Path
#**************************************************************

# For K64F
#set_false_path -from [get_clocks {CTLCLK}] -to [get_clocks {SYSCLK}]
#set_false_path -from [get_clocks {SYSCLK}] -to [get_clocks {CTLCLK}]

# For basic board with oscillator.
#set_false_path -from [get_clocks {cpld512:cpldl512Toplevel|CTLCLKi}] -to [get_clocks {SYSCLK}]
#set_false_path -from [get_clocks {cpld512:cpldl512Toplevel|CTLCLKi}] -to [get_clocks {CTLCLK}]
#set_false_path -from [get_clocks {SYSCLK}] -to [get_clocks {cpld512:cpldl512Toplevel|CTLCLKi}]
#set_false_path -from [get_clocks {SYSCLK}] -to [get_clocks {CTLCLK}]

# For both configurations.
#set_false_path -from {cpld512:cpldl512Toplevel|KEY_SUBSTITUTE}    -to {cpld512:cpldl512Toplevel|CTLCLK_Q}
#set_false_path -from {cpld512:cpldl512Toplevel|MEM_MODE_LATCH[*]}  -to {cpld512:cpldl512Toplevel|CTLCLK_Q}
#set_false_path -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]}    -to {cpld512:cpldl512Toplevel|CTLCLK_Q}
#set_false_path -from {cpld512:cpldl512Toplevel|CPLD_CFG_DATA[*]}   -to {cpld512:cpldl512Toplevel|CTLCLK_Q}
#set_false_path -from {cpld512:cpldl512Toplevel|MZ80B_VRAM_HI_ADDR} -to {cpld512:cpldl512Toplevel|CTLCLK_Q}
#set_false_path -from {cpld512:cpldl512Toplevel|MZ80B_VRAM_LO_ADDR} -to {cpld512:cpldl512Toplevel|CTLCLK_Q}
#set_false_path -from {cpld512:cpldl512Toplevel|MODE_VIDEO_MZ80B}   -to {cpld512:cpldl512Toplevel|CTLCLK_Q}
#set_false_path -from {cpld512:cpldl512Toplevel|GRAM_PAGE_ENABLE}   -to {cpld512:cpldl512Toplevel|CTLCLK_Q}

#**************************************************************
# Set Multicycle Path
#**************************************************************
#set_multicycle_path -from {cpld512:cpldl512Toplevel|CTL_BUSRQni} -to {cpld512:cpldl512Toplevel|CTLCLK_Q} -setup -end 2
#set_multicycle_path -from {cpld512:cpldl512Toplevel|CTL_BUSRQni} -to {cpld512:cpldl512Toplevel|CTLCLK_Q} -hold -end 1

#set_multicycle_path -from {cpld512:cpldl512Toplevel|CTL_BUSRQni} -to {cpld512:cpldl512Toplevel|MEM_MODE_LATCH[*]} -setup -end 2
#set_multicycle_path -from {cpld512:cpldl512Toplevel|CTL_BUSRQni} -to {cpld512:cpldl512Toplevel|MEM_MODE_LATCH[*]} -hold -end 1

#set_multicycle_path -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]} -to {cpld512:cpldl512Toplevel|MEM_MODE_LATCH[*]} -setup -end 2
#set_multicycle_path -from {cpld512:cpldl512Toplevel|CPU_CFG_DATA[*]} -to {cpld512:cpldl512Toplevel|MEM_MODE_LATCH[*]} -hold -end 1

#**************************************************************
# Set Maximum Delay
#**************************************************************



#**************************************************************
# Set Minimum Delay
#**************************************************************



#**************************************************************
# Set Input Transition
#**************************************************************

