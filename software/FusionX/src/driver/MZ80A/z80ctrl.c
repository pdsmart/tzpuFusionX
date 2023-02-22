/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80ctrl.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Control Interface 
//                  This file contains a command line utility tool for controlling the z80drv device
//                  driver. The tool allows manipulation of the emulated Z80, inspection of its
//                  memory and data, transmission of adhoc commands to the underlying CPLD-Z80
//                  gateway and loading/saving of programs and data to/from the Z80 virtual and
//                  host memory.
//
// Credits:         Zilog Z80 CPU Emulator v0.2 written by Manuel Sainz de Baranda y Goñi
//                  The Z80 CPU Emulator is the heart of the Z80 device driver.
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//                  (c) 1999-2023 Manuel Sainz de Baranda y Goñi
//
// History:         Oct 2022 v1.0  - v1.Initial write of the z80 kernel driver software.
//                  Feb 2023 v1.1  - Extended to allow Rom upload for RFS and other drivers.
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
#include <stdio.h>
#include <stdlib.h>  
#include <stdint.h>
#include <unistd.h>  
#include <fcntl.h>  
#include <sys/ioctl.h>  
#include <unistd.h>  
#include <stdarg.h>  
#include <sys/mman.h>  
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <Z/constants/pointer.h>
#include <Z/macros/member.h>
#include <Z/macros/array.h>
#include <Z80.h>
#include "z80driver.h"

#define VERSION                      "1.1"
#define AUTHOR                       "P.D.Smart"
#define COPYRIGHT                    "(c) 2018-23"
  
// Getopt_long is buggy so we use optparse.
#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API                 static
#include "optparse.h"

// Device driver name.
#define DEVICE_FILENAME              "/dev/z80drv"

// Constants for the Sharp MZ80A MZF file format.
#define MZF_HEADER_SIZE              128                                 // Size of the MZF header.
#define MZF_ATTRIBUTE                0x00                                // Code Type, 01 = Machine Code.
#define MZF_FILENAME                 0x01                                // Title/Name (17 bytes).
#define MZF_FILENAME_LEN             17                                  // Length of the filename, it is not NULL terminated, generally a CR can be taken as terminator but not guaranteed.
#define MZF_FILESIZE                 0x12                                // Size of program.
#define MZF_LOADADDR                 0x14                                // Load address of program.
#define MZF_EXECADDR                 0x16                                // Exec address of program.
#define MZF_COMMENT                  0x18                                // Comment, used for details of the file or startup code.
#define MZF_COMMENT_LEN              104                                 // Length of the comment field.
#define CMT_TYPE_OBJCD               0x001                               // MZF contains a binary object.
#define CMT_TYPE_BTX1CD              0x002                               // MZF contains a BASIC program.
#define CMT_TYPE_BTX2CD              0x005                               // MZF contains a BASIC program.
#define CMT_TYPE_TZOBJCD0            0x0F8                               // MZF contains a TZFS binary object for page 0.
#define CMT_TYPE_TZOBJCD1            0x0F9 
#define CMT_TYPE_TZOBJCD2            0x0FA 
#define CMT_TYPE_TZOBJCD3            0x0FB 
#define CMT_TYPE_TZOBJCD4            0x0FC 
#define CMT_TYPE_TZOBJCD5            0x0FD 
#define CMT_TYPE_TZOBJCD6            0x0FE 
#define CMT_TYPE_TZOBJCD7            0x0FF                               // MZF contains a TZFS binary object for page 7.
#define MZ_CMT_ADDR                  0x10F0

// Structure to define a Sharp MZ80A MZF directory structure. This header appears at the beginning of every Sharp MZ80A tape (and more recently archived/emulator) images.
//
typedef struct __attribute__((__packed__)) {
    uint8_t                          attr;                               // MZF attribute describing the file.
    uint8_t                          fileName[MZF_FILENAME_LEN];         // Each directory entry is the size of an MZF filename.
    uint16_t                         fileSize;                           // Size of file.
    uint16_t                         loadAddr;                           // Load address for the file.
    uint16_t                         execAddr;                           // Execution address where the Z80 starts processing.
    uint8_t                          comment[MZF_COMMENT_LEN];           // Text comment field but often contains a startup machine code program.
} t_svcDirEnt;

