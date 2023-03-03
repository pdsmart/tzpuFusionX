/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80io_test.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 IO Interface Test Methods
//                  This file contains the methods used to test the SOM to CPLD interface and evaluate
//                  it's performance. Production builds wont include these methods.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/sched.h>

//--------------------------------------------------------
// Test Methods.
//--------------------------------------------------------
uint8_t z80io_Z80_TestMemory(void)
{
    // Locals.
    //
    uint32_t       addr;
    uint32_t       fullCmd;
    uint8_t        cmd;
    struct timeval start, stop;
    uint32_t       iterations = 100;
    uint32_t       errorCount;
    uint32_t       idx;
    long           totalTime;
    long           bytesMSec;
    uint8_t        result;
    spinlock_t     spinLock;
    unsigned long  flags;

    SPI_SEND8(CPLD_CMD_CLEAR_AUTO_REFRESH);

    SPI_SEND32(0x00E30000 | (0x07 << 8) | CPLD_CMD_WRITEIO_ADDR);
    udelay(100);
    SPI_SEND32(0x00E80000 | (0x82 << 8) | CPLD_CMD_WRITEIO_ADDR);
    udelay(100);
    SPI_SEND32(0x00E20000 | (0x58 << 8) | CPLD_CMD_WRITEIO_ADDR);
    udelay(100);
    SPI_SEND32(0x00E00000 | (0xF7 << 8) | CPLD_CMD_WRITEIO_ADDR);
    udelay(100);
    SPI_SEND32(0x00E90000 | (0x0F << 8) | CPLD_CMD_WRITEIO_ADDR);
    udelay(100);
    SPI_SEND32(0x00EB0000 | (0xCF << 8) | CPLD_CMD_WRITEIO_ADDR);
    udelay(100);
    SPI_SEND32(0x00EB0000 | (0xFF << 8) | CPLD_CMD_WRITEIO_ADDR);
    udelay(100);
    pr_info("Z80 Host Test - IO.\n");
//    for(idx=0; idx < 1000000; idx++)
//    {
//        SPI_SEND32(0x00E80000 | (0xD3 << 8) | CPLD_CMD_WRITEIO_ADDR);
//        SPI_SEND32(0xD0000000 | (0x41 << 8) | CPLD_CMD_WRITE_ADDR);
//        SPI_SEND32(0xD0100000 | (0x41 << 8) | CPLD_CMD_WRITE_ADDR);
//        SPI_SEND32(0xD0200000 | (0x41 << 8) | CPLD_CMD_WRITE_ADDR);
//        SPI_SEND32(0xD0300000 | (0x41 << 8) | CPLD_CMD_WRITE_ADDR);
//        SPI_SEND32(0xD0400000 | (0x41 << 8) | CPLD_CMD_WRITE_ADDR);
//        SPI_SEND32(0xD0500000 | (0x41 << 8) | CPLD_CMD_WRITE_ADDR);
//    }

    spin_lock_init(&spinLock);
    pr_info("Z80 Host Test - Testing IO Write performance.\n");
    do_gettimeofday(&start);
    spin_lock_irqsave(&spinLock, flags);
    for(idx=0; idx < iterations; idx++)
    {
        for(addr=0x0000; addr < 0x10000; addr++)
        {
            fullCmd = 0x00000000| ((uint8_t)addr) << 8 | CPLD_CMD_WRITEIO_ADDR;
            SPI_SEND32(fullCmd);
        }
    }
    spin_unlock_irqrestore(&spinLock, flags);
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations*0xC000)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

    spin_lock_init(&spinLock);
    pr_info("Z80 Host Test - Testing IO Read performance.\n");
    do_gettimeofday(&start);
    spin_lock_irqsave(&spinLock, flags);
    for(idx=0; idx < iterations; idx++)
    {
        // Go through all the accessible IO ports and write to it.
        for(addr=0x0000; addr < 0x10000; addr++)
        {
            fullCmd = 0x00000000 | ((uint8_t)addr) << 8 | CPLD_CMD_READIO_ADDR;
            SPI_SEND32(fullCmd);
        }
    }
    spin_unlock_irqrestore(&spinLock, flags);
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations*0xC000)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

    spin_lock_init(&spinLock);
    pr_info("Z80 Host Test - Testing RAM Write performance.\n");
    do_gettimeofday(&start);
    spin_lock_irqsave(&spinLock, flags);
    for(idx=0; idx < iterations; idx++)
    {
        // Go through all the accessible RAM and write to it.
        for(addr=0x1000; addr < 0xD000; addr++)
        {
            fullCmd = (addr << 16) | ((uint8_t)addr) << 8 | 0x18;
            SPI_SEND32(fullCmd);
        }
    }
    spin_unlock_irqrestore(&spinLock, flags);
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations*0xC000)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

    pr_info("Z80 Host Test - Testing RAM Write performance (opt).\n");
    do_gettimeofday(&start);
    spin_lock_irqsave(&spinLock, flags);
    for(idx=0; idx < iterations; idx++)
    {
        // Go through all the accessible RAM and write to it.
        for(addr=0x1000; addr < 0xD000; addr++)
        {
            if(addr == 0x1000)
            {
                fullCmd = (addr << 16) | ((uint8_t)addr) << 8 | 0x18;
                SPI_SEND32(fullCmd);
            } else
            {
                cmd = 0x19;
                SPI_SEND16(((uint8_t)addr) << 8 | cmd);
            }
        }
    }
    spin_unlock_irqrestore(&spinLock, flags);
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations*0xC000)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

    pr_info("Z80 Host Test - Testing RAM Write/Fetch performance (opt).\n");
    errorCount = 0;
    SET_CPLD_READ_DATA();
    //MHal_RIU_REG(gpio_table[PAD_Z80IO_HIGH_BYTE ].r_out) |= gpio_table[PAD_Z80IO_HIGH_BYTE ].m_out;
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Go through all the accessible RAM and write to it.
        for(addr=0x8000; addr < 0xD000; addr++)
        {
            if(addr == 0x8000)
            {
                fullCmd = (addr << 16) | ((uint8_t)addr) << 8 | 0x18;
                SPI_SEND32(fullCmd);
            } else
            {
                cmd = 0x19;
                SPI_SEND16(((uint8_t)addr) << 8 | cmd);
            }

            // Read back the same byte.
            cmd = 0x10;
            SPI_SEND8(cmd);
            while(CPLD_READY() == 0);

            result = READ_CPLD_DATA_IN();
            if(result != (uint8_t)addr)
            {
                if(errorCount < 50) pr_info("Read byte:0x%x, Written:0x%x\n", result, (uint8_t)addr);
                errorCount++;
            }
        }
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations*0xC000)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, errorCount=%d, %ldBytes/sec\n", totalTime/1000, errorCount, (bytesMSec*1000));

    pr_info("Z80 Host Test - Testing RAM Write/Read performance (opt).\n");
    errorCount = 0;
    SET_CPLD_READ_DATA();
    //MHal_RIU_REG(gpio_table[PAD_Z80IO_HIGH_BYTE ].r_out) |= gpio_table[PAD_Z80IO_HIGH_BYTE ].m_out;
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Go through all the accessible RAM and write to it.
        for(addr=0x8000; addr < 0xD000; addr++)
        {
            if(addr == 0x8000)
            {
                fullCmd = (addr << 16) | ((uint8_t)addr) << 8 | 0x18;
                SPI_SEND32(fullCmd);
            } else
            {
                cmd = 0x19;
                SPI_SEND16(((uint8_t)addr) << 8 | cmd);
            }

            // Read back the same byte.
            cmd = 0x20;
            SPI_SEND8(cmd);
            while(CPLD_READY() == 0);

            result = READ_CPLD_DATA_IN();
            if(result != (uint8_t)addr)
            {
                if(errorCount < 50) pr_info("Read byte:0x%x, Written:0x%x\n", result, (uint8_t)addr);
                errorCount++;
            }
        }
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations*0xC000)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, errorCount=%d, %ldBytes/sec\n", totalTime/1000, errorCount, (bytesMSec*1000));

    pr_info("Z80 Host Test - Testing RAM Fetch performance.\n");
    SET_CPLD_READ_DATA();
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Go through all the accessible RAM and read from it.
        for(addr=0x1000; addr < 0xD000; addr++)
        {
            if(addr == 0x1000)
            {
                fullCmd = (addr << 16) | ((uint8_t)addr) << 8 | 0x10;
                SPI_SEND32(fullCmd);
            } else
            {
                cmd = 0x11;
                SPI_SEND8(cmd);
            }
            while(CPLD_READY() == 0);
            result = READ_CPLD_DATA_IN();
        }
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations*0xC000)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

    pr_info("Z80 Host Test - Testing RAM Read performance (opt).\n");
    SET_CPLD_READ_DATA();
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Go through all the accessible RAM and read from it.
        for(addr=0x1000; addr < 0xD000; addr++)
        {
            if(addr == 0x1000)
            {
                fullCmd = (addr << 16) | ((uint8_t)addr) << 8 | 0x20;
                SPI_SEND32(fullCmd);
            } else
            {
                cmd = 0x21;
                SPI_SEND8(cmd);
            }
            while(CPLD_READY() == 0);
            result = READ_CPLD_DATA_IN();
        }
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations*0xC000)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

    // Go through all the accessible attribute VRAM and initialise it.
    pr_info("Z80 Host Test - Testing VRAM Write performance.\n");
    SPI_SEND32(0x00E80000 | (0xD3 << 8) | CPLD_CMD_WRITEIO_ADDR);
    iterations = 256*10;
    do_gettimeofday(&start);
    for(addr=0xD800; addr < 0xE000; addr++)
    {
        //while(CPLD_READY() == 0);
        if(addr == 0xD800)
        {
            fullCmd = (addr << 16) |(0x71 << 8) | 0x18;
            SPI_SEND32(fullCmd);
        } else
        {
            cmd = 0x19;
            SPI_SEND8(cmd);
        }
    }
    for(idx=0; idx < iterations; idx++)
    {
        // Go through all the accessible VRAM and write to it.
        for(addr=0xD000; addr < 0xD800; addr++)
        {
            //while(CPLD_READY() == 0);
            if(addr == 0xD000)
            {
                fullCmd = (addr << 16) | ((uint8_t)idx << 8) | 0x18;
                SPI_SEND32(fullCmd);
            } else
            {
                cmd = 0x19;
                SPI_SEND8(cmd);
            }
        }
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)((1*iterations*0x800)+0x800)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

    return(0);
}


