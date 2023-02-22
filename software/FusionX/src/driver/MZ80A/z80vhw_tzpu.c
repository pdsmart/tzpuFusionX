/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80vhw_tzpu.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Virtual Hardware Driver - tranZPUter SW
//                  This file contains the methods used to emulate the first tranZPUter Software series
//                  the tranZPUter SW which used a Cortex-M4 to enhance the host machine and provide
//                  an upgraded monitor, the tzfs (tranZPUter Filing System). As the original hardware
//                  use an independent processor to provide services to the Z80, this driver is made up
//                  of two parts, 1) the Z80, ie. this driver, 2) a user space daemon which fulfils the
//                  role of the independent processor.
//
//                  These drivers are intended to be instantiated inline to reduce overhead of a call
//                  and as such, they are included like header files rather than C linked object files.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Feb 2023 v1.0 - Initial write based on the tranZPUter SW hardware.
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


#include <linux/module.h>
#include <linux/init.h> 
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h> 
#include <linux/mm.h> 
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/time.h>
#include "z80io.h"

#include <gpio_table.h>
#include <asm/io.h>
#include <infinity2m/gpio.h>
#include <infinity2m/registers.h>

#include "tzpu.h"

// TZPU Board registers.
typedef struct {
    uint8_t                                   clkSrc;                                   // Source of clock, host - 0, secondary - 1
    uint8_t                                   regCmd;                                   // Internal command register of the CPLD.
    uint8_t                                   regCmdStatus;                             // Internal command status register of the CPLD.
    uint8_t                                   regCpuCfg;                                // Internal FPGA CPU config register.
    uint8_t                                   regCpuInfo;                               // Internal FPGA CPU information register.
    uint8_t                                   regCpldCfg;                               // Internal CPLD config register.
    uint8_t                                   regCpldInfo;                              // Internal CPLD information register.
} t_TZPUCtrl;

// TZPU Board control.
static t_TZPUCtrl TZPUCtrl;

//-------------------------------------------------------------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------------------------------------------------------------

// Memory Modes:     0 - Default, normal Sharp MZ80A operating mode, all memory and IO (except tranZPUter control IO block) are on the mainboard
//                   1 - As 0 except User ROM is mapped to tranZPUter RAM.
//                   2 - TZFS, Monitor ROM 0000-0FFF, Main DRAM 0x1000-0xD000, User/Floppy ROM E800-FFFF are in tranZPUter memory. Two small holes of 2 bytes at F3FE and F7FE exist
//                       for the Floppy disk controller, the fdc uses the rom as a wait detection by toggling the ROM lines according to WAIT, the Z80 at 2MHz hasnt enough ooomph to read WAIT and action it. 
//                       NB: Main DRAM will not be refreshed so cannot be used to store data in this mode.
//                   3 - TZFS, Monitor ROM 0000-0FFF, Main RAM area 0x1000-0xD000, User ROM 0xE800-EFFF are in tranZPUter memory block 0, Floppy ROM F000-FFFF are in tranZPUter memory block 1.
//                       NB: Main DRAM will not be refreshed so cannot be used to store data in this mode.
//                   4 - TZFS, Monitor ROM 0000-0FFF, Main RAM area 0x1000-0xD000, User ROM 0xE800-EFFF are in tranZPUter memory block 0, Floppy ROM F000-FFFF are in tranZPUter memory block 2.
//                       NB: Main DRAM will not be refreshed so cannot be used to store data in this mode.
//                   5 - TZFS, Monitor ROM 0000-0FFF, Main RAM area 0x1000-0xD000, User ROM 0xE800-EFFF are in tranZPUter memory block 0, Floppy ROM F000-FFFF are in tranZPUter memory block 3.
//                       NB: Main DRAM will not be refreshed so cannot be used to store data in this mode.
//                   6 - CPM, all memory on the tranZPUter board, 64K block 4 selected.
//                       Special case for F3FE:F3FF & F7FE:F7FF (floppy disk paging vectors) which resides on the mainboard.
//                   7 - CPM, F000-FFFF are on the tranZPUter board in block 4, 0040-CFFF and E800-EFFF are in block 5 selected, mainboard for D000-DFFF (video), E000-E800 (Memory control) selected.
//                       Special case for 0000:00FF (interrupt vectors) which resides in block 4 and CPM vectors and two small holes of 2 bytes at F3FE and F7FE exist for the Floppy disk controller, the fdc
//                       uses the rom as a wait detection by toggling the ROM lines according to WAIT, the Z80 at 2MHz hasnt enough ooomph to read WAIT and action it. 
//                   8 - Monitor ROM (0000:0FFF) on mainboard, Main RAM (1000:CFFF) in tranZPUter bank 0 and video, memory mapped I/O, User/Floppy ROM on mainboard.
//                       NB: Main DRAM will not be refreshed so cannot be used to store data in this mode.
//                   9 - Monitor ROM 0000-0FFF and Main DRAM 0x1000-0xD000, video and memory mapped I/O are on the host machine, User/Floppy ROM E800-FFFF are in tranZPUter memory. 
//                  10 - MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the mainboard.
//                  11 - MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 0, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the tranZPUter in block 6.
//                  12 - MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the tranZPUter in block 6.
//                  13 - MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 0, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is inaccessible.
//                  14 - MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is inaccessible.
//                  15 - MZ800 Mode  - Running on MZ800 hardware, configuration set according to MZ700/MZ800 mode.
//                  16 - MZ2000 Mode - Running on MZ2000 hardware, configuration set according to the MZ2000 runtime configuration registers.
//                  21 - Access the FPGA memory by passing through the full 24bit Z80 address, typically from the K64F.
//                  22 - Access to the host mainboard 64K address space only.
//                  23 - Access all memory and IO on the tranZPUter board with the K64F addressing the full 512K-1MB RAM.
//                  24 - All memory and IO are on the tranZPUter board, 64K block 0 selected.
//                  25 - All memory and IO are on the tranZPUter board, 64K block 1 selected.
//                  26 - All memory and IO are on the tranZPUter board, 64K block 2 selected.
//                  27 - All memory and IO are on the tranZPUter board, 64K block 3 selected.
//                  28 - All memory and IO are on the tranZPUter board, 64K block 4 selected.
//                  29 - All memory and IO are on the tranZPUter board, 64K block 5 selected.
//                  30 - All memory and IO are on the tranZPUter board, 64K block 6 selected.
//                  31 - All memory and IO are on the tranZPUter board, 64K block 7 selected.

