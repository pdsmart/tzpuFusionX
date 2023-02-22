/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80driver.h
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Driver 
//                  This file contains the declarations used in the z80drv device driver.
//
// Credits:         Zilog Z80 CPU Emulator v0.2 written by Manuel Sainz de Baranda y Goñi
//                  The Z80 CPU Emulator is the heart of this driver and in all ways, is compatible with
//                  the original Z80.
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//                  (c) 1999-2023 Manuel Sainz de Baranda y Goñi
//
// History:         Oct 2022 - v1.0  Initial write of the z80 kernel driver software.
//                  Jan 2023 - v1.1  Added MZ-2000/MZ-80A modes.
//                  Feb 2023 - v1.2  Added RFS virtual driver.
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
#ifndef Z80DRIVER_H
#define Z80DRIVER_H

// Constants.
#define DRIVER_LICENSE                        "GPL"
#define DRIVER_AUTHOR                         "Philip D Smart"
#define DRIVER_DESCRIPTION                    "Z80 CPU Emulator and Hardware Interface Driver"
#define DRIVER_VERSION                        "v1.3"
#define DRIVER_VERSION_DATE                   "Feb 2023"
#define DRIVER_COPYRIGHT                      "(C) 2018-2023"
#define TARGET_HOST_MZ700                     0                  // Target compilation for an MZ700
#define TARGET_HOST_MZ2000                    0                  //                           MZ2000
#define TARGET_HOST_MZ80A                     1                  //                           MZ80A
#define Z80_VIRTUAL_ROM_SIZE                  (65536 * 32)       // Sized to maximum Kernel contiguous allocation size, 2M which is 4x512K ROMS.
#define Z80_VIRTUAL_RAM_SIZE                  (65536 * 32)       // Sized to maximum Kernel contiguous allocation size, 2M.
#define Z80_MEMORY_PAGE_SIZE                  16
#define MAX_SCREEN_WIDTH                      132                // Maximum terminal screen width for memory dump output.
#define MAX_VIRTUAL_DEVICES                   5                  // Maximum number of allowed virtual devices.
#define DEVICE_NAME                           "z80drv"
#define  CLASS_NAME                           "mogu"
#define IO_PROCESSOR_NAME                     "k64fcpu"          // Name of the I/O processor user space application.
#define DEBUG_ENABLED                         1

// Memory and IO page types. Used to create a memory page which maps type of address space to real address space on host or virtual memory.
#define MEMORY_TYPE_VIRTUAL_MASK              0x00FFFFFF
#define MEMORY_TYPE_REAL_MASK                 0x0000FFFF
#define IO_TYPE_MASK                          0x0000FFFF
#define MEMORY_TYPE_INHIBIT                   0x00000000
#define MEMORY_TYPE_PHYSICAL_RAM              0x80000000
#define MEMORY_TYPE_PHYSICAL_ROM              0x40000000
#define MEMORY_TYPE_PHYSICAL_VRAM             0x20000000
#define MEMORY_TYPE_PHYSICAL_HW               0x10000000
#define MEMORY_TYPE_VIRTUAL_RAM               0x08000000
#define MEMORY_TYPE_VIRTUAL_ROM               0x04000000
#define MEMORY_TYPE_VIRTUAL_RAM_RO            0x02000000
#define MEMORY_TYPE_VIRTUAL_HW                0x01000000
#define IO_TYPE_PHYSICAL_HW                   0x80000000
#define IO_TYPE_VIRTUAL_HW                    0x40000000


// Approximate governor delays to regulate emulated CPU speed.
// MZ-700
#if(TARGET_HOST_MZ700 == 1)
#define INSTRUCTION_DELAY_ROM_3_54MHZ         253
#define INSTRUCTION_DELAY_ROM_7MHZ            126
#define INSTRUCTION_DELAY_ROM_14MHZ           63
#define INSTRUCTION_DELAY_ROM_28MHZ           32
#define INSTRUCTION_DELAY_ROM_56MHZ           16
#define INSTRUCTION_DELAY_ROM_112MHZ          8
#define INSTRUCTION_DELAY_ROM_224MHZ          4
#define INSTRUCTION_DELAY_ROM_448MHZ          1
#define INSTRUCTION_DELAY_RAM_3_54MHZ         253
#define INSTRUCTION_DELAY_RAM_7MHZ            126
#define INSTRUCTION_DELAY_RAM_14MHZ           63
#define INSTRUCTION_DELAY_RAM_28MHZ           32
#define INSTRUCTION_DELAY_RAM_56MHZ           16
#define INSTRUCTION_DELAY_RAM_112MHZ          8
#define INSTRUCTION_DELAY_RAM_224MHZ          4
#define INSTRUCTION_DELAY_RAM_448MHZ          1
#define INSTRUCTION_EQUIV_FREQ_3_54MHZ        3540000
#define INSTRUCTION_EQUIV_FREQ_7MHZ           7000000
#define INSTRUCTION_EQUIV_FREQ_14MHZ          14000000
#define INSTRUCTION_EQUIV_FREQ_28MHZ          28000000
#define INSTRUCTION_EQUIV_FREQ_56MHZ          56000000
#define INSTRUCTION_EQUIV_FREQ_112MHZ         112000000
#define INSTRUCTION_EQUIV_FREQ_224MHZ         224000000
#define INSTRUCTION_EQUIV_FREQ_448MHZ         448000000