// A simple test to verify the SOM to CPLD SPI connectivity and give an estimate of its performance. 
// The performance is based on the SPI setup and transmit time along with the close and received data processing.
// In real use, the driver will just send a command and generally ignore received data so increased throughput can be achieved.
//
uint8_t z80io_SPI_Test(void)
{
    // Locals.
    //
    struct timeval start, stop;
    uint32_t       iterations = 10000000;
    uint32_t       idx;
    uint8_t        rxData8;
    uint16_t       rxData16;
    uint16_t       rxData16Last;
    uint32_t       rxData32;
    uint32_t       rxData32Last;
    uint32_t       errorCount;
    long           totalTime;
    long           bytesMSec;

    // Place the CPLD into echo test mode.
    z80io_SPI_Send8(0xfe, &rxData8);

    // 1st. test, 8bit.
    pr_info("SPI Test - Testing 8 bit performance.\n");
    errorCount=0;
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        z80io_SPI_Send8((uint8_t)idx, &rxData8);
        if(idx > 1 && (uint8_t)(idx-1) != rxData8)
        {
            if(errorCount < 20)
                pr_info("0x%x: Last(0x%x) /= New(0x%x)\n",idx, (uint8_t)(idx-1), rxData8 );
            errorCount++;
        }
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(1*iterations)/((long)totalTime/1000);
    pr_info("Loop mode errorCount: %d, time=%ldms, %ldBytes/sec\n", errorCount, totalTime/1000, (bytesMSec*1000));
    
    // 2nd. test, 16bit.
    pr_info("SPI Test - Testing 16 bit performance.\n");
    errorCount=0;
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Byte re-ordering required as the CPLD echo's back the last 8bits received, it doesnt know if a transmission is 8/16/32bits.
        z80io_SPI_Send16((uint16_t)idx, &rxData16);
        if(idx > 0 && (uint16_t)(idx-1) != (uint16_t)(((rxData16&0x00ff) << 8) | ((rxData16Last & 0xff00) >> 8)))
        {
            if(errorCount < 20)
                pr_info("0x%x: Last(0x%x) /= New(0x%x)\n",idx, (uint16_t)(idx-1),  (uint16_t)(((rxData16&0x00ff) << 8) | ((rxData16Last & 0xff00) >> 8)));
            errorCount++;
        }
        rxData16Last = rxData16;
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(2*iterations)/((long)totalTime/1000);
    pr_info("Loop mode errorCount: %d, time=%ldms, %ldBytes/sec\n", errorCount, totalTime/1000, (bytesMSec*1000));

    // 3rd. test, 32bit.
    pr_info("SPI Test - Testing 32 bit performance.\n");
    errorCount=0;
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        z80io_SPI_Send32((uint32_t)idx, &rxData32);
        if(idx > 0 && (uint32_t)(idx-1) != (uint32_t)(((rxData32&0x00ff) << 8) | ((rxData32Last & 0xff000000) >> 8) |  ((rxData32Last & 0xff0000) >> 8) | ((rxData32Last & 0xff00) >> 8)))
        {
            if(errorCount < 20)
                pr_info("0x%x: Last(0x%x) /= New(0x%x)\n",idx, (uint32_t)(idx-1), (uint32_t)(((rxData32&0x00ff) << 8) | ((rxData32Last & 0xff000000) >> 8) |  ((rxData32Last & 0xff0000) >> 8) | ((rxData32Last & 0xff00) >> 8)));
            errorCount++;
        }
        rxData32Last = rxData32;
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(4*iterations)/((long)totalTime/1000);
    pr_info("Loop mode errorCount: %d, time=%ldms, %ldBytes/sec\n", errorCount, totalTime/1000, (bytesMSec*1000));

    pr_info("Press host RESET button Once to reset the CPLD.\n");
    return(0);
}

