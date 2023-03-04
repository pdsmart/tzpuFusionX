/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80driver.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Driver 
//                  This file contains the methods used to create a linux device driver which provides
//                  the services of a Z80 CPU emulation and the control of an underlying Z80'less host
//                  system. In essence this driver is the host Z80 CPU.
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

#define _GNU_SOURCE
#include <Z/constants/pointer.h>
#include <Z/macros/member.h>
#include <Z/macros/array.h>
#include <Z80.h>
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
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include "z80io.h"
#include "z80menu.h"
#include "z80driver.h"

#include <asm/io.h>
#include <infinity2m/gpio.h>
#include <infinity2m/registers.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Philip D Smart");
MODULE_DESCRIPTION("Z80 CPU Emulator and Hardware Interface Driver");

/* Global variables for the threads */
static struct task_struct *kthread_z80;
static int                 threadId_z80 = 1;

// Device class and major numbers.
static struct class       *class;
static struct device      *device;
static int                 major;

// CPU Instance.
static Z80                 Z80CPU;

// Z80 Control data.
static t_Z80Ctrl          *Z80Ctrl = NULL; 

// Runtime control of the CPU. As the CPU runs in a detached thread on core 1, the cpu needs to be suspended before any external
// operations can take place. This is achieved with the runtime mutex.
enum Z80_RUN_STATES        Z80RunMode;
static struct mutex        Z80RunModeMutex;
static DEFINE_MUTEX(Z80DRV_MUTEX);


//-------------------------------------------------------------------------------------------------------------------------------
//
// Z80 CPU Kernel Logic.
//
// THe Z80 CPU is initialised and set running, processing instructions either from the underlying host hardware or internal
// memory. The configuration and flow is controlled via the Z80Ctrl structure which is User Space accessible.
//
//-------------------------------------------------------------------------------------------------------------------------------

// Method to read a byte from physical hardware or internal virtual memory/devices.
// The page table indicates the source and the read is processed accordingly.
static zuint8 z80_read(void *context, zuint16 address)
{
    // Locals.
    //
    zuint8          data;
//    uint16_t        addrDiff = (uint16_t)address - Z80Ctrl->z80PrevAddr;
    Z_UNUSED(context)
       
    // Only read if the address is in physical RAM.
    if(isPhysical(address))
    {
        // Commence cycle to retrieve the data from Real RAM.
        // Optimise SPI according to last address sent to CPLD.
//        if(addrDiff >=0 && addrDiff < 8)
//        {
//            SPI_SEND8((CPLD_CMD_READ_ADDR + addrDiff));
//            Z80Ctrl->z80PrevAddr += addrDiff;
//        } else
//        {
            SPI_SEND32((uint32_t)address << 16 | CPLD_CMD_READ_ADDR);
            Z80Ctrl->z80PrevAddr = address;
//        }
        while(CPLD_READY() == 0);
        data = READ_CPLD_DATA_IN();
      
        // Pause until the Last T-State is detected.
  //      while(CPLD_LAST_TSTATE() == 0);
    }
    else if(isVirtualHW(address))
    {
        // Virtual Hardware - call the handler.
        switch(realAddress(address))
        {
            default:
                break;
        }
    }
    else if(isVirtualMemory(address))
    {
        // Retrieve data from virtual memory.
        data = isVirtualROM(address) ? readVirtualROM(address) : readVirtualRAM(address);
//        if(address >= 0xCF01 && address < 0xCF0A)
//            pr_info("Read:0x%x, 0x%x, 0x%x", address, data, isVirtualROM(address) ? getPageAddr(address, MEMORY_TYPE_VIRTUAL_MASK) + Z80_VIRTUAL_RAM_SIZE : getPageAddr(address, MEMORY_TYPE_VIRTUAL_MASK));
    }

    // Keyport data? Store.
    if(isHW(address) && address == 0xE001 && (Z80Ctrl->keyportStrobe & 0x0f) == 8 && (data & 0x41) == 0)
    {
        Z80Ctrl->keyportShiftCtrl = 0x01;
    } else
    if(isHW(address) && address == 0xE001 && (Z80Ctrl->keyportStrobe & 0x0f) == 0 && (data & 0x80) == 0)
    {
        Z80Ctrl->keyportHotKey = 0x01;
    }
    return(data);
}

// Method to write a byte to physical hardware or internal virtual memory or devices.
// The page table indicates the target and the write is processed accordingly.
static void z80_write(void *context, zuint16 address, zuint8 data)
{
    // Locals.
 //   uint16_t        addrDiff = (uint16_t)address - Z80Ctrl->z80PrevAddr;
    Z_UNUSED(context)

    // To detect Hotkey presses, we need to store the keboard strobe data and on keydata read.
    if(isHW(address) && address == 0xE000)
    {
        Z80Ctrl->keyportStrobe = data;
    }

    // Write to physical host?
    if(isPhysical(address))
    {
        // Commence cycle to write the data to real RAM.
        // Optimise SPI according to last address sent to CPLD.
//        if(addrDiff >=0 && addrDiff < 8)
//        {
//if(address >= 0xD010 && address < 0xD017) { data = 0x41; }
//            SPI_SEND16((data << 8) | (CPLD_CMD_WRITE_ADDR + addrDiff));
//            Z80Ctrl->z80PrevAddr += addrDiff;
//        } else
//        {
            SPI_SEND32((uint32_t)address << 16 | data << 8 | CPLD_CMD_WRITE_ADDR);
            Z80Ctrl->z80PrevAddr = address;
//        }
        
        // Write-thru to virtual memory if we update real memory.
 //       if(isPhysicalRAM(address))
 //           writeVirtualRAM(address, data);
      
//        if(address >= 0xD000 && address < 0xD018)
//            pr_info("Write:0x%x, 0x%x, 0x%x", address, data, isVirtualROM(address) ? getPageAddr(address, MEMORY_TYPE_VIRTUAL_MASK) + Z80_VIRTUAL_RAM_SIZE : getPageAddr(address, MEMORY_TYPE_VIRTUAL_MASK));

        // Pause until the Last T-State is detected.
 //       while(CPLD_LAST_TSTATE() == 0);
    }
    else if(isVirtualHW(address))
    {
        // Virtual Hardware - call the handler.
        switch(realAddress(address))
        {
            default:
                break;
        }
    }
    else if(isVirtualRAM(address))
    {
        // Update virtual memory.
        writeVirtualRAM(address, data);
//pr_info("Write:0x%x, 0x%x, 0x%x", address, data, isVirtualROM(address) ? getPageAddr(address, MEMORY_TYPE_VIRTUAL_MASK) + Z80_VIRTUAL_RAM_SIZE : getPageAddr(address, MEMORY_TYPE_VIRTUAL_MASK));
    }
    // Cannot write to virtual ROM so no logic.
}

