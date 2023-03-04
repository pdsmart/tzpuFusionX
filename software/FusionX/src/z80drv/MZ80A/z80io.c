/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80io.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 IO Interface
//                  This file contains the methods used in interfacing the SOM to the Z80 socket 
//                  and host hardware via a CPLD.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Oct 2022 v1.0 - Initial write of the z80 kernel driver software.
//                  Jan 2023 v1.1 - Numerous new tries at increasing throughput to the CPLD failed.
//                                  Maximum read throughput of an 8bit byte due to the SSD202 GPIO
//                                  structure is approx 2MB/s - or 512K/s for a needed 32bit word.
//                                  Write is slower as you have to clock the data so sticking with SPI.
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


//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
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
// User space driver access.
//
//-------------------------------------------------------------------------------------------------------------------------------


// Initialise the SOM hardware used to communicate with the z80 socket and host hardware. 
// The SOM interfaces to a CPLD which provides voltage level translation and also encapsulates the Z80 timing cycles as recreating
// them within the SOM is much more tricky.
//
// As this is an embedded device and performance/latency are priorities, minimal structured code is used to keep call stack and
// generated code to a mimimum without relying on the optimiser.
int z80io_init(void)
{
    // Locals.
    int      ret = 0;    

    // Initialise GPIO. We call the HAL api to minimise time but for actual bit set/reset and read we go directly to registers to save time, increase throughput and minimise latency.
    // Initialise the HAL.
    MHal_GPIO_Init();

    // Set the pads as GPIO devices. The HAL takes care of allocating and deallocating the padmux resources.
    MHal_GPIO_Pad_Set(PAD_Z80IO_IN_DATA_0);   // Word (16bit) bidirectional bus. Default is read with data set.
    MHal_GPIO_Pad_Set(PAD_Z80IO_IN_DATA_1);
    MHal_GPIO_Pad_Set(PAD_Z80IO_IN_DATA_2);
    MHal_GPIO_Pad_Set(PAD_Z80IO_IN_DATA_3);
    MHal_GPIO_Pad_Set(PAD_Z80IO_IN_DATA_4);
    MHal_GPIO_Pad_Set(PAD_Z80IO_IN_DATA_5);
    MHal_GPIO_Pad_Set(PAD_Z80IO_IN_DATA_6);
    MHal_GPIO_Pad_Set(PAD_Z80IO_IN_DATA_7);
    MHal_GPIO_Pad_Set(PAD_Z80IO_HIGH_BYTE); 
  //MHal_GPIO_Pad_Set(PAD_GPIO8);             // SPIO 4wire control lines setup by the spidev driver but controlled directly in this driver.
  //MHal_GPIO_Pad_Set(PAD_GPIO9);
  //MHal_GPIO_Pad_Set(PAD_GPIO10);
  //MHal_GPIO_Pad_Set(PAD_GPIO11);
    MHal_GPIO_Pad_Set(PAD_Z80IO_READY);   
    MHal_GPIO_Pad_Set(PAD_Z80IO_LTSTATE);  
    MHal_GPIO_Pad_Set(PAD_Z80IO_BUSRQ);
    MHal_GPIO_Pad_Set(PAD_Z80IO_BUSACK);
    MHal_GPIO_Pad_Set(PAD_Z80IO_INT);  
    MHal_GPIO_Pad_Set(PAD_Z80IO_NMI);  
    MHal_GPIO_Pad_Set(PAD_Z80IO_WAIT); 
    MHal_GPIO_Pad_Set(PAD_Z80IO_RESET); 
    MHal_GPIO_Pad_Set(PAD_Z80IO_RSV1); 
#ifdef NOTNEEDED
    MHal_GPIO_Pad_Set(PAD_Z80IO_OUT_DATA_0);   
    MHal_GPIO_Pad_Set(PAD_Z80IO_OUT_DATA_1);
    MHal_GPIO_Pad_Set(PAD_Z80IO_OUT_DATA_2);
    MHal_GPIO_Pad_Set(PAD_Z80IO_OUT_DATA_3);  
    MHal_GPIO_Pad_Set(PAD_Z80IO_OUT_DATA_4);  
    MHal_GPIO_Pad_Set(PAD_Z80IO_OUT_DATA_5); 
    MHal_GPIO_Pad_Set(PAD_Z80IO_OUT_DATA_6); 
    MHal_GPIO_Pad_Set(PAD_Z80IO_OUT_DATA_7); 
    MHal_GPIO_Pad_Set(PAD_Z80IO_WRITE);  
#endif

    // Set required input pads.
    MHal_GPIO_Pad_Odn(PAD_Z80IO_IN_DATA_0);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_IN_DATA_1);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_IN_DATA_2);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_IN_DATA_3);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_IN_DATA_4);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_IN_DATA_5);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_IN_DATA_6);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_IN_DATA_7);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_READY);   
    MHal_GPIO_Pad_Odn(PAD_Z80IO_LTSTATE);  
    MHal_GPIO_Pad_Odn(PAD_Z80IO_BUSRQ);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_BUSACK);
    MHal_GPIO_Pad_Odn(PAD_Z80IO_INT);  
    MHal_GPIO_Pad_Odn(PAD_Z80IO_NMI);  
    MHal_GPIO_Pad_Odn(PAD_Z80IO_WAIT); 
    MHal_GPIO_Pad_Odn(PAD_Z80IO_RESET); 
    MHal_GPIO_Pad_Odn(PAD_Z80IO_RSV1); 

    // Set required output pads.