// Perform any setup operations, such as variable initialisation, to enable use of this module.
void tzpuInit(void)
{
    pr_info("Enabling TZPU driver.\n");
}

// Method to decode an address and make any system memory map changes as required.
//
static inline void tzpuDecodeMemoryMapSetup(zuint16 address, zuint8 data, uint8_t ioFlag, uint8_t readFlag)
{
    // Locals.

    // I/O or Memory?
    if(ioFlag == 0)
    {
        // Memory map switch.
        if(readFlag == 0)
        {

        } else
        {

        }
    } else
    // I/O Decoding.
    {
        // Only lower 8 bits recognised in the tzpu.
        switch(address & 0xFF)
        {
            default:
                break;

        }
    }
}

// Method to read from the tranZPUter SW memory or I/O ports.
static inline uint8_t tzpuRead(zuint16 address, uint8_t ioFlag)
{
    // Locals.
    uint8_t     data = 0x00;

    // The tranZPUter board, in order to autoboot and use valuable space for variables, allows writing into the User ROM 
    // space above 0xEBFF. 0xE800 is used to detect if a ROM is installed and cannot be written.
    if(ioFlag == 0 && isVirtualRAM(address))
    {
        // Retrieve data from virtual memory.
        data = readVirtualRAM(address);
    } else
    if(ioFlag)
    {
      #if(DEBUG_ENABLED & 0x01)
        if(Z80Ctrl->debug & 0x01) pr_info("Read IO:%02x\n", address);
      #endif

        // Only the lower 8 bits of the I/O address are processed as the upper byte is not used in the Sharp models.
        //
        switch(address & 0x00FF)
        {
            case IO_TZ_CTRLLATCH:
                data = Z80Ctrl->memoryMode;
                break;

            case IO_TZ_SETXMHZ:   
                break;
            case IO_TZ_SET2MHZ:   
                break;

            case IO_TZ_CLKSELRD:  
                data = TZPUCtrl.clkSrc;
                break;

            case IO_TZ_SVCREQ:    
                break;
                
            case IO_TZ_SYSREQ:    
                break;

            case IO_TZ_CPLDSTATUS:
                data = TZPUCtrl.regCmdStatus;
                break;

            case IO_TZ_CPUSTATUS: 
                data = TZPUCtrl.regCpuCfg;
                break;

            case IO_TZ_CPUINFO:   
                data = TZPUCtrl.regCpuInfo;
                break;

            case IO_TZ_CPLDCFG:   
                data = TZPUCtrl.regCpldCfg;
                break;

            case IO_TZ_CPLDINFO:  
                data = TZPUCtrl.regCpldInfo;
                break;

            default:
                break;
        }
    }

    return(data);
}