// Primary Opcode fetch method. This method is called each time a single or multi-byte opcode is
// encountered. Opcode data is retrieved via the z80_fetch method.
//
// Depending on the address and the configured page map, the opcode is fetched from hardware
// or internal virtual memory. As this method is the primary timing method for Z80 instructions
// (read/write methods dont affect the timing so much as long as they operate in less than the read/write
// cycle of an original Z80).
// Initially the timing on the virtual memory is set by a governor delay but this will be updated to a more
// precise M/T-State cycle per instruction type delay.
static zuint8 z80_fetch_opcode(void *context, zuint16 address)
{
    // Locals.
    zuint8            opcode = 0x00;
//    uint16_t          addrDiff = (uint16_t)address - Z80Ctrl->z80PrevAddr;
    volatile uint32_t idx;            // Leave as volatile otherwise optimiser will optimise out the delay code.
    Z_UNUSED(context)

    // Normally only opcode fetches occur in RAM but allow any physical address as it could be a Z80 programming trick.
    if(isPhysical(address))
    {
        // Commence cycle to fetch the opcode from potentially Real RAM albeit it could be any physical hardware.
        // Optimise SPI according to last address sent to CPLD.
//        if(addrDiff >=0 && addrDiff < 8)
//        {
//            SPI_SEND8((CPLD_CMD_FETCH_ADDR + addrDiff));
//            Z80Ctrl->z80PrevAddr += addrDiff;
//        } else
//        {
            SPI_SEND32((uint32_t)address << 16 | CPLD_CMD_FETCH_ADDR);
            Z80Ctrl->z80PrevAddr = address;
//        }
        while(CPLD_READY() == 0);
        opcode = READ_CPLD_DATA_IN();

        // Pause until the Last T-State is detected.
//        while(CPLD_LAST_TSTATE() == 0);
    } else
    // Virtual fetches only occur in memory as we are not emulating original hardware.
    if(isVirtualMemory(address))
    {
       // for(idx=0; idx < Z80Ctrl->cpuGovernorDelay; idx++);
   //     for(idx=0; idx < 238; idx++);
        // 243 loaded

        // Retrieve data from virtual memory.
   //     opcode = isVirtualROM(address) ? readVirtualROM(address) : readVirtualRAM(address);

        if(isVirtualROM(address))
        {
            opcode = readVirtualROM(address);
            for(idx=0; idx < Z80Ctrl->cpuGovernorDelayROM; idx++);
        } else
        {
            opcode = readVirtualRAM(address);
            for(idx=0; idx < Z80Ctrl->cpuGovernorDelayRAM; idx++);
        }
    }
//if(opcode == 0xED && address > 0x270)
//  pr_info("Fetch:0x%x, 0x%x, 0x%x", address, opcode, isVirtualROM(address) ? getPageAddr(address, MEMORY_TYPE_VIRTUAL_MASK) + Z80_VIRTUAL_RAM_SIZE : getPageAddr(address, MEMORY_TYPE_VIRTUAL_MASK));
    return(opcode);
}

// Method similar to z80_read, kept seperate to avoid additional what-if logic and doesnt require virtual hardware logic.
//
static zuint8 z80_fetch(void *context, zuint16 address)
{
    // Locals.
    //
    zuint8          data = 0x00;
//    uint16_t        addrDiff = (uint16_t)address - Z80Ctrl->z80PrevAddr;
    Z_UNUSED(context)
       
    // Normally only opcode fetches occur in RAM but allow any physical address as it could be a Z80 programming trick.
    if(isPhysical(address))
    {
        // Commence cycle to retrieve the data from Real RAM.
        // Optimise SPI according to last address sent to CPLD.
//        if(addrDiff >=0 && addrDiff < 8)
//        {
//            SPI_SEND8((CPLD_CMD_READ_ADDR + addrDiff));
//            Z80Ctrl->z80PrevAddr += addrDiff;
//        } else
//        {
            SPI_SEND32((uint32_t)address << 16 | CPLD_CMD_READ_ADDR);
            Z80Ctrl->z80PrevAddr = address;
//        }
        while(CPLD_READY() == 0);
        data = READ_CPLD_DATA_IN();
      
        // Pause until the Last T-State is detected.
//        while(CPLD_LAST_TSTATE() == 0);
    } else
    if(isVirtualMemory(address))
    {
        // Retrieve data from virtual memory.
        data = isVirtualROM(address) ? readVirtualROM(address) : readVirtualRAM(address);
    }
//pr_info("(0x%x)", data);
 
    return(data);
}

