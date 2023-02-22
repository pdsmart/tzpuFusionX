/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            k64fcpu.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 Control Interface 
//                  This file contains a user space daemon which emulates the services provided by
//                  the K64FX512 Cortex-M4 CPU from NXP used as the I/O processor on all tranZPUter SW
//                  boards.
//                  The daemon connects to the Z80 driver memory and awaits requests via a signal
//                  and the host service request API, performs the required task and returns the result.
//                  The primary code base is take from tranzputer.c/.h which is part of the zSoft OS 
//                  package. It is customised for this application.
//
// Credits:         Zilog Z80 CPU Emulator v0.2 written by Manuel Sainz de Baranda y Goñi
//                  The Z80 CPU Emulator is the heart of the Z80 device driver.
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//                  (c) 1999-2023 Manuel Sainz de Baranda y Goñi
//
// History:         Feb 2023 v1.0  - Source copied from zSoft and modified to run as a daemon, stripping
//                                   out all low level control methods.
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
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>  
#include <stdint.h>
#include <unistd.h>  
#include <fcntl.h>  
#include <sys/ioctl.h>  
#include <signal.h>  
#include <unistd.h>  
#include <dirent.h>  
#include <ctype.h>  
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
#include "tzpu.h"

#define VERSION                      "1.0"
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

// Global scope variables used within the zOS kernel.
//
static t_z80Control          z80Control;
static t_osControl           osControl;
static t_svcControl          svcControl;
// Shared memory between this process and the Z80 driver.
static t_Z80Ctrl             *Z80Ctrl = NULL;
static uint8_t               *Z80RAM = NULL;
static uint8_t               *Z80ROM = NULL;
static uint8_t               runControl = 0;

// Mapping table to map Sharp MZ80A Ascii to Standard ASCII.
//
static t_asciiMap asciiMap[] = {
    { 0x00 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x00 }, { 0x20 }, { 0x20 }, // 0x0F
    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0x1F
    { 0x20 }, { 0x21 }, { 0x22 }, { 0x23 }, { 0x24 }, { 0x25 }, { 0x26 }, { 0x27 }, { 0x28 }, { 0x29 }, { 0x2A }, { 0x2B }, { 0x2C }, { 0x2D }, { 0x2E }, { 0x2F }, // 0x2F
    { 0x30 }, { 0x31 }, { 0x32 }, { 0x33 }, { 0x34 }, { 0x35 }, { 0x36 }, { 0x37 }, { 0x38 }, { 0x39 }, { 0x3A }, { 0x3B }, { 0x3C }, { 0x3D }, { 0x3E }, { 0x3F }, // 0x3F
    { 0x40 }, { 0x41 }, { 0x42 }, { 0x43 }, { 0x44 }, { 0x45 }, { 0x46 }, { 0x47 }, { 0x48 }, { 0x49 }, { 0x4A }, { 0x4B }, { 0x4C }, { 0x4D }, { 0x4E }, { 0x4F }, // 0x4F
    { 0x50 }, { 0x51 }, { 0x52 }, { 0x53 }, { 0x54 }, { 0x55 }, { 0x56 }, { 0x57 }, { 0x58 }, { 0x59 }, { 0x5A }, { 0x5B }, { 0x5C }, { 0x5D }, { 0x5E }, { 0x5F }, // 0x5F
    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0x6F
    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0x7F
    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0x8F
    { 0x20 }, { 0x20 }, { 0x65 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x74 }, { 0x67 }, { 0x68 }, { 0x20 }, { 0x62 }, { 0x78 }, { 0x64 }, { 0x72 }, { 0x70 }, { 0x63 }, // 0x9F
    { 0x71 }, { 0x61 }, { 0x7A }, { 0x77 }, { 0x73 }, { 0x75 }, { 0x69 }, { 0x20 }, { 0x4F }, { 0x6B }, { 0x66 }, { 0x76 }, { 0x20 }, { 0x75 }, { 0x42 }, { 0x6A }, // 0XAF
    { 0x6E }, { 0x20 }, { 0x55 }, { 0x6D }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x6F }, { 0x6C }, { 0x41 }, { 0x6F }, { 0x61 }, { 0x20 }, { 0x79 }, { 0x20 }, { 0x20 }, // 0xBF
    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0XCF
    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0XDF
    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0XEF
    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }  // 0XFF
};
static t_dispCodeMap dispCodeMap[] = {
    { 0xCC }, //  NUL '\0' (null character)     
    { 0xE0 }, //  SOH (start of heading)     
    { 0xF2 }, //  STX (start of text)        
    { 0xF3 }, //  ETX (end of text)          
    { 0xCE }, //  EOT (end of transmission)  
    { 0xCF }, //  ENQ (enquiry)              
    { 0xF6 }, //  ACK (acknowledge)          
    { 0xF7 }, //  BEL '\a' (bell)            
    { 0xF8 }, //  BS  '\b' (backspace)       
    { 0xF9 }, //  HT  '\t' (horizontal tab)  
    { 0xFA }, //  LF  '\n' (new line)        
    { 0xFB }, //  VT  '\v' (vertical tab)    
    { 0xFC }, //  FF  '\f' (form feed)       
    { 0xFD }, //  CR  '\r' (carriage ret)    
    { 0xFE }, //  SO  (shift out)            
    { 0xFF }, //  SI  (shift in)                
    { 0xE1 }, //  DLE (data link escape)        
    { 0xC1 }, //  DC1 (device control 1)     
    { 0xC2 }, //  DC2 (device control 2)     
    { 0xC3 }, //  DC3 (device control 3)     
    { 0xC4 }, //  DC4 (device control 4)     
    { 0xC5 }, //  NAK (negative ack.)        
    { 0xC6 }, //  SYN (synchronous idle)     
    { 0xE2 }, //  ETB (end of trans. blk)    
    { 0xE3 }, //  CAN (cancel)               
    { 0xE4 }, //  EM  (end of medium)        
    { 0xE5 }, //  SUB (substitute)           
    { 0xE6 }, //  ESC (escape)               
    { 0xEB }, //  FS  (file separator)       
    { 0xEE }, //  GS  (group separator)      
    { 0xEF }, //  RS  (record separator)     
    { 0xF4 }, //  US  (unit separator)       
    { 0x00 }, //  SPACE                         
    { 0x61 }, //  !                             
    { 0x62 }, //  "                          
    { 0x63 }, //  #                          
    { 0x64 }, //  $                          
    { 0x65 }, //  %                          
    { 0x66 }, //  &                          
    { 0x67 }, //  '                          
    { 0x68 }, //  (                          
    { 0x69 }, //  )                          
    { 0x6B }, //  *                          
    { 0x6A }, //  +                          
    { 0x2F }, //  ,                          
    { 0x2A }, //  -                          
    { 0x2E }, //  .                          
    { 0x2D }, //  /                          
    { 0x20 }, //  0                          
    { 0x21 }, //  1                          
    { 0x22 }, //  2                          
    { 0x23 }, //  3                          
    { 0x24 }, //  4                          
    { 0x25 }, //  5                          
    { 0x26 }, //  6                          
    { 0x27 }, //  7                          
    { 0x28 }, //  8                          
    { 0x29 }, //  9                          
    { 0x4F }, //  :                          
    { 0x2C }, //  ;                          
    { 0x51 }, //  <                          
    { 0x2B }, //  =                          
    { 0x57 }, //  >                          
    { 0x49 }, //  ?                          
    { 0x55 }, //  @
    { 0x01 }, //  A
    { 0x02 }, //  B
    { 0x03 }, //  C
    { 0x04 }, //  D
    { 0x05 }, //  E
    { 0x06 }, //  F
    { 0x07 }, //  G
    { 0x08 }, //  H
    { 0x09 }, //  I
    { 0x0A }, //  J
    { 0x0B }, //  K
    { 0x0C }, //  L
    { 0x0D }, //  M
    { 0x0E }, //  N
    { 0x0F }, //  O
    { 0x10 }, //  P
    { 0x11 }, //  Q
    { 0x12 }, //  R
    { 0x13 }, //  S
    { 0x14 }, //  T
    { 0x15 }, //  U
    { 0x16 }, //  V
    { 0x17 }, //  W
    { 0x18 }, //  X
    { 0x19 }, //  Y
    { 0x1A }, //  Z
    { 0x52 }, //  [
    { 0x59 }, //  \  '\\'
    { 0x54 }, //  ]
    { 0xBE }, //  ^
    { 0x3C }, //  _
    { 0xC7 }, //  `
    { 0x81 }, //  a
    { 0x82 }, //  b
    { 0x83 }, //  c
    { 0x84 }, //  d
    { 0x85 }, //  e
    { 0x86 }, //  f
    { 0x87 }, //  g
    { 0x88 }, //  h
    { 0x89 }, //  i
    { 0x8A }, //  j
    { 0x8B }, //  k
    { 0x8C }, //  l
    { 0x8D }, //  m
    { 0x8E }, //  n
    { 0x8F }, //  o
    { 0x90 }, //  p
    { 0x91 }, //  q
    { 0x92 }, //  r
    { 0x93 }, //  s
    { 0x94 }, //  t
    { 0x95 }, //  u
    { 0x96 }, //  v
    { 0x97 }, //  w
    { 0x98 }, //  x
    { 0x99 }, //  y
    { 0x9A }, //  z
    { 0xBC }, //  {
    { 0x80 }, //  |
    { 0x40 }, //  }
    { 0xA5 }, //  ~
    { 0xC0 }  //  DEL
};


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

    // Loop, displaying memory contents until we get to the last byte then break out.
    //
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
                        printf("%04X", memoryType == 1 ? (uint16_t)Z80RAM[pnt+i] : memoryType == 2 ? (uint16_t)Z80ROM[pnt+i] : memoryType == 3 ? (uint16_t)*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (pnt+i)) : memoryType == 4 ? (uint16_t)Z80Ctrl->iopage[pnt+i] : *(uint16_t *)(pnt+i));
                    else
                        printf("    ");
                    i++;
                    break;

                case 32:
                    if(pnt+i < endAddr)
                        printf("%08lX", memoryType == 1 ? (uint32_t)Z80RAM[pnt+i] : memoryType == 2 ? (uint32_t)Z80ROM[pnt+i] : memoryType == 3 ? (uint32_t)*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (pnt+i)) : memoryType == 4 ? (uint32_t)Z80Ctrl->iopage[pnt+i] : *(uint32_t *)(pnt+i));
                    else
                        printf("        ");
                    i++;
                    break;

                case 8:
                default:
                    if(pnt+i < endAddr)
                        printf("%02X", memoryType == 1 ? (uint8_t)Z80RAM[pnt+i] : memoryType == 2 ? (uint8_t)Z80ROM[pnt+i] : memoryType == 3 ? (uint8_t)*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (pnt+i)) : memoryType == 4 ? (uint8_t)Z80Ctrl->iopage[pnt+i] : *(uint8_t *)(pnt+i));
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
            c = memoryType == 1 ? (char)Z80RAM[pnt+i] : memoryType == 2 ? (char)Z80ROM[pnt+i] : memoryType == 3 ? (char)*(*(Z80Ctrl->page + Z80Ctrl->memoryMode) + (pnt+i)) : memoryType == 4 ? (char)Z80Ctrl->iopage[pnt+i] : (char)*(uint8_t *)(pnt+i);
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

