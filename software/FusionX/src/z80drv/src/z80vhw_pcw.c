/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80vhw_pcw.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Virtual Hardware Driver - Amstrad PCW-8xxx/PCW-9xxx
//                  This file contains the methods used to emulate the Amstrad PCW specific
//                  hardware.
//
//                  These drivers are intended to be instantiated inline to reduce overhead of a call
//                  and as such, they are included like header files rather than C linked object files.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2023 v1.0 - Initial write based on the RFS hardware module.
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

// Device constants.
#define RAM_BASE_ADDR                         0x00000                                   // Base address of the 512K RAM.

// IO Ports.
#define IO_FDC_STATUS                         0x00                                      // NEC765 FDC Status Register.
#define IO_FDC_DATA                           0x01                                      // NEC765 FDC Data Register.
#define IO_MEMBNK0                            0xF0                                      // Memory bank 0000:3FFF register.
#define IO_MEMBNK1                            0xF1                                      // Memory bank 4000:7FFF register.
#define IO_MEMBNK2                            0xF2                                      // Memory bank 8000:BFFF register.
#define IO_MEMBNK3                            0xF3                                      // Memory bank C000:FFFF register.
#define IO_MEMLOCK                            0xF4                                      // CPC mode memory lock range.
#define IO_ROLLERRAM                          0xF5                                      // Set the Roller RAM address.
#define IO_VORIGIN                            0xF6                                      // Set screen vertical origin.
#define IO_SCREENATTR                         0xF7                                      // Set screen attributes.
#define IO_GACMD                              0xF8                                      // Gatearray command register.
#define IO_GASTATUS                           0xF8                                      // Gatearray status register.

// The boot code for the PCW-8256 is located within the printer controller. To avoid special hardware within the CPLD, this code is incorporated
// into this module for rapid loading into RAM.
#define ROM_DIR                               "/apps/FusionX/host/PCW/roms/"
#define ROM_PCW8_BOOT_FILENAME                 ROM_DIR "PCW8256_boot.bin"
#define ROM_PCW9_BOOT_FILENAME                 ROM_DIR "PCW9256_boot.bin"

// Boot ROM rom load and size definitions. 
#define ROM_BOOT_LOAD_ADDR                    0x000000
#define ROM_BOOT_SIZE                         275

// PCW control.
typedef struct {
    uint8_t                                   regMemBank0;                              // Mirror of register F0, memory block select 0x0000-0x3FFF.
    uint8_t                                   regMemBank1;                              // Mirror of register F1, memory block select 0x4000-0x7FFF.
    uint8_t                                   regMemBank2;                              // Mirror of register F2, memory block select 0x8000-0xBFFF.
    uint8_t                                   regMemBank3;                              // Mirror of register F3, memory block select 0xC000-0xFFFF.
    uint8_t                                   regCPCPageMode;                           // Mirror of the CPC paging lock register F4.
    uint8_t                                   regRollerRAM;                             // Mirror of Roller-RAM address register.
    uint8_t                                   regCtrl;                                  // Control register.
} t_PCWCtrl;

// RFS Board control.
static t_PCWCtrl PCWCtrl;

//-------------------------------------------------------------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------------------------------------------------------------

// Method to setup the memory page config to reflect the PCW configuration.
void pcwSetupMemory(enum Z80_MEMORY_PROFILE mode)
{
    // Locals.
    uint32_t    idx;

    // The PCW contains upto 512KB of standard RAM which can be expnded to a physical max of 2MB. The kernel malloc limit is 2MB so the whole virtual
    // memory can be mapped into the PCW memory address range.

    // Setup defaults.
    PCWCtrl.regMemBank0     = 0x00;
    PCWCtrl.regMemBank1     = 0x01;
    PCWCtrl.regMemBank2     = 0x02;
    PCWCtrl.regMemBank3     = 0x03;     // Keyboard is in locations 0x3FF0 - 0x3FFF of this memory block.
    PCWCtrl.regCPCPageMode  = 0x00;
    PCWCtrl.regRollerRAM    = 0x00;
    PCWCtrl.regCtrl         = 0x00;

    // Initialise the page pointers and memory to reflect a PCW, lower 128K is used by video logic so must always be accessed in hardware.
    for(idx=0x0000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
    {
        if(idx >= 0x0000 && idx < 0xFFF0)
        {
            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM_WT, (RAM_BASE_ADDR+idx));
        }
        if(idx >= 0xFFF0 && idx < 0x10000)
        {
            // The keyboard is memory mapped into upper bytes of block 3.
            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW, (RAM_BASE_ADDR+idx));
        }   
    }
    for(idx=0x0000; idx < IO_PAGE_SIZE; idx++)
    {
        Z80Ctrl->iopage[idx] = idx | IO_TYPE_PHYSICAL_HW;
    }
  
    // Enable refresh as using virtual RAM stops refresh of host DRAM.
    Z80Ctrl->refreshDRAM = 2;

    // No I/O Ports on the RFS board.
    pr_info("PCW Memory Setup complete.\n");
}

