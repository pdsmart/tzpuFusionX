/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80vhw_rfs.c.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Virtual Hardware Driver - Rom Filing System (RFS)
//                  This file contains the methods used to emulate the Rom Filing System add on for the
//                  MZ-80A.
//
//                  These drivers are intended to be instantiated inline to reduce overhead of a call
//                  and as such, they are included like header files rather than C linked object files.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Feb 2023 v1.0 - Initial write based on the RFS hardware.
//                           v1.1 - Bug fix, write logic was one byte out of kinter.
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
#define MROM_ADDR                             0x00000                                   // Base address of the 512K Monitor ROM.
#define USER_ROM_I_ADDR                       0x80000                                   // Base address of the first 512K User ROM.
#define USER_ROM_II_ADDR                      0x100000                                  // Base address of the second 512K User ROM.
#define USER_ROM_III_ADDR                     0x180000                                  // Base address of the third 512K User ROM.

// RFS Control Registers.
#define BNKCTRLRST                            0xEFF8                                    // Bank control reset, returns all registers to power up default.
#define BNKCTRLDIS                            0xEFF9                                    // Disable bank control registers by resetting the coded latch.
#define HWSPIDATA                             0xEFFB                                    // Hardware SPI Data register (read/write).
#define HWSPISTART                            0xEFFC                                    // Start an SPI transfer.
#define BNKSELMROM                            0xEFFD                                    // Select RFS Bank1 (MROM) 
#define BNKSELUSER                            0xEFFE                                    // Select RFS Bank2 (User ROM)
#define BNKCTRL                               0xEFFF                                    // Bank Control register (read/write).

//
// RFS v2 Control Register constants.
//
#define BBCLK                                 1                                         // BitBang SPI Clock.
#define SDCS                                  2                                         // SD Card Chip Select, active low.
#define BBMOSI                                4                                         // BitBang MOSI (Master Out Serial In).
#define CDLTCH1                               8                                         // Coded latch up count bit 1
#define CDLTCH2                               16                                        // Coded latch up count bit 2
#define CDLTCH3                               32                                        // Coded latch up count bit 3
#define BK2A19                                64                                        // User ROM Device Select Bit 0 (or Address bit 19).
#define BK2A20                                128                                       // User ROM Device Select Bit 1 (or Address bit 20).
                                                                                        // BK2A20 : BK2A19
                                                                                        //    0        0   = Flash RAM 0 (default).
                                                                                        //    0        1   = Flash RAM 1.
                                                                                        //    1        0   = Flasm RAM 2 or Static RAM 0.
                                                                                        //    1        1   = Reserved.`

#define BNKCTRLDEF                            BBMOSI+SDCS+BBCLK                         // Default on startup for the Bank Control register.

// SD Drive constants.
#define SD_CARD_FILENAME                      "/apps/FusionX/SD/SHARP_MZ80A_RFS_CPM_IMAGE_1.img"// SD Card Binary Image.

// MMC/SD command (SPI mode)
#define CMD0                                  0x40 + 0                                  // GO_IDLE_STATE 
#define CMD1                                  0x40 + 1                                  // SEND_OP_COND 
#define ACMD41                                0x40 + 41                                 // SEND_OP_COND (SDC) 
#define CMD8                                  0x40 + 8                                  // SEND_IF_COND 
#define CMD9                                  0x40 + 9                                  // SEND_CSD 
#define CMD10                                 0x40 + 10                                 // SEND_CID 
#define CMD12                                 0x40 + 12                                 // STOP_TRANSMISSION 
#define CMD13                                 0x40 + 13                                 // SEND_STATUS 
#define ACMD13                                0x40 + 13                                 // SD_STATUS (SDC) 
#define CMD16                                 0x40 + 16                                 // SET_BLOCKLEN 
#define CMD17                                 0x40 + 17                                 // READ_SINGLE_BLOCK 
#define CMD18                                 0x40 + 18                                 // READ_MULTIPLE_BLOCK 
#define CMD23                                 0x40 + 23                                 // SET_BLOCK_COUNT 
#define ACMD23                                0x40 + 23                                 // SET_WR_BLK_ERASE_COUNT (SDC)
#define CMD24                                 0x40 + 24                                 // WRITE_BLOCK 
#define CMD25                                 0x40 + 25                                 // WRITE_MULTIPLE_BLOCK 
#define CMD32                                 0x40 + 32                                 // ERASE_ER_BLK_START 
#define CMD33                                 0x40 + 33                                 // ERASE_ER_BLK_END 
#define CMD38                                 0x40 + 38                                 // ERASE 
#define CMD55                                 0x40 + 55                                 // APP_CMD 
#define CMD58                                 0x40 + 58                                 // READ_OCR 
#define SD_SECSIZE                            512                                       // Default size of an SD Sector 
#define SD_RETRIES                            0x0100                                    // Number of retries before giving up.

