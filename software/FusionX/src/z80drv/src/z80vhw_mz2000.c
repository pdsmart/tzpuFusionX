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

#include "tzpu.h"

// Device constants.
#define RAM_BASE_ADDR                         0x00000                                   // Base address of the 512K RAM.

// System ROM's, either use the host machine ROM or preload a ROM image.
#define ROM_DIR                               "/apps/FusionX/host/MZ-2000/ROMS/"
#define ROM_IPL_ORIG_FILENAME                  ROM_DIR "mz2000_ipl_original.rom"
#define ROM_IPL_FUSIONX_FILENAME               ROM_DIR "mz2000_ipl_fusionx.rom"
#define ROM_IPL_TZPU_FILENAME                  ROM_DIR "mz2000_ipl_tzpu.rom"
#define ROM_1Z001M_FILENAME                    ROM_DIR "1Z001M.rom"

// Boot ROM rom load and size definitions. 
#define ROM_BOOT_LOAD_ADDR                    0x000000
#define ROM_1Z001M_LOAD_ADDR                  0x000000
#define ROM_ORIG_BOOT_SIZE                    0x800
#define ROM_TZPU_BOOT_SIZE                    0x1000
#define ROM_FUSIONX_BOOT_SIZE                 0x1000
#define ROM_1Z001M_BOOT_SIZE                  0x10FB

// Sharp MZ-2000 constants.
#define MBADDR_FDC                            0x0D8                               // MB8866 IO Region 0D8h - 0DBh
#define MBADDR_FDC_CR                         MBADDR_FDC + 0x00                   // Command Register
#define MBADDR_FDC_STR                        MBADDR_FDC + 0x00                   // Status Register
#define MBADDR_FDC_TR                         MBADDR_FDC + 0x01                   // Track Register
#define MBADDR_FDC_SCR                        MBADDR_FDC + 0x02                   // Sector Register
#define MBADDR_FDC_DR                         MBADDR_FDC + 0x03                   // Data Register
#define MBADDR_FDC_MOTOR                      MBADDR_FDC + 0x04                   // DS[0-3] and Motor control. 4 drives  DS= BIT 0 -> Bit 2 = Drive number, 2=1,1=0,0=0 DS0, 2=1,1=0,0=1 DS1 etc
                                                                                  //  bit 7 = 1 MOTOR ON LOW (Active)
#define MBADDR_FDC_SIDE                       MBADDR_FDC + 0x05                   // Side select, Bit 0 when set = SIDE SELECT LOW, 
#define MBADDR_FDC_DDEN                       MBADDR_FDC + 0x06                   // Double density enable, 0 = double density, 1 = single density disks.
#define MBADDR_PPIA                           0x0E0                               // 8255 Port A
#define MBADDR_PPIB                           0x0E1                               // 8255 Port B
#define MBADDR_PPIC                           0x0E2                               // 8255 Port C
#define MBADDR_PPICTL                         0x0E3                               // 8255 Control Port
#define MBADDR_PIOA                           0x0E8                               // Z80 PIO Port A
#define MBADDR_PIOCTLA                        0x0E9                               // Z80 PIO Port A Control Port
#define MBADDR_PIOB                           0x0EA                               // Z80 PIO Port B
#define MBADDR_PIOCTLB                        0x0EB                               // Z80 PIO Port B Control Port
#define MBADDR_CRTBKCOLR                      0x0F4                               // Configure external CRT background colour.
#define MBADDR_CRTGRPHPRIO                    0x0F5                               // Graphics priority register, character or a graphics colour has front display priority.
#define MBADDR_CRTGRPHSEL                     0x0F6                               // Graphics output select on CRT or external CRT
#define MBADDR_GRAMCOLRSEL                    0x0F7                               // Graphics RAM colour bank select.
#define MBADDR_GRAMADDRL                      0x0C000                             // Graphics RAM base address.

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

    // tranZPUter I/O Ports need to be declared virtual to process locally.
    for(idx=0x0000; idx < IO_PAGE_SIZE; idx+=0x0100)
    {
        Z80Ctrl->iopage[idx+IO_TZ_SVCREQ]     = IO_TZ_SVCREQ     | IO_TYPE_VIRTUAL_HW;
        Z80Ctrl->iopage[idx+IO_TZ_SYSREQ]     = IO_TZ_SYSREQ     | IO_TYPE_VIRTUAL_HW;
    }

    // Ensure 40 char mode is enabled.