// Method to load a ROM image into the RAM memory.
//
uint8_t loadROM(const char* romFileName, uint32_t loadAddr, uint32_t loadSize)
{
    // Locals.
    uint8_t     result = 0;
    long        noBytes;
    struct file *fp;

    fp = filp_open(romFileName, O_RDONLY, 0);
    if(IS_ERR(fp))
    {
        pr_info("Error opening ROM Image:%s\n:", romFileName);
        result = 1;
    } else
    {
        vfs_llseek(fp, 0, SEEK_SET);
        noBytes = kernel_read(fp, fp->f_pos, &Z80Ctrl->ram[loadAddr], loadSize);
        if(noBytes < loadSize)
        {
            pr_info("Short load, ROM Image:%s, bytes loaded:%08x\n:", romFileName, loadSize);
        }
        filp_close(fp,NULL);
    }

    return(result);
}

// Perform any setup operations, such as variable initialisation, to enable use of this module.
void pcwInit(uint8_t mode)
{
    // Locals.
    //
    uint32_t  idx;

    // Clear memory as previous use or malloc can leave it randomly set.
    for(idx=0; idx < Z80_VIRTUAL_RAM_SIZE; idx++)
    {
        Z80Ctrl->ram[idx] = 0x00;
    }

    // Disable boot mode, we dont need to fetch the boot rom as we preload it.
    SPI_SEND32( (0x00F8 << 16) | (0x00 << 8) | CPLD_CMD_WRITEIO_ADDR);

    // Load boot ROM.
    loadROM(mode == 0 ? ROM_PCW8_BOOT_FILENAME : ROM_PCW9_BOOT_FILENAME, ROM_BOOT_LOAD_ADDR, ROM_BOOT_SIZE);
   
    // Reset.
    //SPI_SEND32( (0x00F8 << 16) | (0x01 << 8) | CPLD_CMD_WRITEIO_ADDR);

    // First two bytes to NULL as were not using the bootstrap and normal operations after bootstrap would disable the mode.
    Z80Ctrl->ram[0] = 0x00;
    Z80Ctrl->ram[1] = 0x00;

    // Initial memory config.
    pcwSetupMemory(Z80Ctrl->defaultPageMode);

    pr_info("Enabling PCW-%s driver.\n", mode == 0 ? "8256" : "9256");
    return;
}

// Perform any de-initialisation when the driver is removed.
void pcwRemove(void)
{
    pr_info("Removing PCW driver.\n");
    return;
}

