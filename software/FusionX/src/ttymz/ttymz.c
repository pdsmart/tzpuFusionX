/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            ttymz.c
// Created:         Feb 2023
// Author(s):       Philip Smart
// Description:     Sharp MZ TTY
//                  This file contains the methods used to create a linux tty device driver using the
//                  host Sharp keyboard and screen as input/output. 
//                  This driver forms part of the FusionX developments and allows a user sitting at the
//                  Sharp MZ Console to access the underlying FusionX Linux SOM.
// Credits:         Credits to tiny_tty Greg Kroah-Hartman (greg@kroah.com) and Linux pty which were 
//                  used as base and reference in creating this driver.
//
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Feb 2023 - v1.0  Initial write of the Sharp MZ tty driver software.
//                                   Added suspend I/O logic. This is necessary as I want to enable
//                                   switching between a Linux session and a Z80 session, the idea
//                                   being the TTY will continue to run within the mirrored framebuffer
//                                   and when reselected, refresh the hardware screen.
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/fcntl.h>
#include <linux/kprobes.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/devpts_fs.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include "z80io.h"
#include "sharpmz.h"
#include "ttymz.h"

// Meta Information.
MODULE_LICENSE(DRIVER_LICENSE);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_INFO(versiondate, DRIVER_VERSION_DATE);
MODULE_INFO(copyright, DRIVER_COPYRIGHT);

// Device control variables.
static t_TTYMZ            *ttymzConnections[SHARPMZ_TTY_MINORS];    // Initially all NULL, no devices connected.
static struct tty_port    ttymzPort[SHARPMZ_TTY_MINORS];
static struct tty_driver  *ttymzDriver;

// Read method. Keys entered on the host keyboard are sent to the user process via this method.
//
static void ttymz_read(struct tty_struct *tty, char data)
{
    // Locals.
    struct tty_port    *port;

    // Sanity check.
    if (!tty)
        return;

    // Get the port, needed to push data onto the ringbuffer for delivery to the user.
    port = tty->port;

    // If there is no room, push the characters to the user. Add the new character and push again.
    if (!tty_buffer_request_room(port, 1))
        tty_flip_buffer_push(port);
    tty_insert_flip_char(port, data, TTY_NORMAL);
    tty_flip_buffer_push(port);

    return;
}

// Method to receive data from the user application to be written onto the Sharp or SSD202 framebuffer.
//
static int ttymz_write(struct tty_struct *tty, const unsigned char *buffer, int count)
{
    t_TTYMZ *ttymz = tty->driver_data;
    int     idx;
    int     retval = count;

    if (!ttymz)
        return -ENODEV;

    // Lock out other processes.
    mutex_lock(&ttymz->mutex);

    // Sanity check, ensure port is open.
    if (!ttymz->open_count)
        goto exit;

    // Send the characters to the Sharp MZ interface for display.
    for (idx = 0; idx < count; ++idx)
    {
        mzPrintChar(buffer[idx]);
    }

exit:
    mutex_unlock(&ttymz->mutex);
    return retval;
}

// Method to indicate to the kernel how much buffer space is left in the device. 
//
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)) 
static int ttymz_write_room(struct tty_struct *tty)
#else
static unsigned int ttymz_write_room(struct tty_struct *tty)
#endif
{
    // Locals.
    t_TTYMZ  *ttymz = tty->driver_data;
    int      room = -EINVAL;

    // Sanity check.
    if(!ttymz)
        return -ENODEV; 

    if(tty->stopped)
		return 0;

    mutex_lock(&ttymz->mutex);

    if(!ttymz->open_count)
    {
        // Port was not opened
        goto exit;
    }

    // Calculate how much room is left in the device
    room = 255;

exit:
    mutex_unlock(&ttymz->mutex);
    return room;
}