enum Z80_INSTRUCTION_DELAY {
    ROM_DELAY_NORMAL                          = INSTRUCTION_DELAY_ROM_3_54MHZ,
    ROM_DELAY_X2                              = INSTRUCTION_DELAY_ROM_7MHZ,
    ROM_DELAY_X4                              = INSTRUCTION_DELAY_ROM_14MHZ,
    ROM_DELAY_X8                              = INSTRUCTION_DELAY_ROM_28MHZ,
    ROM_DELAY_X16                             = INSTRUCTION_DELAY_ROM_56MHZ,
    ROM_DELAY_X32                             = INSTRUCTION_DELAY_ROM_112MHZ,
    ROM_DELAY_X64                             = INSTRUCTION_DELAY_ROM_224MHZ,
    ROM_DELAY_X128                            = INSTRUCTION_DELAY_ROM_448MHZ,
    RAM_DELAY_NORMAL                          = INSTRUCTION_DELAY_RAM_3_54MHZ,
    RAM_DELAY_X2                              = INSTRUCTION_DELAY_RAM_7MHZ,
    RAM_DELAY_X4                              = INSTRUCTION_DELAY_RAM_14MHZ,
    RAM_DELAY_X8                              = INSTRUCTION_DELAY_RAM_28MHZ,
    RAM_DELAY_X16                             = INSTRUCTION_DELAY_RAM_56MHZ,
    RAM_DELAY_X32                             = INSTRUCTION_DELAY_RAM_112MHZ,
    RAM_DELAY_X64                             = INSTRUCTION_DELAY_RAM_224MHZ,
    RAM_DELAY_X128                            = INSTRUCTION_DELAY_RAM_448MHZ 
    CPU_FREQUENCY_NORMAL                      = INSTRUCTION_EQUIV_FREQ_3_54MHZ,
    CPU_FREQUENCY_X2                          = INSTRUCTION_EQUIV_FREQ_7MHZ,
    CPU_FREQUENCY_X4                          = INSTRUCTION_EQUIV_FREQ_14MHZ,
    CPU_FREQUENCY_X8                          = INSTRUCTION_EQUIV_FREQ_28MHZ,
    CPU_FREQUENCY_X16                         = INSTRUCTION_EQUIV_FREQ_56MHZ,
    CPU_FREQUENCY_X32                         = INSTRUCTION_EQUIV_FREQ_112MHZ,
    CPU_FREQUENCY_X64                         = INSTRUCTION_EQUIV_FREQ_224MHZ,
    CPU_FREQUENCY_X128                        = INSTRUCTION_EQUIV_FREQ_448MHZ,
};
#endif

// MZ-2000
#if(TARGET_HOST_MZ2000 == 1)
#define INSTRUCTION_DELAY_ROM_4MHZ            243
#define INSTRUCTION_DELAY_ROM_8MHZ            122
#define INSTRUCTION_DELAY_ROM_16MHZ           61
#define INSTRUCTION_DELAY_ROM_32MHZ           30
#define INSTRUCTION_DELAY_ROM_64MHZ           15
#define INSTRUCTION_DELAY_ROM_128MHZ          7
#define INSTRUCTION_DELAY_ROM_256MHZ          3
#define INSTRUCTION_DELAY_ROM_512MHZ          1
#define INSTRUCTION_DELAY_RAM_4MHZ            218
#define INSTRUCTION_DELAY_RAM_8MHZ            112
#define INSTRUCTION_DELAY_RAM_16MHZ           56
#define INSTRUCTION_DELAY_RAM_32MHZ           28
#define INSTRUCTION_DELAY_RAM_64MHZ           14
#define INSTRUCTION_DELAY_RAM_128MHZ          7
#define INSTRUCTION_DELAY_RAM_256MHZ          3
#define INSTRUCTION_DELAY_RAM_512MHZ          1
#define INSTRUCTION_EQUIV_FREQ_4MHZ           4000000
#define INSTRUCTION_EQUIV_FREQ_8MHZ           8000000
#define INSTRUCTION_EQUIV_FREQ_16MHZ          16000000
#define INSTRUCTION_EQUIV_FREQ_32MHZ          32000000
#define INSTRUCTION_EQUIV_FREQ_64MHZ          64000000
#define INSTRUCTION_EQUIV_FREQ_128MHZ         128000000
#define INSTRUCTION_EQUIV_FREQ_256MHZ         256000000
#define INSTRUCTION_EQUIV_FREQ_512MHZ         512000000