// Method to perform a Z80 input operation. This normally goes to hardware and the CPLD executes the required cycle. 
// Some ports are dedicated virtual ports providing virtual services to the host computer/application. These are intercepted
// and processed in this driver.
static zuint8 z80_in(void *context, zuint16 port)
{
    // Locals.
    zuint8          value;
//    uint16_t        portDiff = (uint16_t)port - Z80Ctrl->z80PrevPort;
    Z_UNUSED(context)

    // Physical port go direct to hardware to retrieve value.
    if(isPhysicalIO(port))
    {
        // Commence cycle to retrieve the value from the I/O port. Port contains the 16bit BC value.
        // Optimise SPI according to last port sent to CPLD.
//        if(portDiff >=0 && portDiff < 8)
//        {
//            SPI_SEND8((CPLD_CMD_READIO_ADDR + portDiff));
//            Z80Ctrl->z80PrevPort += portDiff;
//        } else
//        {
            SPI_SEND32((uint32_t)port << 16 | CPLD_CMD_READIO_ADDR);
            Z80Ctrl->z80PrevPort = port;
//        }

        // Whilst waiting for the CPLD, we now determine if this is a memory management port and update the memory page if required.
        switch(port & 0x00FF)
        {
            // Port is not a memory management port.
            default:
                break;
        }

        // Finally ensure the data from the port is ready and retrieve it.
        while(CPLD_READY() == 0);
        value = READ_CPLD_DATA_IN();
    } else
    // Virtual I/O Port.
    {
        // Virtual I/O - call the handler.
        switch(realPort(port))
        {
            default:
                value = 0x00;
                break;
        }
    }
//    if((port&0x00ff) == 0xE8)
//    pr_info("z80_in:0x%x, 0x%x\n", port, value);
    return(value);
}

// Method to perform a Z80 output operation. This normally goes to hardware and the CPLD executes the required cycle. 
// Some ports are dedicated virtual ports providing virtual services to the host computer/application. These are intercepted
// and processed in this driver.
// There are also ports which are both hardware and need mirroring in software. These ports, typically memory mapping ports.
// when activated in the hardware need to be mirrored in the page table so correct virtual memory is used when addressed.
static void z80_out(void *context, zuint16 port, zuint8 value)
{
    // Locals.
    uint32_t        idx;
//    uint16_t        portDiff = (uint16_t)port - Z80Ctrl->z80PrevPort;
    Z_UNUSED(context) 

//if((port&0xff) == 0xe8)
//{
//    value |= 0xC0;
//}
//udelay(10);
    // Physical port go direct to hardware to retrieve value.
    if(isPhysicalIO(port))
    {
        // Commence cycle to write the value to the I/O port. Port contains the 16bit BC value.
        // Optimise SPI according to last port sent to CPLD.
//        if(portDiff >=0 && portDiff < 8)
//        {
//            SPI_SEND16((value << 8) | (CPLD_CMD_WRITEIO_ADDR + portDiff));
//            Z80Ctrl->z80PrevPort += portDiff;
//    if((port&0x00ff) == 0xE8)
//pr_info("OUT:0x%x(%x)=0x%x\n", port, portDiff, value);
//        } else
//        {
            SPI_SEND32((uint32_t)port << 16 | value << 8 | CPLD_CMD_WRITEIO_ADDR);
            Z80Ctrl->z80PrevPort = port;
//    if((port&0x00ff) == 0xE8)
//    {
//            SPI_SEND32((uint32_t)port << 16 | value << 8 | CPLD_CMD_WRITEIO_ADDR);
//            udelay(10);
//            SPI_SEND32((uint32_t)port << 16 | value << 8 | CPLD_CMD_WRITEIO_ADDR);
//            udelay(10);
//            SPI_SEND32((uint32_t)port << 16 | value << 8 | CPLD_CMD_WRITEIO_ADDR);
//            udelay(10);
//            SPI_SEND32((uint32_t)port << 16 | value << 8 | CPLD_CMD_WRITEIO_ADDR);
//            udelay(10);
//pr_info("OUT:0x%x=0x%x\n", port, value);
//    }
//        }
     
//udelay(10);
        // Determine if this is a memory management port and update the memory page if required.
        switch(port & 0x00FF)
        {
#if(TARGET_HOST_MZ2000 == 1)
            case IO_ADDR_E0:
                break;

            case IO_ADDR_E1:
                break;

            case IO_ADDR_E2:
                break;

            case IO_ADDR_E3:
                // Program control register.
                if(value & 0x80)
                {
                } else
                {
                    switch((value >> 1) & 0x07)
                    {
                        // NST toggle.
                        case 1:
                            // NST pages in all RAM and resets cpu.
                            if(value & 0x01)
                            {
                                Z80Ctrl->lowMemorySwap = 0;
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
                                z80_instant_reset(&Z80CPU); 
                            }
                            break;

                        default:
                            break;
                    }
                }
                break;

            case IO_ADDR_E8:
                // NEED FLAG TO SET THIS WHEN CALLED WITH NON MEMORY SWITCH BYTE
                if(isPhysical(0xD000) && (value & 0x80) == 0)
                {
                    for(idx=0xC000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        if(Z80Ctrl->defaultPageMode == USE_PHYSICAL_RAM)
                        {
                            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
                        } else
                        {
                            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, (Z80Ctrl->lowMemorySwap ? idx - 0x8000 : idx));
                        }
                    }
                } else
                if(value & 0x80)
                {
                    if(value & 0x40)
                    {
                        setMemoryType(0xD000/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM, 0xD000);
                    } else
                    {
                        for(idx=0xC000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                        {
                            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_VRAM, idx);
                        }
                    }
                }
                break;
#endif

#if(TARGET_HOST_MZ700 == 1)
            // Enable lower 4K block as DRAM
            case IO_ADDR_E0:
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
                }
                break;

            // Enable upper 12K block, including Video/Memory Mapped peripherals area, as DRAM.
            case IO_ADDR_E1:
                if(!Z80Ctrl->inhibitMode)
                {
                    for(idx=0xD000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
                    {
                        // MZ-700 mode we only work in first 64K block.
                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_RAM, idx);
                    }
                }
                break;

            // Enable Monitor ROM in lower 4K block
            case IO_ADDR_E2:
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_ROM, idx);
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
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_PHYSICAL_ROM, idx);
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
#endif

            // Port is not a memory management port.
            default:
                break;
        }
    } else
    if(isVirtualIO(port))
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

        // Determine if this is a memory management port and update the memory page if required.
        switch(port & 0x00FF)
        {
#if(TARGET_HOST_MZ700 == 1)
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
#endif

            // Port is not a memory management port.
            default:
                break;
        }
    } else
    {
        // Virtual I/O - call the handler.
        switch(realPort(port))
        {
            default:
                break;
        }
    }