// Method to decode an address and make any system memory map changes as required.
//
static inline void pcwDecodeMemoryMapSetup(zuint16 address, zuint8 data, uint8_t ioFlag, uint8_t readFlag)
{
    // Locals.
    uint32_t    idx;

    // IO Switch.
    if(ioFlag)
    {
        switch(address&0xff)
        {
            case IO_FDC_STATUS:
//pr_info("FDC_STATUS:%02x\n", data);
                break;

            case IO_FDC_DATA:
//pr_info("FDC_DATA:%02x\n", data);
                break;

            case IO_MEMBNK0:
                if(!readFlag)
                {
                    PCWCtrl.regMemBank0 = (data & 0x80) ? data & 0x7f : PCWCtrl.regMemBank0;
    pr_info("Setting Bank 0:%02x\n", PCWCtrl.regMemBank0);
                    for(idx=0x0000; idx < 0x4000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        setMemoryType((idx+0x0000)/MEMORY_BLOCK_GRANULARITY, PCWCtrl.regMemBank0 >= 8 ? MEMORY_TYPE_VIRTUAL_RAM : MEMORY_TYPE_PHYSICAL_RAM_WT, (RAM_BASE_ADDR+(PCWCtrl.regMemBank0*16384)+idx));
                    }
                }
                break;

            case IO_MEMBNK1:
                if(!readFlag)
                {
                    PCWCtrl.regMemBank1 = (data & 0x80) ? data & 0x7f : PCWCtrl.regMemBank1;
    pr_info("Setting Bank 1:%02x\n", PCWCtrl.regMemBank1);
                    for(idx=0x0000; idx < 0x4000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        setMemoryType((idx+0x4000)/MEMORY_BLOCK_GRANULARITY, PCWCtrl.regMemBank1 >= 8 ? MEMORY_TYPE_VIRTUAL_RAM : MEMORY_TYPE_PHYSICAL_RAM_WT, (RAM_BASE_ADDR+(PCWCtrl.regMemBank1*16384)+idx));
                    }
                }
                break;

            case IO_MEMBNK2:
                if(!readFlag)
                {
                    PCWCtrl.regMemBank2 = (data & 0x80) ? data & 0x7f : PCWCtrl.regMemBank2;
    pr_info("Setting Bank 2:%02x\n", PCWCtrl.regMemBank2);
                    for(idx=0x0000; idx < 0x4000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        setMemoryType((idx+0x8000)/MEMORY_BLOCK_GRANULARITY, PCWCtrl.regMemBank2 >= 8 ? MEMORY_TYPE_VIRTUAL_RAM : MEMORY_TYPE_PHYSICAL_RAM_WT, (RAM_BASE_ADDR+(PCWCtrl.regMemBank2*16384)+idx));
                    }
                }
                break;

            case IO_MEMBNK3:
                if(!readFlag)
                {
                    PCWCtrl.regMemBank3 = (data & 0x80) ? data & 0x7f : PCWCtrl.regMemBank3;
    pr_info("Setting Bank 3:%02x\n", PCWCtrl.regMemBank3);
                    for(idx=0x0000; idx < 0x4000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        if(idx < 0x3FF0)
                            setMemoryType((idx+0xC000)/MEMORY_BLOCK_GRANULARITY, PCWCtrl.regMemBank3 >= 8 ? MEMORY_TYPE_VIRTUAL_RAM : MEMORY_TYPE_PHYSICAL_RAM_WT, (RAM_BASE_ADDR+(PCWCtrl.regMemBank3*16384)+idx));
                    }
                }
                break;

            case IO_MEMLOCK:
                if(!readFlag)
                {
pr_info("MEMLOCK:%02x\n", data);
                    PCWCtrl.regCPCPageMode = data;
                }
                break;

            case IO_ROLLERRAM:
                if(!readFlag)
                {
pr_info("********RollerRAM********:%02x => %04x\n", data, (((data >> 5)&0x7) * 16384)+((data&0x1f)*512));
                    PCWCtrl.regRollerRAM = data;
                }
                break;

            case IO_VORIGIN:
pr_info("VORIGIN:%02x\n", data);
                break;
            case IO_SCREENATTR:
pr_info("SCREENATTR:%02x\n", data);
                break;
            case IO_GACMD:
//pr_info("GACMD:%02x\n", data);
                break;

            default:
pr_info("Unknown:ADDR:%02x,%02x\n", address&0xff, data);
                break;
        }
    } else
    // Memory map switch.
    {
        switch(address)
        {
            default:
                break;
        }
    }
}

// Method to read from either the memory mapped registers if enabled else the RAM.
static inline uint8_t pcwRead(zuint16 address, uint8_t ioFlag)
{
    // Locals.
    uint8_t     data = 0xFF;

    // I/O Operation?
    if(ioFlag)
    {
        switch(address)
        {

            default:
                break;
        }
    } else
    {
        switch(address)
        {
            default:
                // Return the contents of the ROM at given address.
                data = isVirtualROM(address) ? readVirtualROM(address) : readVirtualRAM(address);
                break;
        }
    }

  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 3) pr_info("PCW-Read:%04x, BK0:%02x, BK1:%02x, BK2:%02x, BK3:%02x, CTRL:%02x\n", address, PCWCtrl.regMemBank0, PCWCtrl.regMemBank1, PCWCtrl.regMemBank2, PCWCtrl.regMemBank3, PCWCtrl.regCtrl);
  #endif
    return(data);
}

// Method to handle writes.
static inline void pcwWrite(zuint16 address, zuint8 data, uint8_t ioFlag)
{
    // Locals.
  //  uint32_t     idx;


    // I/O Operation?
    if(ioFlag)
    {
        switch(address)
        {
            default:
                break;
        }
    } else
    {
        switch(address)
        {
            default:
                // Any unprocessed write is commited to RAM.
                writeVirtualRAM(address, data);
                break;
        }
    }
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 3) pr_info("PCW-Write:%04x, BK0:%02x, BK1:%02x, BK2:%02x, BK3:%02x, CTRL:%02x\n", address, PCWCtrl.regMemBank0, PCWCtrl.regMemBank1, PCWCtrl.regMemBank2, PCWCtrl.regMemBank3, PCWCtrl.regCtrl);
  #endif
    return;
}