// Timer methods
//
// Timer to scan the Sharp MZ host keyboard and detect key presses. Any key press detected results
// in a character being pushed into the kernel ringbuffer for delivery to the user application.
//
static void ttymz_keyboardTimer(unsigned long timerAddr)
{
    // Locals.
    t_TTYMZ            *ttymz = (t_TTYMZ *)timerAddr;
    struct tty_struct  *tty;
    int                 key;

    // Sanity check.
    if (!ttymz)
        return;
 
    // Once sanitised, get the tty struct from the given physical address, needed to reset the timer and to
    // push data to the client.
    tty = ttymz->tty;
    
    // Scan the Sharp MZ host keyboard, push any character received. Mode 2 = Ansi scan without wait.
    if((key = mzGetKey(2)) != -1)
    {
        //pr_info("%d ", key);
        ttymz_read(tty, (char)key);
    }

    // Resubmit the timer again.
    ttymz->timerKeyboard.expires = jiffies + 1;
    add_timer(&ttymz->timerKeyboard);
}
//
// Display service timer, used for scheduling tasks within the display driver.
//
static void ttymz_displayTimer(unsigned long timerAddr)
{
    // Locals.
    t_TTYMZ            *ttymz = (t_TTYMZ *)timerAddr;
    struct tty_struct  *tty;

    // Sanity check.
    if (!ttymz)
        return;
 
    // Once sanitised, get the tty struct from the given physical address, needed to reset the timer and to
    // push data to the client.
    tty = ttymz->tty;
    
    // Call the display service routine.
    mzService();

    // Resubmit the timer again.
    ttymz->timerDisplay.expires = jiffies + 1;
    add_timer(&ttymz->timerDisplay);
}


// Device open.
//
// When a user space application open's the tty device driver, this function is called
// to initialise and allocate any required memory or hardware prior to servicing requests from the
// user space application.
static int ttymz_open(struct tty_struct *tty, struct file *file)
{
    // Locals.
    t_TTYMZ        *ttymz;
    int            index;
    int            ret = 0;
    struct winsize ws;

    // Initialize the pointer in case something fails
    tty->driver_data = NULL;

    // Get the serial object associated with this tty pointer
    index = tty->index;
    ttymz = ttymzConnections[index];
    if(ttymz == NULL)
    {
        // First time accessing this device, let's create it
        ttymz = kmalloc(sizeof(*ttymz), GFP_KERNEL);
        if (!ttymz)
            return -ENOMEM;

        mutex_init(&ttymz->mutex);
        ttymz->open_count = 0;

        ttymzConnections[index] = ttymz;
    }

    mutex_lock(&ttymz->mutex);

    // Save our structure within the tty structure
    tty->driver_data = ttymz;
    ttymz->tty = tty;

    // Setup the default terminal size based on compilation (ie. 40/80 cols).
    ws.ws_row    = VC_MAX_ROWS;
    ws.ws_col    = VC_MAX_COLUMNS;
    tty->winsize = ws;

    // Port opened, perform initialisation.
    //
    if(++ttymz->open_count == 1)
    {
        // Create the keyboard sweep timer and submit it
      #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
        init_timer(&ttymz->timerKeyboard);
        ttymz->timerKeyboard.data = (unsigned long)ttymz;
        ttymz->timerKeyboard.function = ttymz_keyboardTimer;
      #else
        timer_setup(timer, (void *)ttymz_keyboardTimer, (unsigned long)ttymz);
        timer->function = (void *)ttymz_keyboardTimer;
      #endif
        // 10 ms sweep timer.
        ttymz->timerKeyboard.expires = jiffies + 1;
        add_timer(&ttymz->timerKeyboard);

        // Create the display periodic timer and submit it.
      #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
        init_timer(&ttymz->timerDisplay);
        ttymz->timerDisplay.data = (unsigned long)ttymz;
        ttymz->timerDisplay.function = ttymz_displayTimer;
      #else
        timer_setup(timer, (void *)ttymz_keyboardTimer, (unsigned long)ttymz);
        timer->function = (void *)ttymz_displayTimer;
      #endif
        // 10 ms service interval.
        ttymz->timerDisplay.expires = jiffies + 1;
        add_timer(&ttymz->timerDisplay);
    } else
    {
        // Not allowed to open the port more than once.
        ret = -EBUSY;
    }

    mutex_unlock(&ttymz->mutex);
    return(ret);
}