//pr_info("z80_out:0x%x, 0x%x\n", port, value);
}

// NOP - No Operation method. This instruction is used for timing, padding out an application or during
// HALT cycles to ensure Refresh occurs. 
// If the address is configured as hardware (via the page table) then a refresh cycle is requested otherwise
// nothing to be done.
static zuint8 z80_nop(void *context, zuint16 address)
{
    // Locals.
    Z_UNUSED(context)

    if(isPhysical(address))
    {
        // If autorefresh is not enabled, send a single refresh request.
        if(Z80Ctrl->refreshDRAM == 0)
            SPI_SEND8(CPLD_CMD_REFRESH);
pr_info("NOP");
    }
    return 0x00;
}

// HALT - CPU executes a HALT instruction which results in the HALT line going active low and then it enters
// a state executing NOP instructions to ensure DRAM refresh until a reset or INT event.
static void z80_halt(void *context, zboolean state)
{
    // Locals.
    Z_UNUSED(context) Z_UNUSED(state)

    // Inform CPLD of halt state.
    printk("z80_halt\n");
    SPI_SEND8(CPLD_CMD_HALT);
    Z80CPU.cycles = Z80_MAXIMUM_CYCLES;
}

// Methods below are not yet implemented, Work In Progress!
static zuint8 z80_context(void *context, zuint16 address)
{
    Z_UNUSED(context)
    printk("z80_context\n");
    return 0x00;
}
static zuint8 z80_nmia(void *context, zuint16 address)
{
    Z_UNUSED(context)
    printk("z80_nmia\n");
    return 0x00;
}
static zuint8 z80_inta(void *context, zuint16 address)
{
    Z_UNUSED(context)
    printk("z80_inta\n");
    return 0x00;
}
static zuint8 z80_intFetch(void *context, zuint16 address)
{
    Z_UNUSED(context)
    printk("z80_int_fetch\n");
    return 0x00;
}
static void z80_ldia(void *context)
{
    Z_UNUSED(context)
    printk("z80_ldia\n");
}
static void z80_ldra(void *context)
{
    Z_UNUSED(context)
    printk("z80_ldra\n");
}
static void z80_reti(void *context)
{
    Z_UNUSED(context)
    printk("z80_reti\n");
}
static void z80_retn(void *context)
{
    Z_UNUSED(context)
    printk("z80_retn\n");
}
static zuint8 z80_illegal(void *context, zuint8 opcode)
{
    Z_UNUSED(context)
    printk("z80_illegal\n");
    return 0x00;
}

// Z80 CPU Emulation Thread
// ------------------------
// This is a kernel thread, bound to CPU 1 with IRQ's disabled.
// The Z80 is controlled by a mutex protected variable to define run, stop, pause and terminate modes.
int thread_z80(void * thread_nr) 
{
    // Locals.
    uint8_t             canRun = 0;
    int t_nr = *(int *) thread_nr;
  //struct sched_param  param = {.sched_priority = 99};
    spinlock_t          spinLock;
    unsigned long       flags; 

    // Initialise spinlock and disable IRQ's. We should be the only process running on core 1.
    spin_lock_init(&spinLock);
    spin_lock_irqsave(&spinLock, flags);

    // Assign this emulation to high priority realtime scheduling. Also the task will be assigned to an isolated CPU.
     //sched_setscheduler(current, SCHED_RR, &param);

    // Run the CPU forever or until a stop occurs.
    while(!kthread_should_stop())
    {
        // Run the Z80 emulation if enabled.
        if(canRun) z80_run(&Z80CPU, 100);

        // Reset pressed?
        if(CPLD_RESET())
        {
            z80_instant_reset(&Z80CPU); 
            setupMemory(Z80Ctrl->defaultPageMode);

            // Wait for release before restarting CPU.
            while(CPLD_RESET());
        } else
        {
            // Update state to indicate request has been actioned.
            mutex_lock(&Z80RunModeMutex);
            if(Z80RunMode == Z80_STOP)     Z80RunMode = Z80_STOPPED;
            if(Z80RunMode == Z80_PAUSE)    Z80RunMode = Z80_PAUSED;
            if(Z80RunMode == Z80_CONTINUE) Z80RunMode = Z80_RUNNING;
            if(Z80RunMode == Z80_RUNNING) canRun=1; else canRun=0;
            mutex_unlock(&Z80RunModeMutex);

            // Hotkey pressed? Bring up user menu.
            if(Z80Ctrl->keyportShiftCtrl && Z80Ctrl->keyportHotKey)
            {
                z80menu();
                Z80Ctrl->keyportShiftCtrl = 0;
                Z80Ctrl->keyportHotKey = 0;
            }
        }
    }

    // Release spinlock as we are unloading driver.
    spin_unlock_irqrestore(&spinLock, flags);
    printk("kthread - Z80 Thread %d finished execution!\n", t_nr);
    return 0;
}


//-------------------------------------------------------------------------------------------------------------------------------
//
// User space driver access.
//
//-------------------------------------------------------------------------------------------------------------------------------


// Device close.
// When a user space application terminates or closes the z80drv device driver, this function is called
// to close any open connections, memory and variables required to handle the user space application
// requests.
static int z80drv_release(struct inode *inodep, struct file *filep)
{    
    // Locals.

    mutex_unlock(&Z80DRV_MUTEX);
    //pr_info("z80drv: Device successfully closed\n");

    return(0);
}