// Possible commands to be issued to the Z80 driver.
enum CTRL_COMMANDS {
    Z80_CMD_STOP                     = 0,
    Z80_CMD_START                    = 1,
    Z80_CMD_PAUSE                    = 2,
    Z80_CMD_CONTINUE                 = 3,
    Z80_CMD_RESET                    = 4,
    Z80_CMD_SPEED                    = 5,
    Z80_CMD_HOST_RAM                 = 6,
    Z80_CMD_VIRTUAL_RAM              = 7,
    Z80_CMD_DUMP_MEMORY              = 8,
    Z80_CMD_MEMORY_TEST              = 9,
    Z80_CMD_ADD_DEVICE               = 10,
    Z80_CMD_DEL_DEVICE               = 11,
    CPLD_CMD_SEND_CMD                = 12,
    CPLD_CMD_SPI_TEST                = 13,
    CPLD_CMD_PRL_TEST                = 14,
  #if(DEBUG_ENABLED != 0)
    Z80_CMD_DEBUG                    = 15,
  #endif
};


// Shared memory between this process and the Z80 driver.
static t_Z80Ctrl  *Z80Ctrl = NULL;
static uint8_t    *Z80RAM = NULL;
static uint8_t    *Z80ROM = NULL;

// Method to obtain and return the output screen width.
//
uint8_t getScreenWidth(void)
{
   return(MAX_SCREEN_WIDTH);
}

struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

int getch(uint8_t wait)
{
    int r;
    unsigned char c;

    if(wait != 0 || (wait == 0 && kbhit()))
    {
        if ((r = read(0, &c, sizeof(c))) < 0) {
            return r;
        } else {
            return c;
        }
    }
    return 0;
}

void delay(int number_of_seconds)
{
    // Converting time into milli_seconds
    int milli_seconds = 1000 * number_of_seconds;
 
    // Storing start time
    clock_t start_time = clock();
 
    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds);
}

// Function to dump out a given section of memory via the UART.
//
int memoryDump(uint32_t memaddr, uint32_t memsize, uint8_t memoryType, uint32_t memwidth, uint32_t dispaddr, uint8_t dispwidth)
{
    uint8_t  displayWidth = dispwidth;;
    uint32_t pnt          = memaddr;
    uint32_t endAddr      = memaddr + memsize;
    uint32_t addr         = dispaddr;
    uint32_t i            = 0;
    //uint32_t data;
    int8_t   keyIn;
    int      result       = -1;
    char     c            = 0;

    // Sanity check. memoryType == 0 required kernel driver to dump so we exit as it cannot be performed here.
    if(memoryType == 0)
        return(-1);

    // Reconfigure terminal to allow non-blocking key input.
    //
    set_conio_terminal_mode();

    // If not set, calculate output line width according to connected display width.
    //
    if(displayWidth == 0)
    {
        switch(getScreenWidth())
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
        printf("%08lX", addr); // print address
        printf(":  ");

        // print hexadecimal data
        for (i=0; i < displayWidth; )
        {
            switch(memwidth)
            {
                case 16:
                    if(pnt+i < endAddr)
                        printf("%04X", memoryType == 1 ? (uint16_t)Z80RAM[pnt+i] : memoryType == 2 ? (uint16_t)Z80ROM[pnt+i] : memoryType == 3 ? (uint16_t)*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (pnt+i)) : (uint16_t)Z80Ctrl->iopage[pnt+i]);
                    else
                        printf("    ");
                    i++;
                    break;

                case 32:
                    if(pnt+i < endAddr)
                        printf("%08lX", memoryType == 1 ? (uint32_t)Z80RAM[pnt+i] : memoryType == 2 ? (uint32_t)Z80ROM[pnt+i] : memoryType == 3 ? (uint32_t)*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (pnt+i)) : (uint32_t)Z80Ctrl->iopage[pnt+i]);
                    else
                        printf("        ");
                    i++;
                    break;

                case 8:
                default:
                    if(pnt+i < endAddr)
                        printf("%02X", memoryType == 1 ? (uint8_t)Z80RAM[pnt+i] : memoryType == 2 ? (uint8_t)Z80ROM[pnt+i] : memoryType == 3 ? (uint8_t)*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (pnt+i)) : (uint8_t)Z80Ctrl->iopage[pnt+i]);
                    else
                        printf("  ");
                    i++;
                    break;
            }
            fputc((char)' ', stdout);
        }

        // print ascii data
        printf(" |");

        // print single ascii char
        for (i=0; i < displayWidth; i++)
        {
            c = memoryType == 1 ? (char)Z80RAM[pnt+i] : memoryType == 2 ? (char)Z80ROM[pnt+i] : memoryType == 3 ? (char)*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (pnt+i)) : (char)Z80Ctrl->iopage[pnt+i];
            if ((pnt+i < endAddr) && (c >= ' ') && (c <= '~'))
                fputc((char)c, stdout);
            else
                fputc((char)' ', stdout);
        }

        printf("|\r\n");
        fflush(stdout);

        // Move on one row.
        pnt  += displayWidth;
        addr += displayWidth;

        // User abort (ESC), pause (Space) or all done?
        //
        keyIn = getch(0);
        if(keyIn == ' ')
        {
            do {
                keyIn = getch(0);
            } while(keyIn != ' ' && keyIn != 0x1b);
        }
        // Escape key pressed, exit with 0 to indicate this to caller.
        if (keyIn == 0x1b)
        {
            sleep(1);
            result = 0;
            goto memoryDumpExit;
        }

        // End of buffer, exit the loop.
        if(pnt >= (memaddr + memsize))
        {
            break;
        }
    }

    // Normal exit, return -1 to show no key pressed.