//    SPI_SEND_32(MBADDR_PIOA, 0x13);

    pr_info("MZ-2000 Memory Setup complete.\n");
}

// Method to load a ROM image into the RAM memory.
//
uint8_t mz2000LoadROM(const char* romFileName, uint8_t useROM, uint32_t loadAddr, uint32_t loadSize)
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
        noBytes = kernel_read(fp, fp->f_pos, useROM == 1 ? &Z80Ctrl->rom[loadAddr] : &Z80Ctrl->ram[loadAddr], loadSize);
        if(noBytes < loadSize)
        {
            pr_info("Short load, Image:%s, bytes loaded:%08x\n:", romFileName, loadSize);
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
    
//    // Initialise the Z80 PIO/8255 controllers.
//    //
//    SPI_SEND_32(MBADDR_PPICTL,      0x82);                              // 8255 A=OUT B=IN C=OUT
//    SPI_SEND_32(MBADDR_PPIC,        0x58);                              // BST=1 NST=0 OPEN=1 WRITE=1
//    SPI_SEND_32(MBADDR_PPIA,        0xF7);                              // All signals inactive, stop cassette.
//    SPI_SEND_32(MBADDR_PIOCTLA,     0x0F);                              // Setup PIO A
//    SPI_SEND_32(MBADDR_PIOCTLB,     0xCF);                              // Setup PIO B
//    SPI_SEND_32(MBADDR_PIOCTLB,     0xFF);
//
//    // Initialise video hardware.
//    //
//    SPI_SEND_32(MBADDR_CRTGRPHSEL,  0x00);                              // Set Graphics VRAM to default, no access to GRAM.
//    SPI_SEND_32(MBADDR_CRTBKCOLR,   0x00);                              // Set background colour on external output to black.
//    SPI_SEND_32(MBADDR_GRAMCOLRSEL, 0x01);                              // Activate Blue graphics RAM bank for graphics operations (this is the default installed bank, red and green are optional). 
//    SPI_SEND_32(MBADDR_CRTGRPHPRIO, 0x07);                              // Enable all colour bank graphic output with character priority.
//    SPI_SEND_32(MBADDR_PIOA,        0x13);                              // A7 : H 0xD000:0xD7FF or 0xC000:0xFFFF VRAN paged in.
//                                                                        // A6 : H Select Character VRAM (H) or Graphics VRAM (L)
//                                                                        // A5 : H Select 80 char mode, 40 char mode = L
//                                                                        // A4 : L Select all key strobe lines active, for detection of any key press.
//                                                                        // A3-A0: Keyboard strobe lines
//    SPI_SEND_32(MBADDR_PPIA,        0xFF);                              // A7 : L APSS Search for next program
//                                                                        // A6 : L Automatic playback at end of rewind
//                                                                        // A5 : L Automatic rewind during playback(recording)
//                                                                        // A4 : L Invert Video
//                                                                        // A3 : L Stop Cassette
//                                                                        // A2 : L Play Cassette
//                                                                        // A1 : L Fast Forward
//                                                                        // A0 : L Rewind
                                                                      
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
   
    // Overwrite the ROM with a modified version (comment out) if needed.
    //mz2000LoadROM(ROM_IPL_ORIG_FILENAME, ROM_BOOT_LOAD_ADDR, ROM_ORIG_BOOT_SIZE);
    //mz2000LoadROM(ROM_IPL_TZPU_FILENAME, ROM_BOOT_LOAD_ADDR, ROM_TZPU_BOOT_SIZE);
    mz2000LoadROM(ROM_IPL_FUSIONX_FILENAME, 1, ROM_BOOT_LOAD_ADDR,   ROM_FUSIONX_BOOT_SIZE);

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
pr_info("NST Reset\n");
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
pr_info("IPL Reset\n");
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
        // Only the lower 8 bits of the I/O address are processed as the upper byte is not used in the Sharp models.
        //
        switch(address & 0x00FF)
        {
            case IO_TZ_SVCREQ:    
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug >= 3) pr_info("SVCREQ:%02x\n", data);
              #endif

                // If a k64f process has registered, send it a service request signal.
pr_info("Sending signal,%02x\n", data);
                sendSignal(Z80Ctrl->ioTask, SIGIO);
                break;
                
            case IO_TZ_SYSREQ:    
              #if(DEBUG_ENABLED & 0x01)
                if(Z80Ctrl->debug >= 3) pr_info("SYSREQ:%02x\n", data);
              #endif
                break;

            default:
                pr_info("PORT:%02x\n", data);
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