enum Z80_INSTRUCTION_DELAY {
    ROM_DELAY_NORMAL                          = INSTRUCTION_DELAY_ROM_4MHZ,
    ROM_DELAY_X2                              = INSTRUCTION_DELAY_ROM_8MHZ,
    ROM_DELAY_X4                              = INSTRUCTION_DELAY_ROM_16MHZ,
    ROM_DELAY_X8                              = INSTRUCTION_DELAY_ROM_32MHZ,
    ROM_DELAY_X16                             = INSTRUCTION_DELAY_ROM_64MHZ,
    ROM_DELAY_X32                             = INSTRUCTION_DELAY_ROM_128MHZ,
    ROM_DELAY_X64                             = INSTRUCTION_DELAY_ROM_256MHZ,
    ROM_DELAY_X128                            = INSTRUCTION_DELAY_ROM_512MHZ,
    RAM_DELAY_NORMAL                          = INSTRUCTION_DELAY_RAM_4MHZ,
    RAM_DELAY_X2                              = INSTRUCTION_DELAY_RAM_8MHZ,
    RAM_DELAY_X4                              = INSTRUCTION_DELAY_RAM_16MHZ,
    RAM_DELAY_X8                              = INSTRUCTION_DELAY_RAM_32MHZ,
    RAM_DELAY_X16                             = INSTRUCTION_DELAY_RAM_64MHZ,
    RAM_DELAY_X32                             = INSTRUCTION_DELAY_RAM_128MHZ,
    RAM_DELAY_X64                             = INSTRUCTION_DELAY_RAM_256MHZ,
    RAM_DELAY_X128                            = INSTRUCTION_DELAY_RAM_512MHZ,
    CPU_FREQUENCY_NORMAL                      = INSTRUCTION_EQUIV_FREQ_4MHZ,
    CPU_FREQUENCY_X2                          = INSTRUCTION_EQUIV_FREQ_8MHZ,
    CPU_FREQUENCY_X4                          = INSTRUCTION_EQUIV_FREQ_16MHZ,
    CPU_FREQUENCY_X8                          = INSTRUCTION_EQUIV_FREQ_32MHZ,
    CPU_FREQUENCY_X16                         = INSTRUCTION_EQUIV_FREQ_64MHZ,
    CPU_FREQUENCY_X32                         = INSTRUCTION_EQUIV_FREQ_128MHZ,
    CPU_FREQUENCY_X64                         = INSTRUCTION_EQUIV_FREQ_256MHZ,
    CPU_FREQUENCY_X128                        = INSTRUCTION_EQUIV_FREQ_512MHZ,
};
#endif

// MZ-80A - These values are dependent on the CPU Freq of the SSD202. Values are for 1.2GHz, in brackets for 1.0GHz
#if(TARGET_HOST_MZ80A == 1)
#define INSTRUCTION_DELAY_ROM_2MHZ            436  // (420)
#define INSTRUCTION_DELAY_ROM_4MHZ            218
#define INSTRUCTION_DELAY_ROM_8MHZ            109
#define INSTRUCTION_DELAY_ROM_16MHZ           54
#define INSTRUCTION_DELAY_ROM_32MHZ           27
#define INSTRUCTION_DELAY_ROM_64MHZ           14
#define INSTRUCTION_DELAY_ROM_128MHZ          7
#define INSTRUCTION_DELAY_ROM_256MHZ          3
#define INSTRUCTION_DELAY_RAM_2MHZ            420
#define INSTRUCTION_DELAY_RAM_4MHZ            210
#define INSTRUCTION_DELAY_RAM_8MHZ            105
#define INSTRUCTION_DELAY_RAM_16MHZ           52
#define INSTRUCTION_DELAY_RAM_32MHZ           26
#define INSTRUCTION_DELAY_RAM_64MHZ           13
#define INSTRUCTION_DELAY_RAM_128MHZ          7
#define INSTRUCTION_DELAY_RAM_256MHZ          0
#define INSTRUCTION_EQUIV_FREQ_2MHZ           2000000
#define INSTRUCTION_EQUIV_FREQ_4MHZ           4000000
#define INSTRUCTION_EQUIV_FREQ_8MHZ           8000000
#define INSTRUCTION_EQUIV_FREQ_16MHZ          16000000
#define INSTRUCTION_EQUIV_FREQ_32MHZ          32000000
#define INSTRUCTION_EQUIV_FREQ_64MHZ          64000000
#define INSTRUCTION_EQUIV_FREQ_128MHZ         128000000
#define INSTRUCTION_EQUIV_FREQ_256MHZ         256000000

