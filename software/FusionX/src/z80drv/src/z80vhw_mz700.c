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
// History:         Mar 2023 - v1.0 - Initial write based on the RFS hardware module.
//                  Apr 2023 - v1.1 - Updates from the PCW/MZ2000 changes.
//                  Jul 2023 - v1.6 - Updated MZ-700 code, adding sub-memory maps to increase page mapping
//                                    speed specifically to enable reliable tape read/write. Addition of
//                                    MZ-1R18 emulation.
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
    uint8_t                                   loDRAMen;                                 // Lower bank 0000:0FFF DRAM enabled, else monitor.
    uint8_t                                   hiDRAMen;                                 // Higher bank D000:FFFF DRAM enabled, else memory mapped I/O.
    uint32_t                                 *ramFileMem;                               // 64K RamFile memory.
    uint16_t                                  ramFileAddr;                              // Address pointer of the MZ-1R18 64K Ram File Board memory.
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
    uint8_t     subMode;
    uint32_t    idx;

    // Setup defaults.
    MZ700Ctrl.regCtrl        = 0x00;
    MZ700Ctrl.loDRAMen       = 0;       // Default is monitor ROM is enabled.
    MZ700Ctrl.hiDRAMen       = 0;       // Default is memory mapped I/O enabled.
    Z80Ctrl->inhibitMode     = 0;

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
            if((idx&0x00ff) == 0xEA || (idx&0x00ff) == 0xEB)
            {
                Z80Ctrl->iopage[idx] = idx | IO_TYPE_VIRTUAL_HW;
            } else
            {
                Z80Ctrl->iopage[idx] = idx | IO_TYPE_PHYSICAL_HW;
            }
        }
        // Setup sub-memory pages to enable rapid switch according to memory bank commands.
        for(subMode=0; subMode < MEMORY_SUB_MODES; subMode++)
        {
            if(Z80Ctrl->page[MEMORY_MODES+subMode] == NULL)
            {
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug >=3) pr_info("Allocating memory sub page:%d\n", subMode);
              #endif
                (Z80Ctrl->page[MEMORY_MODES+subMode]) = (uint32_t *)kmalloc((MEMORY_BLOCK_SLOTS*sizeof(uint32_t)), GFP_KERNEL);
                if ((Z80Ctrl->page[MEMORY_MODES+subMode]) == NULL) 
                {
                    pr_info("z80drv: failed to allocate memory sub mapping page:%d memory!", subMode);
                }
            }
           
            // Duplicate current mode into the sub page prior to setting up specific config.
            memcpy((uint8_t *)Z80Ctrl->page[MEMORY_MODES+subMode], (uint8_t *)Z80Ctrl->page[0], MEMORY_BLOCK_SLOTS*sizeof(uint32_t));
            Z80Ctrl->memoryMode  = MEMORY_MODES + subMode;

            //  MZ700 memory mode switch.
            //
            //              MZ-700                                                     
            //             |0000:0FFF|1000:CFFF|D000:FFFF          
            //             ------------------------------          
            //  OUT 0xE0 = |DRAM     |DRAM     |<last>             
            //  OUT 0xE1 = |<last>   |DRAM     |DRAM               
            //  OUT 0xE2 = |MONITOR  |DRAM     |<last>             
            //  OUT 0xE3 = |<last>   |DRAM     |Memory Mapped I/O  
            //  OUT 0xE4 = |MONITOR  |DRAM     |Memory Mapped I/O  
            //  OUT 0xE5 = |<last>   |DRAM     |Inhibit            
            //  OUT 0xE6 = |<last>   |DRAM     |<return to last>   
            //
            // Sub-memory page maps:
            //
            // LOW BANK    HIGH BANK  PAGE MAP
            //             DRAM          0
            // DRAM        MEMORY MAP    1
            //             Inhibit       2
            //             DRAM          3
            // MONITOR     MEMORY MAP    4
            //             Inhibit       5
            //
            if(subMode >= 0 && subMode < 3)
            {
                // Enable lower 4K block as DRAM
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, idx);
                }
            }
            if(subMode >= 3 && subMode < 6)
            {
                // Enable lower 4K block as Monitor ROM
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, idx);
                }
            }
            if(subMode == 0 || subMode == 3)
            {
                // Enable upper 12K block, including Video/Memory Mapped peripherals area, as DRAM.
                for(idx=0xD000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    // MZ-700 mode we only work in first 64K block.
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, idx);
                }
            }
            if(subMode == 1 || subMode == 4)
            {
                // Enable Video RAM and Memory mapped peripherals in upper 12K block.
                for(idx=0xD000; idx < 0xE000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM, idx);
                }
                for(idx=0xE000; idx < 0xE800; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_HW, idx);
                }
                for(idx=0xE800; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, idx);
                }
            }
            if(subMode == 2 || subMode == 5)
            {
                // Inhibit. Backup current page data in region 0xD000-0xFFFF and inhibit it.
                for(idx=0xD000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_INHIBIT, idx);
                }
            }
        }
        Z80Ctrl->memoryMode  = MEMORY_MODES + 4;
 
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

  #if(DEBUG_ENABLED & 0x01)
    if(Z80Ctrl->debug >=3) pr_info("Allocating MZ-1R18 memory\n");
  #endif
    // Allocate memory for the MZ-1R18 64K Ram File board.
    MZ700Ctrl.ramFileMem = (uint32_t *)kmalloc((65536 * sizeof(uint8_t)), GFP_KERNEL);
    if(MZ700Ctrl.ramFileMem == NULL) 
    {
        pr_info("z80drv: failed to allocate MZ-1R18 Ram File memory!");
    }
    MZ700Ctrl.ramFileAddr = 0x0000;

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
    //uint32_t    idx;

    // Decoding memory address or I/O address?
    if(ioFlag == 0)
    {
      #if(DEBUG_ENABLED & 1)
        if(Z80Ctrl->debug >= 3)
        {
            pr_info("MEM:%04x,%02x,%d,%d\n", address, data, ioFlag, readFlag);
        }
      #endif
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
      #if(DEBUG_ENABLED & 1)
        if(Z80Ctrl->debug >= 3)
        {
            pr_info("IO:%04x,%02x,%d,%d\n", address, data, ioFlag, readFlag);
        }
      #endif

        // Check to see if the memory mode page has been allocated for requested mode, if it hasnt, we need to allocate and then define.
        if(((address&0xFF) - 0xE0) >= 0 && ((address&0xFF) - 0xE0) < 7)
        {
            //  MZ700 memory mode switch.
            //
            //              MZ-700                                                     
            //             |0000:0FFF|1000:CFFF|D000:FFFF          
            //             ------------------------------          
            //  OUT 0xE0 = |DRAM     |DRAM     |<last>             
            //  OUT 0xE1 = |<last>   |DRAM     |DRAM               
            //  OUT 0xE2 = |MONITOR  |DRAM     |<last>             
            //  OUT 0xE3 = |<last>   |DRAM     |Memory Mapped I/O  
            //  OUT 0xE4 = |MONITOR  |DRAM     |Memory Mapped I/O  
            //  OUT 0xE5 = |<last>   |DRAM     |Inhibit            
            //  OUT 0xE6 = |<last>   |DRAM     |<return to last>   
            //
            // Sub-memory page maps:
            //
            // LOW BANK    HIGH BANK  PAGE MAP
            //             DRAM          0
            // DRAM        MEMORY MAP    1
            //             Inhibit       2
            //             DRAM          3
            // MONITOR     MEMORY MAP    4
            //             Inhibit       5
            //
            // Determine if this is a memory management port and update the memory page if required.
            switch(address & 0x00FF)
            {
                case IO_ADDR_E0:
                    MZ700Ctrl.loDRAMen = 1;
                    break;

                case IO_ADDR_E1:
                    MZ700Ctrl.hiDRAMen = 1;
                    break;

                case IO_ADDR_E2:
                    MZ700Ctrl.loDRAMen = 0;
                    break;

                case IO_ADDR_E3:
                    MZ700Ctrl.hiDRAMen = 0;
                    break;

                case IO_ADDR_E4:
                    MZ700Ctrl.loDRAMen  = 0;
                    MZ700Ctrl.hiDRAMen  = 0;
                    Z80Ctrl->inhibitMode = 0;
                    break;

                case IO_ADDR_E5:
                    Z80Ctrl->inhibitMode = 1;
                    break;

                case IO_ADDR_E6:
                    Z80Ctrl->inhibitMode = 0;
                    break;

                default:
                    break;
            }

            // Setup memory mode based on flag state.
            if(Z80Ctrl->inhibitMode)
            {
               if(MZ700Ctrl.loDRAMen)
                   Z80Ctrl->memoryMode = MEMORY_MODES + 2;
               else
                   Z80Ctrl->memoryMode = MEMORY_MODES + 5;
            } else
            if(MZ700Ctrl.loDRAMen)
            {
                if(MZ700Ctrl.hiDRAMen)
                   Z80Ctrl->memoryMode = MEMORY_MODES + 0;
                else
                   Z80Ctrl->memoryMode = MEMORY_MODES + 1;
            } else
            {
                if(MZ700Ctrl.hiDRAMen)
                   Z80Ctrl->memoryMode = MEMORY_MODES + 3;
                else
                   Z80Ctrl->memoryMode = MEMORY_MODES + 4;
            }
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
        switch((address&0xff))
        {
            // MZ-1R18 Ram File Data Register.
            case 0xEA:
                data = MZ700Ctrl.ramFileMem[MZ700Ctrl.ramFileAddr];
                MZ700Ctrl.ramFileAddr++;
                break;

            // MZ-1R18 Ram File Control Register.
            case 0xEB:
                break;

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
        switch((address&0xff))
        {
            // MZ-1R18 Ram File Data Register.
            case 0xEA:
                MZ700Ctrl.ramFileMem[MZ700Ctrl.ramFileAddr] = data;
                MZ700Ctrl.ramFileAddr++;
                break;

            // MZ-1R18 Ram File Control Register.
            case 0xEB:
                MZ700Ctrl.ramFileAddr = (address & 0xff00) | data;
                break;

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
