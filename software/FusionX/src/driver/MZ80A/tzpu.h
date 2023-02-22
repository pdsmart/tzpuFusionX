/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            tzpu.h
// Created:         May 2020
// Author(s):       Philip Smart
// Description:     The TranZPUter library.
//                  This file is copied from the original zSoft tranZPUter library header file and 
//                  modified for the FusionX tranZPUter SW driver. It is used by the Z80 driver and the
//                  user space daemon. 
//                  Hardware references have been removed as the K64F is a virtual process rather
//                  than a physical MPU.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         May 2020 - Initial write of the TranZPUter software.
//                  Jul 2020 - Updates to accommodate v2.1 of the tranZPUter board.
//                  Sep 2020 - Updates to accommodate v2.2 of the tranZPUter board.
//                  May 2021 - Changes to use 512K-1Mbyte Z80 Static RAM, build time configurable.
//                  Feb 2023 - Adaptation of zSoft tranzputer.h for the FusionX tranZPUter SW driver.
//
// Notes:           See Makefile to enable/disable conditional components
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// This source file is free software: you can redistribute it and#or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This source file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef TZPU_H
#define TZPU_H

#ifdef __cplusplus
    extern "C" {
#endif

// Configurable constants.
//
#define REFRESH_BYTE_COUNT           8                                   // This constant controls the number of bytes read/written to the z80 bus before a refresh cycle is needed.
#define RFSH_BYTE_CNT                256                                 // Number of bytes we can write before needing a full refresh for the DRAM.
#define HOST_MON_TEST_VECTOR         0x4                                 // Address in the host monitor to test to identify host type.
#define OS_BASE_DIR                  "/apps/FusionX/disk/MZ-80A/"        // Linux base directory where all the files are stored. On a real tranZPUter this would be the SD card root dir.
#define TZFS_AUTOBOOT_FLAG           OS_BASE_DIR "/TZFSBOOT.FLG"         // Filename used as a flag, if this file exists in the base directory then TZFS is booted automatically.
#define TZ_MAX_Z80_MEM               0x100000                            // Maximum Z80 memory available on the tranZPUter board.

// tranZPUter Memory Modes - select one of the 32 possible memory models using these constants.
//
#define TZMM_ORIG                    0x00                                // Original Sharp MZ80A mode, no tranZPUter features are selected except the I/O control registers (default: 0x60-063).
#define TZMM_BOOT                    0x01                                // Original mode but E800-EFFF is mapped to tranZPUter RAM so TZFS can be booted.
#define TZMM_TZFS                    0x02                                // TZFS main memory configuration. all memory is in tranZPUter RAM, E800-FFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected.
#define TZMM_TZFS2                   0x03                                // TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 1.
#define TZMM_TZFS3                   0x04                                // TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 2.
#define TZMM_TZFS4                   0x05                                // TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 3.
#define TZMM_CPM                     0x06                                // CPM main memory configuration, all memory on the tranZPUter board, 64K block 4 selected. Special case for F3C0:F3FF & F7C0:F7FF (floppy disk paging vectors) which resides on the mainboard.
#define TZMM_CPM2                    0x07                                // CPM main memory configuration, F000-FFFF are on the tranZPUter board in block 4, 0040-CFFF and E800-EFFF are in block 5, mainboard for D000-DFFF (video), E000-E800 (Memory control) selected.
                                                                         // Special case for 0000:003F (interrupt vectors) which resides in block 4, F3C0:F3FF & F7C0:F7FF (floppy disk paging vectors) which resides on the mainboard.
#define TZMM_COMPAT                  0x08                                // Original mode but with main DRAM in Bank 0 to allow bootstrapping of programs from other machines such as the MZ700.
#define TZMM_HOSTACCESS              0x09                                // Mode to allow code running in Bank 0, address E800:FFFF to access host memory. Monitor ROM 0000-0FFF and Main DRAM 0x1000-0xD000, video and memory mapped I/O are on the host machine, User/Floppy ROM E800-FFFF are in tranZPUter memory. 
#define TZMM_MZ700_0                 0x0a                                // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the mainboard.
#define TZMM_MZ700_1                 0x0b                                // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 0, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the tranZPUter in block 6.
#define TZMM_MZ700_2                 0x0c                                // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the tranZPUter in block 6.
#define TZMM_MZ700_3                 0x0d                                // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 0, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is inaccessible.
#define TZMM_MZ700_4                 0x0e                                // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is inaccessible.
#define TZMM_MZ800                   0x0f                                // MZ800 Mode - Host is an MZ-800 and mode provides for MZ-700/MZ-800 decoding per original machine.
#define TZMM_MZ2000                  0x10                                // MZ2000 Mode - Running on MZ2000 hardware, configuration set according to runtime configuration registers.
#define TZMM_FPGA                    0x15                                // Open up access for the K64F to the FPGA resources such as memory. All other access to RAM or mainboard is blocked.
#define TZMM_TZPUM                   0x16                                // Everything is on mainboard, no access to tranZPUter memory.
#define TZMM_TZPU                    0x17                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory. K64F drives A18-A16 allowing full access to RAM.                                                                                                           
//#define TZMM_TZPU0                   0x18                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 0 is selected.
//#define TZMM_TZPU1                   0x19                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 1 is selected.
//#define TZMM_TZPU2                   0x1A                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 2 is selected.
//#define TZMM_TZPU3                   0x1B                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 3 is selected.
//#define TZMM_TZPU4                   0x1C                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 4 is selected.
//#define TZMM_TZPU5                   0x1D                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 5 is selected.
//#define TZMM_TZPU6                   0x1E                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 6 is selected.
//#define TZMM_TZPU7                   0x1F                                // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 7 is selected.

// IO addresses on the tranZPUter or mainboard.
//
#define IO_TZ_CTRLLATCH              0x60                                // Control latch which specifies the Memory Model/mode.
#define IO_TZ_SETXMHZ                0x62                                // Switch to alternate CPU frequency provided by K64F.
#define IO_TZ_SET2MHZ                0x64                                // Switch to system CPU frequency.
#define IO_TZ_CLKSELRD               0x66                                // Read the status of the clock select, ie. which clock is connected to the CPU.
#define IO_TZ_SVCREQ                 0x68                                // Service request from the Z80 to be provided by the K64F.
#define IO_TZ_SYSREQ                 0x6A                                // System request from the Z80 to be provided by the K64F.
#define IO_TZ_CPLDCMD                0x6B                                // Version 2.1 CPLD command register.
#define IO_TZ_CPLDSTATUS             0x6B                                // Version 2.1 CPLD status register.
#define IO_TZ_CPUCFG                 0x6C                                // Version 2.2 CPU configuration register.
#define IO_TZ_CPUSTATUS              0x6C                                // Version 2.2 CPU runtime status register.
#define IO_TZ_CPUINFO                0x6D                                // Version 2.2 CPU information register.
#define IO_TZ_CPLDCFG                0x6E                                // Version 2.1 CPLD configuration register.
#define IO_TZ_CPLDINFO               0x6F                                // Version 2.1 CPLD version information register.
#define IO_TZ_PALSLCTOFF             0xA3                                // set the palette slot Off position to be adjusted.
#define IO_TZ_PALSLCTON              0xA4                                // set the palette slot On position to be adjusted.
#define IO_TZ_PALSETRED              0xA5                                // set the red palette value according to the PALETTE_PARAM_SEL address.
#define IO_TZ_PALSETGREEN            0xA6                                // set the green palette value according to the PALETTE_PARAM_SEL address.
#define IO_TZ_PALSETBLUE             0xA7                                // set the blue palette value according to the PALETTE_PARAM_SEL address.
#define IO_TZ_OSDMNU_SZX             0xA8                                // Get OSD Menu Horizontal Size (X).
#define IO_TZ_OSDMNU_SZY             0xA9                                // Get OSD Menu Vertical Size (Y).
#define IO_TZ_OSDHDR_SZX             0xAA                                // Get OSD Status Header Horizontal Size (X).
#define IO_TZ_OSDHDR_SZY             0xAB                                // Get OSD Status Header Vertical Size (Y).
#define IO_TZ_OSDFTR_SZX             0xAC                                // Get OSD Status Footer Horizontal Size (X).
#define IO_TZ_OSDFTR_SZY             0xAD                                // Get OSD Status Footer Vertical Size (Y).   
#define IO_TZ_PALETTE                0xB0                                // Sets the palette. The Video Module supports 4 bit per colour output but there is only enough RAM for 1 bit per colour so the pallette is used to change the colours output.
                                                                         //    Bits [7:0] defines the pallete number. This indexes a lookup table which contains the required 4bit output per 1bit input.
#define IO_TZ_GPUPARAM               0xB2                                // Set parameters. Store parameters in a long word to be used by the graphics command processor.
                                                                         //    The parameter word is 128 bit and each write to the parameter word shifts left by 8 bits and adds the new byte at bits 7:0.
#define IO_TZ_GPUCMD                 0xB3                                // Set the graphics processor unit commands.
                                                                         //    Bits [5:0] - 0 = Reset parameters.
                                                                         //                 1 = Clear to val. Start Location (16 bit), End Location (16 bit), Red Filter, Green Filter, Blue Filter
#define IO_TZ_VMCTRL                 0xB8                                // Video Module control register. [2:0] - 000 (default) = MZ80A, 001 = MZ-700, 010 = MZ800, 011 = MZ80B, 100 = MZ80K, 101 = MZ80C, 110 = MZ1200, 111 = MZ2000. [3] = 0 - 40 col, 1 - 80 col.
#define IO_TZ_VMGRMODE               0xB9                                // Video Module graphics mode. 7/6 = Operator (00=OR,01=AND,10=NAND,11=XOR), 5=GRAM Output Enable, 4 = VRAM Output Enable, 3/2 = Write mode (00=Page 1:Red, 01=Page 2:Green, 10=Page 3:Blue, 11=Indirect), 1/0=Read mode (00=Page 1:Red, 01=Page2:Green, 10=Page 3:Blue, 11=Not used).
#define IO_TZ_VMREDMASK              0xBA                                // Video Module Red bit mask (1 bit = 1 pixel, 8 pixels per byte).
#define IO_TZ_VMGREENMASK            0xBB                                // Video Module Green bit mask (1 bit = 1 pixel, 8 pixels per byte).
#define IO_TZ_VMBLUEMASK             0xBC                                // Video Module Blue bit mask (1 bit = 1 pixel, 8 pixels per byte).
#define IO_TZ_VMPAGE                 0xBD                                // Video Module memory page register. [1:0] switches in 1 16Kb page (3 pages) of graphics ram to C000 - FFFF. Bits [1:0] = page, 00 = off, 01 = Red, 10 = Green, 11 = Blue. This overrides all MZ700/MZ80B page switching functions. [7] 0 - normal, 1 - switches in CGROM for upload at D000:DFFF.
#define IO_TZ_VMVGATTR               0xBE                                // Select VGA Border colour and attributes. Bit 2 = Red, 1 = Green, 0 = Blue, 4:3 = VGA Mode, 00 = Off, 01 = 640x480, 10 = 800x600, 11 = 50Hz Internal
#define IO_TZ_VMVGAMODE              0xBF                                // Select VGA Output mode, ie. Internal, 640x480 etc. Bits [3:0] specify required mode. Undefined default to internal standard frequency.
#define IO_TZ_GDGWF                  0xCC                                // MZ-800      write format register
#define IO_TZ_GDGRF                  0xCD                                // MZ-800      read format register
#define IO_TZ_GDCMD                  0xCE                                // MZ-800 CRTC Mode register
#define IO_TZ_GDCCTRL                0xCF                                // MZ-800 CRTC control register
#define IO_TZ_MMIO0                  0xE0                                // MZ-700/MZ-800 Memory management selection ports.
#define IO_TZ_MMIO1                  0xE1                                // ""
#define IO_TZ_MMIO2                  0xE2                                // ""
#define IO_TZ_MMIO3                  0xE3                                // ""
#define IO_TZ_MMIO4                  0xE4                                // ""
#define IO_TZ_MMIO5                  0xE5                                // ""
#define IO_TZ_MMIO6                  0xE6                                // ""
#define IO_TZ_MMIO7                  0xE7                                // MZ-700/MZ-800 Memory management selection ports.
#define IO_TZ_PPIA                   0xE0                                // MZ80B/MZ2000 8255 PPI Port A
#define IO_TZ_PPIB                   0xE1                                // MZ80B/MZ2000 8255 PPI Port B
#define IO_TZ_PPIC                   0xE2                                // MZ80B/MZ2000 8255 PPI Port C
#define IO_TZ_PPICTL                 0xE3                                // MZ80B/MZ2000 8255 PPI Control Register
#define IO_TZ_PIT0                   0xE4                                // MZ80B/MZ2000 8253 PIT Timer 0
#define IO_TZ_PIT1                   0xE5                                // MZ80B/MZ2000 8253 PIT Timer 1
#define IO_TZ_PIT2                   0xE6                                // MZ80B/MZ2000 8253 PIT Timer 2
#define IO_TZ_PITCTL                 0xE7                                // MZ80B/MZ2000 8253 PIT Control Register
#define IO_TZ_PIOA                   0xE8                                // MZ80B/MZ2000 Z80 PIO Port A
#define IO_TZ_PIOCTLA                0xE9                                // MZ80B/MZ2000 Z80 PIO Port A Control Register
#define IO_TZ_PIOB                   0xEA                                // MZ80B/MZ2000 Z80 PIO Port B
#define IO_TZ_PIOCTLB                0xEB                                // MZ80B/MZ2000 Z80 PIO Port B Control Register
#define IO_TZ_SYSCTRL                0xF0                                // System board control register. [2:0] - 000 MZ80A Mode, 2MHz CPU/Bus, 001 MZ80B Mode, 4MHz CPU/Bus, 010 MZ700 Mode, 3.54MHz CPU/Bus.
#define IO_TZ_GRAMMODE               0xF4                                // MZ80B Graphics mode.  Bit 0 = 0, Write to Graphics RAM I, Bit 0 = 1, Write to Graphics RAM II. Bit 1 = 1, blend Graphics RAM I output on display, Bit 2 = 1, blend Graphics RAM II output on display.
//#define IO_TZ_GRAMOPT                0xF4                                // MZ80B/MZ2000 GRAM configuration option.
#define IO_TZ_CRTGRPHPRIO            0xF5                                // MZ2000 Graphics priority register, character or a graphics colour has front display priority.
#define IO_TZ_CRTGRPHSEL             0xF6                                // MZ2000 Graphics output select on CRT or external CRT
#define IO_TZ_GRAMCOLRSEL            0xF7                                // MZ2000 Graphics RAM colour bank select.

// Addresses on the tranZPUter board.
//
#define SRAM_BANK0_ADDR              0x00000                             // Address of the 1st 64K RAM bank in the SRAM chip.
#define SRAM_BANK1_ADDR              0x10000                             // ""
#define SRAM_BANK2_ADDR              0x20000                             // ""
#define SRAM_BANK3_ADDR              0x30000                             // ""
#define SRAM_BANK4_ADDR              0x40000                             // ""
#define SRAM_BANK5_ADDR              0x50000                             // ""
#define SRAM_BANK6_ADDR              0x60000                             // ""
#define SRAM_BANK7_ADDR              0x70000                             // ""
#define SRAM_BANK8_ADDR              0x80000                             // ""
#define SRAM_BANK9_ADDR              0x90000                             // ""
#define SRAM_BANKA_ADDR              0xA0000                             // ""
#define SRAM_BANKB_ADDR              0xB0000                             // ""
#define SRAM_BANKC_ADDR              0xC0000                             // ""
#define SRAM_BANKD_ADDR              0xD0000                             // ""
#define SRAM_BANKE_ADDR              0xE0000                             // ""
#define SRAM_BANKF_ADDR              0xF0000                             // Address of the 16th 64K RAM bank in the SRAM chip.

// IO register constants.
//
#define CPUMODE_SET_Z80              0x00                                // Set the CPU to the hard Z80.
#define CPUMODE_SET_T80              0x01                                // Set the CPU to the soft T80.
#define CPUMODE_SET_ZPU_EVO          0x02                                // Set the CPU to the soft ZPU Evolution.
#define CPUMODE_SET_EMU_MZ           0x04                                // 
#define CPUMODE_SET_BBB              0x08                                // Place holder for a future soft CPU.
#define CPUMODE_SET_CCC              0x10                                // Place holder for a future soft CPU.
#define CPUMODE_SET_DDD              0x20                                // Place holder for a future soft CPU.
#define CPUMODE_IS_Z80               0x00                                // Status value to indicate if the hard Z80 available.
#define CPUMODE_IS_T80               0x01                                // Status value to indicate if the soft T80 available.
#define CPUMODE_IS_ZPU_EVO           0x02                                // Status value to indicate if the soft ZPU Evolution available.
#define CPUMODE_IS_EMU_MZ            0x04                                // Status value to indicate if the Sharp MZ Series Emulation is available.
#define CPUMODE_IS_BBB               0x08                                // Place holder to indicate if a future soft CPU is available.
#define CPUMODE_IS_CCC               0x10                                // Place holder to indicate if a future soft CPU is available.
#define CPUMODE_IS_DDD               0x20                                // Place holder to indicate if a future soft CPU is available.
#define CPUMODE_CLK_EN               0x40                                // Toggle the soft CPU clock, 1 = enable, 0 = disable.
#define CPUMODE_RESET_CPU            0x80                                // Reset the soft CPU. Active high, when high the CPU is held in RESET, when low the CPU runs.
#define CPUMODE_IS_SOFT_AVAIL        0x040                               // Marker to indicate if the underlying FPGA can support soft CPU's.
#define CPUMODE_IS_SOFT_MASK         0x03F                               // Mask to filter out the Soft CPU availability flags.

// CPLD Configuration constants.
#define HWMODE_MZ80K                 0x00                                // Hardware mode = MZ80K
#define HWMODE_MZ80C                 0x01                                // Hardware mode = MZ80C
#define HWMODE_MZ1200                0x02                                // Hardware mode = MZ1200
#define HWMODE_MZ80A                 0x03                                // Hardware mode = MZ80A
#define HWMODE_MZ700                 0x04                                // Hardware mode = MZ700
#define HWMODE_MZ800                 0x05                                // Hardware mode = MZ800
#define HWMODE_MZ80B                 0x06                                // Hardware mode = MZ80B
#define HWMODE_MZ2000                0x07                                // Hardware mode = MZ2000
#define MODE_VIDEO_MODULE_ENABLED    0x08                                // Hardware enable (bit 3 = 1) or disable of the Video Module on the newer version, the one below will be removed.
#define MODE_VIDEO_MODULE_DISABLED   0x00                                // Hardware enable (bit 3 = 0) or disable of the Video Module.
#define MODE_PRESERVE_CONFIG         0x80                                // Preserve hardware configuration on RESET.
#define CPLD_HAS_FPGA_VIDEO          0x00                                // Flag to indicate if this device supports enhanced video.
#define CPLD_VERSION                 0x01                                // Version of the CPLD which is being emulated. Version 1 was the original version.

// CPLD Command Instruction constants.
#define CPLD_RESET_HOST              1                                   // CPLD level command to reset the host system.
#define CPLD_HOLD_HOST_BUS           2                                   // CPLD command to hold the host bus.
#define CPLD_RELEASE_HOST_BUS        3                                   // CPLD command to release the host bus.

// Video Module control bits.
#define SYSMODE_MZ80A                0x00                                // System board mode MZ80A, 2MHz CPU/Bus.
#define SYSMODE_MZ80B                0x01                                // System board mode MZ80B, 4MHz CPU/Bus.
#define SYSMODE_MZ700                0x02                                // System board mode MZ700, 3.54MHz CPU/Bus.
#define VMMODE_MASK                  0xF0                                // Mask to mask out video mode.
#define VMMODE_MZ80K                 0x00                                // Video mode = MZ80K
#define VMMODE_MZ80C                 0x01                                // Video mode = MZ80C
#define VMMODE_MZ1200                0x02                                // Video mode = MZ1200
#define VMMODE_MZ80A                 0x03                                // Video mode = MZ80A
#define VMMODE_MZ700                 0x04                                // Video mode = MZ700
#define VMMODE_MZ800                 0x05                                // Video mode = MZ800
#define VMMODE_MZ1500                0x06                                // Video mode = MZ1500
#define VMMODE_MZ80B                 0x07                                // Video mode = MZ80B
#define VMMODE_MZ2000                0x08                                // Video mode = MZ2000
#define VMMODE_MZ2200                0x09                                // Video mode = MZ2200
#define VMMODE_MZ2500                0x0A                                // Video mode = MZ2500
#define VMMODE_80CHAR                0x10                                // Enable 80 character display.
#define VMMODE_80CHAR_MASK           0xEF                                // Mask to filter out display width control bit.
#define VMMODE_COLOUR                0x20                                // Enable colour display.
#define VMMODE_COLOUR_MASK           0xDF                                // Mask to filter out colour control bit.
#define VMMODE_PCGRAM                0x40                                // Enable PCG RAM.
#define VMMODE_VGA_MASK              0xF0                                // Mask to filter out the VGA output mode bits.
#define VMMODE_VGA_OFF               0x00                                // Set VGA mode off, external monitor is driven by standard internal 60Hz signals.
#define VMMODE_VGA_INT               0x00                                // Set VGA mode off, external monitor is driven by standard internal 60Hz signals.
#define VMMODE_VGA_INT50             0x01                                // Set VGA mode off, external monitor is driven by standard internal 50Hz signals.
#define VMMODE_VGA_640x480           0x02                                // Set external monitor to VGA 640x480 @ 60Hz mode.
#define VMMODE_VGA_800x600           0x03                                // Set external monitor to VGA 800x600 @ 60Hz mode.

// VGA mode border control constants.
//
#define VMBORDER_BLACK               0x00                                // VGA has a black border.
#define VMBORDER_BLUE                0x01                                // VGA has a blue border.
#define VMBORDER_RED                 0x02                                // VGA has a red border.
#define VMBORDER_PURPLE              0x03                                // VGA has a purple border.
#define VMBORDER_GREEN               0x04                                // VGA has a green border.
#define VMBORDER_CYAN                0x05                                // VGA has a cyan border.
#define VMBORDER_YELLOW              0x06                                // VGA has a yellow border.
#define VMBORDER_WHITE               0x07                                // VGA has a white border.
#define VMBORDER_MASK                0xF8                                // Mask to filter out current border setting.

// Sharp MZ colour attributes.
#define VMATTR_FG_BLACK              0x00                                // Foreground black character attribute.
#define VMATTR_FG_BLUE               0x10                                // Foreground blue character attribute.
#define VMATTR_FG_RED                0x20                                // Foreground red character attribute.
#define VMATTR_FG_PURPLE             0x30                                // Foreground purple character attribute.
#define VMATTR_FG_GREEN              0x40                                // Foreground green character attribute.
#define VMATTR_FG_CYAN               0x50                                // Foreground cyan character attribute.
#define VMATTR_FG_YELLOW             0x60                                // Foreground yellow character attribute.
#define VMATTR_FG_WHITE              0x70                                // Foreground white character attribute.
#define VMATTR_FG_MASKOUT            0x8F                                // Mask to filter out foreground attribute.
#define VMATTR_FG_MASKIN             0x70                                // Mask to filter out foreground attribute.
#define VMATTR_BG_BLACK              0x00                                // Background black character attribute.
#define VMATTR_BG_BLUE               0x01                                // Background blue character attribute.
#define VMATTR_BG_RED                0x02                                // Background red character attribute.
#define VMATTR_BG_PURPLE             0x03                                // Background purple character attribute.
#define VMATTR_BG_GREEN              0x04                                // Background green character attribute.
#define VMATTR_BG_CYAN               0x05                                // Background cyan character attribute.
#define VMATTR_BG_YELLOW             0x06                                // Background yellow character attribute.
#define VMATTR_BG_WHITE              0x07                                // Background white character attribute.
#define VMATTR_BG_MASKOUT            0xF8                                // Mask to filter out background attribute.
#define VMATTR_BG_MASKIN             0x07                                // Mask to filter out background attribute.

// Sharp MZ constants.
//
#define MZ_MROM_ADDR                 0x00000                             // Monitor ROM start address.
#define MZ_800_MROM_ADDR             0x70000                             // MZ-800 Monitor ROM address.
#define MZ_800_CGROM_ADDR            0x71000                             // MZ-800 CGROM address during reset when it is loaded into the PCG.
#define MZ_800_IPL_ADDR              0x7E000                             // Address of the 9Z_504M IPL BIOS.
#define MZ_800_IOCS_ADDR             0x7F400                             // Address of the MZ-800 common IOCS bios.
#define MZ_MROM_STACK_ADDR           0x01000                             // Monitor ROM start stack address.
#define MZ_MROM_STACK_SIZE           0x000EF                             // Monitor ROM stack size.
#define MZ_UROM_ADDR                 0x0E800                             // User ROM start address.
#define MZ_BANKRAM_ADDR              0x0F000                             // Floppy API address which is used in TZFS as the paged RAM for additional functionality.
#define MZ_CMT_ADDR                  0x010F0                             // Address of the CMT (tape) header record.
#define MZ_CMT_DEFAULT_LOAD_ADDR     0x01200                             // The default load address for a CMT, anything below this is normally illegal.
#define MZ_VID_RAM_ADDR              0x0D000                             // Start of Video RAM
#define MZ_VID_RAM_SIZE              2048                                // Size of Video RAM.
#define MZ_VID_MAX_COL               40                                  // Maximum column for the host display
#define MZ_VID_MAX_ROW               25                                  // Maximum row for the host display
#define MZ_VID_DFLT_BYTE             0x00                                // Default character (SPACE) for video RAM.
#define MZ_ATTR_RAM_ADDR             0xD800                              // On machines with the upgrade, the start of the Attribute RAM.
#define MZ_ATTR_RAM_SIZE             2048                                // Size of the attribute RAM.
#define MZ_ATTR_DFLT_BYTE            0x07                                // Default colour (White on Black) for the attribute.
#define MZ_SCROL_BASE                0xE200                              // Base address of the hardware scroll registers.
#define MZ_SCROL_END                 0xE2FF                              // End address of the hardware scroll registers.
#define MZ_MEMORY_SWAP               0xE00C                              // Address when read swaps the memory from 0000-0FFF -> C000-CFFF
#define MZ_MEMORY_RESET              0xE010                              // Address when read resets the memory to the default location 0000-0FFF.
#define MZ_CRT_NORMAL                0xE014                              // Address when read sets the CRT to normal display mode.
#define MZ_CRT_INVERSE               0xE018                              // Address when read sets the CRT to inverted display mode.
#define MZ_80A_CPU_FREQ              2000000                             // CPU Speed of the Sharp MZ-80A
#define MZ_700_CPU_FREQ              3580000                             // CPU Speed of the Sharp MZ-700
#define MZ_80B_CPU_FREQ              4000000                             // CPU Speed of the Sharp MZ-80B
#define MZ_2000_CPU_FREQ             4000000                             // CPU Speed of the Sharp MZ-2000
#define MZ_800_CPU_FREQ              3580000                             // CPU Speed of the Sharp MZ-800

// Service request constants.
//
#define TZSVC_CMD_STRUCT_ADDR_TZFS   0x0ED80                             // Address of the command structure within TZFS - exists in 64K Block 0.
#define TZSVC_CMD_STRUCT_ADDR_CPM    0x4F560                             // Address of the command structure within CP/M - exists in 64K Block 4.
#define TZSVC_CMD_STRUCT_ADDR_MZ700  0x6FD80                             // Address of the command structure within MZ700 compatible programs - exists in 64K Block 6.
#define TZSVC_CMD_STRUCT_ADDR_ZOS    0x11FD80 // 0x7FD80                             // Address of the command structure for zOS use, exists in shared memory rather than FPGA. Spans top of block 6 and all of block 7.
#define TZSVC_CMD_STRUCT_ADDR_MZ2000_NST 0x6FD80                         // Address of the command structure within MZ2000 compatible programs during normal state - exists in 64K Block 1.
#define TZSVC_CMD_STRUCT_ADDR_MZ2000_IPL 0x07D80                         // Address of the command structure within MZ2000 compatible programs during IPL state - exists in 64K Block 0.
#define TZSVC_CMD_STRUCT_SIZE        0x280                               // Size of the inter z80/K64 service command memory.
#define TZSVC_CMD_SIZE               (sizeof(t_svcControl)-TZSVC_SECTOR_SIZE)
#define TZVC_MAX_CMPCT_DIRENT_BLOCK  TZSVC_SECTOR_SIZE/TZSVC_CMPHDR_SIZE // Maximum number of directory entries per sector.
#define TZSVC_MAX_DIR_ENTRIES        255                                 // Maximum number of files in one directory, any more than this will be ignored.
#define TZSVC_CMPHDR_SIZE            32                                  // Compacted header size, contains everything except the comment field, padded out to 32bytes.
#define MZF_FILLER_LEN               8                                   // Filler to pad a compacted header entry to a power of 2 length.
#define TZVC_MAX_DIRENT_BLOCK        TZSVC_SECTOR_SIZE/MZF_HEADER_SIZE   // Maximum number of directory entries per sector.
#define TZSVC_CMD_READDIR            0x01                                // Service command to open a directory and return the first block of entries.
#define TZSVC_CMD_NEXTDIR            0x02                                // Service command to return the next block of an open directory.
#define TZSVC_CMD_READFILE           0x03                                // Service command to open a file and return the first block.
#define TZSVC_CMD_NEXTREADFILE       0x04                                // Service command to return the next block of an open file.
#define TZSVC_CMD_WRITEFILE          0x05                                // Service command to create a file and save the first block.
#define TZSVC_CMD_NEXTWRITEFILE      0x06                                // Service command to write the next block to the open file.
#define TZSVC_CMD_CLOSE              0x07                                // Service command to close any open file or directory.
#define TZSVC_CMD_LOADFILE           0x08                                // Service command to load a file directly into tranZPUter memory.
#define TZSVC_CMD_SAVEFILE           0x09                                // Service command to save a file directly from tranZPUter memory.
#define TZSVC_CMD_ERASEFILE          0x0a                                // Service command to erase a file on the SD card.
#define TZSVC_CMD_CHANGEDIR          0x0b                                // Service command to change active directory on the SD card.
#define TZSVC_CMD_LOAD40ABIOS        0x20                                // Service command requesting that the 40 column version of the SA1510 BIOS is loaded.
#define TZSVC_CMD_LOAD80ABIOS        0x21                                // Service command requesting that the 80 column version of the SA1510 BIOS is loaded.
#define TZSVC_CMD_LOAD700BIOS40      0x22                                // Service command requesting that the MZ700 1Z-013A 40 column BIOS is loaded.
#define TZSVC_CMD_LOAD700BIOS80      0x23                                // Service command requesting that the MZ700 1Z-013A 80 column patched BIOS is loaded.
#define TZSVC_CMD_LOAD80BIPL         0x24                                // Service command requesting the MZ-80B IPL is loaded.
#define TZSVC_CMD_LOAD800BIOS        0x25                                // Service command requesting that the MZ800 9Z-504M BIOS is loaded.
#define TZSVC_CMD_LOAD2000IPL        0x26                                // Service command requesting the MZ-2000 IPL is loaded.
#define TZSVC_CMD_LOADTZFS           0x2F                                // Service command requesting the loading of TZFS. This service is for machines which normally dont have a monitor BIOS. ie. MZ-80B/MZ-2000 and manually request TZFS.
#define TZSVC_CMD_LOADBDOS           0x30                                // Service command to reload CPM BDOS+CCP.
#define TZSVC_CMD_ADDSDDRIVE         0x31                                // Service command to attach a CPM disk to a drive number.
#define TZSVC_CMD_READSDDRIVE        0x32                                // Service command to read an attached SD file as a CPM disk drive.
#define TZSVC_CMD_WRITESDDRIVE       0x33                                // Service command to write to a CPM disk drive which is an attached SD file.
#define TZSVC_CMD_CPU_BASEFREQ       0x40                                // Service command to switch to the mainboard frequency.
#define TZSVC_CMD_CPU_ALTFREQ        0x41                                // Service command to switch to the alternate frequency provided by the K64F.
#define TZSVC_CMD_CPU_CHGFREQ        0x42                                // Service command to set the alternate frequency in hertz.
#define TZSVC_CMD_CPU_SETZ80         0x50                                // Service command to switch to the external Z80 hard cpu.
#define TZSVC_CMD_CPU_SETT80         0x51                                // Service command to switch to the internal T80 soft cpu.
#define TZSVC_CMD_CPU_SETZPUEVO      0x52                                // Service command to switch to the internal ZPU Evolution cpu.
#define TZSVC_CMD_EMU_SETMZ80K       0x53                                // Service command to switch to the internal Sharp MZ Series Emulation of the MZ80K.
#define TZSVC_CMD_EMU_SETMZ80C       0x54                                // ""                             ""                       ""                 MZ80C.
#define TZSVC_CMD_EMU_SETMZ1200      0x55                                // ""                             ""                       ""                 MZ1200.
#define TZSVC_CMD_EMU_SETMZ80A       0x56                                // ""                             ""                       ""                 MZ80A.
#define TZSVC_CMD_EMU_SETMZ700       0x57                                // ""                             ""                       ""                 MZ700.
#define TZSVC_CMD_EMU_SETMZ800       0x58                                // ""                             ""                       ""                 MZ800.
#define TZSVC_CMD_EMU_SETMZ1500      0x59                                // ""                             ""                       ""                 MZ1500.
#define TZSVC_CMD_EMU_SETMZ80B       0x5A                                // ""                             ""                       ""                 MZ80B.
#define TZSVC_CMD_EMU_SETMZ2000      0x5B                                // ""                             ""                       ""                 MZ2000.
#define TZSVC_CMD_EMU_SETMZ2200      0x5C                                // ""                             ""                       ""                 MZ2200.
#define TZSVC_CMD_EMU_SETMZ2500      0x5D                                // ""                             ""                       ""                 MZ2500.
#define TZSVC_CMD_SD_DISKINIT        0x60                                // Service command to initialise and provide raw access to the underlying SD card.
#define TZSVC_CMD_SD_READSECTOR      0x61                                // Service command to provide raw read access to the underlying SD card.
#define TZSVC_CMD_SD_WRITESECTOR     0x62                                // Service command to provide raw write access to the underlying SD card.
#define TZSVC_CMD_EXIT               0x7F                                // Service command to terminate TZFS and restart the machine in original mode.
#define TZSVC_DEFAULT_TZFS_DIR       "TZFS"                              // Default directory where TZFS files are stored.
#define TZSVC_DEFAULT_CPM_DIR        "CPM"                               // Default directory where CPM files are stored.
#define TZSVC_DEFAULT_MZF_DIR        "MZF"                               // Default directory where MZF files are stored.
#define TZSVC_DEFAULT_CAS_DIR        "CAS"                               // Default directory where BASIC CASsette files are stored.
#define TZSVC_DEFAULT_BAS_DIR        "BAS"                               // Default directory where BASIC text files are stored.
#define TZSVC_DEFAULT_MZF_EXT        "MZF"                               // Default file extension for MZF files.
#define TZSVC_DEFAULT_CAS_EXT        "CAS"                               // Default file extension for CASsette files.
#define TZSVC_DEFAULT_BAS_EXT        "BAS"                               // Default file extension for BASic script files stored in readable text.
#define TZSVC_DEFAULT_WILDCARD       "*"                                 // Default wildcard file matching.
#define TZSVC_RESULT_OFFSET          0x01                                // Offset into structure of the result byte.
#define TZSVC_DIRNAME_SIZE           20                                  // Limit is size of FAT32 directory name.
#define TZSVC_WILDCARD_SIZE          20                                  // Very basic pattern matching so small size.
#define TZSVC_FILENAME_SIZE          MZF_FILENAME_LEN                    // Length of a Sharp MZF filename.
#define TZSVC_LONG_FNAME_SIZE        (sizeof(t_svcCmpDirEnt) - 1)        // Length of a standard filename to fit inside a directory entry.
#define TZSVC_LONG_FMT_FNAME_SIZE    20                                  // Length of a standard filename formatted in a directory listing.
#define TZSVC_SECTOR_SIZE            512                                 // SD Card sector buffer size.
#define TZSVC_STATUS_OK              0x00                                // Flag to indicate the K64F processing completed successfully.
#define TZSVC_STATUS_FILE_ERROR      0x01                                // Flag to indicate a file or directory error.
#define TZSVC_STATUS_BAD_CMD         0x02                                // Flag to indicate a bad service command was requested.
#define TZSVC_STATUS_BAD_REQ         0x03                                // Flag to indicate a bad request was made, the service status request flag was not set.
#define TZSVC_STATUS_REQUEST         0xFE                                // Flag to indicate Z80 has posted a request.
#define TZSVC_STATUS_PROCESSING      0xFF                                // Flag to indicate the K64F is processing a command.
#define TZSVC_OPEN                   0x00                                // Service request to open a directory or file.
#define TZSVC_NEXT                   0x01                                // Service request to return the next directory block or file block or write the next file block.
#define TZSVC_CLOSE                  0x02                                // Service request to close open dir/file.

// ROM file paths.
#define MZ_ROM_SP1002                 "SP1002.rom"                       // Original MZ-80K ROM
#define MZ_ROM_SA1510_40C             "SA1510.rom"                       // Original 40 character Monitor ROM.
#define MZ_ROM_SA1510_80C             "SA1510-8.rom"                     // Original Monitor ROM patched for 80 character screen mode.
#define MZ_ROM_1Z_013A_40C            "1Z-013A.rom"                      // Original 40 character Monitor ROM for the Sharp MZ700.
#define MZ_ROM_1Z_013A_80C            "1Z-013A-8.rom"                    // Original Monitor ROM patched for the Sharp MZ700 patched for 80 column mode.
#define MZ_ROM_1Z_013A_KM_40C         "1Z-013A-KM.rom"                   // Original 40 character Monitor ROM for the Sharp MZ700 with keyboard remapped for the MZ80A.
#define MZ_ROM_1Z_013A_KM_80C         "1Z-013A-KM-8.rom"                 // Original Monitor ROM patched for the Sharp MZ700 with keyboard remapped for the MZ80A and patched for 80 column mode.
#define MZ_ROM_1Z_013A_2000           "1Z-013A-2000.rom"                 // Original 40 character Monitor ROM for the Sharp MZ700 modified to run on an MZ-2000.
#define MZ_ROM_9Z_504M_COMBINED       "MZ800_IPL.rom"                    // Original MZ-800 BIOS which comprises the 1Z_013B BIOS, 9Z_504M IPL, CGROM and IOCS.
#define MZ_ROM_9Z_504M                "MZ800_9Z_504M.rom"                // Modified MZ-800 9Z_504M IPL to contain a select TZFS option.
#define MZ_ROM_1Z_013B                "MZ800_1Z_013B.rom"                // Original MZ-800 1Z_013B MZ-700 compatible BIOS.
#define MZ_ROM_800_CGROM              "MZ800_CGROM.ori"                  // Original MZ-800 Character Generator ROM.
#define MZ_ROM_800_IOCS               "MZ800_IOCS.rom"                   // Original MZ-800 common IOCS bios.
#define MZ_ROM_MZ80B_IPL              "MZ80B_IPL.rom"                    // Original IPL ROM for the Sharp MZ-80B.
#define MZ_ROM_MZ2000_IPL             "MZ2000_IPL.rom"                   // Original IPL ROM for the Sharp MZ-2000.
#define MZ_ROM_MZ2000_IPL_TZPU        "MZ2000_IPL_TZPU.rom"              // Modified IPL ROM for the tranZPUter running on the Sharp MZ-2000.
#define MZ_ROM_MZ2000_CGROM           "MZ2000_CGROM.rom"                 // MZ-2000 CGROM.
#define MZ_ROM_TZFS                   "tzfs.rom"                         // tranZPUter Filing System ROM.
      
// CP/M constants.
//
#define CPM_MAX_DRIVES               16                                  // Maximum number of drives in CP/M.
#define CPM_SD_DRIVES_DIR            TZSVC_DEFAULT_CPM_DIR "/SDC16M/RAW" // Default directory where CPM SD disk drive images are stored.
#define CPM_DRIVE_TMPL               "CPMDSK%02u.RAW"                    // Template for CPM disk drives stored on the SD card.
#define CPM_SECTORS_PER_TRACK        32                                  // Number of sectors in a track on the virtual CPM disk.
#define CPM_TRACKS_PER_DISK          1024                                // Number of tracks on a disk.


// Constants for the Sharp MZ80A MZF file format.
#define MZF_HEADER_SIZE              128                                 // Size of the MZF header.
#define MZF_ATTRIBUTE                0x00                                // Code Type, 01 = Machine Code.
#define MZF_FILENAME                 0x01                                // Title/Name (17 bytes).
#define MZF_FILENAME_LEN             17                                  // Length of the filename, it is not NULL terminated, generally a CR can be taken as terminator but not guaranteed.
#define MZF_FILESIZE                 0x12                                // Size of program.
#define MZF_LOADADDR                 0x14                                // Load address of program.
#define MZF_EXECADDR                 0x16                                // Exec address of program.
#define MZF_COMMENT                  0x18                                // Comment, used for details of the file or startup code.
#define MZF_COMMENT_LEN              104                                 // Length of the comment field.
#define CMT_TYPE_OBJCD               0x001                               // MZF contains a binary object.
#define CMT_TYPE_BTX1CD              0x002                               // MZF contains a BASIC program.
#define CMT_TYPE_BTX2CD              0x005                               // MZF contains a BASIC program.
#define CMT_TYPE_TZOBJCD0            0x0F8                               // MZF contains a TZFS binary object for page 0.
#define CMT_TYPE_TZOBJCD1            0x0F9 
#define CMT_TYPE_TZOBJCD2            0x0FA 
#define CMT_TYPE_TZOBJCD3            0x0FB 
#define CMT_TYPE_TZOBJCD4            0x0FC 
#define CMT_TYPE_TZOBJCD5            0x0FD 
#define CMT_TYPE_TZOBJCD6            0x0FE 
#define CMT_TYPE_TZOBJCD7            0x0FF                               // MZF contains a TZFS binary object for page 7.

// Constants for other handled file formats.
//
#define CAS_HEADER_SIZE              256                                 // Size of the CASsette header.

// Possible targets the K64F can read from/write to.
enum TARGETS {
    MAINBOARD                        = 0,
    TRANZPUTER                       = 1
};

// Possible machine hardware types the tranZPUter is functioning within.
//
enum MACHINE_HW_TYPES {
    HW_MZ80K                         = HWMODE_MZ80K,                     // Host hardware = MZ-80K.
    HW_MZ80C                         = HWMODE_MZ80C,                     // Host hardware = MZ-80C.
    HW_MZ1200                        = HWMODE_MZ1200,                    // Host hardware = MZ-1200.
    HW_MZ80A                         = HWMODE_MZ80A,                     // Host hardware = MZ-80A.
    HW_MZ700                         = HWMODE_MZ700,                     // Host hardware = MZ-700.
    HW_MZ800                         = HWMODE_MZ800,                     // Host hardware = MZ-800.
    HW_MZ80B                         = HWMODE_MZ80B,                     // Host hardware = MZ-80B.
    HW_MZ2000                        = HWMODE_MZ2000,                    // Host hardware = MZ-2000.
    HW_UNKNOWN                       = 0xFF                              // Host hardware unknown, fault or CPLD misconfiguration.
};

// Groups to which the machines belong. This is a lineage route of the Sharp machines.
//
enum MACHINE_GROUP {
    GROUP_MZ80K                      = 0,                                // Machines in the MZ80K group, ie. MZ80K/C/1200/80A
    GROUP_MZ700                      = 1,                                // Machines in the MZ700 group, ie. MZ700/800/1500
    GROUP_MZ80B                      = 2                                 // Machines in the MZ80B group, ie. MZ80B/2000/2200/2500
};

// Types of file which have handlers and can be processed.
//
enum FILE_TYPE {
    MZF                              = 0,                                // Sharp MZF tape image files.
    MZFHDR                           = 1,                                // Sharp MZF Header from file only.
    CAS                              = 2,                                // BASIC CASsette image files.
    BAS                              = 3,                                // BASic ASCII text script files.

    ALL                              = 10,                               // All files to be considered.
    ALLFMT                           = 11                                // Special case for directory listings, all files but truncated and formatted.
};

// File function return code (FRESULT) - From FatFS.
typedef enum {
	FR_OK = 0,				/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	FR_NO_FILE,				/* (4) Could not find the file */
	FR_NO_PATH,				/* (5) Could not find the path */
	FR_INVALID_NAME,		/* (6) The path name format is invalid */
	FR_DENIED,				/* (7) Access denied due to prohibited access or directory full */
	FR_EXIST,				/* (8) Access denied due to prohibited access */
	FR_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
	FR_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
	FR_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES,	/* (18) Number of open files > FF_FS_LOCK */
	FR_INVALID_PARAMETER	/* (19) Given parameter is invalid */
} FRESULT;

// Following are only required for the user space daemon which emulates the Cortex-M4 on the tranZPUter SW
#ifndef __KERNEL_DRIVER__

// Structure to define a Sharp MZ80A MZF directory structure. This header appears at the beginning of every Sharp MZ80A tape (and more recently archived/emulator) images.
//
typedef struct __attribute__((__packed__)) {
    uint8_t                          attr;                               // MZF attribute describing the file.
    uint8_t                          fileName[MZF_FILENAME_LEN];         // Each directory entry is the size of an MZF filename.
    uint16_t                         fileSize;                           // Size of file.
    uint16_t                         loadAddr;                           // Load address for the file.
    uint16_t                         execAddr;                           // Execution address where the Z80 starts processing.
    uint8_t                          comment[MZF_COMMENT_LEN];           // Text comment field but often contains a startup machine code program.
} t_svcDirEnt;

// Structure to define a compacted Sharp MZ80A MZF directory structure (no comment) for use in directory listings.
// This header appears at the beginning of every Sharp MZ80A tape (and more recently archived/emulator) images.
//
typedef struct __attribute__((__packed__)) {
    uint8_t                          attr;                               // MZF attribute describing the file.
    uint8_t                          fileName[MZF_FILENAME_LEN];         // Each directory entry is the size of an MZF filename.
    uint16_t                         fileSize;                           // Size of file.
    uint16_t                         loadAddr;                           // Load address for the file.
    uint16_t                         execAddr;                           // Execution address where the Z80 starts processing.
    uint8_t                          filler[MZF_FILLER_LEN];             // Filler to pad to a power of 2 length.
} t_svcCmpDirEnt;

// Structure to hold the map betwen an SD filename and the Sharp file it contains. The file is an MZF format file with a 128 byte header
// and this header contains the name understood on the Sharp MZ80A.
//
typedef struct __attribute__((__packed__)) {
    uint8_t                          *sdFileName;                        // Name of file on the SD card.
    t_svcCmpDirEnt                   mzfHeader;                          // Compact Sharp header data of this file.
} t_sharpToSDMap;

// Structure to define the control information for a CP/M disk drive.
//
typedef struct {
    uint8_t                          *fileName;                          // FQFN of the CPM disk image file.
    uint32_t                         lastTrack;                          // Track of last successful operation.
    uint32_t                         lastSector;                         // Sector of last successful operation.
    FILE                             *File;                              // Opened file handle of the CPM disk image.
} t_cpmDrive;

// Structure to define which CP/M drives are added to the system, mapping a number from CP/M into a record containing the details of the file on the SD card.
//
typedef struct {
    t_cpmDrive                       *drive[CPM_MAX_DRIVES];             // 1:1 map of CP/M drive number to an actual file on the SD card.
} t_cpmDriveMap;

// Structure to hold a map of an entire directory of files on the SD card and their associated Sharp MZ0A filename.
typedef struct __attribute__((__packed__)) {
    uint8_t                          valid;                              // Is this mapping valid?
    uint8_t                          entries;                            // Number of entries in cache.
    uint8_t                          type;                               // Type of file being cached.
    char                             directory[TZSVC_DIRNAME_SIZE];      // Directory this mapping is associated with.
    union {
        t_sharpToSDMap               *mzfFile[TZSVC_MAX_DIR_ENTRIES];    // File mapping of SD file to its Sharp MZ80A name.
        uint8_t                      *sdFileName[TZSVC_MAX_DIR_ENTRIES]; // No mapping for SD filenames, just the file name.
    };
} t_dirMap;

// Structure to maintain all MZ700 hardware control information in order to emulate the machine.
//
typedef struct {
    uint32_t                         config;                             // Compacted control register, 31:19 = reserved, 18 = Inhibit mode, 17 = Upper D000:FFFF is RAM (=1), 16 = Lower 0000:0FFF is RAM (=1), 15:8 = old memory mode, 7:0 = current memory mode.
    //uint8_t                          memoryMode;                         // The memory mode the MZ700 is currently running under, this is determined by the memory control commands from the MZ700.
    //uint8_t                          lockMemoryMode;                     // The preserved memory mode when entering the locked state.
    //uint8_t                          inhibit;                            // The inhibit flag, blocks the upper 0xD000:0xFFFF region from being accessed, affects the memoryMode temporarily.
    //uint8_t                          update;                             // Update flag, indicates to the ISR that a memory mode update is needed.
    //uint8_t                          b0000;                              // Block 0000:0FFF mode.
    //uint8_t                          bD000;                              // Block D000:FFFF mode.
} t_mz700;

// Structure to maintain all MZ-80B hardware control information in order to emulate the machine as near as possible.
typedef struct {
    uint32_t                         config;                             // Compacted control register, 31:19 = reserved, 18 = Inhibit mode, 17 = Upper D000:FFFF is RAM (=1), 16 = Lower 0000:0FFF is RAM (=1), 15:8 = old memory mode, 7:0 = current memory mode.
} t_mz80b;

// Structure to maintain all the control and management variables of the Z80 and underlying hardware so that the state of run is well known by any called method.
//
typedef struct {
  #if !defined(__APP__) || defined(__TZFLUPD__)
    uint32_t                         svcControlAddr;                     // Address of the service control record within the Z80 static RAM bank.
    uint8_t                          refreshAddr;                        // Refresh address for times when the K64F must issue refresh cycles on the Z80 bus.
    uint8_t                          disableRefresh;                     // Disable refresh if the mainboard DRAM isnt being used.

    enum MACHINE_HW_TYPES            hostType;                           // The underlying host machine, 0 = Sharp MZ-80A, 1 = MZ-700, 2 = MZ-80B
    uint8_t                          iplMode;                            // Flag to indicate if the host is in IPL (boot) or run mode. Applicable on the MZ-2000/MZ-80B only.
    uint8_t                          blockResetActions;                  // Flag to request reset actions are blocked on the next detected reset. This is useful on startup or when loading a monitor ROM set different to the default.
    t_mz700                          mz700;                              // MZ700 emulation control to detect IO commands and adjust the memory map accordingly.
    t_mz80b                          mz80b;                              // MZ-80B emulation control to detect IO commands and adjust the memory map and I/O forwarding accordingly.

    uint8_t                          resetEvent;                         // A Z80_RESET event occurred, probably user pressing RESET button.
    uint32_t                         freqMultiplier;                     // Multipler to be applied to CPU frequency.
    int                              fdZ80;                              // Handle to the Z80 kernel.
  #endif
} t_z80Control;

// Structure to maintain higher level OS control and management variables typically used for TZFS and CPM.
//
typedef struct {
	uint8_t                          tzAutoBoot;                         // Autoboot the tranZPUter into TZFS mode.
    t_dirMap                         dirMap;                             // Directory map of SD filenames to Sharp MZ80A filenames.
    t_cpmDriveMap                    cpmDriveMap;                        // Map of file number to an open SD disk file to be used as a CPM drive.
    uint8_t                          *lastFile;                          // Last file loaded - typically used for CPM to reload itself.
} t_osControl;

// Structure to contain inter CPU communications memory for command service processing and results.
// Typically the z80 places a command into the structure in it's memory space and asserts an I/O request,
// the K64F detects the request and reads the lower portion of the struct from z80 memory space, 
// determines the command and then either reads the remainder or writes to the remainder. This struct
// exists in both the z80 and K64F domains and data is sync'd between them as needed.
//
typedef struct __attribute__((__packed__)) {
    uint8_t                          cmd;                                // Command request.
    uint8_t                          result;                             // Result code. 0xFE - set by Z80, command available, 0xFE - set by K64F, command ack and processing. 0x00-0xF0 = cmd complete and result of processing.
    union {
        uint8_t                      dirSector;                          // Virtual directory sector number.
        uint8_t                      fileSector;                         // Sector within open file to read/write.
        uint8_t                      vDriveNo;                           // Virtual or physical SD card drive number.
    };
    union {
        struct {
            uint16_t                 trackNo;                            // For virtual drives with track and sector this is the track number
            uint16_t                 sectorNo;                           // For virtual drives with track and sector this is the sector number. NB For LBA access, this is 32bit and overwrites fileNo/fileType which arent used during raw SD access.
        };
        uint32_t                     sectorLBA;                          // For LBA access, this is 32bit and used during raw SD access.
        struct {
            uint8_t                  memTarget;                          // Target memory for operation, 0 = tranZPUter, 1 = mainboard.
            uint8_t                  spare1;                             // Unused variable.
            uint16_t                 spare2;                             // Unused variable.
        };
    };
    uint8_t                          fileNo;                             // File number of a file within the last directory listing to open/update.
    uint8_t                          fileType;                           // Type of file being processed.
    union {
        uint16_t                     loadAddr;                           // Load address for ROM/File images which need to be dynamic.
        uint16_t                     saveAddr;                           // Save address for ROM/File images which need to be dynamic.
        uint16_t                     cpuFreq;                            // CPU Frequency in KHz - used for setting of the alternate CPU clock frequency.
    };
    union {
        uint16_t                     loadSize;                           // Size for ROM/File to be loaded.
        uint16_t                     saveSize;                           // Size for ROM/File to be saved.
    };
    uint8_t                          directory[TZSVC_DIRNAME_SIZE];      // Directory in which to look for a file. If no directory is given default to MZF.
    uint8_t                          filename[TZSVC_FILENAME_SIZE];      // File to open or create.
    uint8_t                          wildcard[TZSVC_WILDCARD_SIZE];      // A basic wildcard pattern match filter to be applied to a directory search.
    uint8_t                          sector[TZSVC_SECTOR_SIZE];          // Sector buffer generally for disk read/write.
} t_svcControl;

// Structure to define all the directory entries which are packed into a single SD sector which is used between the Z80<->K64F.
//
typedef struct __attribute__((__packed__)) {
    t_svcDirEnt                      dirEnt[TZVC_MAX_DIRENT_BLOCK];      // Fixed number of directory entries per sector/block. 
} t_svcDirBlock;

// Structure to hold compacted directory entries which are packed into a single SD sector which is used between the Z80<->K64F.
//
typedef struct __attribute__((__packed__)) {
    t_svcCmpDirEnt                   dirEnt[TZVC_MAX_CMPCT_DIRENT_BLOCK];// Fixed number of compacted directory entries per sector/block. 
} t_svcCmpDirBlock;

// Mapping table from Sharp MZ80A Ascii to real Ascii.
//
typedef struct {
    uint8_t                          asciiCode;
} t_asciiMap;

// Mapping table from Ascii to Sharp MZ display code.
//
typedef struct {
    uint8_t                          dispCode;
} t_dispCodeMap;

// Prototypes.
//
void                                  reqResetZ80(uint8_t);
void                                  startZ80(uint8_t memoryMode);
void                                  stopZ80(uint8_t memoryMode);    
uint32_t                              setZ80CPUFrequency(float, uint8_t);
uint8_t                               copyFromZ80(uint8_t *, uint32_t, uint32_t, enum TARGETS);
uint8_t                               copyToZ80(uint32_t, uint8_t *, uint32_t, enum TARGETS);
uint8_t                               writeZ80Memory(uint32_t, uint8_t, enum TARGETS);
uint8_t                               readZ80Memory(uint32_t);
void                                  fillZ80Memory(uint32_t, uint32_t, uint8_t, enum TARGETS);
FRESULT                               loadZ80Memory(const char *, uint32_t, uint32_t, uint32_t, uint32_t *, enum TARGETS);
FRESULT                               saveZ80Memory(const char *, uint32_t, uint32_t, t_svcDirEnt *, enum TARGETS);
FRESULT                               loadMZFZ80Memory(const char *, uint32_t, uint32_t *, uint8_t, enum TARGETS);
int                                   memoryDump(uint32_t memaddr, uint32_t memsize, uint8_t memoryType, uint32_t memwidth, uint32_t dispaddr, uint8_t dispwidth);

// Getter/Setter methods!
void                                  convertSharpFilenameToAscii(char *, char *, uint8_t);
void                                  convertToFAT32FileNameFormat(char *);

// tranZPUter OS i/f methods.
uint8_t                               setZ80SvcStatus(uint8_t);
void                                  svcSetDefaults(enum FILE_TYPE);
uint8_t                               svcReadDir(uint8_t, enum FILE_TYPE);
uint8_t                               svcFindFile(char *, char *, uint8_t, enum FILE_TYPE);
uint8_t                               svcReadDirCache(uint8_t, enum FILE_TYPE);
uint8_t                               svcFindFileCache(char *, char *, uint8_t, enum FILE_TYPE);
uint8_t                               svcCacheDir(const char *, enum FILE_TYPE, uint8_t);
uint8_t                               svcReadFile(uint8_t, enum FILE_TYPE);
uint8_t                               svcWriteFile(uint8_t, enum FILE_TYPE);
uint8_t                               svcLoadFile(enum FILE_TYPE);
uint8_t                               svcSaveFile(enum FILE_TYPE);
uint8_t                               svcEraseFile(enum FILE_TYPE);
uint8_t                               svcAddCPMDrive(void);
uint8_t                               svcReadCPMDrive(void);
uint8_t                               svcWriteCPMDrive(void);
uint32_t                              getServiceAddr(void);
void                                  processServiceRequest(void);
uint8_t                               loadBIOS(const char *, uint32_t);
FRESULT                               loadTZFS(char *, uint32_t);
void                                  loadTranZPUterDefaultROMS(uint8_t);
uint8_t                               testTZFSAutoBoot(void);

#endif  // __KERNEL_DRIVER__

#ifdef __cplusplus
}
#endif
#endif // TZPU_H