enum Z80_INSTRUCTION_DELAY {
    ROM_DELAY_NORMAL                          = INSTRUCTION_DELAY_ROM_2MHZ,
    ROM_DELAY_X2                              = INSTRUCTION_DELAY_ROM_4MHZ,
    ROM_DELAY_X4                              = INSTRUCTION_DELAY_ROM_8MHZ,
    ROM_DELAY_X8                              = INSTRUCTION_DELAY_ROM_16MHZ,
    ROM_DELAY_X16                             = INSTRUCTION_DELAY_ROM_32MHZ,
    ROM_DELAY_X32                             = INSTRUCTION_DELAY_ROM_64MHZ,
    ROM_DELAY_X64                             = INSTRUCTION_DELAY_ROM_128MHZ,
    ROM_DELAY_X128                            = INSTRUCTION_DELAY_ROM_256MHZ,
    RAM_DELAY_NORMAL                          = INSTRUCTION_DELAY_RAM_2MHZ,
    RAM_DELAY_X2                              = INSTRUCTION_DELAY_RAM_4MHZ,
    RAM_DELAY_X4                              = INSTRUCTION_DELAY_RAM_8MHZ,
    RAM_DELAY_X8                              = INSTRUCTION_DELAY_RAM_16MHZ,
    RAM_DELAY_X16                             = INSTRUCTION_DELAY_RAM_32MHZ,
    RAM_DELAY_X32                             = INSTRUCTION_DELAY_RAM_64MHZ,
    RAM_DELAY_X64                             = INSTRUCTION_DELAY_RAM_128MHZ,
    RAM_DELAY_X128                            = INSTRUCTION_DELAY_RAM_256MHZ,
    CPU_FREQUENCY_NORMAL                      = INSTRUCTION_EQUIV_FREQ_2MHZ,
    CPU_FREQUENCY_X2                          = INSTRUCTION_EQUIV_FREQ_4MHZ,
    CPU_FREQUENCY_X4                          = INSTRUCTION_EQUIV_FREQ_8MHZ,
    CPU_FREQUENCY_X8                          = INSTRUCTION_EQUIV_FREQ_16MHZ,
    CPU_FREQUENCY_X16                         = INSTRUCTION_EQUIV_FREQ_32MHZ,
    CPU_FREQUENCY_X32                         = INSTRUCTION_EQUIV_FREQ_64MHZ,
    CPU_FREQUENCY_X64                         = INSTRUCTION_EQUIV_FREQ_128MHZ,
    CPU_FREQUENCY_X128                        = INSTRUCTION_EQUIV_FREQ_256MHZ,
};
#endif

// IOCTL commands. Passed from user space using the IOCTL method to command the driver to perform an action.
#define IOCTL_CMD_Z80_STOP                    's'
#define IOCTL_CMD_Z80_START                   'S'
#define IOCTL_CMD_Z80_PAUSE                   'P'
#define IOCTL_CMD_Z80_RESET                   'R'
#define IOCTL_CMD_Z80_CONTINUE                'C'
#define IOCTL_CMD_USE_HOST_RAM                'x'
#define IOCTL_CMD_USE_VIRTUAL_RAM             'X'
#define IOCTL_CMD_DUMP_MEMORY                 'M'
#define IOCTL_CMD_Z80_CPU_FREQ                'F'
#define IOCTL_CMD_ADD_DEVICE                  'A'
#define IOCTL_CMD_DEL_DEVICE                  'D'
#define IOCTL_CMD_CPLD_CMD                    'z'
#define IOCTL_CMD_SEND                        _IOW('c', 'c', int32_t *)
#define IOCTL_CMD_SETPC                       _IOW('p', 'p', int32_t *)
#define IOCTL_CMD_SYNC_TO_HOST_RAM            'V'
#define IOCTL_CMD_DEBUG                       'd'
#define IOCTL_CMD_SPI_TEST                    '1'
#define IOCTL_CMD_PRL_TEST                    '2'
#define IOCTL_CMD_Z80_MEMTEST                 '3'
 