memoryDumpExit:
    reset_terminal_mode();
    return(result);
}

// Method to load a program or data file into the Z80 memory. First load into Virtual memory and then trigger a sync to bring Host RAM in line.
//
int z80load(int fdZ80, char *fileName, uint32_t memLoadAddr, long fileOffset, long fileLen, uint8_t memoryType, uint8_t isMZF)
{
    // Locals.
    struct ioctlCmd ioctlCmd;
    int             result = 0;
    long            fileSize;
    t_svcDirEnt     mzfHeader;
    FILE            *ptr;

    // Sanity check.
    if(isMZF == 0 && (fileOffset == -1 && fileLen != -1 || fileOffset != -1 && fileLen == -1))
    {
        printf("Error: Both --offset and --len are required, dont specify to use entire file.\n");
        result = 1;
    }
    if(result == 0)
    {
        // Open the file and read directly into the Virtual memory via the share.
        ptr = fopen(fileName, "rb");
        if(ptr)
        {
            // Get size of file for sanity checks.
            fseek(ptr, 0, SEEK_END);
            fileSize = ftell(ptr);
            fseek(ptr, 0, SEEK_SET);

            // Sanity checks.
            if(isMZF == 0)
            {
                fileSize = (fileLen == -1 ? fileSize : fileLen);
                if((memoryType == 1 && (memLoadAddr+fileSize) > Z80_VIRTUAL_RAM_SIZE) || (memoryType == 2 && (memLoadAddr+fileSize) > Z80_VIRTUAL_ROM_SIZE))
                {
                    printf("Error: Binary file out of memory bounds (Size=%ld, Load=%08x)\n", fileSize, memLoadAddr);
                    result = 1;
                }
            } else
            {
                // First the header.
                fread((uint8_t *)&mzfHeader, MZF_HEADER_SIZE, 1, ptr);
                if((mzfHeader.loadAddr + mzfHeader.fileSize) > Z80_VIRTUAL_RAM_SIZE)
                {
                    printf("Error: MZF file out of RAM bounds (Size=%ld, Load=%08x)\n", fileSize, memLoadAddr);
                    result = 1;
                }
            }
        }
        else
        {
            printf("Couldnt open file:%s\n", fileName);
            result = 1;
        }
    }

    // No file errors, read contents into Z80 memory.
    if(result == 0)
    {
        // Pause the Z80.
        //
        ioctlCmd.cmd = IOCTL_CMD_Z80_PAUSE;
        ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

        if(isMZF == 0)
        {
            // Position file if offset provided.
            if(fileOffset != -1) fseek(ptr, fileOffset, SEEK_SET);

            // Now read in the data.
            switch(memoryType)
            {
                case 0:
                    break;

                case 2:
                    fread(&Z80ROM[memLoadAddr], fileSize, 1, ptr);
                    break;

                case 3:
                    fread(&Z80Ctrl->page[memLoadAddr], fileSize, 1, ptr);
                    break;

                case 4:
                    fread(&Z80Ctrl->iopage[memLoadAddr], fileSize, 1, ptr);
                    break;

                case 1:
                default:
                    fread(&Z80RAM[memLoadAddr], fileSize, 1, ptr);
                    break;
            }
            printf("Loaded %s, Size:%08x, Addr:%08x, memoryType:%d\n", fileName, fileSize, memLoadAddr, memoryType);
        }
        else
        {
          #if(TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ80A)
            if(mzfHeader.loadAddr > 0x1000)
            {
          #endif
                // Copy in the header.
                memcpy((uint8_t *)&Z80RAM[MZ_CMT_ADDR], (uint8_t *)&mzfHeader, MZF_HEADER_SIZE);

                // Now read in the data.
                fread(&Z80RAM[mzfHeader.loadAddr], mzfHeader.fileSize, 1, ptr);
                printf("Loaded %s, Size:%04x, Addr:%04x, Exec:%04x\n", fileName, mzfHeader.fileSize, mzfHeader.loadAddr, mzfHeader.execAddr);
          #if(TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ80A)
            }
          #endif
           
            // Sync the loaded image from Virtual memory to hard memory.
            ioctlCmd.cmd = IOCTL_CMD_SYNC_TO_HOST_RAM;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);


          #if(TARGET_HOST_MZ2000 == 1)
            // Set PC to 2 (NST) which switches to RUN mode and executes at 0000H
            ioctlCmd.z80.pc = 2;
          #endif
          #if(TARGET_HOST_MZ700 == 1 || TARGET_HOST_MZ80A)
            // MZ-700 or MZ-80A just use the MZF header exec address.
            ioctlCmd.z80.pc = mzfHeader.execAddr;
          #endif

            // Set PC to required setting ready for run.
            ioctlCmd.cmd = IOCTL_CMD_SETPC;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

            // Resume Z80 processing.
            //
            ioctlCmd.cmd = IOCTL_CMD_Z80_CONTINUE;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
        }
    } 

    return(result);  
}  

