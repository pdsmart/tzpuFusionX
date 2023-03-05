/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            sharpbiter.c
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Sharp Host Arbiter
//                  This daemon application is responsible for switching the FusionX between modes via
//                  host keyboard hotkeys. It allows the host to assume a persona based on user
//                  requirements.
//                  Currently, the following persona's can be invoked:
//                  1. Original host mode (ie. the host behaves as original, no extensions).
//                  2. Original + Rom Filing System. Virtual RFS is installed and the RFS monitor invoked.
//                  3. Original + TZFS. Virtual tranZPUter SW is installed and the TZFS monitor invoked.
//                  4. Linux. Host will run as a smart terminal front to the FusionX Linux OS.
//
//                  The daemon listens for signals sent by the current active process. The signal will
//                  indicate required persona and this daemon will invoke it accordingly.
//
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Feb 2023 v1.0  - Initial write.
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

#define VERSION                               "1.00"
#define AUTHOR                                "P.D.Smart"
#define COPYRIGHT                             "(c) 2018-23"
  
// Getopt_long is buggy so we use optparse.
#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API                          static
#include "optparse.h"

// IOCTL commands. Passed from user space using the IOCTL method to command the driver to perform an action.
#define IOCTL_CMD_FETCH_HOTKEY                _IOW('f', 'f', int32_t *)
#define IOCTL_CMD_SUSPEND_IO                  _IOW('s', 's', int32_t *)
#define IOCTL_CMD_RESUME_IO                   _IOW('r', 'r', int32_t *)

// Device driver name.
#define Z80_DEVICE_FILENAME                   "/dev/z80drv"
#define TTY_DEVICE_FILENAME                   "/dev/ttymz2"              // The Sharp MZ TTY is accessed via port 2, port 0 = host tty, 1 = SSD202 framebuffer tty.

// Structure to maintain all the control and management variables of the arbiter.
//
typedef struct {
    int                              fdZ80;                              // Handle to the Z80 kernel driver.
    int                              fdTTY;                              // Handle to the TTY kernel driver.
    uint8_t                          hotkey;                             // New or last hotkey received.
    uint8_t                          newHotkey;                          // Flag to indicate a hotkey has arrived.
    uint8_t                          runControl;                         // Run control for the daemon, 1 = run, 0 = terminate.
} t_ArbiterControl;

// Global scope variables.
//
static t_ArbiterControl      arbCtrl;

// Shared memory between this process and the Z80 driver.
static t_Z80Ctrl             *Z80Ctrl = NULL;
static uint8_t               *Z80RAM = NULL;
static uint8_t               *Z80ROM = NULL;

// Method to reset the Z80 CPU.
//
void reqResetZ80(uint8_t memoryMode)
{
    // Locals.
    //
    struct ioctlCmd ioctlCmd;

    // Send command to driver to reset Z80.
    ioctlCmd.cmd = IOCTL_CMD_Z80_RESET;
    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
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
    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
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
    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
}

// Handler for the SIGUSR1 signal used by the Z80 Driver when a hotkey is detected.
//
void z80Request(int signalNo)
{
    // Locals.

    //printf("Received SIGUSR1 from Z80 Driver\n");

    // If an existing hotkey has not been processed, wait.
    if(arbCtrl.newHotkey)
    {
        usleep(1000);
    }
    arbCtrl.hotkey = Z80Ctrl->keyportHotKey;
    arbCtrl.newHotkey = 1;
    return;
}

// Handler for the SIGUSR2 signal used by the TTY Driver when a hotkey is detected.
//
void ttyRequest(int signalNo)
{
    // Locals.
    int32_t         result;
   
    //printf("Received SIGUSR2 from TTY Driver\n");

    // Send command to TTY driver to fetch hotkey.
    ioctl(arbCtrl.fdTTY, IOCTL_CMD_FETCH_HOTKEY, &result);

    // If an existing hotkey has not been processed, wait.
    if(arbCtrl.newHotkey)
    {
        usleep(1000);
    }
    arbCtrl.hotkey = (uint8_t)result;
    arbCtrl.newHotkey = 1;
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
        arbCtrl.newHotkey  = 0;
        arbCtrl.runControl = 0;
    }
    return;
}