// Card type flags (CardType)
#define CT_MMC                                0x01                                      // MMC ver 3 
#define CT_SD1                                0x02                                      // SD ver 1 
#define CT_SD2                                0x04                                      // SD ver 2 
#define CT_SDC                                CT_SD1|CT_SD2                             // SD 
#define CT_BLOCK                              0x08                                      // Block addressing

// SD Card control variables.
typedef struct {
    uint8_t                                   trainingCnt;                              // Count of training bits to indicate card being initialised.
    uint8_t                                   initialised;                              // Flag to indicate the SD card has been initialised.
    uint8_t                                   writeFlag;                                // Flag to indicate an SD Write is taking place, assembling data prior to file write.
    uint8_t                                   cmdBuf[6+SD_SECSIZE];                     // SD Command Input Buffer.
    uint16_t                                  rcvCntr;                                  // SD Received byte counter.
    uint8_t const                            *respBuf;                                  // SD Response buffer.
    uint16_t                                  respCntr;                                 // SD Response buffer counter.
    uint16_t                                  respLen;                                  // SD size of data in response buffer.
    struct file                              *fhandle;                                  // Filehandle for the SD card image.
    uint8_t                                   regDataIn;                                // SD Card data in (from virtual card) register.
    uint8_t                                   regDataOut;                               // SD Card data out (to virtual card) register.
    uint8_t                                   dataOutFlag;                              // Flag to indicate data written to the SPI.
} t_SDCtrl;

// RFS Board registers.
typedef struct {
    uint8_t                                   regBank1;                                 // Bank 1, MROM, bank select register.
    uint8_t                                   regBank2;                                 // Bank 2, UROM, bank select register.
    uint8_t                                   regCtrl;                                  // Control register.
    uint8_t                                   upCntr;                                   // Register enable up counter.
    uint32_t                                  mromAddr;                                 // Actual address in MROM of active bank.
    uint32_t                                  uromAddr;                                 // Actual address in UROM of active bank.
    t_SDCtrl                                  sd;                                       // SD Control.
} t_RFSCtrl;

// RFS Board control.
static t_RFSCtrl RFSCtrl;

//-------------------------------------------------------------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------------------------------------------------------------