// Method to reset the Z80 CPU.
//
void reqResetZ80(uint8_t memoryMode)
{
    // Locals.
    //
    struct ioctlCmd ioctlCmd;

    // Send command to driver to reset Z80.
    ioctlCmd.cmd = IOCTL_CMD_Z80_RESET;
    ioctl(z80Control.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
}

// Method to start the Z80 CPU.
//
void startZ80(uint8_t memoryMode)
{
    // Locals.
    //
    struct ioctlCmd ioctlCmd;

    // Send command to driver to reset Z80.
    ioctlCmd.cmd = IOCTL_CMD_Z80_START;
    ioctl(z80Control.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
}

// Method to stop the Z80 CPU.
//
void stopZ80(uint8_t memoryMode)
{
    // Locals.
    //
    struct ioctlCmd ioctlCmd;

    // Send command to driver to reset Z80.
    ioctlCmd.cmd = IOCTL_CMD_Z80_STOP;
    ioctl(z80Control.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
}

// Method to read a memory mapped byte from the Z80 bus.
//
uint8_t readZ80Memory(uint32_t addr)
{
    // Locals.
    uint8_t  data;

    // Fetch the data.
    //
    data = Z80RAM[addr];

    // Finally pass back the byte read to the caller.
    return(data);
}

// Method to write a memory mapped byte onto the Z80 bus.
//
uint8_t writeZ80Memory(uint32_t addr, uint8_t data, enum TARGETS target)
{
    // Locals.

    Z80RAM[addr] = data;
    return(0);
}

// Method to set the service status flag on the Z80 (and duplicated in the internal
// copy).
//
uint8_t setZ80SvcStatus(uint8_t status)
{
    // Locals
    uint8_t result = 1;

    // Update the memory location.
    //
    result=writeZ80Memory(z80Control.svcControlAddr+TZSVC_RESULT_OFFSET, status, TRANZPUTER);

    return(result);
}

// Method to fill memory under the Z80 control, either the mainboard or tranZPUter memory.
//
void fillZ80Memory(uint32_t addr, uint32_t size, uint8_t data, enum TARGETS target)
{
    // Locals.

    // Sanity checks.
    //
    if((target == MAINBOARD && (addr+size) > 0x10000) || (target == TRANZPUTER && (addr+size) > TZ_MAX_Z80_MEM))
        return;

    // Fill the memory by simply writing byte at a time.
    //
    for(uint32_t idx=addr; idx < (addr+size); idx++)
    {
        writeZ80Memory((uint32_t)idx, data, target);
    }

    return;
}

// Method to load a file from the SD card directly into the tranZPUter static RAM or mainboard RAM.
//
FRESULT loadZ80Memory(const char *src, uint32_t fileOffset, uint32_t addr, uint32_t size, uint32_t *bytesRead, enum TARGETS target)
{
    // Locals.
    //
    FILE          *File;
    uint32_t      loadSize       = 0L;
    uint32_t      sizeToRead     = 0L;
    uint32_t      memPtr         = addr;
    unsigned int  readSize;
    unsigned char buf[TZSVC_SECTOR_SIZE];
    FRESULT       fr0;

    // Sanity check on filenames.
    if(src == NULL)
        return(FR_INVALID_PARAMETER);
    
    // Try and open the source file.
    fr0 = (File = fopen(src, "r")) != NULL ? FR_OK : FR_NO_FILE;

    // If no size given get the file size.
    //
    if(size == 0)
    {
        if(!fr0)
            fr0 = (fseek(File, 0, SEEK_END) != -1) ? FR_OK : FR_DISK_ERR;
        if(!fr0)
        {
            size = ftell(File);
            fr0 = (fseek(File, 0, SEEK_SET) != -1) ? FR_OK : FR_DISK_ERR;
        }
    }

    // Seek to the correct location.
    //
    if(!fr0)
        fr0 = (fseek(File, fileOffset, SEEK_SET) != -1) ? FR_OK : FR_DISK_ERR;

  #if(DEBUG_ENABLED & 0x02)
    if(Z80Ctrl->debug & 0x02) printf("Loading file(%s,%08lx,%08lx)\n", src, addr, size);   
  #endif

    // If no errors in opening the file, proceed with reading and loading into memory.
    if(!fr0)
    {
        // Loop, reading a sector at a time from SD file and writing it directly into the Z80 tranZPUter RAM or mainboard RAM.
        //
        loadSize = 0;
        memPtr = addr;
        do {
            sizeToRead = (size-loadSize) > TZSVC_SECTOR_SIZE ? TZSVC_SECTOR_SIZE : size - loadSize;
            readSize = fread(buf, 1, sizeToRead, File);
            if (fr0 || readSize == 0) break;   /* error or eof */

            // Go through each byte in sector and write to the Z80 memory.
            for(unsigned int idx=0; idx < readSize; idx++)
            {
                // And now write the byte and to the next address!
                writeZ80Memory((uint32_t)memPtr, buf[idx], target);
                memPtr++;
            }
            loadSize += readSize;
        } while(loadSize < size);
      
        // Close to sync files.
        fclose(File);
    } else
    {
        printf("File not found:%s\n", src);
    }

    // Return number of bytes read if caller provided a variable.
    //
    if(bytesRead != NULL)
    {
        *bytesRead = loadSize;
    }

    return(fr0 ? fr0 : FR_OK);    
}


// Method to load an MZF format file from the SD card directly into the tranZPUter static RAM or mainboard RAM.
// If the load address is specified then it overrides the MZF header value, otherwise load addr is taken from the header.
//
FRESULT loadMZFZ80Memory(const char *src, uint32_t addr, uint32_t *bytesRead, uint8_t hdrOnly, enum TARGETS target)
{
    // Locals.
    FILE          *File;
    unsigned int  readSize;
    uint32_t      addrOffset = SRAM_BANK0_ADDR;
    uint32_t      cmtHdrAddr = MZ_CMT_ADDR;
    t_svcDirEnt   mzfHeader;
    FRESULT       fr0;

    // Sanity check on filenames.
    if(src == NULL)
        return(FR_INVALID_PARAMETER);

    // Try and open the source file.
    fr0 = (File = fopen(src, "r")) != NULL ? FR_OK : FR_NO_FILE;

    // If no error occurred, read in the header.
    //
    if(!fr0)
        readSize = fread((char *)&mzfHeader, 1, MZF_HEADER_SIZE, File);

    // No errors, process.
    if(!fr0 && readSize == MZF_HEADER_SIZE)
    {
        // Firstly, close the file, no longer needed.
        fclose(File);

        // Setup bank in which to load the header/data. Default is bank 0, different host hardware uses different banks.
        //
        if(target == TRANZPUTER && z80Control.hostType == HW_MZ800)
        {
            addrOffset = SRAM_BANK6_ADDR;
        } 
        else if(target == TRANZPUTER && z80Control.hostType == HW_MZ2000)
        {
            addrOffset = SRAM_BANK6_ADDR;
        }
      
        // Save the header into the CMT area for reference, some applications expect it. If the load address is below 1200H this could be wiped out, the code below stores a copy
        // in the service record sector on exit for this purpose. The caller needs to check the service record and if the Load Address is below >= 1200H use the CMT header else
        // use the service sector.
        //
        // NB: This assumes TZFS is running and made this call. Ignore for MZ2000 when in IPL mode as the header isnt needed.
        //
        if(z80Control.hostType != HW_MZ2000 || (z80Control.hostType == HW_MZ2000 && z80Control.iplMode == 0))
        {
            copyToZ80(addrOffset+cmtHdrAddr, (uint8_t *)&mzfHeader, MZF_HEADER_SIZE, target);
        }

        if(hdrOnly == 0)
        {
            // Now obtain the parameters.
            //
            if(addr == 0xFFFFFFFF)
            {
                // If the address needs to be determined from the header yet the header is below the RAM, set the buffer to 0x1200 and let the caller sort it out.
                if(mzfHeader.loadAddr > 0x1000)
                    addr = mzfHeader.loadAddr;
                else
                    addr = 0x1200;
            }

            // Look at the attribute byte, if it is >= 0xF8 then it is a special tranZPUter binary object requiring loading into a seperate memory bank.
            // The attribute & 0x07 << 16 specifies the memory bank in which to load the image.
            if(mzfHeader.attr >= 0xF8)
            {
                addr += ((mzfHeader.attr & 0x07) << 16);
            } else
            {
                addr += addrOffset;
            }

            // Ok, load up the file into Z80 memory.
            fr0 = loadZ80Memory(src, MZF_HEADER_SIZE, addr, (mzfHeader.attr >= 0xF8 ? 0 : mzfHeader.fileSize), bytesRead, target);

            // If the load address was below 0x11D0, the lowest free point in the original memory map then the load is said to be in lower DRAM. In this case the CMT header wont be available
            // so load the header into the service sector as well so the caller can determine the load state.
            memcpy((uint8_t *)&svcControl.sector, (uint8_t *)&mzfHeader, MZF_HEADER_SIZE);
        }
    }
    return(fr0 ? fr0 : FR_OK);    
}


// Method to read a section of the tranZPUter/mainboard memory and store it in an SD file.
//
FRESULT saveZ80Memory(const char *dst, uint32_t addr, uint32_t size, t_svcDirEnt *mzfHeader, enum TARGETS target)
{
    // Locals.
    //
    FILE          *File;
    uint32_t      saveSize       = 0L;
    uint32_t      sizeToWrite;
    uint32_t      memPtr         = addr;
    unsigned int  writeSize      = 0;        
    unsigned char buf[TZSVC_SECTOR_SIZE];
    FRESULT       fr0;

    // Sanity check on filenames.
    if(dst == NULL || size == 0)
        return(FR_INVALID_PARAMETER);

    // Try and create the destination file.
    fr0 = (File = fopen(dst, "w+")) != NULL ? FR_OK : FR_NO_FILE;

    // If no errors in opening the file, proceed with reading and loading into memory.
    if(!fr0)
    {
        // If an MZF header has been passed, we are saving an MZF file, write out the MZF header first prior to the data.
        //
        if(mzfHeader)
        {
            fr0 = (writeSize = fwrite((char *)mzfHeader, 1, MZF_HEADER_SIZE, File)) == MZF_HEADER_SIZE ? FR_OK : FR_DISK_ERR;
        }

        if(!fr0)
        {
            // Loop, reading a sector worth of data (or upto limit remaining) from the Z80 tranZPUter RAM or mainboard RAM and writing it into the open SD card file.
            //
            saveSize = 0;
            for (;;) {
    
                // Work out how many bytes to write in the sector then fetch from the Z80.
                sizeToWrite = (size-saveSize) > TZSVC_SECTOR_SIZE ? TZSVC_SECTOR_SIZE : size - saveSize;
                for(unsigned int idx=0; idx < sizeToWrite; idx++)
                {
                    // And now read the byte and to the next address!
                    buf[idx] = readZ80Memory((uint32_t)memPtr);
                    memPtr++;
                }
    
                fr0 = (writeSize = fwrite(buf, 1, sizeToWrite, File)) == sizeToWrite ? FR_OK : FR_DISK_ERR;
                saveSize += writeSize;

                if (fr0 || writeSize < sizeToWrite || saveSize >= size) break;       // error, disk full or range written.
            }
            printf("Saved %ld bytes, final address:%lx\n", saveSize, memPtr);
        } else
        {
            printf("Failed to write the MZF header.\n");
        }
       
        // Close to sync files.
        fclose(File);

    } else
    {
        printf("Cannot create file:%s\n", dst);
    }

    return(fr0 ? fr0 : FR_OK);    
}


///////////////////////////////////////////////////////
// Getter/Setter methods to keep z80Control private. //
///////////////////////////////////////////////////////

// Method to convert a Sharp filename into an Ascii filename.
//
void convertSharpFilenameToAscii(char *dst, char *src, uint8_t size)
{
    // Loop through and convert each character via the mapping table.
    for(uint8_t idx=0; idx < size; idx++)
    {
        *dst = (char)asciiMap[(int)*src].asciiCode;
        src++;
        dst++;
    }
    // As the Sharp Filename does not always contain a terminator, set a NULL at the end of the string (size+1) so that it can be processed
    // by standard routines. This means dst must always be size+1 in length.
    //
    *dst = 0x00;
    return;
}

// Helper method to convert a Sharp ASCII filename to one acceptable for use in the FAT32 filesystem.
//
void convertToFAT32FileNameFormat(char *dst)
{
    // Go through the given filename and change any characters which FAT32 doesnt support. This is necessary as the Sharp filenaming convention allows
    // for almost any character!
    //
    for(int idx=0; idx < strlen(dst); idx++)
    {
        if(dst[idx] == '/')
        {
            dst[idx] = '-';
        }
    }
    return;
}

// Method to change the secondary CPU frequency and optionally enable/disable it.
// On the FusionX there is no secondary frequency but as the emulation is being
// governed (delayed), we can change the delay to increase processing speed.
//
// Input: frequency = desired frequency in Hertz.
//        action    = 0 - take no action, just change frequency, 1 - set and enable the secondary CPU frequency, 2 - set and disable the secondary CPU frequency,
//                    3 - enable the secondary CPU frequency, 4 - disable the secondary CPU frequency
// Output: actual set frequency in Hertz.
//
uint32_t setZ80CPUFrequency(float frequency, uint8_t action)
{
    // Locals.
    //
    uint32_t        freqMultiplier = 0;
    struct ioctlCmd ioctlCmd;

    // Setup the alternative clock frequency based on a multiplier of the host base frequency.
    //
    if(action == 0 || action == 1 || action == 2)
    {
        z80Control.freqMultiplier = (uint32_t)(frequency / CPU_FREQUENCY_NORMAL);
        if(z80Control.freqMultiplier <= 1) z80Control.freqMultiplier = 1;
    }

    // Switch to new frequency.
    if(action == 1 || action == 3)
    {
        ioctlCmd.speed.speedMultiplier = z80Control.freqMultiplier;
    }
    // Switch to original frequency.
    if(action == 2 || action == 4)
    {
        ioctlCmd.speed.speedMultiplier = 1;
    }
   
    // Send the change frequency to the driver, it will adjust the governor. 
    ioctlCmd.cmd = IOCTL_CMD_Z80_CPU_FREQ;

    // Send IOCTL command to change to the new multiplier.
    ioctl(z80Control.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

    // Return the actual frequency set, this will be the nearest frequency the timers are capable of resolving.
    return(z80Control.freqMultiplier * CPU_FREQUENCY_NORMAL);
}

// Simple method to set defaults in the service structure if not already set by the Z80.
//
void svcSetDefaults(enum FILE_TYPE type)
{
    // Set according to the type of file were working with.
    //
    switch(type)
    {
        case CAS:
            // If there is no directory path, use the inbuilt default.
            if(svcControl.directory[0] == '\0')
            {
                strcpy((char *)svcControl.directory, TZSVC_DEFAULT_CAS_DIR);
            }
            // If there is no wildcard matching, use default.
            if(svcControl.wildcard[0] == '\0')
            {
                strcpy((char *)svcControl.wildcard, TZSVC_DEFAULT_WILDCARD);
            }
            break;

        case BAS:
            // If there is no directory path, use the inbuilt default.
            if(svcControl.directory[0] == '\0')
            {
                strcpy((char *)svcControl.directory, TZSVC_DEFAULT_BAS_DIR);
            }
            // If there is no wildcard matching, use default.
            if(svcControl.wildcard[0] == '\0')
            {
                strcpy((char *)svcControl.wildcard, TZSVC_DEFAULT_WILDCARD);
            }
            break;

        case MZF:
        default:
            // If there is no directory path, use the inbuilt default.
            if(svcControl.directory[0] == '\0')
            {
                strcpy((char *)svcControl.directory, TZSVC_DEFAULT_MZF_DIR);
            }
   
            // If there is no wildcard matching, use default.
            if(svcControl.wildcard[0] == '\0')
            {
                strcpy((char *)svcControl.wildcard, TZSVC_DEFAULT_WILDCARD);
            }
            break;
    }
    return;
}

// Helper method for matchFileWithWildcard.
// Get the next character from the filename or pattern and modify it if necessary to match the Sharp character set.
static uint32_t getNextChar(const char** ptr)
{
    uint8_t chr;

    // Get a byte
    chr = (uint8_t)*(*ptr)++;

    // To upper ASCII char
    if(islower(chr)) chr -= 0x20;

    return chr;
}

// Match an MZF name with a given wildcard.
// This method originated from the private method in FatFS but adapted to work with MZF filename matching.
// Input:  wildcard    - Pattern to match
//         fileName    - MZF fileName, either CR or NUL terminated or TZSVC_FILENAME_SIZE chars long.
//         skip        - Number of characters to skip due to ?'s
//         infinite    - Infinite search as * specified.
// Output: 0           - No match
//         1           - Match
//
static int matchFileWithWildcard(const char *pattern, const char *fileName, int skip, int infinite)
{
    const char *pp, *np;
    uint32_t   pc, nc;
    int        nm, nx;

    // Pre-skip name chars
    while (skip--)
    {
        // Branch mismatched if less name chars
        if (!getNextChar(&fileName)) return 0;
    }
    // (short circuit)
    if (*pattern == 0 && infinite) return 1;

    do {
        // Top of pattern and name to match
        pp = pattern; np = fileName;
        for (;;)
        {
            // Wildcard?
            if (*pp == '?' || *pp == '*')
            {
                nm = nx = 0;

                // Analyze the wildcard block
                do {
                    if (*pp++ == '?') nm++; else nx = 1;
                } while (*pp == '?' || *pp == '*');

                // Test new branch (recurs upto number of wildcard blocks in the pattern)
                if (matchFileWithWildcard(pp, np, nm, nx)) return 1;

                // Branch mismatched
                nc = *np; break;
            }
         
            // End of filename, Sharp filenames can be terminated with 0x00, CR or size. If we get to the end of the name then it is 
            // a match.
            //
            if((np - fileName) == TZSVC_FILENAME_SIZE) return 1;

            // Get a pattern char 
            pc = getNextChar(&pp);

            // Get a name char 
            nc = getNextChar(&np);

            // Sharp uses null or CR to terminate a pattern and a filename, which is not determinate!
            if((pc == 0x00 || pc == 0x0d) && (nc == 0x00 || nc == 0x0d)) return 1;

            // Branch mismatched?
            if (pc != nc) break;

            // Branch matched? (matched at end of both strings)
            if (pc == 0) return 1;
        }

        // fileName++
        getNextChar(&fileName);

    /* Retry until end of name if infinite search is specified */
    } while (infinite && nc != 0x00 && nc != 0x0d && (np - fileName) < TZSVC_FILENAME_SIZE);

    return 0;
}

// Method to open/read a directory listing. 
// This method opens a given directory on the SD card (the z80 provides the directory name or it defaults to MZF). The directory
// is read and a unique incrementing number is given to each entry, this number can be used in a later request to open the file to save on 
// name entry and matching.
// A basic pattern filter is applied to the results returned, ie A* will return only files starting with A.
// 
// The parameters are passed in the svcControl block.
//
uint8_t svcReadDir(uint8_t mode, enum FILE_TYPE type)
{
    // Locals - dont use global as this is a seperate thread.
    //
    static DIR           *dirFp;
    static uint8_t       dirOpen   = 0;          // Seperate flag as their is no public way to validate that dirFp is open and valid, the method in FatFS is private for this functionality.
    static struct dirent *fno;
    static uint8_t       dirSector = 0;          // Virtual directory sector.
    FRESULT              result    = FR_OK;
    unsigned int         readSize;
    char                 fqfn[256 + 13];
    FILE                 *File;
    t_svcCmpDirBlock     *dirBlock = (t_svcCmpDirBlock *)&svcControl.sector;   

    // Request to open? Validate that we dont already have an open directory then open the requested one.
    if(mode == TZSVC_OPEN)
    {
        // Close if previously open.
        if(dirOpen == 1)
            svcReadDir(TZSVC_CLOSE, type);

        // Setup the defaults
        //
        svcSetDefaults(type);

        // Open the directory.
        sprintf(fqfn, "%s%s", OS_BASE_DIR, svcControl.directory);
        result = (dirFp = opendir((char *)&fqfn)) != NULL ? FR_OK : FR_NO_PATH;
        if(!result)
        {
            // Reentrant call to actually read the data.
            //
            dirOpen   = 1; 
            dirSector = 0;
            result    = (FRESULT)svcReadDir(TZSVC_NEXT, type);
        }
    }

    // Read a block of directory entries into the z80 service buffer sector.
    else if(mode == TZSVC_NEXT && dirOpen == 1)
    {
        // If the Z80 is requesting a non sequential directory sector then we have to start from the beginning as each sector is built up not read.
        //
        if(dirSector != svcControl.dirSector)
        {
            // If the current sector is after the requested sector, rewind by re-opening.
            //
            if(dirSector < svcControl.dirSector)
            {
                result=svcReadDir(TZSVC_OPEN, type);
            }
            if(!result)
            {
                // Now get the sector by advancement.
                for(uint8_t idx=dirSector; idx < svcControl.dirSector && result == FR_OK; idx++)
                {
                    result=svcReadDir(TZSVC_NEXT, type);
                }
            }
        }

        // Proceed if no errors have occurred.
        //
        if(!result)
        {
            // Zero the directory entry block - unused entries will then appear as NULLS.
            memset(dirBlock, 0x00, TZSVC_SECTOR_SIZE);
    
            // Loop the required number of times to fill a sector full of entries.
            //
            uint8_t idx=0;
            while(idx < TZVC_MAX_CMPCT_DIRENT_BLOCK && result == FR_OK)
            {
                // Read an SD directory entry then open the returned SD file so that we can to read out the important MZF data which is stored in it as it will be passed to the Z80.
                //
                fno = readdir(dirFp);

                // If an error occurs or we are at the end of the directory listing close the sector and pass back.
                if(fno == NULL || fno->d_name[0] == 0) break;
            
                // Check to see if this is a valid file for the given type.
                const char *ext = strrchr(fno->d_name, '.');
                if(type == MZF && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_MZF_EXT) != 0))
                    continue;
                if(type == BAS && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_BAS_EXT) != 0))
                    continue;
                if(type == CAS && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_CAS_EXT) != 0))
                    continue;
                if(type == ALL && !ext)
                    continue;
    
                // Sharp files need special handling, the file needs te opened and the Sharp filename read out, this is then returned as the filename.
                //
                if(type == MZF)
                {
                    // Build filename.
                    //
                    sprintf(fqfn, "%s%s/%s", OS_BASE_DIR, svcControl.directory, fno->d_name);

                    // Open the file so we can read out the MZF header which is the information TZFS/CPM needs.
                    //
                    result = (File = fopen(fqfn, "r")) != NULL ? FR_OK : FR_NO_FILE;
        
                    // If no error occurred, read in the header.
                    //
                    if(File != NULL) readSize = fread((char *)&dirBlock->dirEnt[idx], 1, TZSVC_CMPHDR_SIZE, File);
        
                    // No errors, read the header.
                    if(File != NULL && readSize == TZSVC_CMPHDR_SIZE)
                    {
                        // Close the file, no longer needed.
                        fclose(File);
        
                        // Check to see if the file matches any given wildcard.
                        //
                        if(matchFileWithWildcard((char *)&svcControl.wildcard, (char *)&dirBlock->dirEnt[idx].fileName, 0, 0))
                        {
                            // Valid so find next entry.
                            idx++;
                        } else
                        {
                            // Scrub the entry, not valid.
                            memset((char *)&dirBlock->dirEnt[idx], 0x00, TZSVC_CMPHDR_SIZE);
                        }
                    }
                } else
                {
                    // Check to see if the file matches any given wildcard.
                    //
                    if(matchFileWithWildcard((char *)&svcControl.wildcard, fno->d_name, 0, 0))
                    {
                        // If the type is ALL FORMATTED then output a truncated directory entry formatted for display.
                        //
                        if(type == ALLFMT)
                        {
                            // Get the address of the file extension.
                            char *ext = strrchr(fno->d_name, '.');
                          
                            // Although not as efficient, maintain the use of the Sharp MZF header for normal directory filenames just use the extra space for increased filename size.
                            if(ext)
                            {
                                // Although not as efficient, maintain the use of the Sharp MZF header for normal directory filenames just use the extra space for increased filename size.
                                if((ext - fno->d_name) > TZSVC_LONG_FMT_FNAME_SIZE-5)
                                {
                                    fno->d_name[TZSVC_LONG_FMT_FNAME_SIZE-6] = '*';       // Place a '*' to show the filename was truncated.
                                    fno->d_name[TZSVC_LONG_FMT_FNAME_SIZE-5] = 0x00;
                                }
                                *ext = 0x00;
                                ext++;

                                sprintf((char *)&dirBlock->dirEnt[idx].fileName, "%-*s.%3s", TZSVC_LONG_FMT_FNAME_SIZE-5, fno->d_name, ext);
                            } else
                            {
                                fno->d_name[TZSVC_LONG_FMT_FNAME_SIZE] =  0x00;
                                strncpy((char *)&dirBlock->dirEnt[idx].fileName, fno->d_name, TZSVC_LONG_FMT_FNAME_SIZE);
                            }
                        } else
                        // All other types just output the filename upto the limit truncating as necessary.
                        {
                            fno->d_name[TZSVC_LONG_FNAME_SIZE] =  0x00;
                            strncpy((char *)&dirBlock->dirEnt[idx].fileName, fno->d_name, TZSVC_LONG_FNAME_SIZE);
                        }

                        // Set the attribute in the directory record to indicate this is a valid record.
                        dirBlock->dirEnt[idx].attr = 0xff;

                        // Valid so find next entry.
                        idx++;
                    } else
                    {
                        // Scrub the entry, not valid.
                        memset((char *)&dirBlock->dirEnt[idx], 0x00, TZSVC_CMPHDR_SIZE);
                    }
                }
            }
        }

        // Increment the virtual directory sector number as the Z80 expects a sector of directory entries per call.
        if(!result)
            dirSector++;
    }
    // Close the currently open directory.
    else if(mode == TZSVC_CLOSE)
    {
        if(dirOpen)
            closedir(dirFp);
        dirOpen = 0;
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}