// Device open.
// When a user space application open's the z80drv device driver, this function is called
// to initialise and allocate any required memory or devices prior to servicing requests from the
// user space application.
static int z80drv_open(struct inode *inodep, struct file *filep)
{
    // Locals.
    int ret = 0; 

    if(!mutex_trylock(&Z80DRV_MUTEX))
    {
        pr_alert("z80drv: device busy!\n");
        ret = -EBUSY;
        goto out;
    }
 
    //pr_info("z80drv: Device opened\n");

out:
    return(ret);
}

// Map shared memory.
// The z80drv allocates on the stack a chunk of memory and control variables which is used to control the Z80 Emulation state
// and provide it with internal 'virtual memory'. This virtual memory is either used as the core Z80 memory or as banked extensions
// to the host DRAM.
// The user space application is able to bind with the shared memory to perform tasks such as load/save of applications.
static int z80drv_mmap(struct file *filp, struct vm_area_struct *vma)
{
    // Locals.
    int            ret  = 0;
    struct page   *page = NULL;
    unsigned long  size = (unsigned long)(vma->vm_end - vma->vm_start);

    // Make sure requested size is within range of the Z80 control structure. The kernel may page align it so the size may be bigger than
    // the structure.
    if(size < sizeof(t_Z80Ctrl) || size > sizeof(t_Z80Ctrl)*2)
    {
        ret = -EINVAL;
        goto out;  
    } 
   
    // Map the memory and exit.
    page = virt_to_page((unsigned long)Z80Ctrl + (vma->vm_pgoff << PAGE_SHIFT)); 
    ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), size, vma->vm_page_prot);
    if (ret != 0)
    {
        goto out;
    }   

out:
    return ret;
}

// Device read.
// This method allows an application which opens the z80drv driver to read data in a stream. It is here for
// possible future use.
static ssize_t z80drv_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    // Locals.
    int ret;
    
    if (len > Z80_VIRTUAL_RAM_SIZE) 
    {
        pr_info("read overflow!\n");
        ret = -EFAULT;
        goto out;
    }

    if (copy_to_user(buffer, Z80Ctrl, len) == 0) 
    {
        pr_info("z80drv: copy %u char to the user\n", len);
        ret = len;
    } else 
    {
        ret =  -EFAULT;   
    } 

out:
    return ret;
}

// Device write.
// This method allows an application which opens the z80drv driver to write stream data. It is here for
// possible future use.
static ssize_t z80drv_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    // Locals.
    int ret;
 
    if (copy_from_user(Z80Ctrl, buffer, len))
    {
        pr_err("z80drv: write fault!\n");
        ret = -EFAULT;
        goto out;
    }
    pr_info("z80drv: copy %d char from the user\n", len);
    ret = len;

out:
    return ret;
}

// Function to dump out a given section of the physical host memory.
//
int memoryDump(uint32_t memaddr, uint32_t memsize, uint32_t dispaddr, uint8_t dispwidth)
{
    uint8_t  displayWidth = dispwidth;
    uint32_t pnt          = memaddr;
    uint32_t endAddr      = memaddr + memsize;
    uint32_t addr         = dispaddr;
    uint8_t  data;
    uint32_t i            = 0;
    int      result       = -1;
    char     c            = 0;

    // If not set, calculate output line width according to connected display width.
    //
    if(displayWidth == 0)
    {
        switch(MAX_SCREEN_WIDTH)
        {
            case 40:
                displayWidth = 8;
                break;
            case 80:
                displayWidth = 16;
                break;
            default:
                displayWidth = 32;
                break;
        }
    }

    while (1)
    {
        printk(KERN_INFO "%08X", addr); // print address
        printk(KERN_CONT ":  ");

        // print hexadecimal data
        for (i=0; i < displayWidth; )
        {
            if(pnt+i < endAddr)
            {
                SPI_SEND32((uint16_t)(pnt+i) << 16 | CPLD_CMD_READ_ADDR);
                Z80Ctrl->z80PrevAddr = pnt+i;
                while(CPLD_READY() == 0);
                data = READ_CPLD_DATA_IN();
                printk(KERN_CONT "%02X", data);
            }
            else
                printk(KERN_CONT "  ");
            i++;

            printk(KERN_CONT " ");
        }

        // print ascii data
        printk(KERN_CONT " |");

        // print single ascii char
        for (i=0; i < displayWidth; i++)
        {
            SPI_SEND32((uint16_t)(pnt+i) << 16 | CPLD_CMD_READ_ADDR);
            Z80Ctrl->z80PrevAddr = pnt+i;
            while(CPLD_READY() == 0);
            c = (char)READ_CPLD_DATA_IN();
            if ((pnt+i < endAddr) && (c >= ' ') && (c <= '~'))
                printk(KERN_CONT "%c", (char)c);
            else
                printk(KERN_CONT " ");
        }

        printk(KERN_CONT "|\n");

        // Move on one row.
        pnt  += displayWidth;
        addr += displayWidth;

        // End of buffer, exit the loop.
        if(pnt >= (memaddr + memsize))
        {
            break;
        }
    }

    return(result);
}