// Method to test the parallel bus, verifying integrity and assessing performance.
uint8_t z80io_PRL_Test(void)
{
    // Locals.
    //
    struct timeval start, stop;
    uint32_t       iterations = 10000000;
    uint32_t       idx;
    uint8_t        rxData8;
    uint16_t       rxData16;
    long           totalTime;
    long           bytesMSec;
#ifdef NOTNEEDED
    uint32_t       errorCount;
#endif

    // Place the CPLD into echo test mode.

    // 1st. test, 8bit RW.
#ifdef NOTNEEDED
    pr_info("Parallel Test - Testing 8 bit r/w performance.\n");
    errorCount=0;
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Write byte and readback to compare.
        z80io_PRL_Send8((uint8_t)idx);
        rxData8 = z80io_PRL_Read8();
        if((uint8_t)idx != rxData8)
        {
            pr_info("0x%x: Written(0x%x) /= Read(0x%x)\n", idx, (uint8_t)(idx), rxData8);
            errorCount++;
        }
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(iterations)/((long)totalTime/1000);
    pr_info("Loop mode errorCount: %d, time=%ldms, %ldBytes/sec\n", errorCount, totalTime/1000, (bytesMSec*1000));
    
    // 2nd. test, 8bit Write.
    pr_info("Parallel Test - Testing 8 bit write performance.\n");
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Write byte.
        z80io_PRL_Send8((uint8_t)idx);
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(iterations)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));
#endif
    
    // 3rd. test, 8bit Read.
    pr_info("Parallel Test - Testing 8 bit read performance.\n");
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Read byte.
        rxData8 = z80io_PRL_Read8(0);
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(iterations)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

