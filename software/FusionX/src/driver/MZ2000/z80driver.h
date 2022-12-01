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
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//                  (c) 1999-2022 Manuel Sainz de Baranda y Goñi
//
// History:         Oct 2022 - Initial write of the z80 kernel driver software.
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
#define TARGET_HOST_MZ700                     0      
#define TARGET_HOST_MZ2000                    1      
#define Z80_VIRTUAL_ROM_SIZE                  16384       // Sized to maximum ROM which is the MZ-800 ROM.
#define Z80_VIRTUAL_RAM_SIZE                  (65536 * 8) // (PAGE_SIZE * 2)   // max size mmaped to userspace
#define Z80_VIRTUAL_MEMORY_SIZE               Z80_VIRTUAL_RAM_SIZE + Z80_VIRTUAL_ROM_SIZE
#define Z80_MEMORY_PAGE_SIZE                  16
#define MAX_SCREEN_WIDTH                      132
#define DEVICE_NAME                           "z80drv"
#define  CLASS_NAME                           "mogu"

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
#define MEMORY_TYPE_VIRTUAL_HW                0x02000000
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
#endif
// MZ-2000
#if(TARGET_HOST_MZ2000 == 1)
#define INSTRUCTION_DELAY_ROM_3_54MHZ         243
#define INSTRUCTION_DELAY_ROM_7MHZ            122
#define INSTRUCTION_DELAY_ROM_14MHZ           61
#define INSTRUCTION_DELAY_ROM_28MHZ           30
#define INSTRUCTION_DELAY_ROM_56MHZ           15
#define INSTRUCTION_DELAY_ROM_112MHZ          7
#define INSTRUCTION_DELAY_ROM_224MHZ          3
#define INSTRUCTION_DELAY_ROM_448MHZ          1
#define INSTRUCTION_DELAY_RAM_3_54MHZ         218
#define INSTRUCTION_DELAY_RAM_7MHZ            112
#define INSTRUCTION_DELAY_RAM_14MHZ           56
#define INSTRUCTION_DELAY_RAM_28MHZ           28
#define INSTRUCTION_DELAY_RAM_56MHZ           14
#define INSTRUCTION_DELAY_RAM_112MHZ          7
#define INSTRUCTION_DELAY_RAM_224MHZ          3
#define INSTRUCTION_DELAY_RAM_448MHZ          1
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
#define IOCTL_CMD_CPLD_CMD                    'z'
#define IOCTL_CMD_SEND                        _IOW('c', 'c', int32_t *)
#define IOCTL_CMD_SETPC                       _IOW('p', 'p', int32_t *)
#define IOCTL_CMD_SYNC_TO_HOST_RAM            'V'
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
#define MEMORY_BLOCK_GRANULARITY              0x800
#define MEMORY_BLOCK_SLOTS                    (0x10000 / MEMORY_BLOCK_GRANULARITY)
#define MEMORY_BLOCK_MASK                     (0x10000 - MEMORY_BLOCK_GRANULARITY)
#define MEMORY_BLOCK_SHIFT                    11
#define getPageData(a)                        (Z80Ctrl->page[(a & 0xF800) >> MEMORY_BLOCK_SHIFT])
#define getIOPageData(a)                      (Z80Ctrl->iopage[(a & 0xFFFF])
#define getPageType(a, mask)                  (getPageData(a) & mask)
#define getPageAddr(a, mask)                  ((getPageData(a) & mask) + (a & (MEMORY_BLOCK_GRANULARITY-1)))
#define getIOPageType(a, mask)                (getIOPageData(a) & mask)
#define getIOPageAddr(a, mask)                (getIOPageData(a) & mask)
#define realAddress(a)                        (Z80Ctrl->page[getPageAddr(a, MEMORY_TYPE_REAL_MASK)])
#define realPort(a)                           (Z80Ctrl->iopage[a & 0xFFFF] & IO_TYPE_MASK)
#define isPhysicalRAM(a)                      (getPageType(a, MEMORY_TYPE_PHYSICAL_RAM))
#define isPhysicalVRAM(a)                     (getPageType(a, MEMORY_TYPE_PHYSICAL_VRAM))
#define isPhysicalROM(a)                      (getPageType(a, MEMORY_TYPE_PHYSICAL_ROM))
#define isPhysicalMemory(a)                   (getPageType(a, (MEMORY_TYPE_PHYSICAL_ROM | MEMORY_TYPE_PHYSICAL_RAM | MEMORY_TYPE_PHYSICAL_VRAM))])
#define isPhysicalHW(a)                       (getPageType(a, MEMORY_TYPE_PHYSICAL_HW))
#define isPhysical(a)                         (getPageType(a, (MEMORY_TYPE_PHYSICAL_HW | MEMORY_TYPE_PHYSICAL_ROM | MEMORY_TYPE_PHYSICAL_RAM | MEMORY_TYPE_PHYSICAL_VRAM)))
#define isPhysicalIO(a)                       (Z80Ctrl->iopage[a & 0xFFFF] & IO_TYPE_PHYSICAL_HW)
#define isVirtualRAM(a)                       (getPageType(a, MEMORY_TYPE_VIRTUAL_RAM))
#define isVirtualROM(a)                       (getPageType(a, MEMORY_TYPE_VIRTUAL_ROM))
#define isVirtualMemory(a)                    (getPageType(a, (MEMORY_TYPE_VIRTUAL_ROM | MEMORY_TYPE_VIRTUAL_RAM)))
#define isVirtualHW(a)                        (getPageType(a, MEMORY_TYPE_VIRTUAL_HW))
#define isVirtualIO(a)                        (Z80Ctrl->iopage[a & 0xFFFF] & IO_TYPE_VIRTUAL_HW)
#define isHW(a)                               (getPageType(a, (MEMORY_TYPE_PHYSICAL_HW | MEMORY_TYPE_VIRTUAL_HW)))
#define readVirtualRAM(a)                     (Z80Ctrl->memory[ getPageAddr(a, MEMORY_TYPE_VIRTUAL_MASK) ])
#define readVirtualROM(a)                     (Z80Ctrl->memory[ getPageAddr(a, MEMORY_TYPE_VIRTUAL_MASK) + Z80_VIRTUAL_RAM_SIZE ])
#define writeVirtualRAM(a, d)                 { Z80Ctrl->memory[ getPageAddr(a, MEMORY_TYPE_VIRTUAL_MASK) ] = d; }
#define setMemoryType(_block_,_type_,_addr_)  { Z80Ctrl->page[_block_] = _type_ | _addr_; }
#define backupMemoryType(_block_)             { Z80Ctrl->shadowPage[_block_] = Z80Ctrl->page[_block_]; }
#define restoreMemoryType(_block_)            { Z80Ctrl->page[_block_] = Z80Ctrl->shadowPage[_block_]; }

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