// Method to save FusionX memory to a local file.
//
int z80save(int fdZ80, char *fileName, long addr, long size, uint8_t memoryType)
{
    // Locals.
    struct ioctlCmd ioctlCmd;
    int             result = 0;
    t_svcDirEnt     mzfHeader;

    // Pause the Z80.
    //
    //ioctlCmd.cmd = IOCTL_CMD_Z80_PAUSE;
    //ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
  
    // Open the file and write directly into the file from the shared memory segment.
    FILE *ptr;
    ptr = fopen(fileName, "wb");
    if(ptr)
    {
        switch(memoryType)
        {
            case 0:
                break;

            case 2:
                fwrite(&Z80ROM[addr], size, 1, ptr);
                break;

            case 3:
                fwrite(&Z80Ctrl->page[addr], size, 1, ptr);
                break;

            case 4:
                fwrite(&Z80Ctrl->iopage[addr], size, 1, ptr);
                break;

            case 1:
            default:
                fwrite(&Z80RAM[addr], size, 1, ptr);
                break;
        }
    }
    fclose(ptr);

    return(result);
}

// Method to request basic Z80 operations.
//
int ctrlCmd(int fdZ80, enum CTRL_COMMANDS cmd, long param1, long param2, long param3)
{
    // Locals.
    struct ioctlCmd ioctlCmd; 
    uint32_t        idx;
    int             result = 0;

    switch(cmd)
    {
        case Z80_CMD_STOP:
            // Use IOCTL to request Z80 to Stop (power off) processing.
            ioctlCmd.cmd = IOCTL_CMD_Z80_STOP;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case Z80_CMD_START:
            // Use IOCTL to request Z80 to Start (power on) processing.
            ioctlCmd.cmd = IOCTL_CMD_Z80_START;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case Z80_CMD_PAUSE:
            // Use IOCTL to request Z80 to pause processing.
            ioctlCmd.cmd = IOCTL_CMD_Z80_PAUSE;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case Z80_CMD_CONTINUE:
            // Use IOCTL to request Z80 continue processing.
            ioctlCmd.cmd = IOCTL_CMD_Z80_CONTINUE;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case Z80_CMD_RESET:
            // Use IOCTL to request Z80 reset.
            ioctlCmd.cmd = IOCTL_CMD_Z80_RESET;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case Z80_CMD_SPEED:
            // Check value is in range.
            for(idx=1; idx < 256; idx+=idx)
            {
                if((uint32_t)param1 == idx) break;
            }
            if(idx == 256)
            {
               printf("Speed factor is illegal. It must be a multiple value of the original CPU clock, ie. 1x, 2x, 4x etc\n");
               result = -1;
            } else 
            {
                // Use IOCTL to request Z80 cpu freq change.
                ioctlCmd.speed.speedMultiplier = (uint32_t)param1;
                ioctlCmd.cmd = IOCTL_CMD_Z80_CPU_FREQ;
                ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            }
            break;
        case Z80_CMD_ADD_DEVICE:
            ioctlCmd.vdev.device = VIRTUAL_DEVICE_NONE;
            if(strcasecmp((char *)param1, "RFS") == 0)
            {
                ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS;
            }
            else if(strcasecmp((char *)param1, "TZPU") == 0)
            {
                ioctlCmd.vdev.device = VIRTUAL_DEVICE_TZPU;
            }
            if(ioctlCmd.vdev.device != VIRTUAL_DEVICE_NONE)
            {
                ioctlCmd.cmd = IOCTL_CMD_ADD_DEVICE;
                ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            }
            break;
        case Z80_CMD_DEL_DEVICE:
            ioctlCmd.vdev.device = VIRTUAL_DEVICE_NONE;
            if(strcasecmp((char *)param1, "RFS") == 0)
            {
                ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS;
            }
            else if(strcasecmp((char *)param1, "TZPU") == 0)
            {
                ioctlCmd.vdev.device = VIRTUAL_DEVICE_TZPU;
            }
            if(ioctlCmd.vdev.device != VIRTUAL_DEVICE_NONE)
            {
                ioctlCmd.cmd = IOCTL_CMD_DEL_DEVICE;
                ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            }
            break;
        case CPLD_CMD_SEND_CMD:
            // Build up the IOCTL command to request the given data is sent to the CPLD.
            ioctlCmd.cmd = IOCTL_CMD_CPLD_CMD;
            ioctlCmd.cpld.cmd = (uint32_t)param1;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case Z80_CMD_DUMP_MEMORY:
            // If virtual memory, we can dump it via the shared memory segment.
            if((uint8_t)param1)
            {
                memoryDump((uint32_t)param2, (uint32_t)param3, (uint8_t)param1, (uint8_t)param1 == 3 || (uint8_t)param1 == 4 ? 32 : 8, (uint32_t)param2, 0);
            } else
            {
                // Build an IOCTL command to get the driver to dump the memory.
                ioctlCmd.cmd = IOCTL_CMD_DUMP_MEMORY;
                ioctlCmd.addr.start = (uint32_t)param2;
                ioctlCmd.addr.end = (uint32_t)param2+(uint32_t)param3;
                ioctlCmd.addr.size = (uint32_t)param3;
                ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            }
            break;
        case Z80_CMD_HOST_RAM:
            // Use IOCTL to request change to host RAM.
            ioctlCmd.cmd = IOCTL_CMD_USE_HOST_RAM;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case Z80_CMD_VIRTUAL_RAM:
            // Use IOCTL to request change to host RAM.
            ioctlCmd.cmd = IOCTL_CMD_USE_VIRTUAL_RAM;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
      #if(DEBUG_ENABLED != 0)
        case Z80_CMD_DEBUG:
            // Send command to enable/disable debugging output.
            ioctlCmd.cmd = IOCTL_CMD_DEBUG;
            ioctlCmd.debug.level = (uint8_t)param1;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
      #endif
        case Z80_CMD_MEMORY_TEST:
            // Send command to test the SPI.
            ioctlCmd.cmd = IOCTL_CMD_Z80_MEMTEST;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case CPLD_CMD_PRL_TEST:
            // Send command to test the SPI.
            ioctlCmd.cmd = IOCTL_CMD_PRL_TEST;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;
        case CPLD_CMD_SPI_TEST:
            // Send command to test the SPI.
            ioctlCmd.cmd = IOCTL_CMD_SPI_TEST;
            ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
            break;

        default:
            printf("Command not supported!\n");
            result = -1;
            break;
    }

    return(result);  
}  