//  Chip Select map MZ80K-MZ700.
// 
//  0000 - 0FFF = CS_ROMni  : R/W : MZ80K/A/700   = Monitor ROM or RAM (MZ80A rom swap)
//  1000 - CFFF = CS_RAMni  : R/W : MZ80K/A/700   = RAM
//  C000 - CFFF = CS_ROMni  : R/W : MZ80A         = Monitor ROM (MZ80A rom swap)
//  D000 - D7FF = CS_VRAMni : R/W : MZ80K/A/700   = VRAM
//  D800 - DFFF = CS_VRAMni : R/W : MZ700         = Colour VRAM (MZ700)
//  E000 - E003 = CS_8255n  : R/W : MZ80K/A/700   = 8255       
//  E004 - E007 = CS_8254n  : R/W : MZ80K/A/700   = 8254
//  E008 - E00B = CS_LS367n : R/W : MZ80K/A/700   = LS367
//  E00C - E00F = CS_ESWPn  : R   : MZ80A         = Memory Swap (MZ80A)
//  E010 - E013 = CS_ESWPn  : R   : MZ80A         = Reset Memory Swap (MZ80A)
//  E014        = CS_E5n    : R/W : MZ80A/700     = Normal CRT display (in Video Controller)
//  E015        = CS_E6n    : R/W : MZ80A/700     = Reverse CRT display (in Video Controller)
//  E200 - E2FF =           : R/W : MZ80A/700     = VRAM roll up/roll down.
//  E800 - EFFF =           : R/W : MZ80K/A/700   = User ROM socket or DD Eprom (MZ700)
//  F000 - F7FF =           : R/W : MZ80K/A/700   = Floppy Disk interface.
//  F800 - FFFF =           : R/W : MZ80K/A/700   = Floppy Disk interface.
// 
//  Chip Select map MZ800
// 
//  FC - FF     = CS_PIOn   : R/W : MZ800/MZ1500  = Z80 PIO Printer Interface
//  F2          = CS_PSG0n  :   W : MZ800/MZ1500  = Programable Sound Generator, MZ-800 = Mono, MZ-1500 = Left Channel
//  F3          = CS_PSG1n  :   W : MZ1500        = Programable Sound Generator, MZ-1500 = Right Channel
//  E9          = CS_PSG(X)n:   W : MZ1500        = Simultaneous write to both PSG's.
//  F0 - F1     = CS_JOYSTK : R   : MZ800         = Joystick 1 and 2
//  CC          = CS_GWF    :   W : MZ800         = CRTC GWF Write format Register
//  CD          = CS_GRF    :   W : MZ800         = CRTC GRF Read format Register
//  CE          = CS_GDMD   :   W : MZ800         = CRTC GDMD Mode Register
//  CF          = CS_GCRTC  :   W : MZ800         = CRTC GCRTC Control Register
//  D4 - D7     = CS
//  D000 - DFFF

//  MZ700/MZ800 memory mode switch?
// 
//              MZ-700                                                     MZ-800
//             |0000:0FFF|1000:1FFF|1000:CFFF|C000:CFFF|D000:FFFF          |0000:7FFF|1000:1FFF|2000:7FFF|8000:BFFF|C000:CFFF|C000:DFFF|E000:FFFF
//             --------------------------------------------------          ----------------------------------------------------------------------
//  OUT 0xE0 = |DRAM     |         |         |         |                   |DRAM     |         |         |         |         |         |            
//  OUT 0xE1 = |         |         |         |         |DRAM               |         |         |         |         |         |         |DRAM
//  OUT 0xE2 = |MONITOR  |         |         |         |                   |MONITOR  |         |         |         |         |         |             
//  OUT 0xE3 = |         |         |         |         |Memory Mapped I/O  |         |         |         |         |         |         |Upper MONITOR ROM             
//  OUT 0xE4 = |MONITOR  |         |DRAM     |         |Memory Mapped I/O  |MONITOR  |CGROM    |DRAM     |VRAM     |         |DRAM     |Upper MONITOR ROM
//  OUT 0xE5 = |         |         |         |         |Inhibit            |         |         |         |         |         |         |Inhibit
//  OUT 0xE6 = |         |         |         |         |<return>           |         |         |         |         |         |         |<return>
//  IN  0xE0 = |         |CGROM*   |         |VRAM*    |                   |         |CGROM    |         |VRAM     |         |         |              
//  IN  0xE1 = |         |DRAM     |         |DRAM     |                   |         |<return> |         |DRAM     |         |         |                                                                           
// 
//  <return> = Return to the state prior to the complimentary command being invoked.
//  * = MZ-800 host only.