#ifdef NOTNEEDED
    MHal_GPIO_Pad_Oen(PAD_Z80IO_OUT_DATA_0);
    MHal_GPIO_Pad_Oen(PAD_Z80IO_OUT_DATA_1);
    MHal_GPIO_Pad_Oen(PAD_Z80IO_OUT_DATA_2);
    MHal_GPIO_Pad_Oen(PAD_Z80IO_OUT_DATA_3);
    MHal_GPIO_Pad_Oen(PAD_Z80IO_OUT_DATA_4);
    MHal_GPIO_Pad_Oen(PAD_Z80IO_OUT_DATA_5);
    MHal_GPIO_Pad_Oen(PAD_Z80IO_OUT_DATA_6);
    MHal_GPIO_Pad_Oen(PAD_Z80IO_OUT_DATA_7);
    MHal_GPIO_Pad_Oen(PAD_Z80IO_WRITE);
    MHal_GPIO_Pull_High(PAD_Z80IO_WRITE);
#endif

    // Control signals.
    MHal_GPIO_Pad_Oen(PAD_Z80IO_HIGH_BYTE);
    MHal_GPIO_Pull_High(PAD_Z80IO_HIGH_BYTE);

    // Setup the MSPI0 device.
    //
    // Setup control, interrupts are not used.
    MSPI_WRITE(MSPI_CTRL_OFFSET, MSPI_CPU_CLOCK_1_2 | MSPI_CTRL_CPOL_LOW | MSPI_CTRL_CPHA_HIGH | MSPI_CTRL_RESET | MSPI_CTRL_ENABLE_SPI);

    // Setup LSB First mode.
    MSPI_WRITE(MSPI_LSB_FIRST_OFFSET, 0x0);

    // Setup clock.
    CLK_WRITE(MSPI0_CLK_CFG, 0x1100)

    // Setup the frame size (all buffers to 8bits).
    MSPI_WRITE(MSPI_FRAME_WBIT_OFFSET,   0xfff);
    MSPI_WRITE(MSPI_FRAME_WBIT_OFFSET+1, 0xfff);
    MSPI_WRITE(MSPI_FRAME_RBIT_OFFSET,   0xfff);
    MSPI_WRITE(MSPI_FRAME_RBIT_OFFSET+1, 0xfff);

    // Setup Chip Selects to inactive.
    MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_DISABLE);

    // Switch Video and Audio to host.
    z80io_SPI_Send16(0x00f0, NULL);

    return ret;
}


//--------------------------------------------------------
// Parallel bus Methods.
//--------------------------------------------------------

// Methods to read data from the parallel bus.
// The CPLD returns status and Z80 data on the 8bit bus as it is marginally quicker than retrieving it over the SPI bus.
//
inline uint8_t z80io_PRL_Read8(uint8_t dataFlag)
{
    // Locals.
    volatile uint8_t   result = 0;
   
    // Byte according to flag.
    if(dataFlag)
        SET_CPLD_READ_DATA()
    else
        SET_CPLD_READ_STATUS()

    // Read the input registers and set value accordingly.
    result = READ_CPLD_DATA_IN();

    // Return 16bit value read from CPLD.
    return(result);
}