// Method to perform some simple tests on the Z80 emulator.
//
int z80test(int fdZ80)
{
    // Locals.
    struct ioctlCmd ioctlCmd; 
    int             result = 0;

    // Stop the Z80.
    //
printf("Send STOP\n");
    ioctlCmd.cmd = IOCTL_CMD_Z80_STOP;
    ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

    FILE *ptr;
    ptr = fopen("/customer/mz700.rom", "rb");
    if(ptr)
    {
        fread(&Z80RAM, 65536, 1, ptr);
    } else printf("Couldnt open file\n");

    // Configure the Z80.
    //
printf("Send SETPC\n");
    ioctlCmd.z80.pc = 0;
    ioctl(fdZ80, IOCTL_CMD_SETPC, &ioctlCmd);

    memoryDump(0 , 65536, 1, 8, 0, 0);

    // Start the Z80.
    //
printf("Send START\n");
    ioctlCmd.cmd = IOCTL_CMD_Z80_START;
    ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

    delay(10);

printf("Send STOP\n");
    ioctlCmd.cmd = IOCTL_CMD_Z80_STOP;
    ioctl(fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

     memoryDump(0, 65536, 1, 8, 0, 0);
out:
    return(result);  
}  

// Output usage screen. So mamy commands you do need to be prompted!!
void showArgs(char *progName, struct optparse *options)
{
    printf("%s %s %s %s\n\n", progName, VERSION, COPYRIGHT, AUTHOR);
    printf("Synopsis:\n");
    printf("%s --help                                                                   # This help screen.\n", progName);
    printf("          --cmd <command> = RESET                                                  # Reset the Z80\n");
    printf("                          = STOP                                                   # Stop and power off the Z80\n");
    printf("                          = START                                                  # Power on and start the Z80\n");
    printf("                          = PAUSE                                                  # Pause running Z80\n");
    printf("                          = CONTINUE                                               # Continue Z80 execution\n");
    printf("                          = HOSTRAM                                                # Use HOST DRAM\n");
    printf("                          = VIRTRAM                                                # Use Virtual RAM\n");
    printf("                          = ADDDEV  --device <RFS, TZPU>                           # Add a virtual device into the Z80 configuration.\n");
    printf("                          = DELDEV  --device <RFS, TZPU>                           # Remove a virtual device from the Z80 configuration.\n");
    printf("                          = SPEED   --mult <1, 2, 4, 8, 16, 32, 64, 128>           # In Virtual RAM mode, set CPU speed to base clock x factor.\n");
    printf("                          = LOADMZF --file <mzf filename>                          # Load MZF file into RAM.\n");
    printf("                                                                                   # Load contents of binary file into memory at address. default = 0x000000.\n");
    printf("                          = LOADMEM --file <binary filename> --addr <24 bit addr> --type <0 - Host RAM, 1 = Virtual RAM, 2 = Virtual ROM> [--offset <offset> --len <length>]\n");
    printf("                          = SAVE    --file <filename>  --addr <24bit addr> --end <24bit addr> [--size <24bit>] --type <0 - Host RAM, 1 = Virtual RAM, 2 = Virtual ROM, 3 = PageTable, 4 = IOPageTable>\n");
    printf("                          = DUMP    --addr <24bit addr> --end <24bit addr> [--size <24bit>] --type <0 - Host RAM, 1 = Virtual RAM, 2 = Virtual ROM, 3 = PageTable, 4 = IOPageTable>\n");
    printf("                          = CPLDCMD --data <32bit command>                         # Send adhoc 32bit command to CPLD.\n");
  #if(DEBUG_ENABLED != 0)
    printf("                          = DEBUG --level <level>                                  # 0 = off, 1 = driver, 2 = k64f, 3 = both.\n");
  #endif
    printf("                          = Z80TEST                                                # Perform various debugging tests\n");
    printf("                          = SPITEST                                                # Perform SPI testing\n");
    printf("                          = PRLTEST                                                # Perform Parallel Bus testing\n");
    printf("                          = Z80MEMTEST                                             # Perform HOST memory tests.\n");
    printf("          --<cmd>                                                                  # Some commands can be abbreviated, ie. --start.\n");

}

int main(int argc, char *argv[])  
{  
    int        fdZ80;  
    char       buff[64];    
    char       cmd[64]           = { 0 };
    char       fileName[256]     = { 0 };
    char       devName[32]       = { 0 };
    int        opt;
    uint32_t   hexData           = 0;
    long       speedMultiplier   = 1;
    long       startAddr         = 0x0000;
    long       endAddr           = 0x1000;
    long       fileOffset        = -1;
    long       fileLen           = -1;
  #if(DEBUG_ENABLED != 0)
    int        debugLevel        = 0;
  #endif
    int        helpFlag          = 0;
    int        verboseFlag       = 0;
    uint8_t    memoryType        = 0;

    // Define parameters to be processed.
    struct optparse options;
    static struct optparse_long long_options[] =
    {
        {"help",          'h',  OPTPARSE_NONE},
        {"cmd",           'c',  OPTPARSE_REQUIRED},
        {"file",          'f',  OPTPARSE_REQUIRED},
        {"data",          'd',  OPTPARSE_REQUIRED},
        {"mult",          'S',  OPTPARSE_REQUIRED},
        {"type",          'T',  OPTPARSE_REQUIRED},
        {"addr",          'a',  OPTPARSE_REQUIRED},
        {"end",           'e',  OPTPARSE_REQUIRED},
        {"size",          's',  OPTPARSE_REQUIRED},
        {"device",        'D',  OPTPARSE_REQUIRED},
        {"offset",        'O',  OPTPARSE_REQUIRED},
        {"len",           'L',  OPTPARSE_REQUIRED},
      #if(DEBUG_ENABLED != 0)
        {"level",         'l',  OPTPARSE_REQUIRED},
      #endif
        {"verbose",       'v',  OPTPARSE_NONE},
        {"save",          '0',  OPTPARSE_NONE},
        {"dump",          '1',  OPTPARSE_NONE},
        {"loadmzf",       '2',  OPTPARSE_NONE},
        {"loadrom",       '3',  OPTPARSE_NONE},
        {"reset",         '4',  OPTPARSE_NONE},
        {"stop",          '5',  OPTPARSE_NONE},
        {"start",         '6',  OPTPARSE_NONE},
        {"pause",         '7',  OPTPARSE_NONE},
        {"continue",      '8',  OPTPARSE_NONE},
        {"speed",         '9',  OPTPARSE_NONE},
        {"cpldcmd",       '+',  OPTPARSE_NONE},
        {"debug",         'x',  OPTPARSE_NONE},
        {"adddev",        '-',  OPTPARSE_NONE},
        {"deldev",        ':',  OPTPARSE_NONE},
        {0}
    };

    // Parse the command line options.
    //
    optparse_init(&options, argv);
    while((opt = optparse_long(&options, long_options, NULL)) != -1)  
    {  
        switch(opt)  
        {  
            // Hex data.
            case 'd':
               // hexData = (uint32_t)strtol(options.optarg, NULL, 0);
                sscanf(options.optarg, "0x%08x", &hexData);
                printf("Hex data:%08x\n", hexData);
                break;

            // Start address for memory operations.
            case 'a':
                startAddr = strtol(options.optarg, NULL, 0);
                //printf("Start Addr:%04x\n", startAddr);
                break;

            // Speed multiplication factor for CPU governor when running in virtual memory.
            case 'S':
                speedMultiplier = strtol(options.optarg, NULL, 0);
                //printf("Speed = base freq x %d\n", speedFactor);
                break;

            // End address for memory operations.
            case 'e':
                endAddr = strtol(options.optarg, NULL, 0);
                //printf("End Addr:%04x\n", endAddr);
                break;
               
            // Size instead of end address for memory operations.
            case 's':
                endAddr = startAddr + strtol(options.optarg, NULL, 0);
                //printf("End Addr:%04x\n", endAddr);
                break;
              
            // File offset.
            case 'O':
                fileOffset = strtol(options.optarg, NULL, 0);
                //printf("File Offset:%04x\n", fileOffset);
                break;
              
            // Length.
            case 'L':
                fileLen = strtol(options.optarg, NULL, 0);
                //printf("File Len:%04x\n", fileLen);
                break;

            // Memory type flag, 0 = host, 1 = virtual RAM, 2 = virtual ROM, 3 = page table, 4 = iopage table.
            case 'T':
                memoryType = atoi(options.optarg);
                break;
               
          #if(DEBUG_ENABLED != 0)
            // Debug level, 0 = off, 1 = driver, 2 = k64f, 3 = both.
            case 'E':
                debugLevel = atoi(options.optarg);
                break;
          #endif

            // Filename.
            case 'f':
                strcpy(fileName, options.optarg);
                break;

            // Device name.
            case 'D':
                strcpy(devName, options.optarg);
                break;

            // Command to execute.
            case 'c':
                strcpy(cmd, options.optarg);
                break;

            // Quick command flags.
            case '0':
                strcpy(cmd, "SAVE");
                break;
            case '1':
                strcpy(cmd, "DUMP");
                break;
            case '2':
                strcpy(cmd, "LOADMZF");
                break;
            case '3':
                strcpy(cmd, "LOADMEM");
                break;
            case '4':
                strcpy(cmd, "RESET");
                break;
            case '5':
                strcpy(cmd, "STOP");
                break;
            case '6':
                strcpy(cmd, "START");
                break;
            case '7':
                strcpy(cmd, "PAUSE");
                break;
            case '8':
                strcpy(cmd, "CONTINUE");
                break;
            case '9':
                strcpy(cmd, "SPEED");
                break;
            case '+':
                strcpy(cmd, "CPLDCMD");
                break;
            case '-':
                strcpy(cmd, "ADDDEV");
                break;
            case ':':
                strcpy(cmd, "DELDEV");
                break;
          #if(DEBUG_ENABLED != 0)
            case 'x':
                strcpy(cmd, "DEBUG");
                break;
          #endif

            // Verbose mode.
            case 'v':
                verboseFlag = 1;
                break;

            // Command help needed.
            case 'h':
                helpFlag = 1;
                break;

            // Unrecognised, show synopsis.
            case '?':
                showArgs(argv[0], &options);
                printf("%s: %s\n", argv[0], options.errmsg);
                return(1);
        }  
    }

    // Open the z80drv driver and attach to its shared memory, basically the Z80 control structure which includes the virtual Z80 memory.
    fdZ80 = open(DEVICE_FILENAME, O_RDWR|O_NDELAY);  
    if(fdZ80 >= 0)
    { 
        Z80Ctrl = (t_Z80Ctrl *)mmap(0,  sizeof(t_Z80Ctrl),  PROT_READ | PROT_WRITE,  MAP_SHARED,  fdZ80,  0);  

        if(Z80Ctrl == (void *)-1)
        {
            printf("Failed to attach to the Z80 Control structure, cannot continue, exiting....\n");
            close(fdZ80);  
            exit(1);
        }
        Z80RAM = (uint8_t *)mmap(0, Z80_VIRTUAL_RAM_SIZE, PROT_READ | PROT_WRITE,  MAP_SHARED,  fdZ80,  0);  
        if(Z80RAM == (void *)-1)
        {
            printf("Failed to attach to the Z80 RAM, cannot continue, exiting....\n");
            close(fdZ80);  
            exit(1);
        }
        Z80ROM = (uint8_t *)mmap(0, Z80_VIRTUAL_ROM_SIZE+0x1000, PROT_READ | PROT_WRITE,  MAP_SHARED,  fdZ80,  0);  
        if(Z80ROM == (void *)-1)
        {
            printf("Failed to attach to the Z80 ROM, cannot continue, exitting....\n");
            close(fdZ80);  
            exit(1);
        }
    } else
    {
        printf("Failed to open the Z80 Driver, exiting...\n");
        exit(1);
    }

    // Basic string to method mapping. Started off with just 1 or two but has grown, may need a table!
    if(strcasecmp(cmd, "LOADMZF") == 0)
    {
        z80load(fdZ80, fileName, 0, -1, -1, 1, 1);
    } else
    if(strcasecmp(cmd, "LOADMEM") == 0)
    {
        z80load(fdZ80, fileName, startAddr, fileOffset, fileLen, memoryType, 0);
    } else
    if(strcasecmp(cmd, "RESET") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_RESET, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "STOP") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_STOP, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "START") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_START, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "PAUSE") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_PAUSE, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "CONTINUE") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_CONTINUE, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "SPEED") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_SPEED, speedMultiplier, 0, 0);
    } else
    if(strcasecmp(cmd, "DUMP") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_DUMP_MEMORY, memoryType, startAddr, (endAddr - startAddr));
    } else
    if(strcasecmp(cmd, "SAVE") == 0)
    {
        z80save(fdZ80, fileName, startAddr, (endAddr - startAddr), memoryType);
    } else
    if(strcasecmp(cmd, "HOSTRAM") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_HOST_RAM, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "VIRTRAM") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_VIRTUAL_RAM, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "ADDDEV") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_ADD_DEVICE, (long)&devName, 0, 0);
    } else
    if(strcasecmp(cmd, "DELDEV") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_DEL_DEVICE, (long)&devName, 0, 0);
    } else
    if(strcasecmp(cmd, "CPLDCMD") == 0)
    {
        ctrlCmd(fdZ80, CPLD_CMD_SEND_CMD, hexData, 0, 0);
    } else
  #if(DEBUG_ENABLED != 0)
    if(strcasecmp(cmd, "DEBUG") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_DEBUG, debugLevel, 0, 0);
    } else
  #endif

    // Test methods, if the code is built-in to the driver.
    if(strcasecmp(cmd, "Z80TEST") == 0)
    {
        z80test(fdZ80);
    } else
    if(strcasecmp(cmd, "SPITEST") == 0)
    {
        ctrlCmd(fdZ80, CPLD_CMD_SPI_TEST, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "PRLTEST") == 0)
    {
        ctrlCmd(fdZ80, CPLD_CMD_PRL_TEST, 0, 0, 0);
    } else
    if(strcasecmp(cmd, "Z80MEMTEST") == 0)
    {
        ctrlCmd(fdZ80, Z80_CMD_MEMORY_TEST, 0, 0, 0);
    }
    else
    {
        showArgs(argv[0], &options);
        if(helpFlag == 0)
            printf("No command given, nothing done!\n");
    }
 
    // Unmap shared memory and close the device.
    munmap(Z80Ctrl, sizeof(t_Z80Ctrl));  
    close(fdZ80);
   
    return(0);
}