// Macros to lookup and test to see if a given memory block or IO byte is of a given type. Also macros to read/write to the memory block and IO byte.
// The memory page arrays dont check for allocation due to speed, it is assumed a memory mode page has been allocated and defined prior to the memoryMode
// variable being set to that page.
#define MEMORY_MODES                          32                                       // Maximum number of different memory modes.
#define MEMORY_PAGE_SIZE                      0x10000                                  // Total size of directly addressable memory.
#define MEMORY_BLOCK_GRANULARITY              0x1                                      // Any change update MEMORY_BLOCK_SHIFT and mask in MEMORY_BLOCK_MASK
#define MEMORY_BLOCK_SHIFT                    0
#define MEMORY_BLOCK_SLOTS                    (MEMORY_PAGE_SIZE / MEMORY_BLOCK_GRANULARITY)
#define MEMORY_BLOCK_MASK                     (MEMORY_PAGE_SIZE - MEMORY_BLOCK_GRANULARITY)
#define IO_PAGE_SIZE                          0x10000                                  // Total size of directly addressable I/O.
#define IO_BLOCK_GRANULARITY                  0x1                                      // Any change update MEMORY_BLOCK_SHIFT and mask in MEMORY_BLOCK_MASK
#define IO_BLOCK_SHIFT                        0
#define IO_BLOCK_SLOTS                        (IO_PAGE_SIZE / IO_BLOCK_GRANULARITY)
#define IO_BLOCK_MASK                         (IO_PAGE_SIZE - IO_BLOCK_GRANULARITY)
//#define getPageData(a)                        (Z80Ctrl->page[(a & 0xFFFF) >> MEMORY_BLOCK_SHIFT])
//#define getIOPageData(a)                      (Z80Ctrl->iopage[(a & 0xFFFF])
#define getPageData(a)                        (*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + ((a & MEMORY_BLOCK_MASK) >> MEMORY_BLOCK_SHIFT)))
#define getIOPageData(a)                      (Z80Ctrl->iopage[(a & IO_BLOCK_MASK])

#define getPageType(a, mask)                  (getPageData(a) & mask)
#define getPageAddr(a, mask)                  ((getPageData(a) & mask) + (a & (MEMORY_BLOCK_GRANULARITY-1)))
#define getIOPageType(a, mask)                (getIOPageData(a) & mask)
#define getIOPageAddr(a, mask)                (getIOPageData(a) & mask)
//#define realAddress(a)                        (Z80Ctrl->page[getPageAddr(a, MEMORY_TYPE_REAL_MASK)])
#define realAddress(a)                        (*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (getPageAddr(a, MEMORY_TYPE_REAL_MASK))))
#define realPort(a)                           (Z80Ctrl->iopage[a & IO_BLOCK_MASK] & IO_TYPE_MASK)
#define isPhysicalRAM(a)                      (getPageType(a, MEMORY_TYPE_PHYSICAL_RAM))
#define isPhysicalVRAM(a)                     (getPageType(a, MEMORY_TYPE_PHYSICAL_VRAM))
#define isPhysicalROM(a)                      (getPageType(a, MEMORY_TYPE_PHYSICAL_ROM))
#define isPhysicalMemory(a)                   (getPageType(a, (MEMORY_TYPE_PHYSICAL_ROM | MEMORY_TYPE_PHYSICAL_RAM | MEMORY_TYPE_PHYSICAL_VRAM))])
#define isPhysicalHW(a)                       (getPageType(a, MEMORY_TYPE_PHYSICAL_HW))
#define isPhysical(a)                         (getPageType(a, (MEMORY_TYPE_PHYSICAL_HW | MEMORY_TYPE_PHYSICAL_ROM | MEMORY_TYPE_PHYSICAL_RAM | MEMORY_TYPE_PHYSICAL_VRAM)))
#define isPhysicalIO(a)                       (Z80Ctrl->iopage[a & IO_BLOCK_MASK] & IO_TYPE_PHYSICAL_HW)
#define isVirtualRAM(a)                       (getPageType(a, (MEMORY_TYPE_VIRTUAL_RAM | MEMORY_TYPE_VIRTUAL_RAM_RO)))
#define isVirtualRO(a)                        (getPageType(a, MEMORY_TYPE_VIRTUAL_RAM_RO))
#define isVirtualRW(a)                        (getPageType(a, MEMORY_TYPE_VIRTUAL_RAM))
#define isVirtualROM(a)                       (getPageType(a, MEMORY_TYPE_VIRTUAL_ROM))
#define isVirtualMemory(a)                    (getPageType(a, (MEMORY_TYPE_VIRTUAL_ROM | MEMORY_TYPE_VIRTUAL_RAM | MEMORY_TYPE_VIRTUAL_RAM_RO)))
#define isVirtualHW(a)                        (getPageType(a, MEMORY_TYPE_VIRTUAL_HW))
#define isVirtualIO(a)                        (Z80Ctrl->iopage[a & IO_BLOCK_MASK] & IO_TYPE_VIRTUAL_HW)
#define isVirtual(a)                          (getPageType(a, (MEMORY_TYPE_VIRTUAL_ROM | MEMORY_TYPE_VIRTUAL_RAM | MEMORY_TYPE_VIRTUAL_RAM_RO | MEMORY_TYPE_VIRTUAL_HW)))
#define isVirtualDevice(a, d)                 (Z80Ctrl->iopage[a & IO_BLOCK_MASK] & d)
#define isHW(a)                               (getPageType(a, (MEMORY_TYPE_PHYSICAL_HW | MEMORY_TYPE_VIRTUAL_HW)))
#define readVirtualRAM(a)                     (Z80Ctrl->ram[ getPageAddr(a, MEMORY_TYPE_VIRTUAL_MASK) ])
#define readVirtualROM(a)                     (Z80Ctrl->rom[ getPageAddr(a, MEMORY_TYPE_VIRTUAL_MASK) ])
#define writeVirtualRAM(a, d)                 { Z80Ctrl->ram[ getPageAddr(a, MEMORY_TYPE_VIRTUAL_MASK) ] = d; }
#define writeVirtualROM(a, d)                 { Z80Ctrl->rom[ getPageAddr(a, MEMORY_TYPE_VIRTUAL_MASK) ] = d; }
//#define setMemoryType(_block_,_type_,_addr_)  { Z80Ctrl->page[_block_] = _type_ | _addr_; }
#define setMemoryType(_block_,_type_,_addr_)  { *(*(Z80Ctrl->page + Z80Ctrl->memoryMode) +_block_) = _type_ | _addr_; }
#define backupMemoryType(_block_)             { Z80Ctrl->shadowPage[_block_] = *(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (_block_)); }
//#define restoreMemoryType(_block_)            { Z80Ctrl->page[_block_] = Z80Ctrl->shadowPage[_block_]; }
#define restoreMemoryType(_block_)            { *(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (_block_)) = Z80Ctrl->shadowPage[_block_]; }
#define sendSignal(_signal_)                  { struct siginfo sigInfo;\
                                                if(Z80Ctrl->ioTask != NULL)\
                                                {\
                                                    memset(&sigInfo, 0, sizeof(struct siginfo));\
                                                    sigInfo.si_signo = _signal_;\
                                                    sigInfo.si_code = SI_QUEUE;\
                                                    sigInfo.si_int = 1;\
                                                    if(send_sig_info(_signal_, &sigInfo, Z80Ctrl->ioTask) < 0)\
                                                    {\
                                                        pr_info("Error: Failed to send Request to I/O Processor:%d, %s\n", _signal_, Z80Ctrl->ioTask->comm);\
                                                    }\
                                                }\
                                              }
