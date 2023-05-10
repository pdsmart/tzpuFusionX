/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80vhw_mz700.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Virtual Hardware Driver - MZ-700
//                  This file contains the methods used to emulate the original Sharp MZ-700 without
//                  any additions, such as the RFS or TZFS boards.
//
//                  These drivers are intended to be instantiated inline to reduce overhead of a call
//                  and as such, they are included like header files rather than C linked object files.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2023 v1.0 - Initial write based on the RFS hardware module.
//                  Apr 2023 v1.1 - Updates from the PCW/MZ2000 changes.
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

// PCW control.
typedef struct {
    uint8_t                                   regCtrl;                                  // Control register.
} t_MZ700Ctrl;

// RFS Board control.
static t_MZ700Ctrl MZ700Ctrl;

//-------------------------------------------------------------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------------------------------------------------------------

// Method to setup the memory page config to reflect the PCW configuration.
void mz700SetupMemory(enum Z80_MEMORY_PROFILE mode)
{
    // Locals.
    uint32_t    idx;

    // Setup defaults.
    MZ700Ctrl.regCtrl         = 0x00;

    // Setup default mode according to run mode, ie. Physical run or Virtual run.
    //
    if(mode == USE_PHYSICAL_RAM)
    {
        // Initialise the page pointers and memory to use physical RAM.
        for(idx=0x0000; idx < MEMORY_PAGE_SIZE; idx+=MEMORY_BLOCK_GRANULARITY)
        {
            if(idx >= 0 && idx < 0x1000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_ROM, idx);
            }
            else if(idx >= 0x1000 && idx < 0xD000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
            }
            else if(idx >= 0xD000 && idx < 0xE000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM, idx);
            }
            else if(idx >= 0xE000 && idx < 0xE800)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW, idx);
            }
            else if(idx >= 0xE800 && idx < 0x10000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_ROM, idx);
            } else
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
            }
        }
        for(idx=0x0000; idx < IO_PAGE_SIZE; idx++)
        {
            Z80Ctrl->iopage[idx] = idx | IO_TYPE_PHYSICAL_HW;
        }
        // Cancel refresh as using physical RAM for program automatically refreshes DRAM.
        Z80Ctrl->refreshDRAM = 0;
    }
    else if(mode == USE_VIRTUAL_RAM)
    {
        // Initialise the page pointers and memory to use virtual RAM.
        for(idx=0x0000; idx < MEMORY_PAGE_SIZE; idx+=MEMORY_BLOCK_GRANULARITY)
        {
            if(idx >= 0 && idx < 0x1000)
            {
                setMemoryType((idx/MEMORY_BLOCK_GRANULARITY), MEMORY_TYPE_VIRTUAL_ROM, idx);
            }
            else if(idx >= 0x1000 && idx < 0xD000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY,   MEMORY_TYPE_VIRTUAL_RAM, idx);
            }
            else if(idx >= 0xD000 && idx < 0xE000)
            {
                setMemoryType((idx/MEMORY_BLOCK_GRANULARITY), MEMORY_TYPE_PHYSICAL_VRAM, idx);
            }
            else if(idx >= 0xE000 && idx < 0xE800)
            {
                setMemoryType((idx/MEMORY_BLOCK_GRANULARITY), MEMORY_TYPE_PHYSICAL_HW, idx);
            }
            else if(idx >= 0xE800 && idx < 0xF000)
            {
                setMemoryType((idx/MEMORY_BLOCK_GRANULARITY), MEMORY_TYPE_VIRTUAL_ROM, idx);
            }
            else if(idx >= 0xF000 && idx < 0x10000)
            {
                setMemoryType((idx/MEMORY_BLOCK_GRANULARITY), MEMORY_TYPE_VIRTUAL_ROM, idx);
            }
        }
        for(idx=0x0000; idx < IO_PAGE_SIZE; idx++)
        {
            Z80Ctrl->iopage[idx] = idx | IO_TYPE_PHYSICAL_HW;
        }
        // Enable refresh as using virtual RAM stops refresh of host DRAM.
        Z80Ctrl->refreshDRAM = 2;
    }
  
    // Reset memory paging to default.
    SPI_SEND_32(0x00e4, 0x00 << 8 | CPLD_CMD_WRITEIO_ADDR);

    pr_info("MZ-700 Memory Setup complete.\n");
}