// Method to setup a default memory/IO profile. This profile will be changed by the host processing and also can be tweaked
// by the z80ctrl application.
//
void setupMemory(enum Z80_MEMORY_PROFILE mode)
{
    // Locals.
    uint32_t        idx;

    if(mode == USE_PHYSICAL_RAM)
    {
        // Initialise the page pointers and memory to use physical RAM.
        for(idx=0x0000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
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
        for(idx=0x0000; idx < 0x10000; idx++)
        {
            Z80Ctrl->iopage[idx] = idx | IO_TYPE_PHYSICAL_HW;
        }
        // Cancel refresh as using physical RAM for program automatically refreshes DRAM.
        Z80Ctrl->refreshDRAM = 0;
    }
    else if(mode == USE_VIRTUAL_RAM)
    {
#if(TARGET_HOST_MZ2000 == 1)
        // Initialise the page pointers and memory to use virtual RAM.
        // MZ-2000 comes up in IPL mode where lower 32K is ROM and upper 32K is RAM remapped from 0x0000.
        for(idx=0x0000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
        {
            if(idx >= 0 && idx < 0x8000)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, idx);
            }
            else 
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, (Z80Ctrl->lowMemorySwap ? idx - 0x8000 : idx));
            }
        }
        for(idx=0x0000; idx < 0x10000; idx++)
        {
            Z80Ctrl->iopage[idx] = idx | IO_TYPE_PHYSICAL_HW;
        }
        // Enable refresh as using virtual RAM stops refresh of host DRAM.
        Z80Ctrl->refreshDRAM = 1;
 pr_info("Im here\n");
    }
#endif

    // Enable autorefresh if refreshDRAM is set.
    SPI_SEND8(Z80Ctrl->refreshDRAM == 1 ? CPLD_CMD_SET_AUTO_REFRESH : CPLD_CMD_CLEAR_AUTO_REFRESH);

    // Inhibit mode disabled.
    Z80Ctrl->inhibitMode = 0;
    return;
}

// IOCTL Method
// This method allows User Space application to control the Z80 CPU and internal functionality of the
// device driver. This is the preferred control method along with the shared memory segment for the driver.
static long int z80drv_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
    // Locals.
    struct ioctlCmd     ioctlCmd;
    uint16_t            idx;
    uint32_t            tmp[2];
    enum Z80_RUN_STATES currentRunMode;
    enum Z80_RUN_STATES nextRunMode;

    // Get current running mode so any operations on the Z80 return it to original mode unless action overrides it.
    mutex_lock(&Z80RunModeMutex); currentRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);

    switch(cmd)
    {
        // Basic commands.
        case IOCTL_CMD_SEND:
            if(copy_from_user(&ioctlCmd, (int32_t *)arg, sizeof(ioctlCmd))) 
                printk("IOCTL - Couldnt retrieve command!\n");
            else
            {
                //printk("IOCTL - Command (%08x)\n", ioctlCmd.cmd);
                switch(ioctlCmd.cmd)
                {
                    // Command to stop the Z80 CPU and power off.
                    case IOCTL_CMD_Z80_STOP:
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        z80_power(&Z80CPU, FALSE);
                        Z80_PC(Z80CPU) = 0;
                        printk("Z80 stopped.\n");
                        break;

                    // Command to power on and start the Z80 CPU.
                    case IOCTL_CMD_Z80_START:
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_RUNNING; mutex_unlock(&Z80RunModeMutex);

                        z80_power(&Z80CPU, TRUE);
                        printk("Z80 started.\n");
                        break;

                    // Command to pause the Z80.
                    case IOCTL_CMD_Z80_PAUSE:
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_PAUSE; mutex_unlock(&Z80RunModeMutex);
                        printk("Z80 paused.\n");
                        break;

                    // Command to release a paused Z80.
                    case IOCTL_CMD_Z80_CONTINUE:
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_CONTINUE; mutex_unlock(&Z80RunModeMutex);
                        printk("Z80 running.\n");
                        break;

                    // Command to perform a CPU reset.
                    case IOCTL_CMD_Z80_RESET:
                        // Stop the CPU prior to reset.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        z80_instant_reset(&Z80CPU);
                        setupMemory(Z80Ctrl->defaultPageMode);
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        printk("Z80 Reset.\n");
                        break;
                        
                    // Command to setup the page table to use host memory and physical hardware.
                    case IOCTL_CMD_USE_HOST_RAM:
                        // Stop the CPU prior to memory reconfiguration.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        Z80Ctrl->defaultPageMode = USE_PHYSICAL_RAM;
                        setupMemory(Z80Ctrl->defaultPageMode);
                        z80_instant_reset(&Z80CPU);

                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        printk("Z80 Set to use Host Memory.\n");
                        break;

                    // Command to setup the page table to use virtual memory, only physical hardware is accessed on the host.
                    case IOCTL_CMD_USE_VIRTUAL_RAM:
                        // Stop the CPU prior to memory reconfiguration.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        Z80Ctrl->defaultPageMode = USE_VIRTUAL_RAM;
                        setupMemory(Z80Ctrl->defaultPageMode);
                        z80_instant_reset(&Z80CPU);
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        printk("Z80 Set to use Virtual Memory.\n");
                        break;

                    // Command to synchronise virtual memory to host DRAM.
                    case IOCTL_CMD_SYNC_TO_HOST_RAM:
                        // Stop the CPU prior to memory sync.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        // Copy virtual memory to host DRAM.
                        for(idx=0x1000; idx < 0xD000; idx++)
                        {
                            SPI_SEND32((uint32_t)idx << 16 | Z80Ctrl->memory[idx] << 8 | CPLD_CMD_WRITE_ADDR);
                        }

                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        printk("Z80 Host DRAM syncd with Virtual Memory.\n");
                        break;

                    // Command to dump out host memory.
                    case IOCTL_CMD_DUMP_MEMORY:
                        // Need to suspend the Z80 otherwise we will get memory clashes.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_PAUSE; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_PAUSE);

                        // Dump out the physical memory address.
                        memoryDump(ioctlCmd.addr.start, ioctlCmd.addr.end - ioctlCmd.addr.start, ioctlCmd.addr.start, 0);

                        // Z80 can continue.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        break;

                    // Command to set the governor delay to approximate real Z80 cpu frequencies when running in virtual memory.
                    case IOCTL_CMD_Z80_CPU_FREQ:
                        switch(ioctlCmd.speed.speedMultiplier)
                        {
                            case 2:
                                Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_7MHZ;
                                Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_7MHZ;
                                break;

                            case 4:
                                Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_14MHZ;
                                Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_14MHZ;
                                break;

                            case 8:
                                Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_28MHZ;
                                Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_28MHZ;
                                break;

                            case 16:
                                Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_56MHZ;
                                Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_56MHZ;
                                break;

                            case 32:
                                Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_112MHZ;
                                Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_112MHZ;
                                break;

                            case 64:
                                Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_224MHZ;
                                Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_224MHZ;
                                break;

                            case 128:
                                Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_448MHZ;
                                Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_448MHZ;
                                break;

                            case 1:
                            default:
                                Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_3_54MHZ;
                                Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_3_54MHZ;
                                break;
                        }
                        break;

                    // Command to set the Z80 CPU Program Counter value.
                    case IOCTL_CMD_SETPC:
                        // Stop the CPU prior to PC change.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        Z80_PC(Z80CPU) = ioctlCmd.z80.pc;
                       
                        // Z80 can continue.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        printk("Set PC to %04x\n", ioctlCmd.z80.pc);
                        break;

                    // Method to send adhoc commands to the CPLD, ie for switching active display etc.
                    case IOCTL_CMD_CPLD_CMD:
                        // Stop the CPU prior to sending a direct command to the CPLD.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        // Send the command, small delay then send NOP to retrieve the response.
                        z80io_SPI_Send32(ioctlCmd.cpld.cmd, &tmp[0]);
                        udelay(50);
                        z80io_SPI_Send32(0x00000000, &tmp[0]);
                        z80io_SPI_Send32(0x00000000, &tmp[1]);
                        pr_info("CPLD TX:%08x, RX:%08x,%08x\n", ioctlCmd.cpld.cmd, tmp[0], tmp[1]);

                        // Z80 can continue.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        break;

                    // Command to run a series of SOM to CPLD SPI tests.
                    case IOCTL_CMD_SPI_TEST:
                        // Stop the CPU prior to SPI testing.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        // Perform SPI Tests.
                        z80io_SPI_Test();
                      
                        // Z80 can continue.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        break;

                    // Command to run a series of SOM to CPLD Parallel Bus tests.
                    case IOCTL_CMD_PRL_TEST:
                        // Stop the CPU prior to SPI testing.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        // Perform Parallel Bus tests.
                        z80io_PRL_Test();
                       
                        // Z80 can continue.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        break;

                    // Command to run a series of Z80 host memory tests to assess the performance of the SOM->CPLD interface.
                    case IOCTL_CMD_Z80_MEMTEST:
                        // Stop the CPU prior to Host memory testing.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        // Perform host memory tests.
                        z80io_Z80_TestMemory();
                       
                        // Z80 can continue.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        break;

                    default:
                        break;
                }
            }
            break;

        default:
            printk("IOCTL - Unhandled Command (%08x)\n", ioctlCmd.cmd);
            break;

    }
    return 0;
}