// Method to write to the tranZPUter SW memory or I/O ports.
static inline void tzpuWrite(zuint16 address, zuint8 data, uint8_t ioFlag)
{
    // Locals
    uint32_t       idx;

    // The tranZPUter board, in order to autoboot and use valuable space for variables, allows writing into the User ROM 
    // space above 0xE800. 0xE800 is used to detect if a ROM is installed and cannot be written.
    if(ioFlag == 0)
    {
        // Virtual ROM in range 0xEC00:0xEFFF is Read/Write. 
        if(isVirtualRW(address))
        {
            //if(address >= 0xE800) pr_info("Write:%04x,%02x\n", address, data);
            writeVirtualRAM(address, data);
        }
    } else
    if(ioFlag)
    {
        // Only the lower 8 bits of the I/O address are processed as the upper byte is not used in the Sharp models.
        //
        switch(address & 0x00FF)
        {
            case IO_TZ_CTRLLATCH:
                //pr_info("CTRLLATCH:%02x\n", data);
              
                // Check to see if the memory mode page has been allocated for requested mode, if it hasnt, we need to allocate and then define.
                Z80Ctrl->memoryMode = (data & (MEMORY_MODES - 1));
                if(Z80Ctrl->page[Z80Ctrl->memoryMode] == NULL)
                {
                  #if(DEBUG_ENABLED & 0x01)
                    if(Z80Ctrl->debug & 0x01) pr_info("Allocating memory page:%d\n", Z80Ctrl->memoryMode);
                  #endif
                    (Z80Ctrl->page[Z80Ctrl->memoryMode]) = (uint32_t *)kmalloc((MEMORY_BLOCK_SLOTS*sizeof(uint32_t)), GFP_KERNEL);
                    if ((Z80Ctrl->page[Z80Ctrl->memoryMode]) == NULL) 
                    {
                        pr_info("z80drv: failed to allocate  memory mapping page:%d memory!", Z80Ctrl->memoryMode);
                        Z80Ctrl->page[Z80Ctrl->memoryMode] = Z80Ctrl->page[0];
                    }

                    // A lot of the memory maps below are identical, minor changes such as RAM bank. This is a direct conversion of the VHDL code from the CPLD.
                    //
                    for(idx=0x0000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        switch(Z80Ctrl->memoryMode)
                        {
                            // Original Sharp MZ80A mode, no tranZPUter features are selected except the I/O control registers (default: 0x60-063).
                            case TZMM_ORIG:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE800 && idx < 0xEC00)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xEC00 && idx < 0xF000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xF000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                break;

                            // Original mode but E800-EFFF is mapped to tranZPUter RAM so TZFS can be booted.
                            case TZMM_BOOT:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE800 && idx < 0xEC00)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xEC00 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                break;

                            // TZFS main memory configuration. all memory is in tranZPUter RAM, E800-FFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected.
                            case TZMM_TZFS:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx == 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE801 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                break;

                            // TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 1.
                            case TZMM_TZFS2:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE800 && idx < 0xEC00)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xEC00 && idx < 0xF000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xF000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK1_ADDR+idx));
                                }
                                break;

                            // TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 2.
                            case TZMM_TZFS3:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx == 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE801 && idx < 0xF000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xF000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK2_ADDR+idx));
                                }
                                break;

                            // TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 3.
                            case TZMM_TZFS4:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx == 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE801 && idx < 0xF000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xF000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK3_ADDR+idx));
                                }
                                break;

                            // CPM main memory configuration, all memory on the tranZPUter board, 64K block 4 selected. Special case for F3C0:F3FF & F7C0:F7FF (floppy disk paging vectors) which resides on the mainboard.
                            case TZMM_CPM:
                                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,        (SRAM_BANK4_ADDR+idx));
                                break;

                            // CPM main memory configuration, F000-FFFF are on the tranZPUter board in block 4, 0040-CFFF and E800-EFFF are in block 5, mainboard for D000-DFFF (video), E000-E800 (Memory control) selected.
                            // Special case for 0000:003F (interrupt vectors) which resides in block 4, F3C0:F3FF & F7C0:F7FF (floppy disk paging vectors) which resides on the mainboard.
                            case TZMM_CPM2:
                                if(idx >= 0 && idx < 0x40)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK4_ADDR+idx));
                                }
                                else if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK5_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK5_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE800 && idx < 0xEC00)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK5_ADDR+idx));
                                }
                                else if(idx >= 0xEC00 && idx < 0xF000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK5_ADDR+idx));
                                }
                                else if(idx >= 0xF000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK4_ADDR+idx));
                                }
                                break;

                            // Original mode but with main DRAM in Bank 0 to allow bootstrapping of programs from other machines such as the MZ700.
                            case TZMM_COMPAT:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE800 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                break;

                            // Mode to allow code running in Bank 0, address E800:FFFF to access host memory. Monitor ROM 0000-0FFF and Main DRAM 0x1000-0xD000, video and memory mapped I/O are on the host machine, User/Floppy ROM E800-FFFF are in tranZPUter memory. 
                            case TZMM_HOSTACCESS:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE800 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                } 
                                break;

                            // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the mainboard.
                            case TZMM_MZ700_0:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK6_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0xE000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM,  (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE000 && idx < 0xE800)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xE800 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM_RO, (SRAM_BANK0_ADDR+idx));
                                }
                                break;

                            // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 0, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the tranZPUter in block 6.
                            case TZMM_MZ700_1:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK6_ADDR+idx));
                                }
                                break;

                            // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the tranZPUter in block 6.
                            case TZMM_MZ700_2:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK6_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK6_ADDR+idx));
                                } 
                                break;

                            // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 0, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is inaccessible.
                            case TZMM_MZ700_3:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_INHIBIT,        (SRAM_BANK0_ADDR+idx));
                                } 
                                break;

                            // MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is inaccessible.
                            case TZMM_MZ700_4:
                                if(idx >= 0 && idx < 0x1000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK6_ADDR+idx));
                                }
                                else if(idx >= 0x1000 && idx < 0xD000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM,    (SRAM_BANK0_ADDR+idx));
                                }
                                else if(idx >= 0xD000 && idx < 0x10000)
                                {
                                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_INHIBIT,        (SRAM_BANK0_ADDR+idx));
                                } 
                                break;

                            // MZ800 Mode - Host is an MZ-800 and mode provides for MZ-700/MZ-800 decoding per original machine.
                            case TZMM_MZ800:

                            // MZ2000 Mode - Running on MZ2000 hardware, configuration set according to runtime configuration registers.
                            case TZMM_MZ2000:

                            // Open up access for the K64F to the FPGA resources such as memory. All other access to RAM or mainboard is blocked.
                            case TZMM_FPGA:

                            // Everything is on mainboard, no access to tranZPUter memory.
                            case TZMM_TZPUM:

                            // Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory. K64F drives A18-A16 allowing full access to RAM.
                            case TZMM_TZPU:

                            default:
                                pr_info("Memory Mode(%d) not available.\n", data); 
                                break;
                        }
                    }
                }
                // Memory map now created/switched.
                break;

            case IO_TZ_SETXMHZ:   
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("SETXMHZ:%02x\n", data);
              #endif
                TZPUCtrl.clkSrc = 1;
                break;

            case IO_TZ_SET2MHZ:   
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("SET2MHZ:%02x\n", data);
              #endif
                TZPUCtrl.clkSrc = 0;
                break;

            case IO_TZ_CLKSELRD:  
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("CKSELRD:%02x\n", data);
              #endif
                break;

            case IO_TZ_SVCREQ:    
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("SVCREQ:%02x\n", data);
              #endif

                // If a k64f process has registered, send it a service request signal.
                sendSignal(SIGIO);

                // A strange race state exists with CP/M and interrupts during disk requests. If no delay is
                // given for read/write requests, the interrupt line will eventually lockup active and the Z80
                // enters an interrupt loop. Strange because the Z80 interrupt line is a buffered signal from the
                // host motherboard to the SOM via CPLD without latches. The 8253 timer generates the interrupt
                // but the ISR resets it. My guess is memory corruption due to a race state, more time is needed 
                // debugging but in the meantime, a 2ms delay ensures the read/write completes prior to next
                // request.
                if(Z80Ctrl->ram[0x4f560] == 0x32 || Z80Ctrl->ram[0x4f560] == 0x33)
                {
                    udelay(2000);
                }
                break;
                
            case IO_TZ_SYSREQ:    
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("SYSREQ:%02x\n", data);
              #endif
                break;

            case IO_TZ_CPLDCMD:
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("CPLDCMD:%02x\n", data);
              #endif
                TZPUCtrl.regCmd = data;
                break;

            case IO_TZ_CPUINFO:   
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("CPUINFO:%02x\n", data);
              #endif
                break;

            case IO_TZ_CPLDCFG:   
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("CPLDCFG:%02x\n", data);
              #endif
                break;

            case IO_TZ_CPLDINFO:  
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug & 0x01) pr_info("CPLDINFO:%02x\n", data);
              #endif
                break;

            default:
                pr_info("PORT:%02x\n", data);
                break;
        }
    }
    return;
}