inline uint8_t z80io_PRL_Read(void)
{
    // Locals.
    volatile uint8_t   result = 0;
    volatile uint32_t  b7, b6, b5, b4, b3, b2, b1, b0;
   
    // Read the input registers and set value accordingly. Quicker to read registers and then apply shift/logical operators. The I/O Bus is very slow!
    b7 = READ_LONG(((gRIUBaseAddr) + (((PAD_Z80IO_IN_DATA_7_ADDR) & ~1)<<1) + (PAD_Z80IO_IN_DATA_7_ADDR & 1)));
    b6 = READ_LONG(((gRIUBaseAddr) + (((PAD_Z80IO_IN_DATA_6_ADDR) & ~1)<<1) + (PAD_Z80IO_IN_DATA_6_ADDR & 1)));
    b5 = READ_LONG(((gRIUBaseAddr) + (((PAD_Z80IO_IN_DATA_5_ADDR) & ~1)<<1) + (PAD_Z80IO_IN_DATA_5_ADDR & 1)));
    b4 = READ_LONG(((gRIUBaseAddr) + (((PAD_Z80IO_IN_DATA_4_ADDR) & ~1)<<1) + (PAD_Z80IO_IN_DATA_4_ADDR & 1)));
    b3 = READ_LONG(((gRIUBaseAddr) + (((PAD_Z80IO_IN_DATA_3_ADDR) & ~1)<<1) + (PAD_Z80IO_IN_DATA_3_ADDR & 1)));
    b2 = READ_LONG(((gRIUBaseAddr) + (((PAD_Z80IO_IN_DATA_2_ADDR) & ~1)<<1) + (PAD_Z80IO_IN_DATA_2_ADDR & 1)));
    b1 = READ_LONG(((gRIUBaseAddr) + (((PAD_Z80IO_IN_DATA_1_ADDR) & ~1)<<1) + (PAD_Z80IO_IN_DATA_1_ADDR & 1)));
    b0 = READ_LONG(((gRIUBaseAddr) + (((PAD_Z80IO_IN_DATA_0_ADDR) & ~1)<<1) + (PAD_Z80IO_IN_DATA_0_ADDR & 1)));
    result = (b7 & 0x1) << 7 | (b6 & 0x1) << 6 | (b5 & 0x1) << 5 | (b4 & 0x1) << 4 | (b3 & 0x1) << 3 | (b2 & 0x1) << 2 | (b1 & 0x1) << 1 | (b0 & 0x1);

    // Return 16bit value read from CPLD.
    return(result);
}

inline uint16_t z80io_PRL_Read16(void)
{
    // Locals.
    volatile uint16_t   result = 0;
   
    // Low byte first.
    CLEAR_CPLD_HIGH_BYTE();

    // Read the input registers and set value accordingly.
    result = (uint16_t)READ_CPLD_DATA_IN();

    // High byte next.
    SET_CPLD_HIGH_BYTE();

    // Read the input registers and set value accordingly.
    result |= (uint16_t)(READ_CPLD_DATA_IN() << 8);

    // Return 16bit value read from CPLD.
    return(result);
}