// Close helper method, performs the actual deallocation of resources for the open port.
//
static void do_close(t_TTYMZ *ttymz)
{
    // Locals.

    mutex_lock(&ttymz->mutex);

    if(!ttymz->open_count)
    {
        // Port was never opened
        goto exit;
    }

    // Shutdown hardware tasks to exit.
    if(--ttymz->open_count <= 0)
    {
        // Shut down our timers.
        del_timer(&ttymz->timerKeyboard);
        del_timer(&ttymz->timerDisplay);
    }
exit:
    mutex_unlock(&ttymz->mutex);
}

// Device close.
//
// When a user space application terminates or closes the tty device driver, this function is called
// to close any open connections, memory and variables required to handle the user space application
// requests.
static void ttymz_close(struct tty_struct *tty, struct file *file)
{
    // Locals.
    t_TTYMZ *ttymz = tty->driver_data;

    if (ttymz)
        do_close(ttymz);
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))
static void ttymz_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
    // Locals.
    unsigned int cflag;

    //PRINT_PROC_START();

    cflag = tty->termios.c_cflag;

    // Check that they really want us to change something
    if (old_termios)
    {
        if ((cflag == old_termios->c_cflag) && (RELEVANT_IFLAG(tty->termios.c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag)))
        {
            return;
        }
    }

    // Get the byte size
    switch(cflag & CSIZE)
    {
        case CS5:
            pr_info(" - data bits = 5\n");
            break;
        case CS6:
            pr_info(" - data bits = 6\n");
            break;
        case CS7:
            pr_info(" - data bits = 7\n");
            break;
        default:
        case CS8:
            pr_info(" - data bits = 8\n");
            break;
    }

    // Determine the parity
    if (cflag & PARENB)
        if (cflag & PARODD)
            pr_info(" - parity = odd\n");
        else
            pr_info(" - parity = even\n");
    else
        pr_info(" - parity = none\n");

    // Figure out the stop bits requested 
    if (cflag & CSTOPB)
        pr_info(" - stop bits = 2\n");
    else
        pr_info(" - stop bits = 1\n");

    // Figure out the hardware flow control settings
    if (cflag & CRTSCTS)
        pr_info(" - RTS/CTS is enabled\n");
    else
        pr_info(" - RTS/CTS is disabled\n");

    // Determine software flow control
    // if we are implementing XON/XOFF, set the start and
    // stop character in the device
    if (I_IXOFF(tty) || I_IXON(tty))
    {
        unsigned char stop_char  = STOP_CHAR(tty);
        unsigned char start_char = START_CHAR(tty);

        // If we are implementing INBOUND XON/XOFF
        if (I_IXOFF(tty))
            pr_info(" - INBOUND XON/XOFF is enabled, "
                "XON = %2x, XOFF = %2x", start_char, stop_char);
        else
            pr_info(" - INBOUND XON/XOFF is disabled");

        // if we are implementing OUTBOUND XON/XOFF
        if (I_IXON(tty))
            pr_info(" - OUTBOUND XON/XOFF is enabled, "
                "XON = %2x, XOFF = %2x", start_char, stop_char);
        else
            pr_info(" - OUTBOUND XON/XOFF is disabled");
    }

    // Get the baud rate wanted
    pr_info(" - baud rate = %d", tty_get_baud_rate(tty));
}