// Method to load a ROM image into the RAM memory.
//
uint8_t mz700LoadROM(const char* romFileName, uint32_t loadAddr, uint32_t loadSize)
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
void mz700Init(uint8_t mode)
{
    // Locals.
    uint32_t  idx;
    
    // Reset memory paging to default.
    SPI_SEND_32(0x00e4, 0x00 << 8 | CPLD_CMD_WRITEIO_ADDR);

    // Initialise the virtual RAM from the HOST DRAM. This is to maintain compatibility as some applications (in my experience) have 
    // bugs, which Im putting down to not initialising variables. The host DRAM is in a pattern of 0x00..0x00, 0xFF..0xFF repeating
    // when first powered on.    
    pr_info("Sync Host RAM to virtual RAM.\n");
    for(idx=0; idx < Z80_VIRTUAL_RAM_SIZE; idx++)
    {
        if(idx >= 0x1000 && idx < 0xD000)
        {
            SPI_SEND32((uint32_t)idx << 16 | CPLD_CMD_READ_ADDR);
            while(CPLD_READY() == 0);
            Z80Ctrl->ram[idx] = z80io_PRL_Read8(1);
        } else
        {
            Z80Ctrl->ram[idx] = 0x00;
        }
    }

    // Original mode, ie. no virtual devices active, copy the host BIOS into the Virtual ROM and initialise remainder of ROM memory
    // such that the host behaves as per original spec.
    pr_info("Sync Host BIOS to virtual ROM.\n");
    for(idx=0; idx < Z80_VIRTUAL_ROM_SIZE; idx++)
    {
        // Copy BIOS and any add-on ROMS.
        if((idx >= 0x0000 && idx < 0x1000) || (idx >= 0xE800 && idx < 0x10000))
        {
            SPI_SEND32((uint32_t)idx << 16 | CPLD_CMD_READ_ADDR);
            while(CPLD_READY() == 0);
            Z80Ctrl->rom[idx] = z80io_PRL_Read8(1);
        } else
        {
            Z80Ctrl->rom[idx] = 0x00;
        }
    }

    // Initial memory config.
    mz700SetupMemory(Z80Ctrl->defaultPageMode);

    // Add in a test program to guage execution speed.
    Z80Ctrl->ram[0x1200] = 0x01;
    Z80Ctrl->ram[0x1201] = 0x86;
    Z80Ctrl->ram[0x1202] = 0xf2;
    Z80Ctrl->ram[0x1203] = 0x3e;
    Z80Ctrl->ram[0x1204] = 0x15;
    Z80Ctrl->ram[0x1205] = 0x3d;
    Z80Ctrl->ram[0x1206] = 0x20;
    Z80Ctrl->ram[0x1207] = 0xfd;
    Z80Ctrl->ram[0x1208] = 0x0b;
    Z80Ctrl->ram[0x1209] = 0x78;
    Z80Ctrl->ram[0x120a] = 0xb1;
    Z80Ctrl->ram[0x120b] = 0x20;
    Z80Ctrl->ram[0x120c] = 0xf6;
    Z80Ctrl->ram[0x120d] = 0xc3;
    Z80Ctrl->ram[0x120e] = 0x00;
    Z80Ctrl->ram[0x120f] = 0x00;
   
    pr_info("Enabling MZ-700 driver.\n");
    return;
}

// Perform any de-initialisation when the driver is removed.
void mz700Remove(void)
{
    pr_info("Removing MZ-700 driver.\n");
    return;
}