#define resetZ80()                            {\
                                                  sendSignal(SIGUSR1); \
                                                  setupMemory(Z80Ctrl->defaultPageMode);\
                                                  z80_instant_reset(&Z80CPU);\
                                              }

#define IO_ADDR_E0                            0xE0
#define IO_ADDR_E1                            0xE1
#define IO_ADDR_E2                            0xE2
#define IO_ADDR_E3                            0xE3
#define IO_ADDR_E4                            0xE4
#define IO_ADDR_E5                            0xE5
#define IO_ADDR_E6                            0xE6
#define IO_ADDR_E7                            0xE7
#define IO_ADDR_E8                            0xE8
#define IO_ADDR_E9                            0xE9
#define IO_ADDR_EA                            0xEA
#define IO_ADDR_EB                            0xEB


enum Z80_RUN_STATES {
    Z80_STOP                                = 0x00,
    Z80_STOPPED                             = 0x01,
    Z80_PAUSE                               = 0x02,
    Z80_PAUSED                              = 0x03, 
    Z80_CONTINUE                            = 0x04,
    Z80_RUNNING                             = 0x05,
};
enum Z80_MEMORY_PROFILE {
    USE_PHYSICAL_RAM                        = 0x00,
    USE_VIRTUAL_RAM                         = 0x01
};
enum VIRTUAL_DEVICE {
    VIRTUAL_DEVICE_NONE                     = 0x00000000,
    VIRTUAL_DEVICE_RFS                      = 0x02000000,
    VIRTUAL_DEVICE_TZPU                     = 0x01000000
};

