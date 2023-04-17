/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80vhw_mz2000.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Virtual Hardware Driver - MZ-2000
//                  This file contains the methods used to emulate the original Sharp MZ-2000 without
//                  any additions, such as the RFS or TZFS boards.
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

// System ROM's, either use the host machine ROM or preload a ROM image.
#define ROM_DIR                               "/apps/FusionX/host/MZ-2000/ROMS/"
#define ROM_IPL_ORIG_FILENAME                  ROM_DIR "mz2000_ipl.orig"

// Boot ROM rom load and size definitions. 
#define ROM_BOOT_LOAD_ADDR                    0x000000
#define ROM_BOOT_SIZE                         0x800

// PCW control.
typedef struct {
    uint8_t                                   lowMemorySwap;                            // Boot mode lower memory is swapped to 0x8000:0xFFFF
    uint8_t                                   highMemoryVRAM;                           // Flag to indicate high memory range 0xD000:0xFFFF is assigned to VRAM.
    uint8_t                                   graphicsVRAM;                             // Flag to indicate graphics VRAM selected, default is character VRAM (0).
    uint8_t                                   regCtrl;                                  // Control register.
} t_MZ2000Ctrl;

// RFS Board control.
static t_MZ2000Ctrl MZ2000Ctrl;

//-------------------------------------------------------------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------------------------------------------------------------

// Method to setup the memory page config to reflect the PCW configuration.
void mz2000SetupMemory(enum Z80_MEMORY_PROFILE mode)
{
    // Locals.
    uint32_t    idx;

    // The PCW contains upto 512KB of standard RAM which can be expnded to a physical max of 2MB. The kernel malloc limit is 2MB so the whole virtual
    // memory can be mapped into the PCW memory address range.

    // Setup defaults.
    MZ2000Ctrl.lowMemorySwap   = 0x01;   // Set memory swap flag to swapped, ie. IPL mode sees DRAM 0x0000:0x7FFF swapped to 0x8000:0xFFFF and ROM pages into 0x0000.
    MZ2000Ctrl.highMemoryVRAM  = 0x00;
    MZ2000Ctrl.graphicsVRAM    = 0x00;
    MZ2000Ctrl.regCtrl         = 0x00;

    // Setup default mode according to run mode, ie. Physical run or Virtual run.
    //
    if(mode == USE_PHYSICAL_RAM)
    {
        // Initialise the page pointers and memory to use physical RAM.
        for(idx=0x0000; idx < MEMORY_PAGE_SIZE; idx+=MEMORY_BLOCK_GRANULARITY)
        {
            if(idx >= 0 && idx < 0x8000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_ROM, idx);
            }
            else //if(idx >= 0x8000 && idx < 0xD000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
            }
           
            // Video RAM labelled as HW as we dont want to cache it.
            //else if(idx >= 0xD000 && idx < 0xE000)
           // {
           //     setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM, idx);
            //} else
           // {
           //     setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
           // }
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
        // MZ-2000 comes up in IPL mode where lower 32K is ROM and upper 32K is RAM remapped from 0x0000.
        for(idx=0x0000; idx < MEMORY_PAGE_SIZE; idx+=MEMORY_BLOCK_GRANULARITY)
        {
            if(idx >= 0 && idx < 0x8000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, idx);
            }
            else 
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, (MZ2000Ctrl.lowMemorySwap ? idx - 0x8000 : idx));
            }
        }
        for(idx=0x0000; idx < IO_PAGE_SIZE; idx++)
        {
            Z80Ctrl->iopage[idx] = idx | IO_TYPE_PHYSICAL_HW;
        }
        // Enable refresh as using virtual RAM stops refresh of host DRAM.
        Z80Ctrl->refreshDRAM = 2;
    }

    pr_info("MZ-2000 Memory Setup complete.\n");
}