// Parallel Bus methods were tried and tested but due to the GPIO bits being controlled by individual registers per bit, the setup time was longer
// than the transmission time of SPI. These methods are thus deprecated and a fusion of SPI and 8bit parallel is now used.
#ifdef NOTNEEDED
inline uint8_t z80io_PRL_Send8(uint8_t txData)
{
    // Locals.
    //

    // Low byte only.
    MHal_RIU_REG(gpio_table[PAD_Z80IO_HIGH_BYTE].r_out) &= (~gpio_table[PAD_Z80IO_HIGH_BYTE ].m_out);

    // Setup data.
    if(txData & 0x0080) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_7].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_7].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_7].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_7].m_out); }
    if(txData & 0x0040) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_6].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_6].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_6].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_6].m_out); }
    if(txData & 0x0020) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_5].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_5].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_5].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_5].m_out); }
    if(txData & 0x0010) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_4].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_4].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_4].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_4].m_out); }
    if(txData & 0x0008) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_3].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_3].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_3].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_3].m_out); }
    if(txData & 0x0004) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_2].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_2].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_2].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_2].m_out); }
    if(txData & 0x0002) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_1].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_1].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_1].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_1].m_out); }
    if(txData & 0x0001) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_0].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_0].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_0].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_0].m_out); }
  
    // Clock data.
    MHal_RIU_REG(gpio_table[PAD_Z80IO_WRITE].r_out) &= (~gpio_table[PAD_Z80IO_WRITE ].m_out);
    MHal_RIU_REG(gpio_table[PAD_Z80IO_WRITE].r_out) |=  gpio_table[PAD_Z80IO_WRITE ].m_out;

    return(0);
}

inline uint8_t z80io_PRL_Send16(uint16_t txData)
{
    // Locals.
    //

    // Low byte first.
    MHal_RIU_REG(gpio_table[PAD_Z80IO_HIGH_BYTE].r_out) &= (~gpio_table[PAD_Z80IO_HIGH_BYTE ].m_out);

    // Setup data.
    if(txData & 0x0080) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_7].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_7].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_7].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_7].m_out); }
    if(txData & 0x0040) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_6].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_6].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_6].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_6].m_out); }
    if(txData & 0x0020) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_5].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_5].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_5].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_5].m_out); }
    if(txData & 0x0010) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_4].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_4].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_4].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_4].m_out); }
    if(txData & 0x0008) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_3].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_3].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_3].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_3].m_out); }
    if(txData & 0x0004) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_2].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_2].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_2].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_2].m_out); }
    if(txData & 0x0002) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_1].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_1].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_1].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_1].m_out); }
    if(txData & 0x0001) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_0].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_0].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_0].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_0].m_out); }
  
    // Clock data.
    MHal_RIU_REG(gpio_table[PAD_Z80IO_WRITE].r_out) &= (~gpio_table[PAD_Z80IO_WRITE ].m_out);
    MHal_RIU_REG(gpio_table[PAD_Z80IO_WRITE].r_out) |=  gpio_table[PAD_Z80IO_WRITE ].m_out;

    // High byte next.
    MHal_RIU_REG(gpio_table[PAD_Z80IO_HIGH_BYTE ].r_out) |= gpio_table[PAD_Z80IO_HIGH_BYTE ].m_out;

    // Setup high byte.
    if(txData & 0x8000) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_7].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_7].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_7].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_7].m_out); }
    if(txData & 0x4000) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_6].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_6].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_6].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_6].m_out); }
    if(txData & 0x2000) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_5].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_5].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_5].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_5].m_out); }
    if(txData & 0x1000) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_4].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_4].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_4].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_4].m_out); }
    if(txData & 0x0800) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_3].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_3].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_3].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_3].m_out); }
    if(txData & 0x0400) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_2].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_2].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_2].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_2].m_out); }
    if(txData & 0x0200) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_1].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_1].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_1].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_1].m_out); }
    if(txData & 0x0100) { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_0].r_out) |= gpio_table[PAD_Z80IO_OUT_DATA_0].m_out; } else { MHal_RIU_REG(gpio_table[PAD_Z80IO_OUT_DATA_0].r_out) &= (~gpio_table[PAD_Z80IO_OUT_DATA_0].m_out); }

    // Clock data.
    MHal_RIU_REG(gpio_table[PAD_Z80IO_WRITE].r_out) &= (~gpio_table[PAD_Z80IO_WRITE ].m_out);
    MHal_RIU_REG(gpio_table[PAD_Z80IO_WRITE].r_out) |=  gpio_table[PAD_Z80IO_WRITE ].m_out;

    return(0);
}
#endif


//--------------------------------------------------------
// SPI Methods.
//--------------------------------------------------------

