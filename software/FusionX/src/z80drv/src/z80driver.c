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
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//                  (c) 1999-2023 Manuel Sainz de Baranda y Goñi
//
// History:         Oct 2022 - v1.0   Initial write of the z80 kernel driver software.
//                  Jan 2023 - v1.1   Added MZ-2000/MZ-80A modes. There are serious limitations with the
//                                    SSD202 I/O. The I/O Bus appears to run at 72MHz and the GPIO bits
//                                    are split across 2x16 registers per bit. This limits 8bit read 
//                                    speed to < 2MB/s, write speed slower due to select signal. Thus it
//                                    is not feasible to run a program in the host memory at full speed.
//                                    Virtual (Kernel) memory is used for all programs and host is only
//                                    accessed for specific reasons, such as the MZ-80A FDD Bios which 
//                                    changes according to state of READY signal. I/O operations have to
//                                    use lookahead during fetch cycle to steal time in order to meet timings.
//                                    If SigmaStar make a newer SSD or an alternative becomes available which
//                                    has I/O bus running at 4x speed or has 32bit per cycle GPIO access then
//                                    the design needs to be upgraded to fulfill the idea of running programs
//                                    in host memory at full speed.
//                  Feb 2023 - v1.2   Added MZ-80A Rom Filing System device driver. This allows the FusionX
//                                    hosted in an MZ-80A to run the original RFS Monitor and software.
//                  Feb 2023 - v1.3   Added tranZPUter SW device driver. This allows the FusionX hosted
//                                    in any supported host to run TZFS and the updated applications
//                                    such as CP/M, SA-5510 Basic, MS-Basic etc. Adding this device driver
//                                    prepares the ground to add the SOM GPU as the Video emulation of
//                                    the Sharp machines.
//                  Mar 2023 - v1.4   With the advent of the PCW-8XXX series, seperated distinct machine
//                                    configurations into device driver modules, which are only built
//                                    if same as the target machine. This then allows the z80 driver to be
//                                    a vanilla Z80 or customisations for hosts pulled in.
//                  Apr 2023 - v1.4.1 Completed MZ2000 mode to work with arbiter and ttymz.
//                  May 2023 - v1.5   Added MZ1500 modes.
//
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
MODULE_LICENSE(DRIVER_LICENSE);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_INFO(versiondate, DRIVER_VERSION_DATE);
MODULE_INFO(copyright, DRIVER_COPYRIGHT);

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

// Include the virtual hardware drivers.
#if(TARGET_HOST_MZ80A == 1)
  #include "z80vhw_mz80a.c"
#endif
#if(TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1)
  #include "z80vhw_rfs.c"
#endif
#if(TARGET_HOST_MZ700 == 1)
  #include "z80vhw_mz700.c"
#endif
#if(TARGET_HOST_MZ1500 == 1)
  #include "z80vhw_mz1500.c"
#endif
#if(TARGET_HOST_MZ2000 == 1)
  #include "z80vhw_mz2000.c"
#endif
#if(TARGET_HOST_PCW == 0)
  #include "z80vhw_tzpu.c"
#endif
#if(TARGET_HOST_PCW == 1)
  #include "z80vhw_pcw.c"
#endif

//-------------------------------------------------------------------------------------------------------------------------------
//
// Host Memory and I/O Mapping and Execution Logic.
//
// Methods are all static and inline to maximise performance.
//
//-------------------------------------------------------------------------------------------------------------------------------