// Method to decode an address and make any system memory map changes as required.
//
static inline void mz700DecodeMemoryMapSetup(zuint16 address, zuint8 data, uint8_t ioFlag, uint8_t readFlag)
{
    // Locals.
    uint32_t    idx;

    // Decoding memory address or I/O address?
    if(ioFlag == 0)
    {
   //   #if(DEBUG_ENABLED & 1)
   //     if(Z80Ctrl->debug >= 2)
   //     {
   //         pr_info("MEM:%04x,%02x,%d,%d\n", address, data, ioFlag, readFlag);
   //     }
   //   #endif
        // Certain machines have memory mapped I/O, these need to be handled in-situ as some reads may change the memory map.
        // These updates are made whilst waiting for the CPLD to retrieve the requested byte.
        //
        // 0000 - 0FFF : MZ80K/A/700   = Monitor ROM or RAM (MZ80A rom swap)
        // 1000 - CFFF : MZ80K/A/700   = RAM
        // C000 - CFFF : MZ80A         = Monitor ROM (MZ80A rom swap)
        // D000 - D7FF : MZ80K/A/700   = VRAM
        // D800 - DFFF : MZ700         = Colour VRAM (MZ700)
        // E000 - E003 : MZ80K/A/700   = 8255       
        // E004 - E007 : MZ80K/A/700   = 8254
        // E008 - E00B : MZ80K/A/700   = LS367
        // E00C - E00F : MZ80A         = Memory Swap (MZ80A)
        // E010 - E013 : MZ80A         = Reset Memory Swap (MZ80A)
        // E014        : MZ80A/700     = Normat CRT display
        // E015        : MZ80A/700     = Reverse CRT display
        // E200 - E2FF : MZ80A/700     = VRAM roll up/roll down.
        // E800 - EFFF : MZ80K/A/700   = User ROM socket or DD Eprom (MZ700)
        // F000 - F7FF : MZ80K/A/700   = Floppy Disk interface.
        // F800 - FFFF : MZ80K/A/700   = Floppy Disk interface.
        switch(address)
        {
            default:
                break;
        }
    } else
    {
   //   #if(DEBUG_ENABLED & 1)
   //     if(Z80Ctrl->debug >= 2)
   //     {
   //         pr_info("IO:%04x,%02x,%d,%d\n", address, data, ioFlag, readFlag);
   //     }
   //   #endif

        // Determine if this is a memory management port and update the memory page if required.
        switch(address & 0x00FF)
        {
            //  MZ700 memory mode switch.
            // 
            //              MZ-700                                                     
            //             |0000:0FFF|1000:CFFF|D000:FFFF          
            //             ------------------------------          
            //  OUT 0xE0 = |DRAM     |         |                   
            //  OUT 0xE1 = |         |         |DRAM               
            //  OUT 0xE2 = |MONITOR  |         |                   
            //  OUT 0xE3 = |         |         |Memory Mapped I/O  
            //  OUT 0xE4 = |MONITOR  |DRAM     |Memory Mapped I/O  
            //  OUT 0xE5 = |         |         |Inhibit            
            //  OUT 0xE6 = |         |         |<return>           
            // 
            //  <return> = Return to the state prior to the complimentary command being invoked.
            // Enable lower 4K block as DRAM
            case IO_ADDR_E0:
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, idx);
                }
                break;

            // Enable upper 12K block, including Video/Memory Mapped peripherals area, as DRAM.
            case IO_ADDR_E1:
                if(!Z80Ctrl->inhibitMode)
                {
                    for(idx=0xD000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        // MZ-700 mode we only work in first 64K block.
                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, idx);
                    }
                }
                break;
              
            // Enable MOnitor ROM in lower 4K block
            case IO_ADDR_E2:
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, idx);
                }
                break;
               
            // Enable Video RAM and Memory mapped peripherals in upper 12K block.
            case IO_ADDR_E3:
                if(!Z80Ctrl->inhibitMode)
                {
                    for(idx=0xD000; idx < 0xE000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM, idx);
                    }
                    for(idx=0xE000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW, idx);
                    }
                }
                break;

            // Reset to power on condition memory map.
            case IO_ADDR_E4:
                // Lower 4K set to Monitor ROM.
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, idx);
                }
                if(!Z80Ctrl->inhibitMode)
                {
                    // Upper 12K to hardware.
                    for(idx=0xD000; idx < 0xE000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM, idx);
                    }
                    for(idx=0xE000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW, idx);
                    }
                }
                break;

            // Inhibit. Backup current page data in region 0xD000-0xFFFF and inhibit it.
            case IO_ADDR_E5:
                for(idx=0xD000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    backupMemoryType(idx/MEMORY_BLOCK_GRANULARITY);
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_INHIBIT, idx);
                }
                Z80Ctrl->inhibitMode = 1;
                break;

            // Restore D000-FFFF to its original state.
            case IO_ADDR_E6:
                for(idx=0xD000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    restoreMemoryType(idx/MEMORY_BLOCK_GRANULARITY);
                }
                Z80Ctrl->inhibitMode = 0;
                break;

            // Port is not a memory management port.
            default:
                break;
        }
    }
}

// Method to read from either the memory mapped registers if enabled else the RAM.
static inline uint8_t mz700Read(zuint16 address, uint8_t ioFlag)
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
                if(isVirtualMemory(address))
                {
                    // Retrieve data from virtual memory.
                    data = isVirtualROM(address) ? readVirtualROM(address) : readVirtualRAM(address);
                }
                break;
        }
    }

    return(data);
}

// Method to handle writes.
static inline void mz700Write(zuint16 address, zuint8 data, uint8_t ioFlag)
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
                if(isVirtualRAM(address))
                {
                    // Update virtual memory.
                    writeVirtualRAM(address, data);
                }
        }
    }
    return;
}