// Methods to send 8,16 or 32 bits. Each method is seperate to minimise logic and execution time, 8bit being most sensitive.
// Macros have also been defined for inline inclusion which dont read back the response data.
//
uint8_t z80io_SPI_Send8(uint8_t txData, uint8_t *rxData)
{
    // Locals.
    uint32_t timeout = MAX_CHECK_CNT;

    // Insert data into write buffers.
    MSPI_WRITE(MSPI_WRITE_BUF_OFFSET, (uint16_t)txData);
    MSPI_WRITE(MSPI_WBF_SIZE_OFFSET, 1);
 
    // Enable SPI select.
    MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_ENABLE);

    // Send.
    MSPI_WRITE(MSPI_TRIGGER_OFFSET, MSPI_TRIGGER);

    // Wait for completion.
    while((MSPI_READ(MSPI_DONE_OFFSET) & MSPI_DONE_FLAG) == 0)
    {
        if(--timeout == 0)
            break;
    }
 
    // Disable SPI select.
    MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_DISABLE);

    // Clear flag.
    MSPI_WRITE(MSPI_DONE_CLEAR_OFFSET, MSPI_CLEAR_DONE);

    // Fetch data.
    if(rxData != NULL) *rxData = (uint8_t)MSPI_READ(MSPI_FULL_DEPLUX_RD00);
  
    // Done.
    return(timeout == 0);
}
uint8_t z80io_SPI_Send16(uint16_t txData, uint16_t *rxData)
{
    // Locals.
    uint32_t timeout = MAX_CHECK_CNT;

    // Insert data into write buffers.
    MSPI_WRITE(MSPI_WRITE_BUF_OFFSET, txData);
    MSPI_WRITE(MSPI_WBF_SIZE_OFFSET, 2);
   
    // Enable SPI select.
    MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_ENABLE);

    // Send.
    MSPI_WRITE(MSPI_TRIGGER_OFFSET, MSPI_TRIGGER);

    // Wait for completion.
    while((MSPI_READ(MSPI_DONE_OFFSET) & MSPI_DONE_FLAG) == 0)
    {
        if(--timeout == 0)
            break;
    }
  
    // Disable SPI select.
    MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_DISABLE);
  
    // Clear flag.
    MSPI_WRITE(MSPI_DONE_CLEAR_OFFSET, MSPI_CLEAR_DONE);

    // Fetch data.
    if(rxData != NULL) *rxData = MSPI_READ(MSPI_FULL_DEPLUX_RD00);

    // Done.
    return(timeout == 0);
}
uint8_t z80io_SPI_Send32(uint32_t txData, uint32_t *rxData)
{
    // Locals.
    uint32_t timeout = MAX_CHECK_CNT;

    // Insert data into write buffers.
    MSPI_WRITE(MSPI_WRITE_BUF_OFFSET,   (uint16_t)txData);
    MSPI_WRITE(MSPI_WRITE_BUF_OFFSET+1, (uint16_t)(txData >> 16));
    MSPI_WRITE(MSPI_WBF_SIZE_OFFSET, 4);

    // Enable SPI select.
    MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_ENABLE);

    // Send.
    MSPI_WRITE(MSPI_TRIGGER_OFFSET, MSPI_TRIGGER);

    // Wait for completion.
    while((MSPI_READ(MSPI_DONE_OFFSET) & MSPI_DONE_FLAG) == 0)
    {
        if(--timeout == 0)
            break;
    }

    // Disable SPI select.
    MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_DISABLE);

    // Clear flag.
    MSPI_WRITE(MSPI_DONE_CLEAR_OFFSET, MSPI_CLEAR_DONE);

    // Fetch data.
    if(rxData != NULL) *rxData =  (uint32_t)(MSPI_READ(MSPI_FULL_DEPLUX_RD00) | (MSPI_READ(MSPI_FULL_DEPLUX_RD02) << 16));

    // Done.
    return(timeout == 0);
}

//--------------------------------------------------------
// Test Methods.
//--------------------------------------------------------
#ifdef INCLUDE_TEST_METHODS
#include "z80io_test.c"
#else
uint8_t z80io_Z80_TestMemory(void)
{
    pr_info("Z80 Test Memory functionality not built-in.\n");
    return(0);
}
uint8_t z80io_SPI_Test(void)
{
    pr_info("SPI Test functionality not built-in.\n");
    return(0);
}
uint8_t z80io_PRL_Test(void)
{
    pr_info("Parallel Bus Test functionality not built-in.\n");
    return(0);
}
#endif