// Structure to declare public API methods.
// Standard Linux device driver structure to declare accessible methods within the driver.
static const struct file_operations z80drv_fops = {
    .open           = z80drv_open,
    .read           = z80drv_read,
    .write          = z80drv_write,
    .release        = z80drv_release,
    .mmap           = z80drv_mmap,
    .unlocked_ioctl = z80drv_ioctl,
    .owner          = THIS_MODULE,
};

// Initialisation.
// This is the entry point into the device driver when loaded into the kernel.
// The method intialises any required hardware (ie. GPIO's, SPI etc), memory and the Z80
// Emulation. It also allocates the Major and Minor device numbers and sets up the
// device in /dev.
static int __init ModuleInit(void)
{
    // Locals.
    int      idx;
    int      ret = 0;    

    // Setup the Z80 handlers.
    Z80CPU.context      = z80_context;
    Z80CPU.fetch        = z80_fetch;
    Z80CPU.fetch_opcode = z80_fetch_opcode;
    Z80CPU.read         = z80_read;
    Z80CPU.write        = z80_write;
    Z80CPU.nop          = z80_nop;
    Z80CPU.in           = z80_in;
    Z80CPU.out          = z80_out;
    Z80CPU.halt         = z80_halt;
    Z80CPU.nmia         = z80_nmia;
    Z80CPU.inta         = z80_inta;
    Z80CPU.int_fetch    = z80_intFetch;
    Z80CPU.ld_i_a       = z80_ldia;
    Z80CPU.ld_r_a       = z80_ldra;
    Z80CPU.reti         = z80_reti;
    Z80CPU.retn         = z80_retn;
    Z80CPU.illegal      = z80_illegal;

    mutex_init(&Z80DRV_MUTEX);

    // Get device Major number.
    major = register_chrdev(0, DEVICE_NAME, &z80drv_fops);
    if (major < 0) {
        pr_info("z80drv: fail to register major number!");
        ret = major;
        goto initExit;
    }

    class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(class)){ 
        unregister_chrdev(major, DEVICE_NAME);
        pr_info("z80drv: failed to register device class");
        ret = PTR_ERR(class);
        goto initExit;
    }

    device = device_create(class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        class_destroy(class);
        unregister_chrdev(major, DEVICE_NAME);
        ret = PTR_ERR(device);
        goto initExit;
    }

    // Allocate the Z80 memory to be shared between this kernel space driver and the controlling user space application.
    Z80Ctrl = (t_Z80Ctrl *)kmalloc(sizeof(t_Z80Ctrl), GFP_KERNEL); 
    if (Z80Ctrl == NULL) 
    {
        pr_info("z80drv: failed to allocate memory!");
        ret = -ENOMEM; 
        goto initExit;
    }

    // Initialise the hardware to host interface.
    z80io_init();

    // Initialise the virtual RAM from the HOST DRAM. This is to maintain compatibility as some applications (in my experience) have 
    // bugs, which Im putting down to not initialising variables. The host DRAM is in a pattern of 0x00..0x00, 0xFF..0xFF repeating
    // when first powered on.
    for(idx=0; idx < Z80_VIRTUAL_RAM_SIZE; idx++)
    {
#if(TARGET_HOST_MZ700 == 1)
        if(idx >= 0x1000 && idx < 0xD000)
        {
            SPI_SEND32((uint32_t)idx << 16 | CPLD_CMD_READ_ADDR);
            while(CPLD_READY() == 0);
            Z80Ctrl->memory[idx] = z80io_PRL_Read8(1);
        } else
        {
            Z80Ctrl->memory[idx] = 0x00;
        }
#endif
#if(TARGET_HOST_MZ2000 == 1)
        if(idx >= 0x8000 && idx < 0x10000)
        {
            SPI_SEND32((uint32_t)idx << 16 | CPLD_CMD_READ_ADDR);
            while(CPLD_READY() == 0);
            Z80Ctrl->memory[idx-0x8000] = z80io_PRL_Read8(1);
        } else
        {
            if(idx >= 0x0000 && idx < 0x8000)
                Z80Ctrl->memory[idx+0x8000] = 0x00;
            else
                Z80Ctrl->memory[idx] = 0x00;
        }
#endif
    }