typedef struct {
    // Main RAM/ROM memory, linear but indexed as though it were banks in 1K pages.
    uint8_t                                   *ram;
    uint8_t                                   *rom;

    // Compatibility mode, enables virtual mapping and virtual hardware to make the Z80 with the underlying host appear
    // as a host equipped with a specific hardware add on.
    // The devices are stored in an array for ease of reference and lookup in the driver and ctrl program, in actual 
    // use they are a bit map for performance as scanning an array is time consuming.
    // 
    enum VIRTUAL_DEVICE                       virtualDevice[MAX_VIRTUAL_DEVICES];
    uint32_t                                  virtualDeviceBitMap;
    uint8_t                                   virtualDeviceCnt;

    // Page pointer map. 
    //
    // Each pointer points to a byte or block of bytes in the Z80 Memory frame, 64K Real + Banked.
    // This is currently set at a block of size 0x1 per memory pointer for the MZ-700.
    // The LSB of the pointer is a direct memory index to a byte or block of bytes, the upper byte of the pointer indicates type of memory space.
    //                  0x80<FFFFFF> - physical host RAM
    //                  0x40<FFFFFF> - physical host ROM
    //                  0x20<FFFFFF> - physical host VRAM
    //                  0x10<FFFFFF> - physical host hardware
    //                  0x08<FFFFFF> - virtual host RAM
    //                  0x04<FFFFFF> - virtual host ROM
    //                  0x02<FFFFFF> - virtual host hardware
    // 16bit Input Address -> map -> Pointer to 24bit memory address + type flag.
    //                            -> Pointer+<low bits of address> to 24bit memory address + type flag.
    //uint32_t                                  page[MEMORY_BLOCK_SLOTS];
    uint32_t                                 *page[MEMORY_MODES];
    uint32_t                                  shadowPage[MEMORY_BLOCK_SLOTS];               // Shadow page is for manipulation and backup of an existing page.

    // Current memory mode as used by active driver.
    uint8_t                                   memoryMode;

    // I/O Page map.
    //
    // This is a map to indicate the use of the I/O page and allow any required remapping.
    //                  <0x8000><I/O Address> - physical host hardware
    //                  <0x4000><I/O Address> - virtual host hardware
    //                  <0x3FFF><I/O Address> - bit map to indicate allocated device.
    // 16bit Input Address -> map -> Actual 16bit address to use + type flag.
    uint32_t                                  iopage[65536];

    // Default page mode configured. This value reflects the default page and iotable map.
    uint8_t                                   defaultPageMode;

    // Refresh DRAM mode. 1 = Refresh, 0 = No refresh. Only applicable when running code in virtual Kernel RAM.
    uint8_t                                   refreshDRAM;

    // Inhibit mode is where certain memory ranges are inhibitted. The memory page is set to inhibit and this flag
    // blocks actions which arent allowed during inhibit.
    uint8_t                                   inhibitMode;

    // I/O lookahead flags - to overcome SSD202 io slowness.
    uint8_t                                   ioReadAhead;
    uint8_t                                   ioWriteAhead;

  #if(TARGET_HOST_MZ2000 == 1)
    uint8_t                                   lowMemorySwap;
  #endif
  #if(TARGET_HOST_MZ80A == 1)
    // MZ-80A can relocate the lower 4K ROM by swapping RAM at 0xC000.
    uint8_t                                   memSwitch;
  #endif

    // Keyboard strobe and data. Required to detect hotkey press.
    uint8_t                                   keyportStrobe;
    uint8_t                                   keyportShiftCtrl;
    uint8_t                                   keyportHotKey;

    // Governor is the delay in a 32bit loop per Z80 opcode, used to govern execution speed when using virtual memory.
    // This mechanism will eventually be tied into the M/T-state calculation for a more precise delay, but at the moment,
    // with the Z80 assigned to an isolated CPU, it allows time sensitive tasks such as the tape recorder to work.
    // The lower the value the faster the CPU speed. Two values are present as the optimiser, seeing ROM code not changing
    // is quicker than RAM (both are in the same kernel memory) as a pointer calculation needs to be made.
    uint32_t                                  cpuGovernorDelayROM;
    uint32_t                                  cpuGovernorDelayRAM;

    // An I/O processor, running as a User Space daemon, can register to receive signals and events.
    struct task_struct                        *ioTask;

  #if(DEBUG_ENABLED == 1)
    // Debugging flag.
    uint8_t                                   debug;
  #endif
} t_Z80Ctrl;

// IOCTL structure for passing data from user space to driver to perform commands.
//
struct z80_addr {
    uint32_t                                  start;
    uint32_t                                  end;
    uint32_t                                  size;
};
struct z80_ctrl {
    uint16_t                                  pc;
};
struct speed {
    uint32_t                                  speedMultiplier;
};
struct virtual_device {
    enum VIRTUAL_DEVICE                       device;
};
struct cpld_ctrl {
    uint32_t                                  cmd;
};
#if(DEBUG_ENABLED == 1)
struct debug {
    uint8_t                                   level;
};
#endif
struct ioctlCmd {
    int32_t                                   cmd;
    union {
        struct z80_addr                       addr;
        struct z80_ctrl                       z80;
        struct speed                          speed;
        struct virtual_device                 vdev;
        struct cpld_ctrl                      cpld;
      #if(DEBUG_ENABLED == 1)
        struct debug                          debug;
      #endif
    };
};

// Prototypes.
void setupMemory(enum Z80_MEMORY_PROFILE);
int thread_z80(void *);

#endif