// A method to find a file either using a Sharp MZ80A name, a standard filename or a number assigned to a directory listing.
// For the Sharp MZ80A it is a bit long winded as each file that matches the filename specification has to be opened and the MZF header filename 
// has to be checked. For standard files it is just a matter of matching the name. For both types a short cut is a number which is a files position
// in a directory obtained from a previous directory listing.
//
uint8_t svcFindFile(char *file, char *searchFile, uint8_t searchNo, enum FILE_TYPE type)
{
    // Locals
    uint8_t        fileNo    = 0;
    uint8_t        found     = 0;
    unsigned int   readSize;
    char           fqfn[256 + 13];
    FILE           *File;
    struct dirent  *fno;
    DIR            *dirFp;
    FRESULT        result    = FR_OK;
    t_svcCmpDirEnt dirEnt;

    // Setup the defaults
    //
    svcSetDefaults(type);

    // Open the directory.
    sprintf(fqfn, "%s%s", OS_BASE_DIR, svcControl.directory);
    result = (dirFp = opendir((char *)&fqfn)) != NULL ? FR_OK : FR_NO_PATH;
    if(result == FR_OK)
    {
        fileNo = 0;
        
        do {
            // Read an SD directory entry then open the returned SD file so that we can to read out the important MZF data for name matching.
            //
            result = (fno = readdir(dirFp)) != NULL ? FR_OK : FR_NO_FILE;

            // If an error occurs or we are at the end of the directory listing close the sector and pass back.
            if(result != FR_OK || fno->d_name[0] == 0) break;

            // Check to see if this is a valid file for the given type.
            const char *ext = strrchr(fno->d_name, '.');
            if(type == MZF && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_MZF_EXT) != 0))
                continue;
            if(type == BAS && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_BAS_EXT) != 0))
                continue;
            if(type == CAS && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_CAS_EXT) != 0))
                continue;
            if(type == ALL && !ext)
                continue;
    
            // Sharp files need special handling, the file needs te opened and the Sharp filename read out, this is then used as the filename for matching.
            //
            if(type == MZF)
            {
                // Build filename.
                //
                sprintf(fqfn, "%s%s/%s", OS_BASE_DIR, svcControl.directory, fno->d_name);

                // Open the file so we can read out the MZF header which is the information TZFS/CPM needs.
                //
                result = (File = fopen(fqfn, "r")) != NULL ? FR_OK : FR_NO_FILE;

                // If no error occurred, read in the header.
                //
                if(!result) readSize = fread((char *)&dirEnt, 1, TZSVC_CMPHDR_SIZE, File);

                // No errors, read the header.
                if(!result && readSize == TZSVC_CMPHDR_SIZE)
                {
                    // Close the file, no longer needed.
                    fclose(File);

                    // Check to see if the file matches any given wildcard. If we dont have a match loop to next directory entry.
                    //
                    if(matchFileWithWildcard((char *)&svcControl.wildcard, (char *)&dirEnt.fileName, 0, 0))
                    {
                        // If a filename has been given, see if this file matches it.
                        if(searchFile != NULL)
                        {
                            // Check to see if the file matches the name given with wildcard expansion if needed.
                            //
                            if(matchFileWithWildcard(searchFile, (char *)&dirEnt.fileName, 0, 0))
                            {
                                found = 2;
                            }
                        }
              
                        // If we are searching on file number and the latest directory entry retrieval matches, exit and return the filename.
                        if(searchNo != 0xFF && fileNo == (uint8_t)searchNo)
                        {
                            found = 1;
                        } else
                        {
                            fileNo++;
                        }
                    }
                }
            } else
            {
                // Check to see if the file matches any given wildcard. If we dont have a match loop to next directory entry.
                //
                if(matchFileWithWildcard((char *)&svcControl.wildcard, (char *)&fno->d_name, 0, 0))
                {
                    // If a filename has been given, see if this file matches it.
                    if(searchFile != NULL)
                    {
                        // Check to see if the file matches the name given with wildcard expansion if needed.
                        //
                        if(matchFileWithWildcard(searchFile, (char *)&fno->d_name, 0, 0))
                        {
                            found = 2;
                        }
                    }
          
                    // If we are searching on file number and the latest directory entry retrieval matches, exit and return the filename.
                    if(searchNo != 0xFF && fileNo == (uint8_t)searchNo)
                    {
                        found = 1;
                    } else
                    {
                        fileNo++;
                    }
                }
            }
        } while(!result && !found);

        // If the file was found, copy the FQFN into its buffer.
        //
        if(found)
        {
            strcpy(file, fqfn);
        }
    }

    // Return 1 if file was found, 0 for all other cases.
    //
    return(result == FR_OK ? (found == 0 ? 0 : 1) : 0);
}