#ifdef NOTNEEDED
    // 4th test, 16bit.
    pr_info("Parallel Test - Testing 16 bit r/w performance.\n");
    errorCount=0;
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Byte re-ordering required as the CPLD echo's back the last 8bits received, it doesnt know if a transmission is 8/16/32bits.
        z80io_PRL_Send16((uint16_t)idx);
        rxData16 = z80io_PRL_Read16();
        if((uint16_t)idx != rxData16)
        {
            pr_info("0x%x: Written(0x%x) /= Read(0x%x)\n", idx, (uint16_t)(idx), rxData16);
            errorCount++;
        }
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(2*iterations)/((long)totalTime/1000);
    pr_info("Loop mode errorCount: %d, time=%ldms, %ldBytes/sec\n", errorCount, totalTime/1000, (bytesMSec*1000));
    
    // 5th test, 16bit Write.
    pr_info("Parallel Test - Testing 16 bit write performance.\n");
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Write word.
        z80io_PRL_Send16((uint16_t)idx);
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(2*iterations)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));
#endif
    
    // 6th test, 16bit Read.
    pr_info("Parallel Test - Testing 16 bit read performance.\n");
    do_gettimeofday(&start);
    for(idx=0; idx < iterations; idx++)
    {
        // Read word.
        rxData16 = z80io_PRL_Read16();
    }
    do_gettimeofday(&stop);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesMSec = (long)(2*iterations)/((long)totalTime/1000);
    pr_info("Loop mode time=%ldms, %ldBytes/sec\n", totalTime/1000, (bytesMSec*1000));

    pr_info("Press host RESET button Once to reset the CPLD.\n");
    return(0);
}