// Method to decode an address and make any system memory map changes as required.
//
static inline void decodeMemoryMapSetup(zuint16 address, zuint8 data, uint8_t ioFlag, uint8_t readFlag)
{
    // Locals.

    // If the RFS module is enabled, it must set the map otherwise use the default handler.
    //
  #if(TARGET_HOST_MZ80A == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ80A)
    {
        mz80aDecodeMemoryMapSetup(address, data, ioFlag, readFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ700 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ700)
    {
        mz700DecodeMemoryMapSetup(address, data, ioFlag, readFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ1500 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ1500)
    {
        mz1500DecodeMemoryMapSetup(address, data, ioFlag, readFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ2000 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ2000)
    {
        mz2000DecodeMemoryMapSetup(address, data, ioFlag, readFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_RFS)
    {
        rfsDecodeMemoryMapSetup(address, data, ioFlag, readFlag);
    } else
  #endif
  #if(TARGET_HOST_PCW == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_PCW)
    {
        pcwDecodeMemoryMapSetup(address, data, ioFlag, readFlag);
    } else
  #endif
  #if(TARGET_HOST_PCW == 0)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_TZPU)
    {
        tzpuDecodeMemoryMapSetup(address, data, ioFlag, readFlag);
    } else
  #endif
    {
    }

    return;
}

// Method to decode address and invoke virtual RAM, ROM or hardware to handle accordingly.
//
static inline zuint8 readVirtual(zuint16 address, uint8_t ioFlag)
{
    // Locals.
    //
    zuint8    data = 0xFF;

    // Invoke the specific hardware emulation.
    //
  #if(TARGET_HOST_MZ80A == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ80A)
    {
        data = mz80aRead(address, ioFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ700 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ700)
    {
        data = mz700Read(address, ioFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ1500 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ1500)
    {
        data = mz1500Read(address, ioFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ2000 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ2000)
    {
        data = mz2000Read(address, ioFlag);
    } else
  #endif
  #if(TARGET_HOST_PCW == 1)
    if((Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_PCW))
    {
        data = pcwRead(address, ioFlag);
    } else
  #endif

  #if(TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1)
    // RFS only has memory mapped registers.
    if((Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_RFS) && ioFlag == 0)
    {
        data = rfsRead(address, ioFlag);
    } else
  #endif

  #if(TARGET_HOST_PCW == 0)
    if((Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_TZPU))
    {
        data = tzpuRead(address, ioFlag);
    } else
  #endif

    {
    }

    return(data);
}

// Method to decode address and invoke virtual ROM, RAM or hardware to handle accordingly.
//
static inline void writeVirtual(zuint16 address, zuint8 data, uint8_t ioFlag)
{
    // Locals.
  
    // Invoke the specific hardware emulation.
    //
  #if(TARGET_HOST_MZ80A == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ80A)
    {
        mz80aWrite(address, data, ioFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ700 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ700)
    {
        mz700Write(address, data, ioFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ1500 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ1500)
    {
        mz1500Write(address, data, ioFlag);
    } else
  #endif
  #if(TARGET_HOST_MZ2000 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ2000)
    {
        mz2000Write(address, data, ioFlag);
    } else
  #endif
  #if(TARGET_HOST_PCW == 1)
    if((Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_PCW))
    {
        pcwWrite(address, data, ioFlag);
    } else
  #endif

  #if(TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1)
    // RFS only has memory mapped registers.
    if((Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_RFS) && ioFlag == 0)
    {
        rfsWrite(address, data, ioFlag);
    } else
  #endif

  #if(TARGET_HOST_PCW == 0)
    if((Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_TZPU))
    {
        tzpuWrite(address, data, ioFlag);
    } else
  #endif

    {
    }

    return;
}

// The first design, using the SSD202 from SigmaStar suffers from very slow I/O, ie. reading an 8bit GPIO value, in a tight loop
// attains just short of 2MB/s as you need to read 8xindividual I/O bits and the I/O subsystem is running much slower than the CPU (~72MHz).
// Due to this, we use the SPI as it is quicker but we need to look ahead and send off requests to the CPLD ahead of time to minimise delay in 
// time critical operations such as Floppy Disk read/write.
//
// This method attempts to decode the current opcode and if it is a hardware operation, make the request ahead of the Z80 emulator.
static inline uint8_t lookAhead(zuint16 address, zuint8 opcode, zuint8 opcode2)
{
    //  IN A,(C) ED 78
    //  IN B,(C) ED 40
    //  IN C,(C) ED 48
    //  IN D,(C) ED 50
    //  IN E,(C) ED 58
    //  IN H,(C) ED 60
    //  IN L,(C) ED 68
    if( opcode == 0xED && (opcode2 == 0x78 || opcode2 == 0x40 || opcode2 == 0x48 || opcode2 == 0x50 || opcode2 == 0x58 || opcode2 == 0x60 || opcode2 == 0x68) )
    {
        if(isPhysicalIO(Z80CPU.bc.uint16_value))
        {
            if((Z80CPU.bc.uint16_value&0x00ff) < 8)
            {
                SPI_SEND_8(CPLD_CMD_READIO_ADDR + (Z80CPU.bc.uint16_value&0x00ff));
            } else
            {
                //SPI_SEND_16((Z80CPU.bc.uint16_value&0x00ff) << 8  | CPLD_CMD_READIO_ADDR);
                SPI_SEND_32(Z80CPU.bc.uint16_value, CPLD_CMD_READIO_ADDR);
            }
            Z80Ctrl->ioReadAhead = 1;
        }
    }
    //  IND   ED AA
    //  INDR  ED BA
    //  INI   ED A2
    //  INIR  ED B2
    else if( opcode == 0xED && (opcode2 == 0xAA || opcode2 == 0xBA || opcode2 == 0xA2 || opcode2 == 0xB2) )
    {
        if(isPhysicalIO(Z80CPU.bc.uint16_value))
        {
          #if(TARGET_HOST_PCW == 0)
            if((Z80CPU.bc.uint16_value&0x00ff) < 8)
            {
                SPI_SEND_8(CPLD_CMD_READIO_ADDR + (Z80CPU.bc.uint16_value&0x00ff));
            } else
            {
                SPI_SEND_32(Z80CPU.bc.uint16_value, CPLD_CMD_READIO_ADDR);
            }
            Z80Ctrl->ioReadAhead = 1;
          #else
            if((Z80CPU.bc.uint16_value&0x00ff) < 8)
            {
                SPI_SEND_8(CPLD_CMD_READIO_WRITE_ADDR + (Z80CPU.bc.uint16_value&0x00ff));
            } else
            {
                //SPI_SEND_32(Z80CPU.bc.uint16_value, CPLD_CMD_READIO_WRITE_ADDR);
                SPI_SEND_16((Z80CPU.bc.uint16_value&0x00ff) << 8  | CPLD_CMD_READIO_WRITE_ADDR);
            }
        
            // Send destination address.
            SPI_SEND_P_16(Z80CPU.hl.uint16_value);
            Z80Ctrl->ioReadAhead = 2;
          #endif
        }
    }
    // IN A,(N) DB XX
    else if(opcode == 0xDB)
    {
        if(isPhysicalIO(opcode2))
        {
            if(opcode2 < 8)
            {
                SPI_SEND_8(CPLD_CMD_READIO_ADDR + opcode2);
            } else
            {
                SPI_SEND_32(((Z80CPU.bc.uint16_value & 0xff00) | opcode2), CPLD_CMD_READIO_ADDR);
              //  SPI_SEND_16(opcode2 << 8  | CPLD_CMD_READIO_ADDR);
            }
            //SPI_SEND_32(((Z80CPU.bc.uint16_value & 0xff00) | opcode2), CPLD_CMD_READIO_ADDR);
            Z80Ctrl->ioReadAhead = 1;
        }
    }

    //  OUT (C),A  ED 79
    //  OUT (C),B  ED 41
    //  OUT (C),C  ED 49
    //  OUT (C),D  ED 51
    //  OUT (C),E  ED 59
    //  OUT (C),H  ED 61
    //  OUT (C),L  ED 69
    //  OTDR  ED BB
    //  OTIR  ED B3
    //  OUTD  ED AB
    //  OUTI  ED A3
    else if( opcode == 0xED && (opcode2 == 0x79 || opcode2 == 0x41 || opcode2 == 0x49 || opcode2 == 0x51 || opcode2 == 0x59 || opcode2 == 0x61 || opcode2 == 0x69 || opcode2 == 0xBB || opcode2 == 0xB3 || opcode2 == 0xAB || opcode2 == 0xA3) )
    {
        uint16_t data = ((opcode2 == 0x79 ? Z80CPU.af.uint8_values.at_1 :
                          opcode2 == 0x41 ? Z80CPU.bc.uint8_values.at_1 :
                          opcode2 == 0x49 ? Z80CPU.bc.uint8_values.at_0 :
                          opcode2 == 0x51 ? Z80CPU.de.uint8_values.at_1 :
                          opcode2 == 0x59 ? Z80CPU.de.uint8_values.at_0 :
                          opcode2 == 0x61 ? Z80CPU.hl.uint8_values.at_1 :
                          opcode2 == 0x69 ? Z80CPU.hl.uint8_values.at_0 :
                          isVirtualROM((Z80CPU.hl.uint16_value)) ? readVirtualROM((Z80CPU.hl.uint16_value)) : readVirtualRAM((Z80CPU.hl.uint16_value))) << 8) | CPLD_CMD_WRITEIO_ADDR;

        if(isPhysicalIO(Z80CPU.bc.uint16_value))
        {
            if((Z80CPU.bc.uint16_value&0x00ff) < 8)
            {
                SPI_SEND_16(data + (Z80CPU.bc.uint16_value&0x00ff));
            } else
            {
                SPI_SEND_32(Z80CPU.bc.uint16_value, data);
            }
            Z80Ctrl->ioWriteAhead = 1;
        }
    }

    // OUT (N),A  D3 XX
    else if(opcode == 0xD3)
    {
        if(isPhysicalIO(((Z80CPU.bc.uint16_value & 0xff00) | opcode2)))
        {
            if(opcode2 < 8)
            {
                SPI_SEND_16((Z80CPU.af.uint8_values.at_1 << 8) | (CPLD_CMD_WRITEIO_ADDR + opcode2));
            } else
            {
                SPI_SEND_32(((Z80CPU.bc.uint16_value & 0xff00) | opcode2), (Z80CPU.af.uint8_values.at_1 << 8) | CPLD_CMD_WRITEIO_ADDR);
            }
            Z80Ctrl->ioWriteAhead = 1;
        }
    }

    return(Z80Ctrl->ioReadAhead | Z80Ctrl->ioWriteAhead);
}

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
    Z_UNUSED(context)
       
    // Only read if the address is in physical RAM.
  #if(TARGET_HOST_PCW == 0)
    if(isPhysical(address))
  #elif(TARGET_HOST_MZ1500 == 1)
    // MZ-1500 take into account PCG being active, always go to hardware when active. Cannot use the map because
    // this can change during PCG active mode and must be reflected when PCG is deactivated.
    if(isPhysicalHW(address) || (Z80Ctrl->pcgMode == 1 && address >= 0xD000))
  #else
    if(isPhysicalHW(address))
  #endif
    {
        // Commence cycle to retrieve the data from Real RAM.
        SPI_SEND_32(address, CPLD_CMD_READ_ADDR);

        // Decode address to action any host specific memory map changes.
        decodeMemoryMapSetup(address, 0, 0, true);

        // Data arrived?
        while(CPLD_READY() == 0);
        data = z80io_PRL_Read();
      
        // Pause until the Last T-State is detected.
        //while(CPLD_LAST_TSTATE() == 0);
    }
    else if(isVirtual(address))
    {
        // Decode the address and if virtual RAM, ROM or logic exists, invoke it.
        data = readVirtual(address, 0);
    }

  #if (TARGET_HOST_MZ80A == 1)
    // Keyport data? Store.
    if(isHW(address) && address == 0xE001 && (Z80Ctrl->keyportStrobe & 0x0f) == 0)
    {
        Z80Ctrl->keyportShiftCtrl = (data & 0x80) == 0 ? 0x01 : 0x00;
    } else
    // If CTRL key pressed followed by a key on Row 8/9 (Keypad), set Hotkey for later processing.
    if(isHW(address) && address == 0xE001 && Z80Ctrl->keyportShiftCtrl == 1)
    {
        if((Z80Ctrl->keyportStrobe & 0x0f) == 8 && (data & 0x1D) != 0x1D)
        {
            Z80Ctrl->keyportHotKey = (data & 0x01) == 0 ? HOTKEY_ORIGINAL :
                                     (data & 0x04) == 0 ? HOTKEY_RFS40    :
                                     (data & 0x08) == 0 ? HOTKEY_RFS80    :
                                     (data & 0x10) == 0 ? HOTKEY_LINUX    : 0x00;
            Z80Ctrl->keyportTrigger = Z80Ctrl->keyportHotKey;
        } else
        if((Z80Ctrl->keyportStrobe & 0x09) == 9 && (data & 0x04) != 0x04)
        {
            Z80Ctrl->keyportHotKey = HOTKEY_TZFS;
            Z80Ctrl->keyportTrigger = 1;
        } else
        {
            Z80Ctrl->keyportTrigger = 0;
        }
    }
  #elif (TARGET_HOST_MZ700 == 1)
    // Keyport data? Store.
    if(isHW(address) && address == 0xE001 && (Z80Ctrl->keyportStrobe & 0x0f) == 8)
    {
        Z80Ctrl->keyportShiftCtrl = (data & 0x40) == 0 ? 0x01 : 0x00;
    } else
    if(isHW(address) && address == 0xE001 && Z80Ctrl->keyportShiftCtrl == 1)
    {
        if((Z80Ctrl->keyportStrobe & 0x0f) == 5 && (data & 0xF0) != 0xF0)
        {
            Z80Ctrl->keyportHotKey = (data & 0x80) == 0 ? HOTKEY_ORIGINAL :
                                     (data & 0x40) == 0 ? HOTKEY_RFS40    :
                                     (data & 0x20) == 0 ? HOTKEY_TZFS     :
                                     (data & 0x10) == 0 ? HOTKEY_LINUX    : 0x00;
            Z80Ctrl->keyportTrigger = Z80Ctrl->keyportHotKey;
        } else
        {
            Z80Ctrl->keyportTrigger = 0;
        }
    }
  #elif (TARGET_HOST_MZ1500 == 1)
    // Keyport data? Store.
    if(isHW(address) && address == 0xE001 && (Z80Ctrl->keyportStrobe & 0x0f) == 8)
    {
        Z80Ctrl->keyportShiftCtrl = (data & 0x40) == 0 ? 0x01 : 0x00;
    } else
    if(isHW(address) && address == 0xE001 && Z80Ctrl->keyportShiftCtrl == 1)
    {
        if((Z80Ctrl->keyportStrobe & 0x0f) == 5 && (data & 0xF0) != 0xF0)
        {
            Z80Ctrl->keyportHotKey = (data & 0x80) == 0 ? HOTKEY_ORIGINAL :
                                     (data & 0x40) == 0 ? HOTKEY_RFS40    :
                                     (data & 0x20) == 0 ? HOTKEY_TZFS     :
                                     (data & 0x10) == 0 ? HOTKEY_LINUX    : 0x00;
            Z80Ctrl->keyportTrigger = Z80Ctrl->keyportHotKey;
        } else
        {
            Z80Ctrl->keyportTrigger = 0;
        }
    }
  #elif (TARGET_HOST_MZ2000 == 1)

  #endif

  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 3)
    {
        pr_info("Read:%04x,%02x,%d\n", address, data, CPLD_Z80_INT());
    }
  #endif

    return(data);
}

// Method to write a byte to physical hardware or internal virtual memory or devices.
// The page table indicates the target and the write is processed accordingly.
static void z80_write(void *context, zuint16 address, zuint8 data)
{
    // Locals.
    Z_UNUSED(context)

  #if (TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1)
    // To detect Hotkey presses, we need to store the keyboard strobe data and on keydata read.
    if(isHW(address) && address == 0xE000)
    {
        Z80Ctrl->keyportStrobe = data;
    }
  #endif

    // Write to physical host?
    if(Z80Ctrl->ioReadAhead == 2)
    {
        // Write-thru to virtual memory if we update real memory.
        if(isPhysicalRAM(address))
            writeVirtualRAM(address, data);
        Z80Ctrl->ioReadAhead = 0;
    } else 

  #if(TARGET_HOST_MZ1500 == 1)
    // MZ-1500 take into account PCG being active, always go to hardware when active. Cannot use the map because
    // this can change during PCG active mode and must be reflected when PCG is deactivated.
    if(isPhysical(address) || (Z80Ctrl->pcgMode == 1 && address >= 0xD000))
  #else
    if(isPhysical(address))
  #endif
    {
        // Commence cycle to write the data to real RAM.
        SPI_SEND_32(address, data << 8 | CPLD_CMD_WRITE_ADDR);
        //SPI_SEND32(address << 16 | data << 8 | CPLD_CMD_WRITE_ADDR);
        
        // Write-thru to virtual memory if we update real memory.
        if(isPhysicalRAM(address))
            writeVirtualRAM(address, data);
      
        // Decode address to action any host specific memory map changes.
        decodeMemoryMapSetup(address, data, 0, false);

        // Pause until the Last T-State is detected.
        //while(CPLD_LAST_TSTATE() == 0);
    }
    // Virtual ROM, technically isnt writable, but some devices such as the TZPU use RAM as ROM and mask it 
    // according to operating mode. 
    // Virtual Hardware is driver dependent, common method called to write to ROM/HW.
    // Virtual RAM is generally a direct write but any driver may change the action.
    else if(isVirtual(address))
    {
        // Decode the address and process.
        writeVirtual(address, data, 0);
    }
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 3)
    {
        pr_info("Write:%04x,%02x,%d\n", address, data, CPLD_Z80_INT());
    }
  #endif
    return;
}

// Primary Opcode fetch method. This method is called each time a single or multi-byte opcode is
// encountered. Opcode data is retrieved via the z80_fetch method.
//
// Depending on the address and the configured page map, the opcode is fetched from hardware
// or internal virtual memory. As this method is the primary timing method for Z80 instructions
// (read/write methods dont affect the timing so much as long as they operate in less than the read/write
// cycle of an original Z80).
//
// Initially the timing on the virtual memory is set by a governor delay but this will be updated to a more
// precise M/T-State cycle per instruction type delay.
static zuint8 z80_fetch_opcode(void *context, zuint16 address)
{
    // Locals.
    zuint8            opcode    = 0x00;
  #if(TARGET_HOST_PCW == 1)
    static zuint8     opcodeNxt = 0x00;
  #endif
    volatile uint32_t idx;               // Leave as volatile otherwise optimiser will optimise out the delay code.
    Z_UNUSED(context)

    // Normally only opcode fetches occur in RAM but allow any physical address as it could be a Z80 programming trick.
    
    // PCW machines operate with write-thru < 128K and virtual >= 128K
#if(TARGET_HOST_PCW == 0)
  #if(TARGET_HOST_MZ80A == 1)
    // MZ-80A floppy disk controller uses address 0xF3FE/0xF7FE to direct program flow according to READY state of the 
    // MB8866 controller.
    if(isPhysical(address) || address == 0xF3FE)
  #else
    if(isPhysical(address))
  #endif
    {
        // Commence cycle to fetch the opcode from potentially Real RAM albeit it could be any physical hardware.
        SPI_SEND_32(address, CPLD_CMD_FETCH_ADDR);

        // Set up to bypass governor if this is floppy access.
        if(address == 0xF3FE)
            Z80Ctrl->governorSkip = INSTRUCTION_GOVERNOR_IO_SKIP;

        // Wait for the data and retrieve.
        while(CPLD_READY() == 0);
        opcode = z80io_PRL_Read();
    } else
#endif

  #if(TARGET_HOST_PCW == 1)
    // Virtual fetches only occur in memory as we are not emulating original hardware.
    if(isVirtualMemory(address))
    {
        // Read the opcode and operand, operand needed for lookahead decisions.
        if(isVirtualROM(address))
        {
            opcode    = readVirtualROM(address);
            opcodeNxt = readVirtualROM((address+1));
        } else
        {
            opcode    = readVirtualRAM(address);
            opcodeNxt = readVirtualRAM((address+1));
        }
       
        // Multibyte opcodes, if triggered by an initial I/O read/write ahead operation, return immediately with the opcode.
        if(Z80Ctrl->ioReadAhead == 0 && Z80Ctrl->ioWriteAhead == 0)
        {
            // Check if this operation is I/O or known memory I/O so we can look ahead to optimise sending request to CPLD.
            if(!lookAhead(address, opcode, opcodeNxt) && !Z80Ctrl->governorSkip)
            {
                // Delay loop is to govern execution speed to the same as the original host. Timing is primarily based on the main opcode
                // fetch so long as the additional opcode parameter and read/write take less time than original host.
                if(isVirtualROM(address))
                {
                    //    opcode = readVirtualROM(address);
                    for(idx=0; idx < Z80Ctrl->cpuGovernorDelayROM; idx++);
                } else
                {
                    //    opcode = readVirtualRAM(address);
                    for(idx=0; idx < Z80Ctrl->cpuGovernorDelayRAM; idx++);
                }
            } else
            {
                // If skip is active, we bypass the delay governor in order to catch up on time wasted with the I/O overhead of the SSD202.
                if(Z80Ctrl->governorSkip)
                {
                    Z80Ctrl->governorSkip--;
                } else
                {
                    Z80Ctrl->governorSkip = INSTRUCTION_GOVERNOR_IO_SKIP;
                }
            }
        } else
        {
            Z80Ctrl->ioReadAhead = 0;
            Z80Ctrl->ioWriteAhead = 0;
        }
    }
  #else
    // Apply delay if required to match emulated CPU to host speed.
    if(isVirtualROM(address))
    {
        opcode = readVirtualROM(address);
        if(Z80Ctrl->governorSkip)
            Z80Ctrl->governorSkip--;
        else
            for(idx=0; idx < Z80Ctrl->cpuGovernorDelayROM; idx++);
    } else
    if(isVirtualRAM(address))
    {
        opcode = readVirtualRAM(address);
        if(Z80Ctrl->governorSkip)
            Z80Ctrl->governorSkip--;
        else
            for(idx=0; idx < Z80Ctrl->cpuGovernorDelayRAM; idx++);
    }
  #endif

  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 3)
    {
       // if(address < 0xF036 || address > 0xF197)
       if(address >= 0x0000)
       {
           pr_info("Fetch:%04x,%02x,%d,%d,%d\n", address, opcode, Z80Ctrl->ioReadAhead, Z80Ctrl->ioWriteAhead, CPLD_Z80_INT());
           udelay(2000);
           udelay(1000);
       }

        // If max level, add delay so that the kernel log doesnt overflow.
        if(Z80Ctrl->debug >= 15)
        {
           udelay(2000);
        }
    }
    //if(address >= 0xE800) pr_info("Fetch:%04x,%02x\n", address, opcode);
  #endif
    return(opcode);
}

// Method similar to z80_read, kept seperate to avoid additional what-if logic and doesnt require virtual hardware logic.
//
static zuint8 z80_fetch(void *context, zuint16 address)
{
    // Locals.
    //
    zuint8          data = 0x00;
    Z_UNUSED(context)
       
    // Normally only opcode fetches occur in RAM but allow any physical address as it could be a Z80 programming trick.
    if(isPhysical(address))
    {
        // Given the limitation of the SigmaStar I/O, it isnt possible to fetch all code from ROM real time, it is therefore cached
        // and read from cache.
        data = isPhysicalROM(address) ? readVirtualROM(address) : readVirtualRAM(address);
    } else
    if(isVirtualMemory(address))
    {
        // Retrieve data from virtual memory.
        data = isVirtualROM(address) ? readVirtualROM(address) : readVirtualRAM(address);
    }

    // Check for interrupts.
    if(CPLD_Z80_NMI() != 0)
    {
        z80_nmi(&Z80CPU);
    }
    z80_int(&Z80CPU, CPLD_Z80_INT() != 0);

  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 4)
    {
        if(address < 0xF036 || address > 0xF197)
            pr_info("FetchB:%04x,%02x,%d\n", address, data, CPLD_Z80_INT());
        udelay(2000);
    }
  #endif
    return(data);
}

// Method to perform a Z80 input operation. This normally goes to hardware and the CPLD executes the required cycle. 
// Some ports are dedicated virtual ports providing virtual services to the host computer/application. These are intercepted
// and processed in this driver.
static zuint8 z80_in(void *context, zuint16 port)
{
    // Locals.
    zuint8          value;
    Z_UNUSED(context)

    // Physical port go direct to hardware to retrieve value.
    if(isPhysicalIO(port))
    {
      #if(TARGET_HOST_PCW == 1)
        if(Z80Ctrl->ioReadAhead == 0)
        {
            // Commence cycle to retrieve the value from the I/O port. Port contains the 16bit BC value.
            SPI_SEND_32(port, CPLD_CMD_READIO_ADDR);
        }
      
        // Whilst waiting for the CPLD, we now determine if this is a memory management port and update the memory page if required.
        decodeMemoryMapSetup(port, 0, 1, true);

        if(Z80Ctrl->ioReadAhead != 2)
            Z80Ctrl->ioReadAhead = 0;
      #else
        // Commence cycle to retrieve the value from the I/O port. Port contains the 16bit BC value.
        SPI_SEND_32(port, CPLD_CMD_READIO_ADDR);
        Z80Ctrl->governorSkip = INSTRUCTION_GOVERNOR_IO_SKIP;
       
        // Whilst waiting for the CPLD, we now determine if this is a memory management port and update the memory page if required.
        decodeMemoryMapSetup(port, 0, 1, true);
      #endif

        // Finally ensure the data from the port is ready and retrieve it.
        while(CPLD_READY() == 0);
        value = z80io_PRL_Read();
      
      #if (TARGET_HOST_MZ2000 == 1)
        // Keyport data? Store.
        if((port&0xff) == 0xEA)
        {
            // If this is the CTRL key row, check the CTRL key.
            if((Z80Ctrl->keyportStrobe & 0x1f) == 0x1b)
            {
                Z80Ctrl->keyportShiftCtrl = (value & 0x08) == 0 ? 0x01 : 0x00;
                if(Z80Ctrl->keyportShiftCtrl == 1)
                {
                    Z80Ctrl->keyportTrigger = 0;
                }
            } else
            // If CTRL key is held and we scan the Function Key Row (not all keys mode), then action the pressed key.
            if(Z80Ctrl->keyportShiftCtrl == 1 && (Z80Ctrl->keyportStrobe & 0x1f) == 0x10 && (value&0x0f) != 0x0f)
            {
                Z80Ctrl->keyportHotKey = (value & 0x01) == 0 ? HOTKEY_ORIGINAL :
                                      // (value & 0x02) == 0 ? HOTKEY_RFS40    :
                                      // (value & 0x04) == 0 ? HOTKEY_TZFS     :
                                         (value & 0x08) == 0 ? HOTKEY_LINUX    : 0x00;
                Z80Ctrl->keyportTrigger = Z80Ctrl->keyportHotKey;
            }
        }
      #endif
    } else
    // Virtual I/O Port.
    if(isVirtualIO(port))
    {
        // Virtual I/O - call the handler.
        value = readVirtual(port, 1);
    }

  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 2) pr_info("z80_in:0x%x, 0x%x\n", port, value);
  #endif
  //  pr_info("z80_in:0x%x, 0x%x\n", port, value);
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
    Z_UNUSED(context) 

    // Physical port go direct to hardware to retrieve value.
    if(isPhysicalIO(port))
    {
      #if(TARGET_HOST_PCW == 1)
        // If byte has already been written during the fetch phase, skip.
        if(Z80Ctrl->ioWriteAhead == 0)
        {
            // Commence cycle to write the value to the I/O port. Port contains the 16bit BC value.
            SPI_SEND_32(port, value << 8 | CPLD_CMD_WRITEIO_ADDR);
        }
        Z80Ctrl->ioWriteAhead = 0;
     
        // Decode address to action any host specific memory map changes.
        decodeMemoryMapSetup(port, value, 1, false);
      #else
        // Commence cycle to write the value to the I/O port. Port contains the 16bit BC value.
        SPI_SEND_32(port, value << 8 | CPLD_CMD_WRITEIO_ADDR);
        Z80Ctrl->governorSkip = INSTRUCTION_GOVERNOR_IO_SKIP;
       
        // Decode address to action any host specific memory map changes.
        decodeMemoryMapSetup(port, value, 1, false);
      #endif

      #if (TARGET_HOST_MZ2000 == 1)
        // To detect Hotkey presses, we need to store the keyboard strobe data and on keydata read.
        if((port&0xff) == 0xE8)
        {
            Z80Ctrl->keyportStrobe = value;
        }
      #endif
    } else
    if(isVirtualIO(port))
    { 
        // Decode the address and if virtual logic exists, invoke and write to it.
        writeVirtual(port, value, 1);
    }

  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 2) pr_info("z80_out:0x%x, 0x%x\n", port, value);
  #endif
  //  pr_info("z80_out:0x%x, 0x%x\n", port, value);
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
            SPI_SEND_32(0x0000, CPLD_CMD_REFRESH);
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
    pr_info("z80_halt\n");
    SPI_SEND_32(0x0000, CPLD_CMD_HALT);
    Z80CPU.cycles = Z80_MAXIMUM_CYCLES;
}

// Methods below are not yet implemented, Work In Progress!
static zuint8 z80_context(void *context, zuint16 address)
{
    Z_UNUSED(context)
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 2) pr_info("z80_context\n");
  #endif
    return 0x00;
}
static zuint8 z80_nmia(void *context, zuint16 address)
{
    Z_UNUSED(context)
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 2) pr_info("z80_nmia\n");
  #endif
    return 0x00;
}
static zuint8 z80_inta(void *context, zuint16 address)
{
    Z_UNUSED(context)
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 2) pr_info("z80_inta\n");
  #endif
    return 0x00;
}
static zuint8 z80_intFetch(void *context, zuint16 address)
{
    Z_UNUSED(context)
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 2) pr_info("z80_int_fetch\n");
  #endif
    return 0x00;
}
static void z80_ldia(void *context)
{
    Z_UNUSED(context)
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 2) pr_info("z80_ldia\n");
  #endif
}
static void z80_ldra(void *context)
{
    Z_UNUSED(context)
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 2) pr_info("z80_ldra\n");
  #endif
}
static void z80_reti(void *context)
{
    Z_UNUSED(context)
    if(CPLD_Z80_INT() != 0)
    {
      #if(DEBUG_ENABLED & 1)
        if(Z80Ctrl->debug >= 2)
        {
            pr_info("LOCKUP:%d\n", CPLD_Z80_INT());
        }
      #endif
        z80_int(&Z80CPU, false);
    }
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 3) pr_info("z80_reti\n");
  #endif
}
static void z80_retn(void *context)
{
    Z_UNUSED(context)
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 3) pr_info("z80_retn\n");
  #endif
}
static zuint8 z80_illegal(void *context, zuint8 opcode)
{
    Z_UNUSED(context)
  #if(DEBUG_ENABLED & 1)
    if(Z80Ctrl->debug >= 3) pr_info("z80_illegal\n");
  #endif
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
        if(canRun) z80_run(&Z80CPU, 10);

        // Reset pressed?
        if(CPLD_RESET())
        {
            // Wait for release before restarting CPU.
            while(CPLD_RESET());

            resetZ80();
        }

        // Update state to indicate request has been actioned.
        mutex_lock(&Z80RunModeMutex);
        if(Z80RunMode == Z80_STOP)     Z80RunMode = Z80_STOPPED;
        if(Z80RunMode == Z80_PAUSE)    Z80RunMode = Z80_PAUSED;
        if(Z80RunMode == Z80_CONTINUE) Z80RunMode = Z80_RUNNING;
        if(Z80RunMode == Z80_RUNNING) canRun=1; else canRun=0;
        mutex_unlock(&Z80RunModeMutex);

      #if (TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1 || TARGET_HOST_MZ2000 == 1)
        // Hotkey pressed? Bring up user menu.
        if(Z80Ctrl->keyportTrigger != 0x00 && Z80Ctrl->keyportTriggerLast == 0)
        {
            z80menu();

            // Send signal to arbiter to change run mode.
            sendSignal(Z80Ctrl->arbTask, SIGUSR1);
            Z80Ctrl->keyportShiftCtrl = 0;

            // Suspend processing until arbiter sets up new environment.
            mutex_lock(&Z80RunModeMutex);
            Z80RunMode = Z80_STOPPED;
            canRun = 0;
            mutex_unlock(&Z80RunModeMutex);
        }
        Z80Ctrl->keyportTriggerLast = Z80Ctrl->keyportTrigger;
      #endif
    }

    // Release spinlock as we are unloading driver.
    spin_unlock_irqrestore(&spinLock, flags);
    pr_info("kthread - Z80 Thread %d finished execution!\n", t_nr);
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
    struct task_struct *task = get_current();

    // Is this the K64F de-registering?
    if(Z80Ctrl->ioTask == task) 
    {
        Z80Ctrl->ioTask = NULL;
        pr_info("I/O processor stopped.\n");
    } else
    // Is this the Arbiter de-registering?
    if(Z80Ctrl->arbTask == task) 
    {
        Z80Ctrl->arbTask = NULL;
        pr_info("Arbiter stopped.\n");
    } else
    {
        // Free up the mutex preventing more than one control process taking control at the same time.
        mutex_unlock(&Z80DRV_MUTEX);
    }
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
    struct task_struct *task = get_current();

    // I/O Processor?
    if(Z80Ctrl->ioTask == NULL && strcmp(task->comm, IO_PROCESSOR_NAME) == 0)
    {
        Z80Ctrl->ioTask = task;
        pr_info("Registering I/O Processor:%s\n", Z80Ctrl->ioTask->comm);
    } else
    if(Z80Ctrl->ioTask != NULL && strcmp(task->comm, IO_PROCESSOR_NAME) == 0)
    {
        pr_info("I/O Processor already registered, PID:%d\n", Z80Ctrl->ioTask->pid);
        ret = -EBUSY;
        goto out;
    } else
    // Arbiter?
    if(Z80Ctrl->arbTask == NULL && strcmp(task->comm, ARBITER_NAME) == 0)
    {
        Z80Ctrl->arbTask = task;
        pr_info("Registering Aribter:%s\n", Z80Ctrl->arbTask->comm);
    } else
    if(Z80Ctrl->arbTask != NULL && strcmp(task->comm, ARBITER_NAME) == 0)
    {
        pr_info("Arbiter already registered, PID:%d\n", Z80Ctrl->arbTask->pid);
        ret = -EBUSY;
        goto out;
    } else
    if(!mutex_trylock(&Z80DRV_MUTEX))
    {
        pr_alert("z80drv: Device busy!\n");
        ret = -EBUSY;
        goto out;
    }
    //pr_info("z80drv: Device successfully opened\n");
 
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
    int            idx;
    int            ret  = 0;
    struct page   *page = NULL;
    unsigned long  size = (unsigned long)(vma->vm_end - vma->vm_start);

    // Z80Ctrl?
    if(size >= sizeof(t_Z80Ctrl) && size <= (sizeof(t_Z80Ctrl)+0x1000))
    {
        // Map the memory and exit.
        page = virt_to_page((unsigned long)Z80Ctrl + (vma->vm_pgoff << PAGE_SHIFT)); 
        ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), size, vma->vm_page_prot);
        if (ret != 0)
        {
            goto out;
        }   
    }
    else if(size >= Z80_VIRTUAL_RAM_SIZE && size < (Z80_VIRTUAL_RAM_SIZE+0x1000))
    {
        // Map the memory and exit.
        page = virt_to_page((unsigned long)Z80Ctrl->ram + (vma->vm_pgoff << PAGE_SHIFT)); 
        ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), size, vma->vm_page_prot);
        if (ret != 0)
        {
            goto out;
        }   
    }
    // To distinguish a ROM map request from a RAM request, the ROM size is 1 page greater than actual memory.
    else if(size >= (Z80_VIRTUAL_ROM_SIZE+0x1000) && size < (Z80_VIRTUAL_ROM_SIZE+0x2000))
    {
        // Map the memory and exit.
        page = virt_to_page((unsigned long)Z80Ctrl->rom + (vma->vm_pgoff << PAGE_SHIFT)); 
        ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), size, vma->vm_page_prot);
        if (ret != 0)
        {
            goto out;
        }   
    }
    // Another one, as the memory bank page maps are allocated dynamically, need to send a size which indicates which memory block to map. This is done by the size of a memory map
    // added to it the map slot as 0x1000 per slot.
    else if(size >= ((MEMORY_BLOCK_SLOTS*sizeof(uint32_t)) + 0x1000) && size < ((MEMORY_BLOCK_SLOTS*sizeof(uint32_t)) + (MEMORY_MODES * 0x1000)))
    {
        // Loop through all the memory page slots, if active and the size is in range, then map the memory to user space.
        for(idx=0; idx < MEMORY_MODES; idx++)
        {
            if(size >= ((MEMORY_BLOCK_SLOTS*sizeof(uint32_t)) + ((idx+1)*0x1000)) && size < ((MEMORY_BLOCK_SLOTS*sizeof(uint32_t)) + ((idx+2) * 0x1000)))
            {
                // Map the memory if allocated and exit.
                if(Z80Ctrl->page[idx] != NULL)
                {
                    page = virt_to_page((unsigned long)Z80Ctrl->page[idx] + (vma->vm_pgoff << PAGE_SHIFT)); 
                    ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), size, vma->vm_page_prot);
                    if (ret != 0)
                    {
                        goto out;
                    }   
                } else
                {
                    ret = -EINVAL;
                    goto out;  
                } 
            }
        }
    }
    else
    {
        ret = -EINVAL;
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
        pr_info(KERN_INFO "%08X", addr); // print address
        pr_info(KERN_CONT ":  ");

        // print hexadecimal data
        for (i=0; i < displayWidth; )
        {
            if(pnt+i < endAddr)
            {
                SPI_SEND_32((uint16_t)(pnt+i), CPLD_CMD_READ_ADDR);
                while(CPLD_READY() == 0);
                data = z80io_PRL_Read();
                pr_info(KERN_CONT "%02X", data);
            }
            else
                pr_info(KERN_CONT "  ");
            i++;

            pr_info(KERN_CONT " ");
        }

        // print ascii data
        pr_info(KERN_CONT " |");

        // print single ascii char
        for (i=0; i < displayWidth; i++)
        {
            SPI_SEND_32((uint16_t)(pnt+i), CPLD_CMD_READ_ADDR);
            while(CPLD_READY() == 0);
            c = (char)z80io_PRL_Read();
            if ((pnt+i < endAddr) && (c >= ' ') && (c <= '~'))
                pr_info(KERN_CONT "%c", (char)c);
            else
                pr_info(KERN_CONT " ");
        }

        pr_info(KERN_CONT "|\n");

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

    // Check to see if the memory mode page has been allocated for current mode.
    if(Z80Ctrl->page[Z80Ctrl->memoryMode] == NULL)
    {
        pr_info("Allocating memory page:%d\n", Z80Ctrl->memoryMode);
        (Z80Ctrl->page[Z80Ctrl->memoryMode]) = (uint32_t *)kmalloc((MEMORY_BLOCK_SLOTS*sizeof(uint32_t)), GFP_KERNEL);
        if ((Z80Ctrl->page[Z80Ctrl->memoryMode]) == NULL)
        {
            pr_info("z80drv: failed to allocate  memory mapping page:%d memory!", Z80Ctrl->memoryMode);
            Z80Ctrl->page[Z80Ctrl->memoryMode] = Z80Ctrl->page[0];
        }
    }

    // Call driver specific methods to change default memory map per device requirements.
  #if(TARGET_HOST_MZ80A == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ80A)
        mz80aSetupMemory(mode);
    else
  #endif
  #if(TARGET_HOST_MZ700 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ700)
        mz700SetupMemory(mode);
    else
  #endif
  #if(TARGET_HOST_MZ1500 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ1500)
        mz1500SetupMemory(mode);
    else
  #endif
  #if(TARGET_HOST_MZ2000 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_MZ2000)
        mz2000SetupMemory(mode);
    else
  #endif
    // RFS board only works on an MZ-80A/MZ-700/MZ-1500 at present.
  #if(TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_RFS)
        rfsSetupMemory(mode);
    else
  #endif
    // tranZPUter operates in all supported Sharp machines.
  #if(TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_TZPU)
        tzpuSetupMemory(mode);
    else
  #endif
  #if(TARGET_HOST_PCW == 1)
    if(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_PCW)
        pcwSetupMemory(mode);
    else
  #endif
    {
    }

    // Enable autorefresh if refreshDRAM is set. 0 = disable, 1 = enable, > 1 = ignore and use CPLD default.
    if(Z80Ctrl->refreshDRAM < 2)
        SPI_SEND_32(0x0000, Z80Ctrl->refreshDRAM == 1 ? CPLD_CMD_SET_AUTO_REFRESH : CPLD_CMD_CLEAR_AUTO_REFRESH);

    // Inhibit mode disabled.
    Z80Ctrl->inhibitMode = 0;

  #if defined(TARGET_HOST_MZ1500)
    // Flag to indicate PCG active, all memory accesses from D000:FFFF are sent to hardware.
    Z80Ctrl->pcgMode = 0;
  #endif
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
                pr_info("IOCTL - Couldnt retrieve command!\n");
            else
            {
              #if(DEBUG_ENABLED & 1)
                if(Z80Ctrl->debug >=3) pr_info("IOCTL - Command (%08x)\n", ioctlCmd.cmd);
              #endif
                switch(ioctlCmd.cmd)
                {
                    // Command to stop the Z80 CPU and power off.
                    case IOCTL_CMD_Z80_STOP:
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        z80_power(&Z80CPU, FALSE);
                        Z80_PC(Z80CPU) = 0;
                      #if(DEBUG_ENABLED & 1)
                        if(Z80Ctrl->debug >= 3) pr_info("Z80 stopped.\n");
                      #endif
                        break;

                    // Command to power on and start the Z80 CPU.
                    case IOCTL_CMD_Z80_START:
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_RUNNING; mutex_unlock(&Z80RunModeMutex);
                        z80_power(&Z80CPU, TRUE);

                      #if(DEBUG_ENABLED & 1)
                        if(Z80Ctrl->debug >= 3) pr_info("Z80 started.\n");
                      #endif
                        break;

                    // Command to pause the Z80.
                    case IOCTL_CMD_Z80_PAUSE:
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_PAUSE; mutex_unlock(&Z80RunModeMutex);
                      #if(DEBUG_ENABLED & 1)
                        if(Z80Ctrl->debug >= 3) pr_info("Z80 paused.\n");
                      #endif
                        break;

                    // Command to release a paused Z80.
                    case IOCTL_CMD_Z80_CONTINUE:
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_CONTINUE; mutex_unlock(&Z80RunModeMutex);
                      #if(DEBUG_ENABLED & 1)
                        if(Z80Ctrl->debug >= 3) pr_info("Z80 running.\n");
                      #endif
                        break;

                    // Command to perform a CPU reset.
                    case IOCTL_CMD_Z80_RESET:
                        // Stop the CPU prior to reset.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        resetZ80();

                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_RUNNING; mutex_unlock(&Z80RunModeMutex);
                      #if(DEBUG_ENABLED & 1)
                        if(Z80Ctrl->debug >= 3) pr_info("Z80 Reset.\n");
                      #endif
                        break;
                        
                    // Command to setup the page table to use host memory and physical hardware.
                    case IOCTL_CMD_USE_HOST_RAM:
                        // Stop the CPU prior to memory reconfiguration.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        Z80Ctrl->defaultPageMode = USE_PHYSICAL_RAM;
                        resetZ80();

                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                      #if(DEBUG_ENABLED & 1)
                        if(Z80Ctrl->debug >= 3) pr_info("Z80 Set to use Host Memory.\n");
                      #endif
                        break;

                    // Command to setup the page table to use virtual memory, only physical hardware is accessed on the host.
                    case IOCTL_CMD_USE_VIRTUAL_RAM:
                        // Stop the CPU prior to memory reconfiguration.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        Z80Ctrl->defaultPageMode = USE_VIRTUAL_RAM;
                        resetZ80();

                        //setupMemory(Z80Ctrl->defaultPageMode);
                       // z80_instant_reset(&Z80CPU);
                    
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                      #if(DEBUG_ENABLED & 1)
                        if(Z80Ctrl->debug >= 3) pr_info("Z80 Set to use Virtual Memory.\n");
                      #endif
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
                            SPI_SEND_32(idx, Z80Ctrl->ram[idx] << 8 | CPLD_CMD_WRITE_ADDR);
                        }

                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                      #if(DEBUG_ENABLED & 1)
                        if(Z80Ctrl->debug >= 3) pr_info("Z80 Host DRAM syncd with Virtual Memory.\n");
                      #endif
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
                                Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_X2;
                                Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_X2;
                                break;

                            case 4:
                                Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_X4;
                                Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_X4;
                                break;

                            case 8:
                                Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_X8;
                                Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_X8;
                                break;

                            case 16:
                                Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_X16;
                                Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_X16;
                                break;

                            case 32:
                                Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_X32;
                                Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_X32;
                                break;

                            case 64:
                                Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_X64;
                                Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_X64;
                                break;

                            case 128:
                                Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_X128;
                                Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_X128;
                                break;

                            case 1:
                            default:
                                Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_NORMAL;
                                Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_NORMAL;
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
                        pr_info("PC set to %04x\n", ioctlCmd.z80.pc);
                        break;

                    // Command to add a virtual device into the Z80 configuration.
                    case IOCTL_CMD_ADD_DEVICE:
                        // Check that there is space to add the device and that it doesnt already exist.
                        if(Z80Ctrl->virtualDeviceCnt == MAX_VIRTUAL_DEVICES)
                        {
                            pr_info("Virtual Device table full, cannot add new device.\n");
                            break;
                        }
                        for(idx = 0; idx < Z80Ctrl->virtualDeviceCnt; idx++)
                        {
                            if(Z80Ctrl->virtualDevice[idx] == ioctlCmd.vdev.device)
                            {
                                pr_info("Virtual Device already installed.\n");
                                break;
                            }
                        }
                        if(idx < Z80Ctrl->virtualDeviceCnt)
                            break;

                      #if(TARGET_HOST_MZ80A == 0 && TARGET_HOST_MZ700 == 0 && TARGET_HOST_MZ1500 == 0)
                        if(ioctlCmd.vdev.device & VIRTUAL_DEVICE_RFS)
                        {
                            pr_info("RFS Board currently supported on MZ-80A/MZ-700/MZ-1500 Hosts only.\n");
                            break;
                        }
                      #endif

                        // Stop the CPU prior to adding virtual device.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        // Add in device, setting up hooks etc as required. The devices are stored in an array for ease of reference and lookup
                        // in the driver and ctrl program, in actual use they are a bit map for performance as scanning an array is time consuming.
                        switch(ioctlCmd.vdev.device)
                        {
                          #if(TARGET_HOST_MZ80A == 1)
                            case VIRTUAL_DEVICE_MZ80A:
                                Z80Ctrl->virtualDevice[Z80Ctrl->virtualDeviceCnt++] = ioctlCmd.vdev.device;
                                Z80Ctrl->virtualDeviceBitMap |= ioctlCmd.vdev.device;
                                mz80aInit(0);
                                break;
                          #endif

                          #if(TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1)
                            case VIRTUAL_DEVICE_RFS40:
                            case VIRTUAL_DEVICE_RFS80:
                                Z80Ctrl->virtualDevice[Z80Ctrl->virtualDeviceCnt++] = ioctlCmd.vdev.device;
                                Z80Ctrl->virtualDeviceBitMap |= ioctlCmd.vdev.device;
                                rfsInit(Z80Ctrl->virtualDeviceBitMap & VIRTUAL_DEVICE_RFS40 ? 0 : 1);
                                break;
                          #endif

                          #if(TARGET_HOST_MZ700 == 1)
                            case VIRTUAL_DEVICE_MZ700:
                                Z80Ctrl->virtualDevice[Z80Ctrl->virtualDeviceCnt++] = ioctlCmd.vdev.device;
                                Z80Ctrl->virtualDeviceBitMap |= ioctlCmd.vdev.device;
                                mz700Init(0);
                                break;
                          #endif

                          #if(TARGET_HOST_MZ1500 == 1)
                            case VIRTUAL_DEVICE_MZ1500:
                                Z80Ctrl->virtualDevice[Z80Ctrl->virtualDeviceCnt++] = ioctlCmd.vdev.device;
                                Z80Ctrl->virtualDeviceBitMap |= ioctlCmd.vdev.device;
                                mz1500Init(0);
                                break;
                          #endif

                          #if(TARGET_HOST_MZ2000 == 1)
                            case VIRTUAL_DEVICE_MZ2000:
                                Z80Ctrl->virtualDevice[Z80Ctrl->virtualDeviceCnt++] = ioctlCmd.vdev.device;
                                Z80Ctrl->virtualDeviceBitMap |= ioctlCmd.vdev.device;
                                mz2000Init(0);
                                break;
                          #endif

                          #if(TARGET_HOST_PCW == 0)
                            case VIRTUAL_DEVICE_TZPU:
                                Z80Ctrl->virtualDevice[Z80Ctrl->virtualDeviceCnt++] = VIRTUAL_DEVICE_TZPU;
                                Z80Ctrl->virtualDeviceBitMap |= VIRTUAL_DEVICE_TZPU;
                                tzpuInit();
                                break;
                          #endif

                          #if(TARGET_HOST_PCW == 1)
                            case VIRTUAL_DEVICE_PCW:
                                Z80Ctrl->virtualDevice[Z80Ctrl->virtualDeviceCnt++] = VIRTUAL_DEVICE_PCW;
                                Z80Ctrl->virtualDeviceBitMap |= VIRTUAL_DEVICE_PCW;

                              #ifdef TARGET_HOST_PCW8XXX
                                pcwInit(0);
                              #else
                                pcwInit(1);
                              #endif
                                break;
                          #endif

                            case VIRTUAL_DEVICE_NONE:
                            default:
                                break;
                        }

                        // Z80 can continue.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        pr_info("Virtual device added.\n");
                        break;

                    // Command to remove a device from the Z80 configuration.
                    case IOCTL_CMD_DEL_DEVICE:
                        // Locate the device.
                        for(idx = 0; idx < Z80Ctrl->virtualDeviceCnt; idx++)
                        {
                            if(Z80Ctrl->virtualDevice[idx] == ioctlCmd.vdev.device)
                                break;
                        }
                        if(idx == Z80Ctrl->virtualDeviceCnt)
                        {
                            //pr_info("Virtual Device not found.\n");
                            break;
                        }
                       
                        // Stop the CPU prior to virtual device removal.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);
                        do { mutex_lock(&Z80RunModeMutex); nextRunMode = Z80RunMode ; mutex_unlock(&Z80RunModeMutex);
                        } while(nextRunMode == Z80_STOP);

                        while(idx < (Z80Ctrl->virtualDeviceCnt-1))
                        {
                            Z80Ctrl->virtualDevice[idx] = Z80Ctrl->virtualDevice[idx+1];
                        }
                        Z80Ctrl->virtualDeviceCnt--;

                        // Delete the device, removing hooks etc as required.
                        switch(ioctlCmd.vdev.device)
                        {
                          #if(TARGET_HOST_MZ80A == 1)
                            case VIRTUAL_DEVICE_MZ80A:
                                Z80Ctrl->virtualDeviceBitMap &= ~ioctlCmd.vdev.device;
                                mz80aRemove();
                                break;
                          #endif

                          #if(TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1)
                            case VIRTUAL_DEVICE_RFS40:
                            case VIRTUAL_DEVICE_RFS80:
                                Z80Ctrl->virtualDeviceBitMap &= ~ioctlCmd.vdev.device;
                                rfsRemove();
                                break;
                          #endif

                          #if(TARGET_HOST_MZ700 == 1)
                            case VIRTUAL_DEVICE_MZ700:
                                Z80Ctrl->virtualDeviceBitMap &= ~ioctlCmd.vdev.device;
                                mz700Remove();
                                break;
                          #endif

                          #if(TARGET_HOST_MZ1500 == 1)
                            case VIRTUAL_DEVICE_MZ1500:
                                Z80Ctrl->virtualDeviceBitMap &= ~ioctlCmd.vdev.device;
                                mz1500Remove();
                                break;
                          #endif

                          #if(TARGET_HOST_MZ2000 == 1)
                            case VIRTUAL_DEVICE_MZ2000:
                                Z80Ctrl->virtualDeviceBitMap &= ~ioctlCmd.vdev.device;
                                mz2000Remove();
                                break;
                          #endif

                          #if(TARGET_HOST_PCW == 0)
                            case VIRTUAL_DEVICE_TZPU:
                                Z80Ctrl->virtualDeviceBitMap &= ~VIRTUAL_DEVICE_TZPU;
                                tzpuRemove();
                                break;
                          #endif

                          #if(TARGET_HOST_PCW == 1)
                            case VIRTUAL_DEVICE_PCW:
                                Z80Ctrl->virtualDeviceBitMap &= ~VIRTUAL_DEVICE_PCW;
                                pcwRemove();
                                break;
                          #endif

                            case VIRTUAL_DEVICE_NONE:
                            default:
                                break;
                        }
                       
                        // Z80 can continue.
                        mutex_lock(&Z80RunModeMutex); Z80RunMode = currentRunMode; mutex_unlock(&Z80RunModeMutex);
                        //pr_info("Device removed\n");
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

                  #if(DEBUG_ENABLED & 1)
                    // Method to turn on/off debug output.
                    case IOCTL_CMD_DEBUG:
                        Z80Ctrl->debug = ioctlCmd.debug.level;
                        pr_info("Debug level set to:%d\n", Z80Ctrl->debug);
                        break;
                  #endif

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
            pr_info("IOCTL - Unhandled Command (%08x)\n", ioctlCmd.cmd);
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

    // Version information.
    pr_info("%s\n%s %s %s\n", DRIVER_DESCRIPTION, DRIVER_VERSION, DRIVER_COPYRIGHT, DRIVER_AUTHOR);

    mutex_init(&Z80DRV_MUTEX);

    // Get device Major number.
    major = register_chrdev(0, DEVICE_NAME, &z80drv_fops);
    if (major < 0) {
        pr_info("z80drv: fail to register major number!\n");
        ret = major;
        goto initExit;
    }

    class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(class)){ 
        unregister_chrdev(major, DEVICE_NAME);
        pr_info("z80drv: failed to register device class\n");
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
        pr_info("z80drv: failed to allocate ctrl memory!\n");
        ret = -ENOMEM; 
        goto initExit;
    }
    Z80Ctrl->ram= (uint8_t *)kmalloc(Z80_VIRTUAL_RAM_SIZE, GFP_KERNEL); 
    if (Z80Ctrl->ram == NULL) 
    {
        pr_info("z80drv: failed to allocate RAM memory!\n");
        ret = -ENOMEM; 
        goto initExit;
    }
    Z80Ctrl->rom= (uint8_t *)kmalloc(Z80_VIRTUAL_ROM_SIZE, GFP_KERNEL); 
    if (Z80Ctrl->rom == NULL) 
    {
        pr_info("z80drv: failed to allocate ROM memory!\n");
        ret = -ENOMEM; 
        goto initExit;
    }
    // Default memory mode is 0, ie. Original. Additional modes may be used by drivers such as the tzpu driver.
    Z80Ctrl->memoryMode = 0;
    for(idx=0; idx < MEMORY_MODES; idx++)
    {
        (Z80Ctrl->page[idx]) = NULL;
    }

    // Allocate standard memory mode mapping page.
    (Z80Ctrl->page[Z80Ctrl->memoryMode]) = (uint32_t *)kmalloc((MEMORY_BLOCK_SLOTS*sizeof(uint32_t)), GFP_KERNEL);
    if ((Z80Ctrl->page[Z80Ctrl->memoryMode]) == NULL) 
    {
        pr_info("z80drv: failed to allocate default memory mapping page memory!\n");
        ret = -ENOMEM; 
        goto initExit;
    }

    // Initialise the hardware to host interface.
    z80io_init();
    SPI_SET_FRAME_SIZE();

    // Initialise the virtual device array.
    for(idx=0; idx < MAX_VIRTUAL_DEVICES; idx++)
    {
        Z80Ctrl->virtualDevice[idx] = VIRTUAL_DEVICE_NONE;
    }
    Z80Ctrl->virtualDeviceCnt = 0;
    Z80Ctrl->virtualDeviceBitMap = 0;
  
    // Setup Refresh so that an automatic refresh mode is performed by the CPLD whilst running in Virtual memory. This is necessary as opcode fetches from
    // host memory, by the CPLD, normally performs the refresh and when running in virtual memory, these refresh cycles arent performed.
    Z80Ctrl->refreshDRAM = 0;

    // Setup the governor delay, it is the delay per opcode fetch to restrict the Z80 CPU to a given speed.
    Z80Ctrl->cpuGovernorDelayROM = ROM_DELAY_NORMAL;
    Z80Ctrl->cpuGovernorDelayRAM = RAM_DELAY_NORMAL;
    Z80Ctrl->governorSkip        = 0;

    // Setup the default Page Mode. This is needed if an event such as a reset occurs which needs to return the page and iotable back to default.
    Z80Ctrl->defaultPageMode = USE_VIRTUAL_RAM;

    // Setup memory profile to use internal virtual RAM (SOM kernel RAM rather than HOST DRAM).
    setupMemory(Z80Ctrl->defaultPageMode);

    // Initialise control handles.
    Z80Ctrl->ioTask  = NULL;
    Z80Ctrl->arbTask = NULL;

    // Initialse run control.
    mutex_init(&Z80RunModeMutex);
    mutex_lock(&Z80RunModeMutex); Z80RunMode = Z80_STOP; mutex_unlock(&Z80RunModeMutex);

    // Initialise control flags.
    Z80Ctrl->ioReadAhead = 0;
    Z80Ctrl->ioWriteAhead = 0;

  #if (TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ1500 == 1 || TARGET_HOST_MZ2000 == 1)
    // Initialse hotkey detection variables.
    Z80Ctrl->keyportStrobe      = 0x00;
    Z80Ctrl->keyportShiftCtrl   = 0x00;
    Z80Ctrl->keyportHotKey      = 0x00;
    Z80Ctrl->keyportTrigger     = 0x00;
    Z80Ctrl->keyportTriggerLast = 0x00;
  #endif

    // PC to start.
    Z80_PC(Z80CPU) = 0;
    //z80_power(&Z80CPU, TRUE);

    // Initialise Debug logic if compile time enabled.
  #if(DEBUG_ENABLED != 0)
    Z80Ctrl->debug              = 0;
  #endif

    // Init done.
    pr_info("Initialisation complete.\n");

    // Create thread to run the Z80 cpu.
    kthread_z80 = kthread_create(thread_z80, &threadId_z80, "z80");
    if(kthread_z80 != NULL)
    {
        pr_info("kthread - Thread Z80 was created, waking...!\n");
        kthread_bind(kthread_z80, 1);
        wake_up_process(kthread_z80);
    }
    else {
        pr_info("kthread - Thread Z80 could not be created!\n");
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
    // Locals.
    uint32_t        idx;
    int             result;

    // Stop the internal threads.
    result = kthread_stop(kthread_z80);
    if(result != 0)
    {
        pr_info("Failed to stop Z80 thread, reason:%d\n", result);
    }

    // Return the memory used for the Z80 'virtual memory' and control variables.
    for(idx=0; idx < MEMORY_MODES; idx++)
    {
        if(Z80Ctrl->page[idx] != NULL)
        {
            kfree(Z80Ctrl->page[idx]);
            Z80Ctrl->page[idx] = NULL;
        }
    }
    kfree(Z80Ctrl->ram); Z80Ctrl->ram = NULL;
    kfree(Z80Ctrl->rom); Z80Ctrl->rom = NULL;
    kfree(Z80Ctrl);

    // Nothing to be done for the hardware.

    // Cleanup and remove the device.
    mutex_destroy(&Z80DRV_MUTEX); 
    device_destroy(class, MKDEV(major, 0));  
    class_unregister(class);
    class_destroy(class); 
    unregister_chrdev(major, DEVICE_NAME);
    
    pr_info("z80drv: unregistered!\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