// Output usage screen. So mamy commands you do need to be prompted!!
void showArgs(char *progName, struct optparse *options)
{
    printf("%s %s %s %s\n\n", progName, VERSION, COPYRIGHT, AUTHOR);
    printf("Synopsis:\n");
    printf("%s --help              # This help screen.\n", progName);
    return;
}


// This is a daemon process, process arguments, initialise logic and enter a loop waiting for signals to arrive.
// The signals indicate the active process has detected a hotkey combination and this daemon needs to process it
// to invoke the required FusionX persona.
int main(int argc, char *argv[])  
{  
    uint32_t   hexData           = 0;
    int        opt               = 0;
    int        verboseFlag       = 0;
    int32_t    result;
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
                showArgs(argv[0], &options);
                return(1);

            // Unrecognised, show synopsis.
            case '?':
                showArgs(argv[0], &options);
                printf("%s: %s\n", argv[0], options.errmsg);
                return(1);
        }  
    }

    // Open the z80drv driver and attach to its shared memory, basically the Z80 control structure which includes the virtual Z80 memory.
    arbCtrl.fdZ80 = open(Z80_DEVICE_FILENAME, O_RDWR|O_NDELAY);  
    if(arbCtrl.fdZ80 >= 0)
    { 
        Z80Ctrl = (t_Z80Ctrl *)mmap(0,  sizeof(t_Z80Ctrl),  PROT_READ | PROT_WRITE,  MAP_SHARED,  arbCtrl.fdZ80,  0);  
        if(Z80Ctrl == (void *)-1)
        {
            printf("Failed to attach to the Z80 Control structure, cannot continue, exiting....\n");
            close(arbCtrl.fdZ80);  
            exit(1);
        }
        Z80RAM = (uint8_t *)mmap(0, Z80_VIRTUAL_RAM_SIZE, PROT_READ | PROT_WRITE,  MAP_SHARED,  arbCtrl.fdZ80,  0);  
        if(Z80RAM == (void *)-1)
        {
            printf("Failed to attach to the Z80 RAM, cannot continue, exiting....\n");
            close(arbCtrl.fdZ80);  
            exit(1);
        }
        Z80ROM = (uint8_t *)mmap(0, Z80_VIRTUAL_ROM_SIZE+0x1000, PROT_READ | PROT_WRITE,  MAP_SHARED,  arbCtrl.fdZ80,  0);  
        if(Z80ROM == (void *)-1)
        {
            printf("Failed to attach to the Z80 ROM, cannot continue, exitting....\n");
            close(arbCtrl.fdZ80);  
            exit(1);
        }
    } else
    {
        printf("Failed to open the Z80 Driver, exiting...\n");
        exit(1);
    }

    arbCtrl.fdTTY = open(TTY_DEVICE_FILENAME, O_RDWR|O_NDELAY);  
    if(arbCtrl.fdTTY >= 0)
    { 
        printf("Opened device:%s\n", TTY_DEVICE_FILENAME);
    } else
    {
        printf("Failed to open the TTY Driver, exiting...\n");
        exit(1);
    }
  
    // Register the request handler for processing signals from the z80drv driver.
    signal(SIGUSR1, z80Request);

    // Register the request handler for processing signals from the ttymz driver.
    signal(SIGUSR2, ttyRequest);

    // Register close down handlers.
    signal(SIGHUP,  shutdownRequest);
    signal(SIGINT,  shutdownRequest);
    signal(SIGQUIT, shutdownRequest);
    signal(SIGTERM, shutdownRequest);

    // Enter a loop, process requests as the come in and terminate if requested by signals.
    arbCtrl.runControl = 1;
    while(arbCtrl.runControl)
    {
        if(arbCtrl.newHotkey)
        {
            printf("New hotkey:%02x\n", arbCtrl.hotkey);
            switch(arbCtrl.hotkey)
            {
                case HOTKEY_ORIGINAL:
                    // Disable TTY I/O.
                    ioctl(arbCtrl.fdTTY, IOCTL_CMD_SUSPEND_IO, &result);
                   
                    // Stop the Z80.
                    ioctlCmd.cmd = IOCTL_CMD_Z80_STOP;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

                    // Remove drivers. Remove all because an external event could have changed last configured driver.
                    //
                    ioctlCmd.cmd = IOCTL_CMD_DEL_DEVICE;
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS80;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS40;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_TZPU;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     

                    // Reset and start the Z80.
                    ioctlCmd.cmd = IOCTL_CMD_Z80_RESET;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
                    ioctlCmd.cmd = IOCTL_CMD_Z80_START;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
                    break;

                case HOTKEY_RFS80:
                case HOTKEY_RFS40:
                    // Disable TTY I/O.
                    ioctl(arbCtrl.fdTTY, IOCTL_CMD_SUSPEND_IO, &result);
                  
                    // Stop the Z80.
                    ioctlCmd.cmd = IOCTL_CMD_Z80_STOP;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

                    // Remove drivers. Remove all because an external event could have changed last configured driver.
                    //
                    ioctlCmd.cmd = IOCTL_CMD_DEL_DEVICE;
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS80;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS40;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_TZPU;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     

                    // Add in the required driver.
                    ioctlCmd.cmd = IOCTL_CMD_ADD_DEVICE;
                    ioctlCmd.vdev.device = (arbCtrl.hotkey == HOTKEY_RFS80) ? VIRTUAL_DEVICE_RFS80 : VIRTUAL_DEVICE_RFS40;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     

                    // Reset and start the Z80.
                    ioctlCmd.cmd = IOCTL_CMD_Z80_RESET;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
                    ioctlCmd.cmd = IOCTL_CMD_Z80_START;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
                    break;

                case HOTKEY_TZFS:
                    // Disable TTY I/O.
                    ioctl(arbCtrl.fdTTY, IOCTL_CMD_SUSPEND_IO, &result);

                    // Stop the Z80.
                    ioctlCmd.cmd = IOCTL_CMD_Z80_STOP;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

                    // Remove drivers. Remove all because an external event could have changed last configured driver.
                    //
                    ioctlCmd.cmd = IOCTL_CMD_DEL_DEVICE;
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS80;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS40;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_TZPU;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                  
                    // Add in the required driver.
                    ioctlCmd.cmd = IOCTL_CMD_ADD_DEVICE;
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_TZPU;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                   
                    // Reset and start the Z80.
                    ioctlCmd.cmd = IOCTL_CMD_Z80_RESET;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
                 //   ioctlCmd.cmd = IOCTL_CMD_Z80_START;
                 //   ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);
                    break;

                case HOTKEY_LINUX:
                    // Stop the Z80.
                    ioctlCmd.cmd = IOCTL_CMD_Z80_STOP;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);

                    // Remove drivers. Remove all because an external event could have changed last configured driver.
                    //
                    ioctlCmd.cmd = IOCTL_CMD_DEL_DEVICE;
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS80;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_RFS40;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     
                    ioctlCmd.vdev.device = VIRTUAL_DEVICE_TZPU;
                    ioctl(arbCtrl.fdZ80, IOCTL_CMD_SEND, &ioctlCmd);                     

                    // Enable TTY I/O.
                    ioctl(arbCtrl.fdTTY, IOCTL_CMD_RESUME_IO, &result);
                    break;

                default:
                    break;

            }
            arbCtrl.newHotkey = 0;
        }
        usleep(1000);
    }

    // Unmap shared memory and close the device.
    munmap(Z80Ctrl, sizeof(t_Z80Ctrl));  
    close(arbCtrl.fdZ80);
    close(arbCtrl.fdTTY);
   
    return(0);
}

#ifdef __cplusplus
}
#endif