typedef struct {
    // Main memory, linear but indexed as though it were banks in 1K pages.
    uint8_t                                   memory[Z80_VIRTUAL_MEMORY_SIZE];

    // Page pointer map. 
    //
    // Each pointer points to a byte or block of bytes in the Z80 Memory frame, 64K Real + Banked.
    // This is currently set at a block of size 0x800 per memory pointer for the MZ-700.
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
    uint32_t                                  page[MEMORY_BLOCK_SLOTS];
    uint32_t                                  shadowPage[MEMORY_BLOCK_SLOTS];

    // I/O Page map.
    //
    // This is a map to indicate the use of the I/O page and allow any required remapping.
    //                  <0x80>FF<I/O Address> - physical host hardware
    //                  <0x40>FF<I/O Address> - virtual host hardware
    // 16bit Input Address -> map -> Actual 16bit address to use + type flag.
    uint32_t                                  iopage[65536];

    // Default page mode configured. This value reflects the default page and iotable map.
    uint8_t                                   defaultPageMode;

    // Refresh DRAM mode. 1 = Refresh, 0 = No refresh. Only applicable when running code in virtual Kernel RAM.
    uint8_t                                   refreshDRAM;

    // Inhibit mode is where certain memory ranges are inhibitted. The memory page is set to inhibit and this flag
    // blocks actions which arent allowed during inhibit.
    uint8_t                                   inhibitMode;

    // Address caching. Used to minimise instruction length sent to CPLD.
    uint16_t                                  z80PrevAddr;
    uint16_t                                  z80PrevPort;

#if(TARGET_HOST_MZ2000 == 1)
    uint8_t                                   lowMemorySwap;
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
} t_Z80Ctrl;

// IOCTL structure for passing data from user space to driver to perform commands.
//
struct z80_addr {
   uint32_t                                   start;
   uint32_t                                   end;
   uint32_t                                   size;
};
struct z80_ctrl {
   uint16_t                                   pc;
};
struct speed {
   uint32_t                                   speedMultiplier;
};
struct cpld_ctrl {
   uint32_t                                   cmd;
};
struct ioctlCmd {
   int32_t                                    cmd;
   union {
       struct z80_addr                        addr;
       struct z80_ctrl                        z80;
       struct speed                           speed;
       struct cpld_ctrl                       cpld;
   };
};


// Prototypes.
void setupMemory(enum Z80_MEMORY_PROFILE mode);


#endif
