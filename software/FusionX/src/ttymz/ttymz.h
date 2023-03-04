/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            ttymz.h
// Created:         Feb 2023
// Author(s):       Philip Smart
// Description:     Sharp MZ TTY
//                  This file contains the definitions required by the linux tty device driver ttymz.c.
//                  This driver forms part of the FusionX developments and allows a user sitting at the
//                  Sharp MZ Console to access the underlying FusionX Linux SOM.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Feb 2023 - v1.0  Initial write of the Sharp MZ tty driver software.
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
#ifndef TTYMZ_H
#define TTYMZ_H

// Constants.
#define DRIVER_LICENSE                        "GPL"
#define DRIVER_AUTHOR                         "Philip D Smart"
#define DRIVER_DESCRIPTION                    "Sharp MZ TTY Driver"
#define DRIVER_VERSION                        "v1.01"
#define DRIVER_VERSION_DATE                   "Mar 2023"
#define DRIVER_COPYRIGHT                      "(C) 2018-2023"
#define DEVICE_NAME                           "ttymz"
#define DRIVER_NAME                           "SharpMZ_tty"
#define DEBUG_ENABLED                         0                                   // 0 = disabled, 1 .. debug level.

// Fake UART values
#define MCR_DTR                               0x01
#define MCR_RTS                               0x02
#define MCR_LOOP                              0x04
#define MSR_CTS                               0x08
#define MSR_CD                                0x10
#define MSR_RI                                0x20
#define MSR_DSR                               0x40

// IOCTL commands. Passed from user space using the IOCTL method to command the driver to perform an action.
#define IOCTL_CMD_SUSPEND_IO                  _IOW('s', 's', int32_t *)
#define IOCTL_CMD_RESUME_IO                   _IOW('r', 'r', int32_t *)

#define SHARPMZ_TTY_MAJOR                     240                                 // Experimental range
#define SHARPMZ_TTY_MINORS                    2                                   // Assign 2 devices, Sharp VRAM and SigmaStar SSD202 Framebuffer.

// Macros.
#define from_timer(var, callback_timer, timer_fieldname) container_of(callback_timer, typeof(*var), timer_fieldname)
#define PRINT_PROC_START()                    do { pr_info("Start: %s\n", __func__); } while (0)
#define PRINT_PROC_EXIT()                     do { pr_info("Finish: %s\n", __func__); } while (0)

typedef struct {
    struct tty_struct                         *tty;                               // pointer to the tty for this device
    int                                       open_count;                         // number of times this port has been opened
    struct mutex                              mutex;                              // locks this structure
    struct timer_list                         timerKeyboard;                      // Keyboard sweep timer.
    struct timer_list                         timerDisplay;                       // Display service timer.

    /* for tiocmget and tiocmset functions */
    int                                       msr;                                // MSR shadow
    int                                       mcr;                                // MCR shadow

    /* for ioctl fun */
    struct serial_struct                      serial;
    wait_queue_head_t                         wait;
    struct async_icount                       icount;
} t_TTYMZ;

#if(DEBUG_ENABLED != 0)
struct debug {
    uint8_t                                   level;
};
#endif
struct ioctlCmd {
    int32_t                                   cmd;
    union {
      #if(DEBUG_ENABLED != 0)
        struct debug                          debug;
      #endif
    };
};

// Prototypes.

#endif