// Method to setup the memory page config to reflect installation of an RFS Board.
void rfsSetupMemory(enum Z80_MEMORY_PROFILE mode)
{
    // Locals.
    uint32_t    idx;

    // The RFS Board occupies the MROM slot 0x0000:0x0FFF and the User ROM slot 0xE800:0xEFFF. The actual part of the ROM which appears in these windows
    // is controlled by the REG_BANK1 (MROM) and REG_BANK2 (UROM) registers with the upper UROM registers provided in REG_CTRL.
    // So on initial setup, map the MROM page to point to the Z80 ROM area 0x00000 which is the base of the first 512K Flash ROM (virtual).

    // Setup defaults.
    RFSCtrl.regBank1        = 0x00;
    RFSCtrl.regBank2        = 0x00;
    RFSCtrl.regCtrl         = 0x00;
    RFSCtrl.mromAddr        = MROM_ADDR;
    RFSCtrl.uromAddr        = USER_ROM_I_ADDR;
    Z80Ctrl->memSwitch      = 0;
    RFSCtrl.sd.trainingCnt  = 0;
    RFSCtrl.sd.initialised  = 0;
    RFSCtrl.sd.dataOutFlag  = 0;
    RFSCtrl.sd.rcvCntr      = 0;
    RFSCtrl.sd.writeFlag    = 0;
    RFSCtrl.sd.respCntr     = 0;
    RFSCtrl.sd.respLen      = 0;
    RFSCtrl.sd.fhandle      = NULL;

    // Setup the initial state of the latch up-counter, used to enable access to the programmable registers.
    RFSCtrl.upCntr = ((RFSCtrl.regCtrl & 0x20) >> 2) | ((RFSCtrl.regCtrl & 0x10) >> 2) | ((RFSCtrl.regCtrl & 0x08) >> 2);

    // Initialise the page pointers and memory to use physical RAM.
    for(idx=0x0000; idx < 0x10000; idx+=MEMORY_BLOCK_GRANULARITY)
    {
        if(idx >= 0x0000 && idx < 0x1000)
        {
            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, (RFSCtrl.mromAddr+idx));
        }
        if(idx >= 0xE800 && idx < 0xF000)
        {
            // Memory is both ROM and hardware, the registers share the same address space.
            setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM | MEMORY_TYPE_VIRTUAL_HW, (RFSCtrl.uromAddr+(idx-0xE800)));
        }
    }
  
    // No I/O Ports on the RFS board.
    pr_info("RFS Memory Setup complete.\n");
}

// Perform any setup operations, such as variable initialisation, to enable use of this module.
void rfsInit(void)
{
    pr_info("Enabling RFS driver.\n");
}

// Method to decode an address and make any system memory map changes as required.
//
static inline void rfsDecodeMemoryMapSetup(zuint16 address, zuint8 data, uint8_t ioFlag, uint8_t readFlag)
{
    // Locals.
    uint32_t    idx;

    // Memory map switch.
    if(readFlag)
    {
        if(address == 0xE00C || address == 0xE00D || address == 0xE00E || address == 0xE00F)
        {
            for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
            {
                setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, (0xC000+idx));
                setMemoryType((idx+0xC000)/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, (RFSCtrl.mromAddr+idx));
            }
            Z80Ctrl->memSwitch = 0x01;
        }
                    
        // Reset memory map switch.
        else if(address == 0xE010 || address == 0xE011 || address == 0xE012 || address == 0xE013)
        {
            if(readFlag)
            {
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, (RFSCtrl.mromAddr+idx));
                    setMemoryType((idx+0xC000)/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_RAM, (0xC000+idx));
                }
            }
            Z80Ctrl->memSwitch = 0x00;
        }
    }
}

