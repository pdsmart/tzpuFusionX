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
#define ARBITER_NAME                          "sharpbiter"

// Fake UART values
#define MCR_DTR                               0x01
#define MCR_RTS                               0x02
#define MCR_LOOP                              0x04
#define MSR_CTS                               0x08
#define MSR_CD                                0x10
#define MSR_RI                                0x20
#define MSR_DSR                               0x40

// IOCTL commands. Passed from user space using the IOCTL method to command the driver to perform an action.
#define IOCTL_CMD_FETCH_HOTKEY                _IOW('f', 'f', int32_t *)
#define IOCTL_CMD_SUSPEND_IO                  _IOW('s', 's', int32_t *)
#define IOCTL_CMD_RESUME_IO                   _IOW('r', 'r', int32_t *)

#define SHARPMZ_TTY_MAJOR                     240                                 // Experimental range
#define SHARPMZ_TTY_MINORS                    3                                   // Assign 2 devices, 0) Sharp VRAM, 1) SigmaStar SSD202 Framebuffer, 2) Control.

// Macros.
#define from_timer(var, callback_timer, timer_fieldname) container_of(callback_timer, typeof(*var), timer_fieldname)
#define PRINT_PROC_START()                    do { pr_info("Start: %s\n", __func__); } while (0)
#define PRINT_PROC_EXIT()                     do { pr_info("Finish: %s\n", __func__); } while (0)
#define sendSignal(__task__, _signal_)        { struct siginfo sigInfo;\
                                                if(__task__ != NULL)\
                                                {\
                                                    memset(&sigInfo, 0, sizeof(struct siginfo));\
                                                    sigInfo.si_signo = _signal_;\
                                                    sigInfo.si_code = SI_QUEUE;\
                                                    sigInfo.si_int = 1;\
                                                    if(send_sig_info(_signal_, &sigInfo, __task__) < 0)\
                                                    {\
                                                        pr_info("Error: Failed to send signal:%02x to:%s\n", _signal_, __task__->comm);\
                                                    }\
                                                }\
                                              }
#define resetZ80()                            {\
                                                  sendSignal(Z80Ctrl->ioTask, SIGUSR1); \
                                                  setupMemory(Z80Ctrl->defaultPageMode);\
                                                  z80_instant_reset(&Z80CPU);\
                                              }

// TTY control structure, per port.
typedef struct {
    struct tty_struct                         *tty;                               // pointer to the tty for this device
    int                                       open_count;                         // number of times this port has been opened
    struct mutex                              mutex;                              // locks this structure
    struct timer_list                         timerKeyboard;                      // Keyboard sweep timer.
    struct timer_list                         timerDisplay;                       // Display service timer.

    // for tiocmget and tiocmset functions
    int                                       msr;                                // MSR shadow
    int                                       mcr;                                // MCR shadow

    // for ioctl
    struct serial_struct                      serial;
    wait_queue_head_t                         wait;
    struct async_icount                       icount;
} t_TTYMZ;

// Driver control variables.
typedef struct {
    struct tty_driver                        *ttymzDriver;
    struct tty_port                           ttymzPort[SHARPMZ_TTY_MINORS];    
    struct task_struct                       *arbTask;
    int32_t                                  hotkey;
} t_TTYMZCtrl;

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
