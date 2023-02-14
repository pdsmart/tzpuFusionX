/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80vhw_tzpu.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Virtual Hardware Driver - tranZPUter SW
//                  This file contains the methods used to emulate the first tranZPUter Software series
//                  the tranZPUter SW which used a Cortex-M4 to enhance the host machine and provide
//                  an upgraded monitor, the tzfs (tranZPUter Filing System).
//
//                  These drivers are intended to be instantiated inline to reduce overhead of a call
//                  and as such, they are included like header files rather than C linked object files.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Feb 2023 - Initial write based on the RFS hardware.
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

//-------------------------------------------------------------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------------------------------------------------------------

// Method to setup the memory page config to reflect installation of a tranZPUter SW Board.
void tzpuSetupMemory(enum Z80_MEMORY_PROFILE mode)
{
}

// Perform any setup operations, such as variable initialisation, to enable use of this module.
void tzpuInit(void)
{
}

// Method to decode an address and make any system memory map changes as required.
//
static inline void tzpuDecodeMemoryMapSetup(zuint16 address, zuint8 data, uint8_t ioFlag, uint8_t readFlag)
{
    // Locals.
}

// Method to read from the tranZPUter SW memory or I/O ports.
static inline uint8_t tzpuRead(zuint16 address)
{
    // Locals.
    uint8_t     data = 0x00;

    return(data);
}

// Method to write to the tranZPUter SW memory or I/O ports.
static inline void tzpuWrite(zuint16 address, zuint8 data)
{
    return;
}