// Method to setup the memory page config to reflect installation of a tranZPUter SW Board. This sets up the default
// as the memory map changes according to selection and handled in-situ.
void tzpuSetupMemory(enum Z80_MEMORY_PROFILE mode)
{
    // Locals.
    uint32_t      idx;

    // The tranZPUter SW uses a CPLD to set a 4K Z80 memory range window into a 512K-1MB linear RAM block. The actual map required
    // at any one time is governed by the Memory Config register at I/O port 0x60.
    // This method sets the initial state, which is a normal Sharp operating mode, all memory and IO (except tranZPUter
    // control IO block) are on the mainboard.

    // Setup defaults.
    TZPUCtrl.clkSrc         = 0x00;   // Clock defaults to host.
    TZPUCtrl.regCmd         = 0x00;   // Default for the CPLD Command.
    TZPUCtrl.regCmdStatus   = 0x00;   // Default for the CPLD Command Status.
    TZPUCtrl.regCpuCfg      = 0x00;   // Not used, as no FPGA available, but need to store/return value if addressed.
    TZPUCtrl.regCpuInfo     = 0x00;   // Not used, as no FPGA available, but need to store/return value if addressed.
    // Setup the CPLD status value, this is used by the host for configuration of tzfs.
  #if(TARGET_HOST_MZ80A == 1)
    TZPUCtrl.regCpldInfo    = (CPLD_VERSION << 4) | (CPLD_HAS_FPGA_VIDEO << 3) | HWMODE_MZ80A;
  #endif
  #if(TARGET_HOST_MZ700 == 1)
    TZPUCtrl.regCpldInfo    = (CPLD_VERSION << 4) | (CPLD_HAS_FPGA_VIDEO << 3) | HWMODE_MZ700;
  #endif
  #if(TARGET_HOST_MZ2000 == 1)
    TZPUCtrl.regCpldInfo    = (CPLD_VERSION << 4) | (CPLD_HAS_FPGA_VIDEO << 3) | HWMODE_MZ2000;
  #endif
    TZPUCtrl.regCpldCfg     = 0x00;   // Not used, as no CPLD available, but need to store/return value if addressed.

    // Go through and clear all memory maps, valid for startup and reset.
    for(idx=0; idx < MEMORY_MODES; idx++)
    {
        if(Z80Ctrl->page[idx] != NULL)
        {
            kfree(Z80Ctrl->page[idx]);
            Z80Ctrl->page[idx] = NULL;
        }
    }

    // Setup all initial TZFS memory modes
    tzpuWrite(IO_TZ_CTRLLATCH, TZMM_ORIG,  1);
    tzpuWrite(IO_TZ_CTRLLATCH, TZMM_TZFS,  1);
    tzpuWrite(IO_TZ_CTRLLATCH, TZMM_TZFS2, 1);
    tzpuWrite(IO_TZ_CTRLLATCH, TZMM_TZFS3, 1);
    tzpuWrite(IO_TZ_CTRLLATCH, TZMM_TZFS4, 1);
    Z80Ctrl->memoryMode     = 0x02;   // Default memory mode, MZ-80A.

    // I/O Ports on the tranZPUter SW board. All hosts have the same ports for the tzpu board.
    for(idx=0x0000; idx < 0x10000; idx+=0x0100)
    {
        Z80Ctrl->iopage[idx+IO_TZ_CTRLLATCH]  = IO_TZ_CTRLLATCH  | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_SETXMHZ]    = IO_TZ_SETXMHZ    | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_SET2MHZ]    = IO_TZ_SET2MHZ    | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_CLKSELRD]   = IO_TZ_CLKSELRD   | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_SVCREQ]     = IO_TZ_SVCREQ     | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_SYSREQ]     = IO_TZ_SYSREQ     | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_CPLDCMD]    = IO_TZ_CPLDCMD    | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_CPLDSTATUS] = IO_TZ_CPLDSTATUS | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_CPUCFG]     = IO_TZ_CPUCFG     | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_CPUSTATUS]  = IO_TZ_CPUSTATUS  | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_CPUINFO]    = IO_TZ_CPUINFO    | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_CPLDCFG]    = IO_TZ_CPLDCFG    | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_CPLDINFO]   = IO_TZ_CPLDINFO   | IO_TYPE_VIRTUAL_HW;
    }

    pr_info("TZPU Memory Setup complete.\n");
}