#if(TARGET_HOST_MZ700 == 1)
    Z80Ctrl->memory[0x1200] = 0x01;
    Z80Ctrl->memory[0x1201] = 0x86;
    Z80Ctrl->memory[0x1202] = 0xf2;
    Z80Ctrl->memory[0x1203] = 0x3e;
    Z80Ctrl->memory[0x1204] = 0x15;
    Z80Ctrl->memory[0x1205] = 0x3d;
    Z80Ctrl->memory[0x1206] = 0x20;
    Z80Ctrl->memory[0x1207] = 0xfd;
    Z80Ctrl->memory[0x1208] = 0x0b;
    Z80Ctrl->memory[0x1209] = 0x78;
    Z80Ctrl->memory[0x120a] = 0xb1;
    Z80Ctrl->memory[0x120b] = 0x20;
    Z80Ctrl->memory[0x120c] = 0xf6;
    Z80Ctrl->memory[0x120d] = 0xc3;
    Z80Ctrl->memory[0x120e] = 0x00;
    Z80Ctrl->memory[0x120f] = 0x00;
#endif

    // Copy the host BIOS into the Virtual ROM.
    for(idx=0; idx < Z80_VIRTUAL_ROM_SIZE; idx++)
    {
        SPI_SEND32((uint32_t)idx << 16 | CPLD_CMD_READ_ADDR);
        while(CPLD_READY() == 0);
        Z80Ctrl->memory[Z80_VIRTUAL_RAM_SIZE+idx] = z80io_PRL_Read8(1);
    }

#if(TARGET_HOST_MZ2000 == 1)
    Z80Ctrl->lowMemorySwap    = 1;
#endif
  
    // Setup Refresh so that an automatic refresh mode is performed by the CPLD whilst running in Virtual memory. This is necessary as opcode fetches from
    // host memory, by the CPLD, normally performs the refresh and when running in virtual memory, these refresh cycles arent performed.
    Z80Ctrl->refreshDRAM = 0;

    // Setup the governor delay, it is the delay per opcode fetch to restrict the Z80 CPU to a given speed.
    Z80Ctrl->cpuGovernorDelayROM = INSTRUCTION_DELAY_ROM_3_54MHZ;
    Z80Ctrl->cpuGovernorDelayRAM = INSTRUCTION_DELAY_RAM_3_54MHZ;

    // Setup the default Page Mode. This is needed if an event such as a reset occurs which needs to return the page and iotable back to default.
    Z80Ctrl->defaultPageMode = USE_VIRTUAL_RAM;

    // Setup memory profile to use internal virtual RAM (SOM kernel RAM rather than HOST DRAM).
    setupMemory(Z80Ctrl->defaultPageMode);

    // Initialse run control.
    mutex_init(&Z80RunModeMutex);
    mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);

    // Setup the address and port history for communicating addresses to the CPLD. Used to shorten the instruction length to increase latency.
    Z80Ctrl->z80PrevAddr = 0xFFFF;
    Z80Ctrl->z80PrevPort = 0xFFFF;

    // Initialse hotkey detection variables.
    Z80Ctrl->keyportStrobe    = 0x00;
    Z80Ctrl->keyportShiftCtrl = 0x00;
    Z80Ctrl->keyportHotKey    = 0x00;

    // PC to start and power on the CPU
    Z80_PC(Z80CPU) = 0;
    z80_power(&Z80CPU, TRUE);

    // Create thread to run the Z80 cpu.
    kthread_z80 = kthread_create(thread_z80, &threadId_z80, "kthread_z80");
    if(kthread_z80 != NULL)
    {
        printk("kthread - Thread Z80 was created, waking...!\n");
        kthread_bind(kthread_z80, 1);
        wake_up_process(kthread_z80);
    }
    else {
        printk("kthread - Thread Z80 could not be created!\n");
        ret = -1;
        goto initExit;
    }

initExit: 
    return ret;
}

// Exit 
// This method is called when the device driver is removed from the kernel with the rmmod command.
// It is responsible for closing and freeing all allocated memory, terminating all threads and removing
// the device from the /dev directory.
static void __exit ModuleExit(void) 
{
    // Stop the internal threads.
  //printk("kthread - Stop Z80 thread\n");
    kthread_stop(kthread_z80);

    // Return the memory used for the Z80 'virtual memory' and control variables.
    kfree(Z80Ctrl);

    // Nothing to be done for the hardware.

    // Cleanup and remove the device.
    mutex_destroy(&Z80DRV_MUTEX); 
    device_destroy(class, MKDEV(major, 0));  
    class_unregister(class);
    class_destroy(class); 
    unregister_chrdev(major, DEVICE_NAME);
    
  //pr_info("z80drv: unregistered!\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