// Emulation of the RFS SD card. The emulation is made easier as the RFS uses automatic shift registers so we just handle byte data not assembly
// of bit data.
//
// All SD commands are added to the processor but logic (apart from a standard response) is only added for commands which RFS uses.
// 
void rfsSDCard(void)
{
    // Locals.
    //
    uint32_t      lbaAddr;
    int           noBytes;

    // If Chip select is active and we arent initialised, exit, need to initialise before processing data.
    if((RFSCtrl.regCtrl & SDCS) == 0 && RFSCtrl.sd.initialised == 0)
        return;

    // If the Chip select is inactive and we are initialised, exit, can only process data if selected.
    if((RFSCtrl.regCtrl & SDCS) && RFSCtrl.sd.initialised == 1)
        return;

    // SD Card initialised? If not, follow the SD protocol.
    //
    if(RFSCtrl.sd.initialised == 0)
    {
        // RFS initialisation sends 10 x 8bits, which is a little more than standard 74bits. Just check for 7 bytes and then set initialised flag.
        if(++RFSCtrl.sd.trainingCnt >= 7)
        {
            // Try and open the SD Card image. If it cannot be open then the SD card isnt registered as initialised.
            RFSCtrl.sd.fhandle = filp_open(SD_CARD_FILENAME, O_RDWR, S_IWUSR | S_IRUSR);
            if(IS_ERR(RFSCtrl.sd.fhandle))
            {
                pr_info("Error opening SD Card Image:%s\n:", SD_CARD_FILENAME);
            } else
            {
                RFSCtrl.sd.initialised = 1;
                RFSCtrl.sd.trainingCnt = 0;
            }
        }
        //pr_info("Training:%d, Initialised:%d\n", RFSCtrl.sd.trainingCnt, RFSCtrl.sd.initialised);
        
    } else
    {
        // If we are not receiving a command and response data is available, send it.
        if((RFSCtrl.sd.rcvCntr == 0 && RFSCtrl.sd.regDataOut == 0xFF) || RFSCtrl.sd.respBuf || RFSCtrl.sd.dataOutFlag == 0)
        {
            if(RFSCtrl.sd.respBuf)
            {
                RFSCtrl.sd.regDataIn = RFSCtrl.sd.respBuf[RFSCtrl.sd.respCntr++];
                //pr_info("Sending Response:%02x(%d)\n", RFSCtrl.sd.regDataIn, RFSCtrl.sd.respCntr);
                if(RFSCtrl.sd.respCntr == RFSCtrl.sd.respLen)
                {
                    RFSCtrl.sd.respBuf = NULL;
                    RFSCtrl.sd.respCntr = 0;
                }
            } else
            {
                //pr_info("Sending Response filler:0xFF\n");
                RFSCtrl.sd.regDataIn = 0xFF;
            }
        } 
        else 
        {
            // Clear out response buffer as latest data maybe an override, such as cancel.
            RFSCtrl.sd.respBuf = NULL;
            RFSCtrl.sd.respCntr = 0;

            // Clear out data write flag, this indicates when data is written into the SPI for transmission.
            RFSCtrl.sd.dataOutFlag = 0;

            // Store incoming data.
            RFSCtrl.sd.cmdBuf[RFSCtrl.sd.rcvCntr++] = RFSCtrl.sd.regDataOut;
            //pr_info("Received:%02x(%d)\n", RFSCtrl.sd.regDataOut, RFSCtrl.sd.rcvCntr);

            // 0xFE, <sector> <crc> 
            if(RFSCtrl.sd.rcvCntr == (SD_SECSIZE+3) && RFSCtrl.sd.writeFlag == 1)
            {
                static uint8_t response[] = { 0x05 };

                // Write the sector to the SD card image.
               // RFSCtrl.sd.cmdBuf[SD_SECSIZE+2] = 0x00;  // CRC but we dont set, not needed in this application.
               // RFSCtrl.sd.cmdBuf[SD_SECSIZE+3] = 0x00;
                noBytes = kernel_write(RFSCtrl.sd.fhandle, &RFSCtrl.sd.cmdBuf[1], SD_SECSIZE, RFSCtrl.sd.fhandle->f_pos);
                if(noBytes == 0) response[0] = 0x06; // Write error.
                RFSCtrl.sd.respBuf = response;
                RFSCtrl.sd.respLen = sizeof(response);
                //pr_info("Writing %d bytes, CRC(%02x,%02x)\n", noBytes, RFSCtrl.sd.cmdBuf[SD_SECSIZE+2], RFSCtrl.sd.cmdBuf[SD_SECSIZE+3]);

                // Reset to idle.
                RFSCtrl.sd.rcvCntr = 0;
                RFSCtrl.sd.writeFlag = 0;
            }

            // If we are not writing (receiving data from the Z80) and we have assembled a full command, process.
            else if(RFSCtrl.sd.rcvCntr == 6 && RFSCtrl.sd.writeFlag == 0)
            {
                RFSCtrl.sd.rcvCntr = 0;

                //pr_info("Received Command:%02x,%02x,%02x,%02x,%02x,%02x\n", RFSCtrl.sd.cmdBuf[0],RFSCtrl.sd.cmdBuf[1],RFSCtrl.sd.cmdBuf[2],RFSCtrl.sd.cmdBuf[3],RFSCtrl.sd.cmdBuf[4],RFSCtrl.sd.cmdBuf[5]);
                switch(RFSCtrl.sd.cmdBuf[0])
                {
                    case CMD0:    // GO_IDLE_STATE 
                        //pr_info("GO_IDLE_STATE\n");
                        {
                            // Initialise to SPI Mode. RFS doesnt support other modes.
                            static const uint8_t response[] = { 0x01 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD1:    // SEND_OP_COND 
                        //pr_info("SEND_OP_COND\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case ACMD41:  // SEND_OP_COND (SDC) 
                        //pr_info("SEND_OP_COND\n");
                        {
                            static const uint8_t response[] = { 0x00 }; // 0 = Ready, 1 = Idle
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD8:    // SEND_IF_COND 
                        //pr_info("SEND_IF_COND\n");
                        {
                            static const uint8_t response[] = { 0x01, 0x00, 0x00, 0x01, 0xAA };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD9:    // SEND_CSD 
                        //pr_info("SEND_CSD\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD10:   // SEND_CID 
                        //pr_info("SEND_CID\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD12:   // STOP_TRANSMISSION 
                        //pr_info("STOP_TRANSMISSION\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case ACMD13:  // SD_STATUS (SDC) 
                        //pr_info("SD_STATUS\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD16:   // SET_BLOCKLEN 
                        //pr_info("SD: Set Block Size:%d\n", (RFSCtrl.sd.cmdBuf[2] << 8 | RFSCtrl.sd.cmdBuf[3]));
                        {
                            static const uint8_t response[] = { 0x01 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD17:   // READ_SINGLE_BLOCK 
                        //pr_info("READ_SINGLE_BLOCK\n");
                        {
                            static uint8_t response[6 + SD_SECSIZE];
                            memset(response, 0, sizeof(response));

                            // Assemble LBA address and seek to the right location in the SD card image.
                            lbaAddr = RFSCtrl.sd.cmdBuf[1] << 24 | RFSCtrl.sd.cmdBuf[2] << 16 | RFSCtrl.sd.cmdBuf[3] << 8 | RFSCtrl.sd.cmdBuf[4];
                            //pr_info("Reading LBA %d\n", lbaAddr);
                            vfs_llseek(RFSCtrl.sd.fhandle, lbaAddr * SD_SECSIZE, SEEK_SET);

                            // Assemble response buffer including SD card sector.
                            response[0] = 0x00;  // Response R1
                            response[1] = 0xFE;  // Start of Data marker.
                            noBytes = kernel_read(RFSCtrl.sd.fhandle, RFSCtrl.sd.fhandle->f_pos, &response[2], SD_SECSIZE);
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = SD_SECSIZE + 2 + 2; // Sector Size + 2 bytes (R1 + Data Start) + 2 CRC
                        }
                        break;

                    case CMD18:   // READ_MULTIPLE_BLOCK 
                        //pr_info("READ_MULTIPLE_BLOCK\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;
                        
                    case ACMD23:  // SET_WR_BLK_ERASE_COUNT (SDC)
                        //pr_info("SET_WR_BLK_ERASE_COUNT\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD24:   // WRITE_BLOCK 
                        //pr_info("WRITE_BLOCK\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            
                            // Assemble LBA address and seek to the right location in the SD card image.
                            lbaAddr = RFSCtrl.sd.cmdBuf[1] << 24 | RFSCtrl.sd.cmdBuf[2] << 16 | RFSCtrl.sd.cmdBuf[3] << 8 | RFSCtrl.sd.cmdBuf[4];
                            //pr_info("Writing LBA %d\n", lbaAddr);
                            vfs_llseek(RFSCtrl.sd.fhandle, lbaAddr * SD_SECSIZE, SEEK_SET);

                            // Assemble response which is Ready, data can now be sent for sector. Set write flag so we know data is being received as sector data.
                            RFSCtrl.sd.respBuf   = response;
                            RFSCtrl.sd.respLen   = sizeof(response);
                            RFSCtrl.sd.writeFlag = 1;
                        }
                        break;

                    case CMD25:   // WRITE_MULTIPLE_BLOCK 
                        //pr_info("WRITE_MULTIPLE_BLOCK\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD32:   // ERASE_ER_BLK_START 
                        //pr_info("ERASE_ER_BLK_START\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD33:   // ERASE_ER_BLK_END 
                        //pr_info("ERASE_ER_BLK_END\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD38:   // ERASE 
                        //pr_info("ERASE\n");
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD55:   // APP_CMD 
                        //pr_info("APP_CMD\n");
                        {
                            static const uint8_t response[] = { 0x01 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    case CMD58:   // READ_OCR 
                        //pr_info("READ_OCR\n");
                        {
                            static const uint8_t response[] = { 0x00, 0x00, 0x00, 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                    default:
                        pr_info("UNHANDLED REQUEST:%02x,%02x,%02x,%02x,%02x,%02x\n", RFSCtrl.sd.cmdBuf[0],RFSCtrl.sd.cmdBuf[1],RFSCtrl.sd.cmdBuf[2],RFSCtrl.sd.cmdBuf[3],RFSCtrl.sd.cmdBuf[4],RFSCtrl.sd.cmdBuf[5]);
                        {
                            static const uint8_t response[] = { 0x00 };
                            RFSCtrl.sd.respBuf = response;
                            RFSCtrl.sd.respLen = sizeof(response);
                        }
                        break;

                }

            }
            else
            {
                // Standard response when a byte is received but not enough assembled to process.
                RFSCtrl.sd.regDataIn = 0xFF;
            }
        }
    }

    return;
}

// Method to read from either the memory mapped registers if enabled else the ROM.
static inline uint8_t rfsRead(zuint16 address, uint8_t ioFlag)
{
    // Locals.
    uint8_t     data = 0xFF;

    //pr_info("RFS+Read:%04x, BK1:%02x, BK2:%02x, CTRL:%02x, MROM:%08x, UROM:%08x\n", address, RFSCtrl.regBank1, RFSCtrl.regBank2, RFSCtrl.regCtrl, RFSCtrl.mromAddr, RFSCtrl.uromAddr);

    // Any access to the control region increments the enable counter till it reaches terminal count and enables writes to the registers. When the counter
    // gets to 15 the registers are enabled and the EPROM, in the control region, is disabled.
    if((address >= BNKCTRLRST && address <= BNKCTRL) && RFSCtrl.upCntr < 15)
    {
        RFSCtrl.upCntr++;
    }

    // Address in control region and register bank enabled?
    //
    if(RFSCtrl.upCntr >= 15 && (address >= BNKCTRLRST && address <= BNKCTRL))
    {
        switch(address)
        {
            // Bank control reset, returns all registers to power up default.
            case BNKCTRLRST:
                break;

            // Disable bank control registers by resetting the coded latch.
            case BNKCTRLDIS:
                RFSCtrl.upCntr = (RFSCtrl.regCtrl >> 2) & 0x0E;
                break;

            // Hardware SPI Data register (read/write).
            case HWSPIDATA:
                // Action data, ie. run the SD emulation.
                data = RFSCtrl.sd.regDataIn;
                //pr_info("HWSPIDATA ReadOut:%02x\n", data);
                break;

            // Start an SPI transfer.
            case HWSPISTART:
                break;

            // Select RFS Bank1 (MROM) - No action for read, real hardware would store an undefined value into register.
            case BNKSELMROM:
                break;

            // Select RFS Bank2 (User ROM) - No action for read.
            case BNKSELUSER:
                break;

            // Bank Control register (read/write).
            case BNKCTRL:
                break;
        }
    } else
    {
        // Return the contents of the ROM at given address.
        data = isVirtualROM(address) ? readVirtualROM(address) : readVirtualRAM(address);
    }

    //pr_info("RFS-Read:%04x, Data:%02x, BK1:%02x, BK2:%02x, CTRL:%02x, MROM:%08x, UROM:%08x\n", address, data, RFSCtrl.regBank1, RFSCtrl.regBank2, RFSCtrl.regCtrl, RFSCtrl.mromAddr, RFSCtrl.uromAddr);
    return(data);
}

// Method to handle writes to the RFS board. Generally the RFS board.
static inline void rfsWrite(zuint16 address, zuint8 data, uint8_t ioFlag)
{
    // Locals.
    uint32_t     idx;

    //pr_info("RFS+Write:%04x, Data:%02x, BK1:%02x, BK2:%02x, CTRL:%02x, MROM:%08x, UROM:%08x\n", address, data, RFSCtrl.regBank1, RFSCtrl.regBank2, RFSCtrl.regCtrl, RFSCtrl.mromAddr, RFSCtrl.uromAddr);

    // Any access to the control region increments the enable counter till it reaches terminal count and enables writes to the registers. When the counter
    // gets to 15 the registers are enabled and the EPROM, in the control region, is disabled.
    if((address >= BNKCTRLRST && address <= BNKCTRL) && RFSCtrl.upCntr < 15)
    {
        RFSCtrl.upCntr++;
    }

    // Address in control region and register bank enabled?
    //
    if(RFSCtrl.upCntr >= 15 && (address >= BNKCTRLRST && address <= BNKCTRL))
    {
        switch(address)
        {
            // Bank control reset, returns all registers to power up default.
            case BNKCTRLRST: // 0xEFF8
                break;

            // Disable bank control registers by resetting the coded latch.
            case BNKCTRLDIS: // 0xEFF9
            default:
                RFSCtrl.upCntr = (RFSCtrl.regCtrl >> 2) & 0x0E;
                break;
             
            // Hardware SPI Data register (read/write).
            case HWSPIDATA:
                RFSCtrl.sd.regDataOut = data;
                RFSCtrl.sd.dataOutFlag = 1;
                //pr_info("HWSPIDATA WriteIn:%02x\n", data);
                break;

            // Start an SPI transfer.
            case HWSPISTART:
                // Action data, ie. run the SD emulation as a write to this address starts the SPI clock which clocks out 8bit and clocks in 8 bits.
                //pr_info("HWSPISTART\n");
                rfsSDCard();
                break;

            // Select RFS Bank1 (MROM) 
            case BNKSELMROM:
                RFSCtrl.regBank1 = data;

                // Update monitor ROM address as register contains upper address bits of the ROM.
                RFSCtrl.mromAddr = (uint32_t)(RFSCtrl.regBank1 << 12);
              
                // Update memory map to reflect register change.
                for(idx=0x0000; idx < 0x1000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    if(Z80Ctrl->memSwitch)
                    {
                        // Monitor ROM is located at 0xC000.
                        setMemoryType((0xC000+idx)/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, (RFSCtrl.mromAddr+idx));
                    }
                    else
                    {
                        // Monitor ROM is located at 0x000.
                        setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM, (RFSCtrl.mromAddr+idx));
                    }
                }
                break;

            // Select RFS Bank2 (User ROM)
            case BNKSELUSER:
            // Bank Control register (read/write).
            case BNKCTRL:
                if(address == BNKSELUSER) { RFSCtrl.regBank2 = data; } else { RFSCtrl.regCtrl = data; }

                // Update user ROM address as register contain upper address bits of the ROM.
                RFSCtrl.uromAddr = (uint32_t)(((((RFSCtrl.regCtrl&0xB0) << 2) | RFSCtrl.regBank2) << 11) + USER_ROM_I_ADDR);

                // Update memory map to reflect register change.
                for(idx=0xE800; idx < 0xF000; idx+=MEMORY_BLOCK_GRANULARITY)
                {
                    // Memory is both ROM and hardware, the registers share the same address space.
                    setMemoryType(idx/MEMORY_BLOCK_GRANULARITY, MEMORY_TYPE_VIRTUAL_ROM | MEMORY_TYPE_VIRTUAL_HW, (RFSCtrl.uromAddr+(idx-0xE800)));
                }
                break;
        }
    } else
    {
        // Any unprocessed write is commited to RAM.
        writeVirtualRAM(address, data);
    }
    //pr_info("RFS-Write:%04x, Data:%02x, BK1:%02x, BK2:%02x, CTRL:%02x, MROM:%08x, UROM:%08x\n", address, data, RFSCtrl.regBank1, RFSCtrl.regBank2, RFSCtrl.regCtrl, RFSCtrl.mromAddr, RFSCtrl.uromAddr);
    return;
}