// Method to load a ROM image into the RAM memory.
//
uint8_t mz2000LoadROM(const char* romFileName, uint32_t loadAddr, uint32_t loadSize)
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
void mz2000Init(uint8_t mode)
{
    // Locals.
    uint32_t  idx;
    
    // Initialise the virtual RAM from the HOST DRAM. This is to maintain compatibility as some applications (in my experience) have 
    // bugs, which Im putting down to not initialising variables. The host DRAM is in a pattern of 0x00..0x00, 0xFF..0xFF repeating
    // when first powered on.
    pr_info("Sync Host RAM to virtual RAM.\n");
    for(idx=0; idx < Z80_VIRTUAL_RAM_SIZE; idx++)
    {
        // Lower memory is actually upper on startup, but ROM paged in, so set to zero.
        if(idx >= 0x0000 && idx < 0x8000)
        {
            Z80Ctrl->ram[idx+0x8000] = 0x00;
        } else
        // Lower memory is paged in at 0x8000:0xFFFF
        if(idx >= 0x8000 && idx < 0x10000)
        {
            SPI_SEND32((uint32_t)idx << 16 | CPLD_CMD_READ_ADDR);
            while(CPLD_READY() == 0);
            Z80Ctrl->ram[idx-0x8000] = z80io_PRL_Read8(1);
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
        if(idx >= 0x0000 && idx < 0x8000)
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
    mz2000SetupMemory(Z80Ctrl->defaultPageMode);

    // mz2000LoadROM(ROM_IPL_ORIG_FILENAME, ROM_BOOT_LOAD_ADDR, ROM_BOOT_SIZE);

    pr_info("Enabling MZ-2000 driver.\n");
    return;
}

// Perform any de-initialisation when the driver is removed.
void mz2000Remove(void)
{
    pr_info("Removing MZ-2000 driver.\n");
    return;
}

// Method to decode an address and make any system memory map changes as required.
//
static inline void mz2000DecodeMemoryMapSetup(zuint16 address, zuint8 data, uint8_t ioFlag, uint8_t readFlag)
{
    // Locals.
    uint32_t    idx;

    // Decoding memory address or I/O address?
    if(ioFlag == 0)
    {
        // Certain machines have memory mapped I/O, these need to be handled in-situ as some reads may change the memory map.
        // These updates are made whilst waiting for the CPLD to retrieve the requested byte.
        //
        switch(address)
        {
            default:
                break;
        }
    } else
    {
        // Determine if this is a memory management port and update the memory page if required.
        switch(address & 0x00FF)
        {
            // 8255 - Port A
            case IO_ADDR_E0:
                break;

            // 8255 - Port B
            case IO_ADDR_E1:
                break;

            // 8255 - Port C
            // Bit 3 - L = Reset and enter IPL mode.
            // Bit 1 - H = Set memory to normal state and reset cpu, RAM 0x0000:0xFFFF, L = no change.
            case IO_ADDR_E2:
                if(data & 0x01)
                    data = 0x03;
                else if((data & 0x08) == 0)
                    data = 0x06;
                else
                    break;

            // 8255 - Control Port
            // Bit 7 - H = Control word, L 3:1 define port C bit, bit 0 defines its state.
            case IO_ADDR_E3:
//pr_info("E3:%02x\n", data);
                // Program control register.
                if(data & 0x80)
                {
                    // Do nothing, this is the register which sets the 8255 mode.
                } else
                {
                    switch((data >> 1) & 0x07)
                    {
                        // NST toggle.
                        case 1:
                            // NST pages in all RAM and resets cpu.
                            if(data & 0x01)
                            {
                                MZ2000Ctrl.lowMemorySwap = 0;
                                for(idx=0x0000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                                {
                                    if(Z80Ctrl->defaultPageMode == USE_PHYSICAL_RAM)
                                    {
                                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
                                    }
                                    else
                                    {
                                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, idx);
                                    }
                                }
                                //resetZ80();
                            }
                            break;

                        // IPL start.
                        case 3:
                            // If IPL is active (L), reconfigure memory for power on state.
                            if((data & 0x01) == 0)
                            {
                                mz2000SetupMemory(Z80Ctrl->defaultPageMode);
                            }
                            break;

                        default:
                            break;
                    }
                }
                break;

            // Port A - Z80 PIO, contains control bits affecting memory mapping.
            // Bit
            // 7   - Assign address range 0xD000:0xFFFF to V-RAM when H, when L assign RAM
            // 6   - Character VRAM (H), Graphics VRAM (L)
            // 4   - Change screen to 80 Char (H), 40 Char (L)
            // NB. When the VRAM is paged in, if Character VRAM is selected, range 0xD000:0xD7FF is VRAM, 0xC000:0xCFFF, 0xE000:0xFFFF is RAM.
            case IO_ADDR_E8:
                // High memory being assigned to VRAM or reverting?
                if(MZ2000Ctrl.highMemoryVRAM && (data & 0x80) == 0)
                {
                    for(idx=0xC000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        if(Z80Ctrl->defaultPageMode == USE_PHYSICAL_RAM)
                        {
                            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
                        } else
                        {
                            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, (MZ2000Ctrl.lowMemorySwap ? idx - 0x8000 : idx));
                        }
                    }
                    MZ2000Ctrl.highMemoryVRAM = 0;
                } else
                // If this is the first activation of the VRAM or the state of it changes, ie. character <-> graphics, then update the memory mapping.
                if( (!MZ2000Ctrl.highMemoryVRAM && (data & 0x80) != 0) || (MZ2000Ctrl.highMemoryVRAM  && (MZ2000Ctrl.graphicsVRAM >> 6) != (data & 0x40)) )
                {
                    for(idx=0xC000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        // Graphics RAM see's the entire range set to PHYSICAL, Character RAM only see's 0xD000:0xD7FF set to PHYSICAL.
                        if( ((data & 0x40) && (idx >= 0xD000 && idx < 0xD800)) || ((data & 0x40) == 0) )
                        {
                            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM, idx);
                        }
                    }
                    MZ2000Ctrl.highMemoryVRAM = 1;
                }
                MZ2000Ctrl.graphicsVRAM = (data & 0x040) ? 1 : 0;
                break;

            // Port is not a memory management port.
            default:
                break;
        }
    }
}

// Method to read from either the memory mapped registers if enabled else the RAM.
static inline uint8_t mz2000Read(zuint16 address, uint8_t ioFlag)
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
static inline void mz2000Write(zuint16 address, zuint8 data, uint8_t ioFlag)
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