static int ttymz_tiocmget(struct tty_struct *tty)
{
    // Locals.
    t_TTYMZ      *ttymz = tty->driver_data;
    unsigned int result = 0;
    unsigned int msr = ttymz->msr;
    unsigned int mcr = ttymz->mcr;

    //PRINT_PROC_START();

    result = ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |    // DTR is set
             ((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |    // RTS is set
             ((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |    // LOOP is set
             ((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |    // CTS is set
             ((msr & MSR_CD)   ? TIOCM_CAR  : 0) |    // Carrier detect is set
             ((msr & MSR_RI)   ? TIOCM_RI   : 0) |    // Ring Indicator is set
             ((msr & MSR_DSR)  ? TIOCM_DSR  : 0);     // DSR is set

    return result;
}

static int ttymz_tiocmset(struct tty_struct *tty, unsigned int set, unsigned int clear)
{
    // Locals.
    t_TTYMZ      *ttymz = tty->driver_data;
    unsigned int mcr = ttymz->mcr;

    //PRINT_PROC_START();

    if (set & TIOCM_RTS)
        mcr |= MCR_RTS;
    if (set & TIOCM_DTR)
        mcr |= MCR_RTS;

    if (clear & TIOCM_RTS)
        mcr &= ~MCR_RTS;
    if (clear & TIOCM_DTR)
        mcr &= ~MCR_RTS;

    // Set the new MCR value in the device
    ttymz->mcr = mcr;
    return 0;
}

#define ttymz_ioctl ttymz_ioctl_tiocgserial
static int ttymz_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{
    // Locals.
    struct serial_struct tmp;
    t_TTYMZ              *ttymz = tty->driver_data;

    //PRINT_PROC_START();

    if(cmd == TIOCGSERIAL)
    {
        if (!arg)
            return -EFAULT;

        memset(&tmp, 0, sizeof(tmp));

        tmp.type            = ttymz->serial.type;
        tmp.line            = ttymz->serial.line;
        tmp.port            = ttymz->serial.port;
        tmp.irq             = ttymz->serial.irq;
        tmp.flags           = ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
        tmp.xmit_fifo_size  = ttymz->serial.xmit_fifo_size;
        tmp.baud_base       = ttymz->serial.baud_base;
        tmp.close_delay     = 5*HZ;
        tmp.closing_wait    = 30*HZ;
        tmp.custom_divisor  = ttymz->serial.custom_divisor;
        tmp.hub6            = ttymz->serial.hub6;
        tmp.io_type         = ttymz->serial.io_type;

        if (copy_to_user((void __user *)arg, &tmp, sizeof(struct serial_struct)))
            return -EFAULT;
        return 0;
    }
    return -ENOIOCTLCMD;
}
#undef ttymz_ioctl

#define ttymz_ioctl ttymz_ioctl_tiocmiwait
static int ttymz_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{
    // Locals.
    DECLARE_WAITQUEUE(wait, current);
    struct async_icount cnow;
    struct async_icount cprev;
    t_TTYMZ             *ttymz = tty->driver_data;

    //PRINT_PROC_START();

    if (cmd == TIOCMIWAIT)
    {
        cprev = ttymz->icount;
        while (1)
        {
            add_wait_queue(&ttymz->wait, &wait);
            set_current_state(TASK_INTERRUPTIBLE);
            schedule();
            remove_wait_queue(&ttymz->wait, &wait);

            // See if a signal woke us up
            if (signal_pending(current))
                return -ERESTARTSYS;

            cnow = ttymz->icount;
            if(cnow.rng == cprev.rng && cnow.dsr == cprev.dsr && cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
                return -EIO; // no change => error
            if(((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
               ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
               ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
               ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)))
            {
                return 0;
            }
            cprev = cnow;
        }

    }
    return -ENOIOCTLCMD;
}
#undef ttymz_ioctl

#define ttymz_ioctl ttymz_ioctl_tiocgicount
static int ttymz_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{
    // Locals.
    t_TTYMZ                       *ttymz = tty->driver_data;
    struct async_icount           cnow = ttymz->icount;
    struct serial_icounter_struct icount;

    //PRINT_PROC_START();

    if(cmd == TIOCGICOUNT)
    {
        icount.cts     = cnow.cts;
        icount.dsr     = cnow.dsr;
        icount.rng     = cnow.rng;
        icount.dcd     = cnow.dcd;
        icount.rx      = cnow.rx;
        icount.tx      = cnow.tx;
        icount.frame   = cnow.frame;
        icount.overrun = cnow.overrun;
        icount.parity  = cnow.parity;
        icount.brk     = cnow.brk;
        icount.buf_overrun = cnow.buf_overrun;

        if (copy_to_user((void __user *)arg, &icount, sizeof(icount)))
            return -EFAULT;
        return 0;
    }
    return -ENOIOCTLCMD;
}
#undef ttymz_ioctl

// IOCTL Method
// This method allows User Space application to control the SharpMZ TTY device driver internal functionality.
//
static int ttymz_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{
    // Locals.

    //PRINT_PROC_START();

    switch(cmd)
    {
        case TIOCGSERIAL:
            return ttymz_ioctl_tiocgserial(tty, cmd, arg);
        case TIOCMIWAIT:
            return ttymz_ioctl_tiocmiwait(tty, cmd, arg);
        case TIOCGICOUNT:
            return ttymz_ioctl_tiocgicount(tty, cmd, arg);

        // Suspend control. This method stops all physical hardware updates of the host framebuffer and keyboard 
        // scan whilst maintining the functionality of the tty within the mirrored framebuffer. This mode is
        // necessary if the user wishes to switch into a Z80 driver and use the host as original.
        case IOCTL_CMD_SUSPEND_IO:
            mzSuspendIO();
            return(0);

        // Resume control. This method re-initialises host hardware, updates the host framebuffer from the mirror
        // (refresh) and re-enabled hardware access and keyboard scanning.
        case IOCTL_CMD_RESUME_IO:
            mzResumeIO();
            return(0);
    }

    return -ENOIOCTLCMD;
}


// Window resize method.
// On the Sharp framebuffer this could be 40/80 chars wide.
// On the SSD202 framebuffer, tba.
static int ttymz_resize(struct tty_struct *tty,  struct winsize *ws)
{
    PRINT_PROC_START();

    pr_info("Resize to:%d,%d\n", ws->ws_row, ws->ws_col);

    // Check columns and setup the display according to requirement.
    if(ws->ws_col == 40)
    {
        ws->ws_row   = VC_MAX_ROWS;
    } else
    if(ws->ws_col == 80)
    {
        ws->ws_row   = VC_MAX_ROWS;
    } else
    {
        // Ignore all other values.
        return -EINVAL;
    }

    // Setup the hardware to accommodate new column width.
    mzSetDisplayWidth(ws->ws_col);
    tty->winsize = *ws;

    return 0;
}

//static void ttymz_remove(struct tty_driver *driver, struct tty_struct *tty)
//{
//    // Locals.
//
//    PRINT_PROC_START();
//}

static void ttymz_cleanup(struct tty_struct *tty)
{
//    tty_port_put(tty->port);
}

static void ttymz_flush_buffer(struct tty_struct *tty)
{
    // Locals.

    //PRINT_PROC_START();
}

//  pty_chars_in_buffer    -    characters currently in our tx queue
//
//  Report how much we have in the transmit queue. As everything is
//  instantly at the other end this is easy to implement.
//
static int ttymz_chars_in_buffer(struct tty_struct *tty)
{
    return 0;
}

// The unthrottle routine is called by the line discipline to signal
// that it can receive more characters.  For PTY's, the TTY_THROTTLED
// flag is always set, to force the line discipline to always call the
// unthrottle routine when there are fewer than TTY_THRESHOLD_UNTHROTTLE
// characters in the queue.  This is necessary since each time this
// happens, we need to wake up any sleeping processes that could be
// (1) trying to send data to the pty, or (2) waiting in wait_until_sent()
// for the pty buffer to be drained.
// 
static void ttymz_unthrottle(struct tty_struct *tty)
{
    //PRINT_PROC_START();
    tty_wakeup(tty->link);
    set_bit(TTY_THROTTLED, &tty->flags);
}

// Structure to declare public API methods.
// Standard Linux device driver structure to declare accessible methods within the driver.
static const struct tty_operations serial_ops = {
  //.install         = ttymz_install,
    .open            = ttymz_open,
    .close           = ttymz_close,
    .write           = ttymz_write,
    .write_room      = ttymz_write_room,
    .flush_buffer    = ttymz_flush_buffer,
    .chars_in_buffer = ttymz_chars_in_buffer,
    .unthrottle      = ttymz_unthrottle,
    .set_termios     = ttymz_set_termios,
    .tiocmget        = ttymz_tiocmget,
    .tiocmset        = ttymz_tiocmset,
    .ioctl           = ttymz_ioctl,
    .cleanup         = ttymz_cleanup,
    .resize          = ttymz_resize,
//  .remove          = ttymz_remove
};


// Initialisation.
// This is the entry point into the device driver when loaded into the kernel.
// The method intialises any required hardware (ie. GPIO's, SPI etc), memory.
// It also allocates the Major and Minor device numbers and sets up the device in /dev.
static int __init ttymz_init(void)
{
    // Locals.
    int  retval;
    int  i;
    char buf[80];

    // Allocate the tty driver handles, one per potential device.
    ttymzDriver = alloc_tty_driver(SHARPMZ_TTY_MINORS);
    if(!ttymzDriver)
        return -ENOMEM;

    // Initialize the tty driver
    ttymzDriver->owner                = THIS_MODULE;
    ttymzDriver->driver_name          = DRIVER_NAME;
    ttymzDriver->name                 = DEVICE_NAME;
    ttymzDriver->major                = SHARPMZ_TTY_MAJOR,
    ttymzDriver->type                 = TTY_DRIVER_TYPE_SERIAL,
    ttymzDriver->subtype              = SERIAL_TYPE_NORMAL,
    ttymzDriver->flags                = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
    ttymzDriver->init_termios         = tty_std_termios;
    ttymzDriver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    tty_set_operations(ttymzDriver, &serial_ops);
    for (i = 0; i < SHARPMZ_TTY_MINORS; i++)
    {
        tty_port_init(ttymzPort + i);
        tty_port_link_device(ttymzPort + i, ttymzDriver, i);
    }

    // Register the tty driver
    retval = tty_register_driver(ttymzDriver);
    if(retval)
    {
        pr_err("Failed to register SharpMZ tty driver");
        put_tty_driver(ttymzDriver);
        return retval;
    }

    // Register the devices.
    for (i = 0; i < SHARPMZ_TTY_MINORS; ++i)
        tty_register_device(ttymzDriver, i, NULL);

    // Initialise the hardware to host interface.
    z80io_init();

    // Initialise the Sharp MZ interface.
    mzInit();

    // Sign on.
    sprintf(buf, "%s %s",   DRIVER_DESCRIPTION, DRIVER_VERSION); mzWriteString(0, 0, buf, -1);
    sprintf(buf, "%s %s\n", DRIVER_COPYRIGHT, DRIVER_AUTHOR);    mzWriteString(0, 1, buf, -1);

    pr_info(DRIVER_DESCRIPTION " " DRIVER_VERSION "\n");

    return retval;
}

// Exit 
// This method is called when the device driver is removed from the kernel with the rmmod command.
// It is responsible for closing and freeing all allocated memory, disabling hardware and removing
// the device from the /dev directory.
static void __exit ttymz_exit(void)
{
    // Locals.
    t_TTYMZ *ttymz;
    int     idx;

    // De-register the devices and driver.
    for(idx = 0; idx < SHARPMZ_TTY_MINORS; ++idx)
    {
        tty_unregister_device(ttymzDriver, idx);
        tty_port_destroy(ttymzPort + idx);
    }
    tty_unregister_driver(ttymzDriver);

    // Shut down all of the timers and free the memory.
    for(idx = 0; idx < SHARPMZ_TTY_MINORS; ++idx)
    {
        ttymz = ttymzConnections[idx];
        if (ttymz)
        {
            // Close the port.
            while (ttymz->open_count)
                do_close(ttymz);

            // Shut down our timer and free the memory.
            del_timer(&ttymz->timerKeyboard);
            del_timer(&ttymz->timerDisplay);
            kfree(ttymz);
            ttymzConnections[idx] = NULL;
        }
    }

    pr_info("ttymz: unregistered!\n");
}

module_init(ttymz_init);
module_exit(ttymz_exit);