// Method to read the current directory from the cache. If the cache is invalid resort to using the standard direct method.
//
uint8_t svcReadDirCache(uint8_t mode, enum FILE_TYPE type)
{
    // Locals - dont use global as this is a seperate thread.
    //
    static uint8_t    dirOpen     = 0;          // Seperate flag as their is no public way to validate that dirFp is open and valid, the method in FatFS is private for this functionality.
    static uint8_t    dirSector   = 0;          // Virtual directory sector.
    static uint8_t    dirEntry    = 0;          // Last cache entry number processed.
    FRESULT           result      = FR_OK;
    t_svcCmpDirBlock  *dirBlock = (t_svcCmpDirBlock *)&svcControl.sector;   

    // Setup the defaults
    //
    svcSetDefaults(type);
   
    // Need to refresh cache directory?
    if(!osControl.dirMap.valid || strcasecmp((const char *)svcControl.directory, osControl.dirMap.directory) != 0 || osControl.dirMap.type != type)
        result=svcCacheDir((const char *)svcControl.directory, svcControl.fileType, 0);

    // If there is no cache revert to direct directory read.
    //
    if(!osControl.dirMap.valid || result)
    {
        result = svcReadDir(mode, type);
    } else
    {
        // Request to open? No need with cache, just return next block.
        if(mode == TZSVC_OPEN)
        {
            // Reentrant call to actually read the data.
            //
            dirOpen   = 1; 
            dirSector = 0;
            dirEntry  = 0;
            result    = (FRESULT)svcReadDirCache(TZSVC_NEXT, type);
        }
    
        // Read a block of directory entries into the z80 service buffer sector.
        else if(mode == TZSVC_NEXT && dirOpen == 1)
        {
            // If the Z80 is requesting a non sequential directory sector then calculate the new cache entry position.
            //
            if(dirSector != svcControl.dirSector)
            {
                dirEntry = svcControl.dirSector * TZVC_MAX_CMPCT_DIRENT_BLOCK;
                dirSector = svcControl.dirSector;
                if(dirEntry > osControl.dirMap.entries)
                {
                    dirEntry = osControl.dirMap.entries;
                    dirSector = osControl.dirMap.entries / TZVC_MAX_CMPCT_DIRENT_BLOCK;
                }
            }
    
            // Zero the directory entry block - unused entries will then appear as NULLS.
            memset(dirBlock, 0x00, TZSVC_SECTOR_SIZE);
    
            // Loop the required number of times to fill a sector full of entries.
            //
            uint8_t idx=0;
            while(idx < TZVC_MAX_CMPCT_DIRENT_BLOCK && dirEntry < osControl.dirMap.entries && result == FR_OK)
            {
                // Check to see if the file matches any given wildcard.
                //
                if(matchFileWithWildcard((char *)&svcControl.wildcard, type == MZF ? (char *)&osControl.dirMap.mzfFile[dirEntry]->mzfHeader.fileName : (char *)osControl.dirMap.sdFileName[dirEntry], 0, 0))
                {
                    // Sharp files we copy the whole header into the entry.
                    //
                    if(type == MZF)
                    {
                        // Valid so store and find next entry.
                        //
                        memcpy((char *)&dirBlock->dirEnt[idx], (char *)&osControl.dirMap.mzfFile[dirEntry]->mzfHeader, TZSVC_CMPHDR_SIZE);
                    } else
                    {
                        // If the type is ALL FORMATTED then output a truncated directory entry formatted for display.
                        //
                        if(type == ALLFMT)
                        {
                            // Duplicate the cache entry, formatting is destructuve and less time neeed to duplicate than repair.
                            char *fname = strdup((char *)osControl.dirMap.sdFileName[dirEntry]);
                            if(fname != NULL)
                            {
                                // Get the address of the file extension.
                                char *ext = strrchr(fname, '.');
                          
                                // Although not as efficient, maintain the use of the Sharp MZF header for normal directory filenames just use the extra space for increased filename size.
                                if(ext)
                                {
                                    // Although not as efficient, maintain the use of the Sharp MZF header for normal directory filenames just use the extra space for increased filename size.
                                    if((ext - fname) > TZSVC_LONG_FMT_FNAME_SIZE-5)
                                    {
                                        fname[TZSVC_LONG_FMT_FNAME_SIZE-6] = '*';       // Place a '*' to show the filename was truncated.
                                        fname[TZSVC_LONG_FMT_FNAME_SIZE-5] = 0x00;
                                    }
                                    *ext = 0x00;
                                    ext++;
                                    sprintf((char *)&dirBlock->dirEnt[idx].fileName, "%-*s.%3s", TZSVC_LONG_FMT_FNAME_SIZE-5, fname, ext);
                                } else
                                {
                                    fname[TZSVC_LONG_FMT_FNAME_SIZE] = 0x00;
                                    strncpy((char *)&dirBlock->dirEnt[idx].fileName, fname, TZSVC_LONG_FMT_FNAME_SIZE);
                                }

                                // Release the duplicate memory.
                                free(fname);
                            } else
                            {
                                printf("Out of memory duplicating directory filename.\n");
                            }
                        } else
                        // All other types just output the filename upto the limit truncating as necessary.
                        {
                            osControl.dirMap.sdFileName[dirEntry][TZSVC_LONG_FNAME_SIZE] = 0x00;
                            strncpy((char *)&dirBlock->dirEnt[idx].fileName, (char *)osControl.dirMap.sdFileName[dirEntry],  TZSVC_LONG_FNAME_SIZE);
                        }

                        // Set the attribute in the directory record to indicate this is a valid record.
                        dirBlock->dirEnt[idx].attr = 0xff;
                    } 
                    idx++;
                }
                dirEntry++;
            }

            // Increment the virtual directory sector number as the Z80 expects a sector of directory entries per call.
            if(!result)
                dirSector++;
        }
        // Close the currently open directory.
        else if(mode == TZSVC_CLOSE)
        {
            dirOpen = 0;
        }
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// A method to find a file using the cached directory. If the cache is not available (ie. no memory) use the standard method.
//
uint8_t svcFindFileCache(char *file, char *searchFile, uint8_t searchNo, enum FILE_TYPE type)
{
    // Locals
    uint8_t        fileNo    = 0;
    uint8_t        found     = 0;
    uint8_t        idx       = 0;
    FRESULT        result    = FR_OK;

    // If there is no cache revert to direct search.
    //
    if(!osControl.dirMap.valid)
    {
        result = (found = svcFindFile(file, searchFile, searchNo, type)) != 0 ? FR_OK : FR_NO_FILE;
    } else
    {
        // If we are searching on file number and there is no filter in place, see if it is valid and exit with data.
        if(searchNo != 0xFF && strcmp((char *)svcControl.wildcard, TZSVC_DEFAULT_WILDCARD) == 0)
        {
            if(searchNo < osControl.dirMap.entries && osControl.dirMap.mzfFile[searchNo]) // <- this is the same for standard files as mzfFile is a union for sdFileName
            {
                found = 1;
                idx   = searchNo;
            } else
            {
                result = FR_NO_FILE;
            }
        } else
        {
            do {
                // Check to see if the file matches any given wildcard. If we dont have a match loop to next directory entry.
                //
                if(matchFileWithWildcard((char *)&svcControl.wildcard, type == MZF ? (char *)&osControl.dirMap.mzfFile[idx]->mzfHeader.fileName : (char *)&osControl.dirMap.sdFileName[idx], 0, 0))
                {
                    // If a filename has been given, see if this file matches it.
                    if(searchFile != NULL)
                    {
                        // Check to see if the file matches the name given with wildcard expansion if needed.
                        //
                        if(matchFileWithWildcard(searchFile, type == MZF ? (char *)&osControl.dirMap.mzfFile[idx]->mzfHeader.fileName : (char *)&osControl.dirMap.sdFileName[idx], 0, 0))
                        {
                            found = 2;
                        }
                    }

                    // If we are searching on file number then see if it matches (after filter has been applied above).
                    if(searchNo != 0xFF && fileNo == (uint8_t)searchNo)
                    {
                        found = 1;
                    } else
                    {
                        fileNo++;
                    }
                }
                if(!found)
                {
                    idx++;
                }
            } while(!result && !found && idx < osControl.dirMap.entries);
        }

        // If the file was found, copy the FQFN into its buffer.
        //
        if(found)
        {
            // Build filename.
            //
            sprintf(file, "%s%s/%s", OS_BASE_DIR, osControl.dirMap.directory, type == MZF ? osControl.dirMap.mzfFile[idx]->sdFileName : osControl.dirMap.sdFileName[idx]);
        }
    }

    // Return 1 if file was found, 0 for all other cases.
    //
    return(result == FR_OK ? (found == 0 ? 0 : 1) : 0);
}

// Method to build up a cache of all the files on the SD card in a given directory along with any mapping to Sharp MZ80A headers if required.
// For Sharp MZ80A files this involves scanning each file to extract the MZF header and creating a map.
//
uint8_t svcCacheDir(const char *directory, enum FILE_TYPE type, uint8_t force)
{
    // Locals
    uint8_t        fileNo    = 0;
    unsigned int   readSize;
    char           fqfn[256 + 13];
    FILE           *File;
    struct dirent  *fno;
    DIR            *dirFp;
    FRESULT        result    = FR_OK;
    t_svcCmpDirEnt dirEnt;

    // No need to cache directory if we have already cached it.
    if(force == 0 && osControl.dirMap.valid && strcasecmp(directory, osControl.dirMap.directory) == 0 && osControl.dirMap.type == type)
        return(1);

    // Invalidate the map and free existing memory incase of errors.
    //
    osControl.dirMap.valid = 0;
    for(uint8_t idx=0; idx < osControl.dirMap.entries; idx++)
    {
        if(osControl.dirMap.type == MZF && osControl.dirMap.mzfFile[idx])
        {
            free(osControl.dirMap.mzfFile[idx]->sdFileName);
            free(osControl.dirMap.mzfFile[idx]);
            osControl.dirMap.mzfFile[idx] = 0;
        } else
        {
            free(osControl.dirMap.sdFileName[idx]);
            osControl.dirMap.sdFileName[idx] = 0;
        }
    }
    osControl.dirMap.entries = 0;
    osControl.dirMap.type = MZF;

    // Open the directory and extract all files.
    sprintf(fqfn, "%s%s", OS_BASE_DIR, directory);
    result = (dirFp = opendir(fqfn)) != NULL ? FR_OK : FR_NO_PATH;
    if(result == FR_OK)
    {
        fileNo = 0;
        
        do {
            // Read an SD directory entry. If reading Sharp MZ80A files then open the returned SD file so that we can to read out the important MZF data for name matching.
            //
            fno = readdir(dirFp);

            // If an error occurs or we are at the end of the directory listing close the sector and pass back.
            if(fno == NULL || fno->d_name[0] == 0) break;

            // Check to see if this is a valid file for the given type.
            const char *ext = strrchr(fno->d_name, '.');
            if(type == MZF && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_MZF_EXT) != 0))
                continue;
            if(type == BAS && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_BAS_EXT) != 0))
                continue;
            if(type == CAS && (!ext || strcasecmp(++ext, TZSVC_DEFAULT_CAS_EXT) != 0))
                continue;
            if(type == ALL && !ext)
                continue;
            // Sharp files need special handling, the file needs te opened and the Sharp filename read out, this is then used in the cache.
            //
            if(type == MZF)
            {
                // Build filename.
                //
                sprintf(fqfn, "%s%s/%s", OS_BASE_DIR, directory, fno->d_name);

                // Open the file so we can read out the MZF header which is the information TZFS/CPM needs.
                //
                result = (File = fopen(fqfn, "r")) != NULL ? FR_OK : FR_NO_FILE;
             
                // If no error occurred, read in the header.
                //
                if(!result) readSize = fread((char *)&dirEnt, 1, TZSVC_CMPHDR_SIZE, File);

                // No errors, read the header.
                if(!result && readSize == TZSVC_CMPHDR_SIZE)
                {
                    // Close the file, no longer needed.
                    fclose(File);

                    // Cache this entry. The SD filename is dynamically allocated as it's size can be upto 255 characters for LFN names. The Sharp name is 
                    // fixed at 17 characters as you cant reliably rely on terminators and the additional data makes it a constant 32 chars long.
                    osControl.dirMap.mzfFile[fileNo] = (t_sharpToSDMap *)malloc(sizeof(t_sharpToSDMap));
                    if(osControl.dirMap.mzfFile[fileNo] != NULL)
                        osControl.dirMap.mzfFile[fileNo]->sdFileName = (uint8_t *)malloc(strlen(fno->d_name)+1);

                    if(osControl.dirMap.mzfFile[fileNo] == NULL || osControl.dirMap.mzfFile[fileNo]->sdFileName == NULL)
                    {
                        printf("Out of memory cacheing directory:%s\n", directory);
                        for(uint8_t idx=0; idx <= fileNo; idx++)
                        {
                            if(osControl.dirMap.mzfFile[idx])
                            {
                                free(osControl.dirMap.mzfFile[idx]->sdFileName);
                                free(osControl.dirMap.mzfFile[idx]);
                                osControl.dirMap.mzfFile[idx] = 0;
                            }
                        }
                        result = FR_NOT_ENOUGH_CORE;
                    } else
                    {
                        // Copy in details into this maps node.
                        strcpy((char *)osControl.dirMap.mzfFile[fileNo]->sdFileName, fno->d_name);
                        memcpy((char *)&osControl.dirMap.mzfFile[fileNo]->mzfHeader, (char *)&dirEnt, TZSVC_CMPHDR_SIZE);
                        fileNo++;
                    }
                }
            } else
            {
                // Cache this entry. The SD filename is dynamically allocated as it's size can be upto 255 characters for LFN names. The SD service header is based
                // on Sharp names which are fixed at 17 characters which shouldnt be a problem, just truncate the name as there is no support for longer names yet!
                osControl.dirMap.sdFileName[fileNo] = (uint8_t *)malloc(strlen(fno->d_name)+1);

                if(osControl.dirMap.sdFileName[fileNo] == NULL)
                {
                    printf("Out of memory cacheing directory:%s\n", directory);
                    for(uint8_t idx=0; idx <= fileNo; idx++)
                    {
                        if(osControl.dirMap.sdFileName[idx])
                        {
                            free(osControl.dirMap.sdFileName[idx]);
                            osControl.dirMap.sdFileName[idx] = 0;
                        }
                    }
                    result = FR_NOT_ENOUGH_CORE;
                } else
                {
                    // Copy in details into this maps node.
                    strcpy((char *)osControl.dirMap.sdFileName[fileNo], fno->d_name);
                    fileNo++;
                }
            }
        } while(!result && fileNo < TZSVC_MAX_DIR_ENTRIES);
    }

    // Success?
    if(result == FR_OK && (fno == NULL || fno->d_name[0] == 0 || fileNo == TZSVC_MAX_DIR_ENTRIES))
    {
        // Validate the cache.
        osControl.dirMap.valid = 1;
        osControl.dirMap.entries = fileNo;
        strcpy(osControl.dirMap.directory, directory);
        // Save the filetype.
        osControl.dirMap.type = type;
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Method to open a file for reading and return requested sectors.
//
uint8_t svcReadFile(uint8_t mode, enum FILE_TYPE type)
{
    // Locals - dont use global as this is a seperate thread.
    //
    static FILE       *File;
    static uint8_t    fileOpen   = 0;         // Seperate flag as their is no public way to validate that File is open and valid, the method in FatFS is private for this functionality.
    static uint8_t    fileSector = 0;         // Sector number being read.
    FRESULT           result    = FR_OK;
    unsigned int      readSize;
    char              fqfn[256 + 13];

    // Find the required file.
    // Request to open? Validate that we dont already have an open file then find and open the file.
    if(mode == TZSVC_OPEN)
    {
        // Close if previously open.
        if(fileOpen == 1)
            svcReadFile(TZSVC_CLOSE, type);

        // Setup the defaults
        //
        svcSetDefaults(type);

        if(type == CAS || type == BAS)
        {
            // Build the full filename from what has been provided.
            // Cassette and basic images, create filename as they are not cached.
            sprintf(fqfn, "%s%s/%s.%s", OS_BASE_DIR, svcControl.directory, svcControl.filename, (type == MZF ? TZSVC_DEFAULT_MZF_EXT : type == CAS ? TZSVC_DEFAULT_CAS_EXT : TZSVC_DEFAULT_BAS_EXT));
        }

        // Find the file using the given file number or file name.
        //
        if( (type == MZF && (svcFindFileCache(fqfn, (char *)&svcControl.filename, svcControl.fileNo, type))) || type == CAS || type == BAS )
        {
            // Open the file, fqfn has the FQFN of the correct file on the SD drive.
            result = (File = fopen(fqfn, "r")) != NULL ? FR_OK : FR_NO_FILE;

            if(result == FR_OK)
            {
                // Reentrant call to actually read the data.
                //
                fileOpen   = 1; 
                fileSector = 0;
                result     = (FRESULT)svcReadFile(TZSVC_NEXT, type);
            }
        }
    }
  
    // Read the next sector from the file.
    else if(mode == TZSVC_NEXT && fileOpen == 1)
    {
        // If the Z80 is requesting a non sequential sector then seek to the correct location prior to the read.
        //
        if(fileSector != svcControl.fileSector)
        {
            result = (fseek(File, (svcControl.fileSector * TZSVC_SECTOR_SIZE), SEEK_SET) != -1) ? FR_OK : FR_DISK_ERR;
            fileSector = svcControl.fileSector;
        }

        // Proceed if no errors have occurred.
        //
        if(!result)
        {
            // Read the required sector.
            readSize = fread((char *)&svcControl.sector, 1, TZSVC_SECTOR_SIZE, File);

            // Place number of bytes read into the record for the Z80 to know where EOF is.
            //
            svcControl.loadSize = readSize;
        }

        // Move onto next sector.
        fileSector++;
    }
   
    // Close the currently open file.
    else if(mode == TZSVC_CLOSE)
    {
        if(fileOpen)
            fclose(File);
        fileOpen = 0;
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Method to create a file for writing and on subsequent calls write the data into that file.
//
uint8_t svcWriteFile(uint8_t mode, enum FILE_TYPE type)
{
    // Locals - dont use global as this is a seperate thread.
    //
    static FILE       *File;
    static uint8_t    fileOpen   = 0;         // Seperate flag as their is no public way to validate that File is open and valid, the method in FatFS is private for this functionality.
    static uint8_t    fileSector = 0;         // Sector number being read.
    FRESULT           result    = FR_OK;
    unsigned int      readSize;
    char              fqfn[256 + 13];

    // Request to createopen? Validate that we dont already have an open file and the create the file.
    if(mode == TZSVC_OPEN)
    {
        // Close if previously open.
        if(fileOpen == 1)
            svcWriteFile(TZSVC_CLOSE, type);

        // Setup the defaults
        //
        svcSetDefaults(type);
       
        // Build the full filename from what has been provided.
        sprintf(fqfn, "%s%s/%s.%s", OS_BASE_DIR, svcControl.directory, svcControl.filename, (type == MZF ? TZSVC_DEFAULT_MZF_EXT : type == CAS ? TZSVC_DEFAULT_CAS_EXT : TZSVC_DEFAULT_BAS_EXT));

        // Create the file, fqfn has the FQFN of the correct file on the SD drive.
        result = (File = fopen(fqfn, "w+")) != NULL ? FR_OK : FR_NO_FILE;

        // If the file was opened, set the flag to enable writing.
        if(!result)
            fileOpen = 1;
    }
  
    // Write the next sector into the file.
    else if(mode == TZSVC_NEXT && fileOpen == 1)
    {
        // If the Z80 is requesting a non sequential sector then seek to the correct location prior to the write.
        //
        if(fileSector != svcControl.fileSector)
        {
            result = (fseek(File, (svcControl.fileSector * TZSVC_SECTOR_SIZE), SEEK_SET) != -1) ? FR_OK : FR_DISK_ERR;
            fileSector = svcControl.fileSector;
        }

        // Proceed if no errors have occurred.
        //
        if(!result)
        {
            // Write the required sector.
            result = (readSize = fwrite((char *)&svcControl.sector, 1, svcControl.saveSize, File)) == svcControl.saveSize ? FR_OK : FR_DISK_ERR;
        }

        // Move onto next sector.
        fileSector++;
    }
   
    // Close the currently open file.
    else if(mode == TZSVC_CLOSE)
    {
        if(fileOpen)
            fclose(File);
        fileOpen = 0;
    } else
    {
        printf("WARNING: svcWriteFile called with unknown mode:%d\n", mode);
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Method to load a file from SD directly into the tranZPUter memory.
//
uint8_t svcLoadFile(enum FILE_TYPE type)
{
    // Locals - dont use global as this is a seperate thread.
    //
    FRESULT           result    = FR_OK;
    uint32_t          bytesRead;
    char              fqfn[256 + 13];

    // Setup the defaults
    //
    svcSetDefaults(type);

    // MZF and MZF Headers are handled with their own methods as it involves looking into the file to determine the name and details.
    //
    if(type == MZF || type == MZFHDR)
    {
        // Find the file using the given file number or file name.
        //
        if(svcFindFileCache(fqfn, (char *)&svcControl.filename, svcControl.fileNo, MZF))
        {
            // Call method to load an MZF file.
            result = loadMZFZ80Memory(fqfn, (svcControl.loadAddr == 0xFFFF ? 0xFFFFFFFF : svcControl.loadAddr), 0, (type == MZFHDR ? 1 : 0), (svcControl.memTarget == 0 ? TRANZPUTER : MAINBOARD));

            // Store the filename, used in reload or immediate saves.
            //
            osControl.lastFile = (uint8_t *)realloc(osControl.lastFile, strlen(fqfn)+1);
            if(osControl.lastFile == NULL)
            {
                printf("Out of memory saving last file name, dependent applications (ie. CP/M) wont work!\n");
                result = FR_NOT_ENOUGH_CORE;
            } else
            {
                strcpy((char *)osControl.lastFile, fqfn);
            }

        } else
        {
            result = FR_NO_FILE;
        }
    } else
    // Cassette images are for NASCOM/Microsoft Basic. The files are in NASCOM format so the header needs to be skipped.
    // BAS files are for human readable BASIC files.
    if(type == CAS || type == BAS)
    {
        // Build the full filename from what has been provided.
        sprintf(fqfn, "%s%s/%s.%s", OS_BASE_DIR, svcControl.directory, svcControl.filename, type == CAS ? TZSVC_DEFAULT_CAS_EXT : TZSVC_DEFAULT_BAS_EXT);

        // For the tokenised cassette, skip the header and load directly into the memory location provided. The load size given is the maximum size of file
        // and the loadZ80Memory method will only load upto the given size.
        //
        if((result=loadZ80Memory((const char *)fqfn, 0, svcControl.loadAddr, svcControl.loadSize, &bytesRead, TRANZPUTER)) != FR_OK)
        {
            printf("Error: Failed to load CAS:%s into tranZPUter memory.\n", fqfn);
        } else
        {
            // Return the size of load in the service control record.
            svcControl.loadSize = (uint16_t) bytesRead;
        }
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Method to save a file from tranZPUter memory directly into a file on the SD card.
//
uint8_t svcSaveFile(enum FILE_TYPE type)
{
    // Locals - dont use global as this is a seperate thread.
    //
    FRESULT           result    = FR_OK;
    char              fqfn[256 + 13];
    char              asciiFileName[TZSVC_FILENAME_SIZE+1];
    uint32_t          addrOffset = SRAM_BANK0_ADDR;
    t_svcDirEnt       mzfHeader;

    // Setup the defaults
    //
    svcSetDefaults(type);
   
    // MZF are handled with their own methods as it involves looking into the file to determine the name and details.
    //
    if(type == MZF)
    {
        // Setup bank in which to load the header/data. Default is bank 0, different host hardware uses different banks.
        //
        if(svcControl.memTarget == 0 && z80Control.hostType == HW_MZ800)
        {
            addrOffset = SRAM_BANK6_ADDR;
        } 
        else if(svcControl.memTarget == 0 && z80Control.hostType == HW_MZ2000)
        {
            addrOffset = SRAM_BANK6_ADDR;
        }
      
        // Get the MZF header which contains the details of the file to save.
        copyFromZ80((uint8_t *)&mzfHeader, addrOffset + MZ_CMT_ADDR, MZF_HEADER_SIZE, TRANZPUTER);

        // Need to extract and convert the filename to create a file.
        //
        convertSharpFilenameToAscii(asciiFileName, (char *)mzfHeader.fileName, TZSVC_FILENAME_SIZE);

        // Build filename.
        //
        sprintf(fqfn, "%s%s/%s.%s", OS_BASE_DIR, svcControl.directory, asciiFileName, TZSVC_DEFAULT_MZF_EXT);

        // Convert any non-FAT32 characters prior to save.
        convertToFAT32FileNameFormat(fqfn);

        // Call the main method to save memory passing in the correct MZF details and header.
        result = saveZ80Memory(fqfn, (mzfHeader.loadAddr < MZ_CMT_DEFAULT_LOAD_ADDR-3 ? addrOffset+MZ_CMT_DEFAULT_LOAD_ADDR : addrOffset+mzfHeader.loadAddr), mzfHeader.fileSize, &mzfHeader, (svcControl.memTarget == 0 ? TRANZPUTER : MAINBOARD));
    } else
    // Cassette images are for NASCOM/Microsoft Basic. The files are in NASCOM format so the header needs to be skipped.
    // BAS files are for human readable BASIC files.
    if(type == CAS || type == BAS)
    {
        // Build the full filename from what has been provided.
        sprintf(fqfn, "%s%s/%s.%s", OS_BASE_DIR, svcControl.directory, svcControl.filename, type == CAS ? TZSVC_DEFAULT_CAS_EXT : TZSVC_DEFAULT_BAS_EXT);

        // Convert any non-FAT32 characters prior to save.
        convertToFAT32FileNameFormat(fqfn);
      
        // Call the main method to save memory passing in the correct details from the service record.
        result = saveZ80Memory(fqfn, svcControl.saveAddr, svcControl.saveSize, NULL, TRANZPUTER);
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Method to erase a file on the SD card.
//
uint8_t svcEraseFile(enum FILE_TYPE type)
{
    // Locals - dont use global as this is a seperate thread.
    //
    FRESULT           result    = FR_OK;
    char              fqfn[256 + 13];

    // Setup the defaults
    //
    svcSetDefaults(MZF);

    // MZF are handled with their own methods as it involves looking into the file to determine the name and details.
    //
    if(type == MZF)
    {
        // Find the file using the given file number or file name.
        //
        if(svcFindFileCache(fqfn, (char *)&svcControl.filename, svcControl.fileNo, type))
        {
            // Call method to load an MZF file.
            result = remove(fqfn);
        } else
        {
            result = FR_NO_FILE;
        }
    } else
    // Cassette images are for NASCOM/Microsoft Basic. The files are in NASCOM format so the header needs to be skipped.
    if(type == CAS)
    {
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Method to add a SD disk file as a CP/M disk drive for read/write by CP/M.
//
uint8_t svcAddCPMDrive(void)
{
    // Locals
    char           fqfn[256 + 13];
    FRESULT        result    = FR_OK;
    // Sanity checks.
    //
    if(svcControl.fileNo >= CPM_MAX_DRIVES)
        return(TZSVC_STATUS_FILE_ERROR);

    // Disk already allocated? May be a reboot or drive reassignment so free up the memory to reallocate.
    //
    if(osControl.cpmDriveMap.drive[svcControl.fileNo] != NULL)
    {
        if(osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName != NULL)
        {
            free(osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName);
            osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName = 0;
        }
        free(osControl.cpmDriveMap.drive[svcControl.fileNo]);
        osControl.cpmDriveMap.drive[svcControl.fileNo] = 0;
    }

    // Build filename for the drive.
    //
    sprintf(fqfn, "%s/%s/" CPM_DRIVE_TMPL, OS_BASE_DIR, CPM_SD_DRIVES_DIR, svcControl.fileNo);
    osControl.cpmDriveMap.drive[svcControl.fileNo] = (t_cpmDrive *)malloc(sizeof(t_cpmDrive));

    if(osControl.cpmDriveMap.drive[svcControl.fileNo] == NULL)
    {
        printf("Out of memory adding CP/M drive:%s\n", fqfn);
        result = FR_NOT_ENOUGH_CORE;
    } else
    {
        osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName = (uint8_t *)malloc(strlen(fqfn)+1);
        if(osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName == NULL)
        {
            printf("Out of memory adding filename to CP/M drive:%s\n", fqfn);
            result = FR_NOT_ENOUGH_CORE;
        } else
        {
            strcpy((char *)osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName, fqfn);
          
            // Open the file to verify it exists and is valid, also to assign the file handle.
            //
            result = (osControl.cpmDriveMap.drive[svcControl.fileNo]->File = fopen(osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName, "r+")) != NULL ? FR_OK : FR_NO_FILE;

            // If no error occurred, read in the header.
            //
            if(!result)
            {
                osControl.cpmDriveMap.drive[svcControl.fileNo]->lastTrack = 0;
                osControl.cpmDriveMap.drive[svcControl.fileNo]->lastSector = 0;
            } else
            {
                // Error opening file so free up and release slot, return error.
                free(osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName);
                osControl.cpmDriveMap.drive[svcControl.fileNo]->fileName = 0;
                free(osControl.cpmDriveMap.drive[svcControl.fileNo]);
                osControl.cpmDriveMap.drive[svcControl.fileNo] = 0;
                result = FR_NOT_ENOUGH_CORE;
            }
        }
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Method to read one of the opened, attached CPM drive images according to the Track and Sector provided.
// Inputs:
//     svcControl.trackNo  = Track to read from.
//     svcControl.sectorNo = Sector to read from.
//     svcControl.fileNo   = CPM virtual disk number, which should have been attached with the svcAddCPMDrive method.
// Outputs:
//     svcControl.sector   = 512 bytes read from file.
//
uint8_t svcReadCPMDrive(void)
{
    // Locals.
    FRESULT           result    = FR_OK;
    uint32_t          fileOffset;
    unsigned int      readSize;

    // Sanity checks.
    //
    if(svcControl.fileNo >= CPM_MAX_DRIVES || osControl.cpmDriveMap.drive[svcControl.fileNo] == NULL)
    {
        printf("svcReadCPMDrive: Illegal input values: fileNo=%d, driveMap=%08lx\n", svcControl.fileNo,  (uint32_t)osControl.cpmDriveMap.drive[svcControl.fileNo]);
        return(TZSVC_STATUS_FILE_ERROR);
    }

    // Calculate the offset into the file.
    fileOffset = ((svcControl.trackNo * CPM_SECTORS_PER_TRACK) + svcControl.sectorNo) * TZSVC_SECTOR_SIZE;

    // Seek to the correct location as directed by the track/sector.
    result = (fseek(osControl.cpmDriveMap.drive[svcControl.fileNo]->File, fileOffset, SEEK_SET) != -1) ? FR_OK : FR_DISK_ERR;
    if(!result)
        readSize = fread((char *)svcControl.sector, 1, TZSVC_SECTOR_SIZE, osControl.cpmDriveMap.drive[svcControl.fileNo]->File);
   
    // No errors but insufficient bytes read, either the image is bad or there was an error!
    if(!result && readSize != TZSVC_SECTOR_SIZE)
    {
        result = FR_DISK_ERR;
    } else
    {
        osControl.cpmDriveMap.drive[svcControl.fileNo]->lastTrack  = svcControl.trackNo;
        osControl.cpmDriveMap.drive[svcControl.fileNo]->lastSector = svcControl.sectorNo;
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Method to write to one of the opened, attached CPM drive images according to the Track and Sector provided.
// Inputs:
//     svcControl.trackNo  = Track to write into.
//     svcControl.sectorNo = Sector to write into.
//     svcControl.fileNo   = CPM virtual disk number, which should have been attached with the svcAddCPMDrive method.
//     svcControl.sector   = 512 bytes to write into the file.
// Outputs:
//
uint8_t svcWriteCPMDrive(void)
{
    // Locals.
    FRESULT           result    = FR_OK;
    uint32_t          fileOffset;
    unsigned int      writeSize;

    // Sanity checks.
    //
    if(svcControl.fileNo >= CPM_MAX_DRIVES || osControl.cpmDriveMap.drive[svcControl.fileNo] == NULL)
    {
        printf("svcWriteCPMDrive: Illegal input values: fileNo=%d, driveMap=%08lx\n", svcControl.fileNo,  (uint32_t)osControl.cpmDriveMap.drive[svcControl.fileNo]);
        return(TZSVC_STATUS_FILE_ERROR);
    }

    // Calculate the offset into the file.
    fileOffset = ((svcControl.trackNo * CPM_SECTORS_PER_TRACK) + svcControl.sectorNo) * TZSVC_SECTOR_SIZE;

    // Seek to the correct location as directed by the track/sector.
    result = (fseek(osControl.cpmDriveMap.drive[svcControl.fileNo]->File, fileOffset, SEEK_SET) != -1) ? FR_OK : FR_DISK_ERR;
    if(!result)
    {
      #if(DEBUG_ENABLED & 0x02)
        if(Z80Ctrl->debug & 0x02) 
        {
            printf("Writing offset=%08lx\n", fileOffset);
            for(uint16_t idx=0; idx < TZSVC_SECTOR_SIZE; idx++)
            {
                printf("%02x ", svcControl.sector[idx]);
                if(idx % 32 == 0)
                    printf("\n");
            }
            printf("\n");
        }
      #endif

        writeSize = fwrite((char *)svcControl.sector, 1, TZSVC_SECTOR_SIZE, osControl.cpmDriveMap.drive[svcControl.fileNo]->File);
    }
  
    // No errors but insufficient bytes written, either the image is bad or there was an error!
    if(!result && writeSize != TZSVC_SECTOR_SIZE)
    {
        result = FR_DISK_ERR;
    } else
    {
        osControl.cpmDriveMap.drive[svcControl.fileNo]->lastTrack  = svcControl.trackNo;
        osControl.cpmDriveMap.drive[svcControl.fileNo]->lastSector = svcControl.sectorNo;
    }

    // Return values: 0 - Success : maps to TZSVC_STATUS_OK
    //                1 - Fail    : maps to TZSVC_STATUS_FILE_ERROR
    return(result == FR_OK ? TZSVC_STATUS_OK : TZSVC_STATUS_FILE_ERROR);
}

// Simple method to get the service record address which is dependent upon memory mode which in turn is dependent upon software being run.
//
uint32_t getServiceAddr(void)
{
    // Locals.
    uint32_t addr       = TZSVC_CMD_STRUCT_ADDR_TZFS;
    uint8_t  memoryMode = Z80Ctrl->memoryMode;

    // If in CPM mode then set the service address accordingly.
    if(memoryMode == TZMM_CPM || memoryMode == TZMM_CPM2)
        addr = TZSVC_CMD_STRUCT_ADDR_CPM;
    
    // If in MZ700 mode then set the service address accordingly.
    if(memoryMode == TZMM_MZ700_0 || memoryMode == TZMM_MZ700_2 || memoryMode == TZMM_MZ700_3 || memoryMode == TZMM_MZ700_4)
        addr = TZSVC_CMD_STRUCT_ADDR_MZ700;

    // If in MZ2000 mode then the service address differs according to boot state.
    if(memoryMode == TZMM_MZ2000)
    {
        // Get the machine state and set the service address accordingly.
        if(z80Control.iplMode)
        {
            addr = TZSVC_CMD_STRUCT_ADDR_MZ2000_IPL;
        }
        else
        {
            addr = TZSVC_CMD_STRUCT_ADDR_MZ2000_NST;
        }
    }
    return(addr);
}


// Method to copy memory from the K64F to the Z80.
//
uint8_t copyFromZ80(uint8_t *dst, uint32_t src, uint32_t size, enum TARGETS target)
{
    // Locals.
    uint8_t    result = 0;

    // Sanity checks.
    //
    if((target == MAINBOARD && (src+size) > 0x10000) || (target == TRANZPUTER && (src+size) > TZ_MAX_Z80_MEM))
        return(1);

    // Simple copy in FusionX.
    for(uint32_t idx=0; idx < size; idx++)
    {
        // And now read the byte and store.
        *dst = readZ80Memory((uint32_t)src);
        src++;
        dst++;
    }
  
    return(result);
}

// Method to copy memory to the K64F from the Z80.
//
uint8_t copyToZ80(uint32_t dst, uint8_t *src, uint32_t size, enum TARGETS target)
{
    // Locals.
    uint8_t    result = 0;

    // Sanity checks.
    //
    if((target == MAINBOARD && (dst+size) > 0x10000) || (target == TRANZPUTER && (dst+size) > TZ_MAX_Z80_MEM))
        return(1);

    // Simple copy in FusionX.
    for(uint32_t idx=0; idx < size; idx++)
    {
        // And now write the byte and to the next address!
        writeZ80Memory((uint32_t)dst, *src, target);
        src++;
        dst++;
    }

    return(result);
}

// Method to test if the autoboot TZFS flag file exists on the SD card. If the file exists, set the autoboot flag.
//
uint8_t testTZFSAutoBoot(void)
{
    // Locals.
    uint8_t   result = 0;
    FILE      *File;

    // Detect if the autoboot tranZPUter TZFS flag is set. This is a file called TZFSBOOT in the SD root directory.
    if((File = fopen(TZFS_AUTOBOOT_FLAG, "r")) != NULL)
    {
        result = 1;
        fclose(File);
    }

    return(result);
}

// Method to load a BIOS into the tranZPUter and configure for a particular host type.
//
uint8_t loadBIOS(const char *biosFileName, uint32_t loadAddr)
{
    // Locals.
    uint8_t result = FR_OK;
   
    // Load up the given BIOS into tranZPUter memory.
    if((result=loadZ80Memory(biosFileName, 0, loadAddr, 0, 0, TRANZPUTER)) != FR_OK)
    {
        printf("Error: Failed to load %s into tranZPUter memory.\n", biosFileName);
    } else
    {
        // Change frequency to default.
        setZ80CPUFrequency(0, 4);
    }
    return(result);
}

// Method to load TZFS with an optional BIOS.
//
FRESULT loadTZFS(char *biosFile, uint32_t loadAddr)
{
    // Locals.
    FRESULT        result = 0;
    char           fqBiosFile[256 + 13];

    if(biosFile)
    {
        sprintf(fqBiosFile, "%s%s/%s", OS_BASE_DIR, TZSVC_DEFAULT_TZFS_DIR, biosFile);
        result = loadBIOS(fqBiosFile, loadAddr);
    }
    if(!result && (result=loadZ80Memory((const char *)OS_BASE_DIR TZSVC_DEFAULT_TZFS_DIR "/" MZ_ROM_TZFS, 0,      MZ_UROM_ADDR,            0x1800, 0, TRANZPUTER) != FR_OK))
    {
        printf("Error: Failed to load bank 1 of %s into tranZPUter memory.\n", MZ_ROM_TZFS);
    }
    if(!result && (result=loadZ80Memory((const char *)OS_BASE_DIR TZSVC_DEFAULT_TZFS_DIR "/" MZ_ROM_TZFS, 0x1800, MZ_BANKRAM_ADDR+0x10000, 0x1000, 0, TRANZPUTER) != FR_OK))
    {
        printf("Error: Failed to load page 2 of %s into tranZPUter memory.\n", MZ_ROM_TZFS);
    }
    if(!result && (result=loadZ80Memory((const char *)OS_BASE_DIR TZSVC_DEFAULT_TZFS_DIR "/" MZ_ROM_TZFS, 0x2800, MZ_BANKRAM_ADDR+0x20000, 0x1000, 0, TRANZPUTER) != FR_OK))
    {
        printf("Error: Failed to load page 3 of %s into tranZPUter memory.\n", MZ_ROM_TZFS);
    }
    if(!result && (result=loadZ80Memory((const char *)OS_BASE_DIR TZSVC_DEFAULT_TZFS_DIR "/" MZ_ROM_TZFS, 0x3800, MZ_BANKRAM_ADDR+0x30000, 0x1000, 0, TRANZPUTER) != FR_OK))
    {
        printf("Error: Failed to load page 4 of %s into tranZPUter memory.\n", MZ_ROM_TZFS);
    }
    return(result);
}

// Method to load the default ROMS into the tranZPUter RAM ready for start.
// If the autoboot flag is set, perform autoboot by wiping the STACK area
// of the SA1510 - this has the effect of JP 00000H.
//
void loadTranZPUterDefaultROMS(uint8_t cpuConfig)
{
    // Locals.
    FRESULT    result = 0;

    // Now load the default BIOS into memory for the host type.
    switch(z80Control.hostType)
    {
        case HW_MZ700:
            result = loadTZFS(MZ_ROM_1Z_013A_40C, MZ_MROM_ADDR);
            break;

        case HW_MZ800:
            // The MZ-800 uses a composite ROM containing the modified BIOS of the MZ-700 (1Z_013B), the IPL of the MZ-800 (9Z_504M), the CGROM for video text output
            // and the BASIC IOCS, a common code area for BASIC,
            //
            // First we load the MZ-700 compatible BIOS in page 0 to support TZFS when activated.
            result = loadTZFS(MZ_ROM_1Z_013A_40C, MZ_MROM_ADDR);

            // Next we load the MZ-800 BIOS in page 7.
            if(!result)
            {
              #if(DEBUG_ENABLED & 0x2)
                if(Z80Ctrl->debug & 0x02) printf("Loading 1Z_013B\n");
              #endif

                //result = loadBIOS(MZ_ROM_1Z_013B,      MZ800, MZ_800_MROM_ADDR);
                result = loadBIOS(MZ_ROM_1Z_013B,      MZ_800_MROM_ADDR);
            }

            // Load up the CGROM into RAM.
            //printf("Loading CGROM\n");
            //result = loadBIOS(MZ_ROM_800_CGROM,   MZ800, MZ_800_CGROM_ADDR);

            // Next we load the modified 9Z-504M - modified to add an option to start TZFS.
            if(!result)
            {
              #if(DEBUG_ENABLED & 0x2)
                if(Z80Ctrl->debug & 0x02) printf("Loading 9Z_504M\n");
              #endif
                result = loadBIOS(MZ_ROM_9Z_504M,      MZ_800_IPL_ADDR);
            }
           
            // Finally we load the common IOCS.
            if(!result)
            {
              #if(DEBUG_ENABLED & 0x2)
                if(Z80Ctrl->debug & 0x02) printf("Loading BASIC IOCS\n");
              #endif
                result = loadBIOS(MZ_ROM_800_IOCS,     MZ_800_IOCS_ADDR);
            }
            break;

        case HW_MZ80B:
            //result = loadBIOS(MZ_ROM_MZ80B_IPL,        MZ80B,  MZ_MROM_ADDR);
            result = loadBIOS(MZ_ROM_MZ80B_IPL,        MZ_MROM_ADDR);
            break;

        case HW_MZ2000:
            // Load up the IPL BIOS at least try even if the CGROM failed.
          #if(DEBUG_ENABLED & 0x2)
            if(Z80Ctrl->debug & 0x02) printf("Loading IPL\n");
          #endif
            result = loadBIOS(MZ_ROM_MZ2000_IPL_TZPU,  MZ_MROM_ADDR);
            if(result != FR_OK)
            {
                printf("Error: Failed to load IPL ROM %s into tranZPUter memory.\n", MZ_ROM_MZ2000_IPL_TZPU);
            }
            break;

        case HW_MZ80A:
        case HW_UNKNOWN:
        default:
            result = loadTZFS(MZ_ROM_SA1510_40C, MZ_MROM_ADDR);
            break;
    }

    // If all was ok loading the roms, complete the startup.
    //
    if(!result)
    {
        // Check to see if autoboot is needed.
        osControl.tzAutoBoot = testTZFSAutoBoot();
    
        // If autoboot flag set, force a restart to the ROM which will call User ROM startup code.
        if(osControl.tzAutoBoot)
        {
            if(z80Control.hostType == HW_MZ800)
            {
                // On the MZ-800, once all firmware loaded, set the memory mode to MZ-800 and reset to enable processing on the tranZPUter memory rather than host memory.
                reqResetZ80(TZMM_MZ800);
            }
            // MZ-2000 stays in original mode, user will start TZFS via key press in modified IPL bios.
            else if(z80Control.hostType == HW_MZ2000)
            {
                reqResetZ80(TZMM_MZ2000);
            }
            else
            {
                // Set the memory model to BOOT so we can bootstrap TZFS.
                reqResetZ80(TZMM_BOOT);
            }
        }
    } else
    {
        printf("Firmware load failure\n");
    }
    return;
}

// Method to process a service request from the z80 running TZFS or CPM.
//
void processServiceRequest(void)
{
    // Locals.
    //
    uint8_t    refreshCacheDir = 0;
    uint8_t    status          = 0;
    uint8_t    doExit          = 0;
    uint8_t    doReset         = 0;
    uint32_t   actualFreq;
    uint32_t   copySize        = TZSVC_CMD_STRUCT_SIZE;
    uint32_t   idx             = 0;

    // Update the service control record address according to memory mode.
    //
    z80Control.svcControlAddr = getServiceAddr();

    // Get the command and associated parameters.
    copyFromZ80((uint8_t *)&svcControl, z80Control.svcControlAddr, TZSVC_CMD_SIZE, TRANZPUTER);

    // Need to get the remainder of the data for the write operations.
    if(svcControl.cmd == TZSVC_CMD_WRITEFILE || svcControl.cmd == TZSVC_CMD_NEXTWRITEFILE || svcControl.cmd == TZSVC_CMD_WRITESDDRIVE || svcControl.cmd == TZSVC_CMD_SD_WRITESECTOR)
    {
        copyFromZ80((uint8_t *)&svcControl.sector, z80Control.svcControlAddr+TZSVC_CMD_SIZE, TZSVC_SECTOR_SIZE, TRANZPUTER);
    }

    //memoryDump((uint32_t)&svcControl, sizeof(svcControl), 5, 8, 0, 0);
    // Check this is a valid request.
    if(svcControl.result == TZSVC_STATUS_REQUEST)
    {
        // Set status to processing. Z80 can use this to decide if the K64F received its request after a given period of time.
        setZ80SvcStatus(TZSVC_STATUS_PROCESSING);

        //memoryDump((uint32_t)&svcControl, sizeof(svcControl), 5, 8, 0, 0);
        // Action according to command given.
        //
        switch(svcControl.cmd)
        {
            // Open a directory stream and return the first block.
            case TZSVC_CMD_READDIR:
                status=svcReadDirCache(TZSVC_OPEN, svcControl.fileType);
                break;

            // Read the next block in the directory stream.
            case TZSVC_CMD_NEXTDIR:
                status=svcReadDirCache(TZSVC_NEXT, svcControl.fileType);
                break;

            // Open a file stream and return the first block.
            case TZSVC_CMD_READFILE:
                status=svcReadFile(TZSVC_OPEN, svcControl.fileType);
                break;

            // Read the next block in the file stream.
            case TZSVC_CMD_NEXTREADFILE:
                status=svcReadFile(TZSVC_NEXT, svcControl.fileType);
                break;

            // Create a file for data write.
            case TZSVC_CMD_WRITEFILE:
                status=svcWriteFile(TZSVC_OPEN, svcControl.fileType);
                break;
                  
            // Write a block of data to an open file.
            case TZSVC_CMD_NEXTWRITEFILE:
                status=svcWriteFile(TZSVC_NEXT, svcControl.fileType);
                break;

            // Close an open dir/file.
            case TZSVC_CMD_CLOSE:
                svcReadDir(TZSVC_CLOSE, svcControl.fileType);
                svcReadFile(TZSVC_CLOSE, svcControl.fileType);
                svcWriteFile(TZSVC_CLOSE, svcControl.fileType);
               
                // Only need to copy the command section back to the Z80 for a close operation.
                //
                copySize = TZSVC_CMD_SIZE;
                break;

            // Load a file directly into target memory.
            case TZSVC_CMD_LOADFILE:
                status=svcLoadFile(svcControl.fileType);
                break;
              
            // Save a file directly from target memory.
            case TZSVC_CMD_SAVEFILE:
                status=svcSaveFile(svcControl.fileType);
                refreshCacheDir = 1;
                break;
              
            // Erase a file from the SD Card.
            case TZSVC_CMD_ERASEFILE:
                status=svcEraseFile(svcControl.fileType);
                refreshCacheDir = 1;
                break;

            // Change active directory. Do this immediately to validate the directory name given.
            case TZSVC_CMD_CHANGEDIR:
                status=svcCacheDir((const char *)svcControl.directory, svcControl.fileType, 0);
                break;

            // Load the 40 column version of the default host bios into memory.
            case TZSVC_CMD_LOAD40ABIOS:
                loadBIOS(MZ_ROM_SA1510_40C, MZ_MROM_ADDR);
               
                // Set the frequency of the CPU if we are emulating the hardware.
                if(z80Control.hostType != HW_MZ80A)
                {
                    // Change frequency to match Sharp MZ-80A
                    setZ80CPUFrequency(MZ_80A_CPU_FREQ, 1);
                }
                break;
              
            // Load the 80 column version of the default host bios into memory.
            case TZSVC_CMD_LOAD80ABIOS:
                loadBIOS(MZ_ROM_SA1510_80C, MZ_MROM_ADDR);
               
                // Set the frequency of the CPU if we are emulating the hardware.
                if(z80Control.hostType != HW_MZ80A)
                {
                    // Change frequency to match Sharp MZ-80A
                    setZ80CPUFrequency(MZ_80A_CPU_FREQ, 1);
                }
                break;
               
            // Load the 40 column MZ700 1Z-013A bios into memory for compatibility switch.
            case TZSVC_CMD_LOAD700BIOS40:
                loadBIOS(MZ_ROM_1Z_013A_40C, MZ_MROM_ADDR);

                // Set the frequency of the CPU if we are emulating the hardware.
                if(z80Control.hostType != HW_MZ700)
                {
                    // Change frequency to match Sharp MZ-700
                    setZ80CPUFrequency(MZ_700_CPU_FREQ, 1);
                }
                break;

            // Load the 80 column MZ700 1Z-013A bios into memory for compatibility switch.
            case TZSVC_CMD_LOAD700BIOS80:
                loadBIOS(MZ_ROM_1Z_013A_80C, MZ_MROM_ADDR);
              
                // Set the frequency of the CPU if we are emulating the hardware.
                if(z80Control.hostType != HW_MZ700)
                {
                    // Change frequency to match Sharp MZ-700
                    setZ80CPUFrequency(MZ_700_CPU_FREQ, 1);
                }
                break;
              
            // Load the MZ800 9Z-504M bios into memory for compatibility switch.
            case TZSVC_CMD_LOAD800BIOS:
                loadBIOS(MZ_ROM_9Z_504M,  MZ_MROM_ADDR);

                // Set the frequency of the CPU if we are emulating the hardware.
                if(z80Control.hostType != HW_MZ800)
                {
                    // Change frequency to match Sharp MZ-800
                    setZ80CPUFrequency(MZ_800_CPU_FREQ, 1);
                }
                break;
               
            // Load the MZ-80B IPL ROM into memory for compatibility switch.
            case TZSVC_CMD_LOAD80BIPL:
                loadBIOS(MZ_ROM_MZ80B_IPL, MZ_MROM_ADDR);
              
                // Set the frequency of the CPU if we are emulating the hardware.
                if(z80Control.hostType != HW_MZ80B)
                {
                    // Change frequency to match Sharp MZ-80B
                    setZ80CPUFrequency(MZ_80B_CPU_FREQ, 1);
                }
                break;
               
            // Load the MZ-2000 IPL ROM into memory for compatibility switch.
            case TZSVC_CMD_LOAD2000IPL:
                loadBIOS(MZ_ROM_MZ2000_IPL, MZ_MROM_ADDR);
              
                // Set the frequency of the CPU if we are emulating the hardware.
                if(z80Control.hostType != HW_MZ2000)
                {
                    // Change frequency to match Sharp MZ-80B
                    setZ80CPUFrequency(MZ_2000_CPU_FREQ, 1);
                }
                break;

            // Load TZFS upon request. This service is for the MZ-80B/MZ-2000 which dont have a monitor BIOS installed and TZFS isnt loaded upon reset but rather through user request.
            case TZSVC_CMD_LOADTZFS:
                switch(z80Control.hostType)
                {
                    case HW_MZ80B:
                        break;

                    case HW_MZ2000:
                        // Load TZFS and a modified 1Z-013A MZ700 Monitor to provide an interactive TZFS session during IPL mode.
                        if(loadTZFS(MZ_ROM_1Z_013A_2000, MZ_MROM_ADDR) == FR_OK)
                        {
                            // Request a cold IPL start with no ROM load, this will invoke the loaded TZFS as the BOOT IPL.
                            z80Control.blockResetActions = 1;
                        }
                        break;

                    // Default, ie. not a valid host, is to do nothing.
                    default:
                        break;
                }
                break;

            // Load the CPM CCP+BDOS from file into the address given.
            case TZSVC_CMD_LOADBDOS:
                // Flush out drives prior to BDOS reload.
                for(idx=0; idx < CPM_MAX_DRIVES; idx++)
                {
                    if(osControl.cpmDriveMap.drive[idx] == NULL || osControl.cpmDriveMap.drive[svcControl.fileNo]->File == NULL)
                        continue;

                    // Flush out drive contents.
                    fflush(osControl.cpmDriveMap.drive[svcControl.fileNo]->File);
                }
                if((status=loadZ80Memory((const char *)osControl.lastFile, MZF_HEADER_SIZE, svcControl.loadAddr+0x40000, svcControl.loadSize, 0, TRANZPUTER)) != FR_OK)
                {
                    printf("Error: Failed to load BDOS:%s into tranZPUter memory.\n", (char *)osControl.lastFile);
                }
                break;

            // Add a CP/M disk to the system for read/write access by CP/M on the Sharp MZ80A.
            //
            case TZSVC_CMD_ADDSDDRIVE:
                status=svcAddCPMDrive();
                break;
             
            // Read an assigned CP/M drive sector giving an LBA address and the drive number.
            //
            case TZSVC_CMD_READSDDRIVE:
                status=svcReadCPMDrive();
                break;
              
            // Write a sector to an assigned CP/M drive giving an LBA address and the drive number.
            //
            case TZSVC_CMD_WRITESDDRIVE:
                status=svcWriteCPMDrive();
             
                // Only need to copy the command section back to the Z80 for a write operation.
                //
                copySize = TZSVC_CMD_SIZE;
                break;

            // Switch to the mainboard frequency (default).
            case TZSVC_CMD_CPU_BASEFREQ:
                setZ80CPUFrequency(0, 4);
                break;

            // Switch to the alternate frequency managed by the K64F counters.
            case TZSVC_CMD_CPU_ALTFREQ:
                setZ80CPUFrequency(0, 3);
                break;

            // Set the alternate frequency. The TZFS command provides the frequency in KHz so multiply up to Hertz before changing.
            case TZSVC_CMD_CPU_CHGFREQ:
                actualFreq = setZ80CPUFrequency(svcControl.cpuFreq * 1000, 1);
                svcControl.cpuFreq = (uint16_t)(actualFreq / 1000);
                break;

            // Switch the hardware to select the hard Z80 CPU. This involves the switch then a reset procedure.
            //
            case TZSVC_CMD_CPU_SETZ80:
                printf("Switch to Z80 unsupported\n");
               
                // Set a reset event so that the ROMS are reloaded and the Z80 set.
                z80Control.resetEvent = 1;
                break;

            // Switch the hardware to select the soft T80 CPU. This involves the switch.
            //
            case TZSVC_CMD_CPU_SETT80:
                printf("Switch to T80 unsupported\n");
              
                // Set a reset event so that the ROMS are reloaded and a reset occurs.
                z80Control.resetEvent = 1;
                break;
               
            // Switch the hardware to select the soft ZPU Evolution CPU. This involves the switch.
            //
            case TZSVC_CMD_CPU_SETZPUEVO:
                printf("Switch to EVO unsupported\n");
              
                // Set a reset event so that the ROMS are reloaded and a reset occurs.
                z80Control.resetEvent = 1;
                break;

            // Not yet supported on the FusionX!
            // 
            case TZSVC_CMD_EMU_SETMZ80K:
            case TZSVC_CMD_EMU_SETMZ80C:
            case TZSVC_CMD_EMU_SETMZ1200:
            case TZSVC_CMD_EMU_SETMZ80A:
            case TZSVC_CMD_EMU_SETMZ700:
            case TZSVC_CMD_EMU_SETMZ1500:
            case TZSVC_CMD_EMU_SETMZ800:
            case TZSVC_CMD_EMU_SETMZ80B:
            case TZSVC_CMD_EMU_SETMZ2000:
            case TZSVC_CMD_EMU_SETMZ2200:
            case TZSVC_CMD_EMU_SETMZ2500:
                printf("Error: Unsupported Emulation feature.\n");
                break;

            // Raw direct access to the SD card. This request initialises the requested disk - if shared with this I/O processor then dont perform
            // physical initialisation.
            case TZSVC_CMD_SD_DISKINIT:
                // No logic needed as the K64F initialises the underlying drive and the host accesses, via a mapping table, the 2-> partitions.
                break;

            // Raw read access to the SD card.
            //
            case TZSVC_CMD_SD_READSECTOR:
                printf("Error: Unsupported Raw SD Read feature.\n");
                break;

            // Raw write access to the SD card.
            //
            case TZSVC_CMD_SD_WRITESECTOR:
                printf("Error: Unsupported Raw SD Write feature.\n");
                break;

            // Command to exit from TZFS and return machine to original mode.
            case TZSVC_CMD_EXIT:
                // Disable secondary frequency.
                setZ80CPUFrequency(0, 4);

                // Clear the stack and monitor variable area as DRAM wont have been refreshed so will contain garbage.
                fillZ80Memory(MZ_MROM_STACK_ADDR, MZ_MROM_STACK_SIZE, 0x00, MAINBOARD);

                // Set to refresh mode just in case the I/O processor performs any future action in silent mode!
                z80Control.disableRefresh = 0;
               
                // Set a flag to perform the physical reset. This is necessary as the service routine holds the Z80 bus and there is an issue with the Z80 if you reset whilst
                // the bus is being held (it sometimes misses the first instruction).
                doExit = 1;
                break;

            default:
                printf("WARNING: Unrecognised command:%02x\n", svcControl.cmd);
                status=TZSVC_STATUS_BAD_CMD;
                break;
        }
    } else
    {
        // Indicate error condition.
        status=TZSVC_STATUS_BAD_REQ;
    }
  
    // Update the status in the service control record then copy it across to the Z80.
    //
    svcControl.result = status;
    copyToZ80(z80Control.svcControlAddr, (uint8_t *)&svcControl, copySize, TRANZPUTER);

    //memoryDump((uint32_t)&svcControl, sizeof(svcControl), 5, 8, 0, 0);

    // Need to refresh the directory? Do this at the end of the routine so the Sharp MZ80A isnt held up.
    if(refreshCacheDir)
        svcCacheDir((const char *)svcControl.directory, svcControl.fileType, 1);

    // If the doExit flag is set it means the CPU should be set to original memory mode and reset.
    if(doExit == 1)
    {
        // Now reset the machine so everything starts as power on.
        reqResetZ80(TZMM_ORIG);
    }
    // If the doReset force a reset of the CPU.
    if(doReset == 1)
    {
        // Reset the machine requested by earlier action.
        reqResetZ80(TZMM_BOOT);
    }
    return;
}

// Handler for the SIGIO signal used by the Z80 Driver when a service request is required.
//
void z80ServiceRequest(int signalNo)
{
    // Locals.

    // Simply call the handler to process the request.
    processServiceRequest();
    return;
}

// Handler for the SIGUSR1 signal which indicates the Z80 has an external reset applied.
//
void z80ResetRequest(int signalNo)
{
    // Locals.
    uint32_t status;

    // Stop the Z80 in case it is running.
    stopZ80(TZMM_BOOT);

    // Setup memory on Z80 to default.
    loadTranZPUterDefaultROMS(CPUMODE_SET_Z80);

    // Setup the defaults
    svcSetDefaults(MZF);

    // Cache initial directory.
    svcCacheDir(TZSVC_DEFAULT_MZF_DIR, MZF, 1);
  
    // Start the Z80 after initialisation of the memory.
    startZ80(TZMM_BOOT);
    return;
}

// Handler for the HUP, INT, QUIT or TERM signals. On receipt, start a shutdown request.
void shutdownRequest(int signalNo)
{
    // Locals.

    // Terminate signals flag the request by clearing runControl.
    if(signalNo == SIGHUP || signalNo == SIGINT || signalNo == SIGQUIT || signalNo == SIGTERM)
    {
        printf("Terminate request.\n");
        runControl = 0;
    }
    return;
}


// Output usage screen. So mamy commands you do need to be prompted!!
void showArgs(char *progName, struct optparse *options)
{
    printf("%s %s %s %s\n\n", progName, VERSION, COPYRIGHT, AUTHOR);
    printf("Synopsis:\n");
    printf("%s --help                                                                   # This help screen.\n", progName);
    return;
}


// This is a daemon process, process arguments, initialise logic and enter a loop waiting for signals to arrive.
// The signals indicate an interrupt (service request), shutwon or indicate that the Z80 has had a reset request.
// Loop until a terminate signal comes in.
int main(int argc, char *argv[])  
{  
    char       buff[64];    
    char       cmd[64]           = { 0 };
    char       fileName[256]     = { 0 };
    char       devName[32]       = { 0 };
    int        opt;
    uint32_t   hexData           = 0;
    long       fileOffset        = -1;
    long       fileLen           = -1;
    int        helpFlag          = 0;
    int        verboseFlag       = 0;
    uint8_t    memoryType        = 0;
    struct ioctlCmd ioctlCmd;

    // Define parameters to be processed.
    struct optparse options;
    static struct optparse_long long_options[] =
    {
        {"help",          'h',  OPTPARSE_NONE},
        {"verbose",       'v',  OPTPARSE_NONE},
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
                sscanf(options.optarg, "0x%08x", &hexData);
                printf("Hex data:%08x\n", hexData);
                break;

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
    z80Control.fdZ80 = open(DEVICE_FILENAME, O_RDWR|O_NDELAY);  
    if(z80Control.fdZ80 >= 0)
    { 
        Z80Ctrl = (t_Z80Ctrl *)mmap(0,  sizeof(t_Z80Ctrl),  PROT_READ | PROT_WRITE,  MAP_SHARED,  z80Control.fdZ80,  0);  

        if(Z80Ctrl == (void *)-1)
        {
            printf("Failed to attach to the Z80 Control structure, cannot continue, exiting....\n");
            close(z80Control.fdZ80);  
            exit(1);
        }
        Z80RAM = (uint8_t *)mmap(0, Z80_VIRTUAL_RAM_SIZE, PROT_READ | PROT_WRITE,  MAP_SHARED,  z80Control.fdZ80,  0);  
        if(Z80RAM == (void *)-1)
        {
            printf("Failed to attach to the Z80 RAM, cannot continue, exiting....\n");
            close(z80Control.fdZ80);  
            exit(1);
        }
        Z80ROM = (uint8_t *)mmap(0, Z80_VIRTUAL_ROM_SIZE+0x1000, PROT_READ | PROT_WRITE,  MAP_SHARED,  z80Control.fdZ80,  0);  
        if(Z80ROM == (void *)-1)
        {
            printf("Failed to attach to the Z80 ROM, cannot continue, exitting....\n");
            close(z80Control.fdZ80);  
            exit(1);
        }
    } else
    {
        printf("Failed to open the Z80 Driver, exiting...\n");
        exit(1);
    }
  
    // Register the service request handler.
    signal(SIGIO, z80ServiceRequest);

    // Register the reset request handler.
    signal(SIGUSR1, z80ResetRequest);

    // Register close down handlers.
    signal(SIGHUP,  shutdownRequest);
    signal(SIGINT,  shutdownRequest);
    signal(SIGQUIT, shutdownRequest);
    signal(SIGTERM, shutdownRequest);

    // Initiate a reset which loads the default roms and caches SD directory.
    z80ResetRequest(0);

    // Enter a loop, process requests as the come in and terminate if requested by signals.
    runControl = 1;
    while(runControl)
    {
        usleep(1000);
    }

    // Unmap shared memory and close the device.
    munmap(Z80Ctrl, sizeof(t_Z80Ctrl));  
    close(z80Control.fdZ80);
   
    return(0);
}

#ifdef __cplusplus
}
#endif
