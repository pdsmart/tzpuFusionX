/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            sharpmz.c
// Created:         February 2023
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Sharp MZ Interface Library.
//                  This file contains methods which allow the Linux TTY driver to access and control the 
//                  Sharp MZ series computer hardware. 
//
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         v1.0 Feb 2023  - Initial write of the Sharp MZ series hardware interface software.
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
#ifndef SHARPMZ_H
#define SHARPMZ_H

#ifdef __cplusplus
    extern "C" {
#endif

// Build time target. Overrides if compile time definition given.
#if defined(TARGET_HOST_MZ700)
  #define TARGET_HOST_MZ700                   1
  #define TARGET_HOST_MZ2000                  0
  #define TARGET_HOST_MZ80A                   0
  #define TARGET_HOST_PCW                     0
#elif defined(TARGET_HOST_MZ2000)
  #define TARGET_HOST_MZ2000                  1
  #define TARGET_HOST_MZ700                   0
  #define TARGET_HOST_MZ80A                   0
  #define TARGET_HOST_PCW                     0
#elif defined(TARGET_HOST_MZ80A)
  #define TARGET_HOST_MZ80A                   1
  #define TARGET_HOST_MZ2000                  0
  #define TARGET_HOST_MZ700                   0
  #define TARGET_HOST_PCW                     0
#elif defined(TARGET_HOST_PCW8XXX) || defined(TARGET_HOST_PCW9XXX)
  #define TARGET_HOST_PCW                     1
  #define TARGET_HOST_MZ2000                  0
  #define TARGET_HOST_MZ700                   0
  #define TARGET_HOST_MZ80A                   0
#else
  #define TARGET_HOST_MZ700                   0                                   // Target compilation for an MZ700
  #define TARGET_HOST_MZ2000                  0                                   //                           MZ2000
  #define TARGET_HOST_MZ80A                   0                                   //                           MZ80A
  #define TARGET_HOST_PCW                     0                                   //                           Amstrad PCW8XXX/9XXX
#endif

// Video display constants.
#define VC_MAX_ROWS                           25                                  // Maximum number of rows on display.
#if (TARGET_HOST_MZ700 == 1)
  #define VC_MAX_COLUMNS                      40                                  // Maximum number of columns on display.
#else
  #define VC_MAX_COLUMNS                      80                                  // Maximum number of columns on display.
#endif
#define VC_MAX_BUFFER_ROWS                    50                                  // Maximum number of backing store rows for scrollback feature.
#define VC_DISPLAY_BUFFER_SIZE                VC_MAX_COLUMNS * VC_MAX_BUFFER_ROWS // Size of the display buffer for scrollback.

// Keyboard constants.
#define KEYB_AUTOREPEAT_INITIAL_TIME          800                                 // Time in milliseconds before starting autorepeat.
#define KEYB_AUTOREPEAT_TIME                  100                                 // Time in milliseconds between auto repeating characters.
#define KEYB_FLASH_TIME                       350                                 // Time in milliseconds for the cursor flash change.
#define MAX_KEYB_BUFFER_SIZE                  32                                  // Maximum size of the keyboard buffer.
#if (TARGET_HOST_MZ80A == 1 || TARGET_HOST_MZ700 == 1)
  #define KEY_SCAN_ROWS                       10                                  // Number of rows on keyboard to scan.
  #define CURSOR_CHR_THICK_BLOCK              0x43                                // Thick block cursor for Shift Lock.
  #define CURSOR_CHR_BLOCK                    0xEF                                // Block cursor for CAPS Lock.
  #define CURSOR_CHR_GRAPH                    0xFF                                // Graphic cursor for GRAPH mode.
  #define CURSOR_CHR_UNDERLINE                0x3E                                // Underline for lower case CAPS OFF.
#elif (TARGET_HOST_MZ2000 == 1)
  #define KEY_SCAN_ROWS                       12                                  
  #define CURSOR_CHR_THICK_BLOCK              0x1E                                // Thick block cursor for Shift Lock.
  #define CURSOR_CHR_BLOCK                    0x82                                // Block cursor for CAPS Lock.
  #define CURSOR_CHR_GRAPH                    0x93                                // Graphic cursor for GRAPH mode.
  #define CURSOR_CHR_UNDERLINE                0x1F                                // Underline for lower case CAPS OFF.
#endif

// Audio constants.
#define TIMER_8253_MZ80A_FREQ                 2000000                             // Base input frequency of Timer 0 for square wave generation.
#define TIMER_8253_MZ700_FREQ                 768000                              // Base input frequency of Timer 0 for square wave generation.

// Base addresses and sizes within the Video Controller.
#define VIDEO_BASE_ADDR                       0x000000                            // Base address of the Video Controller.
#define VIDEO_VRAM_BASE_ADDR                  VIDEO_BASE_ADDR + 0x00D000          // Base address of the character video RAM using direct addressing.
#define VIDEO_VRAM_SIZE                       0x800                               // Size of the video RAM.
#define VIDEO_ARAM_BASE_ADDR                  VIDEO_BASE_ADDR + 0x00D800          // Base address of the character attribute RAM using direct addressing.
#define VIDEO_ARAM_SIZE                       0x800                               // Size of the attribute RAM.

// Video Module control bits.
#define VMMODE_MASK                           0xF8                                // Mask to mask out video mode.
#define VMMODE_MZ80K                          0x00                                // Video mode = MZ80K
#define VMMODE_MZ80C                          0x01                                // Video mode = MZ80C
#define VMMODE_MZ1200                         0x02                                // Video mode = MZ1200
#define VMMODE_MZ80A                          0x03                                // Video mode = MZ80A
#define VMMODE_MZ700                          0x04                                // Video mode = MZ700
#define VMMODE_MZ800                          0x05                                // Video mode = MZ800
#define VMMODE_MZ1500                         0x06                                // Video mode = MZ1500
#define VMMODE_MZ80B                          0x07                                // Video mode = MZ80B
#define VMMODE_MZ2000                         0x08                                // Video mode = MZ2000
#define VMMODE_MZ2200                         0x09                                // Video mode = MZ2200
#define VMMODE_MZ2500                         0x0A                                // Video mode = MZ2500
#define VMMODE_80CHAR                         0x80                                // Enable 80 character display.
#define VMMODE_80CHAR_MASK                    0x7F                                // Mask to filter out display width control bit.
#define VMMODE_COLOUR                         0x20                                // Enable colour display.
#define VMMODE_COLOUR_MASK                    0xDF                                // Mask to filter out colour control bit.
        
// Sharp MZ colour attributes.
#define VMATTR_FG_BLACK                       0x00                                // Foreground black character attribute.
#define VMATTR_FG_BLUE                        0x10                                // Foreground blue character attribute.
#define VMATTR_FG_RED                         0x20                                // Foreground red character attribute.
#define VMATTR_FG_PURPLE                      0x30                                // Foreground purple character attribute.
#define VMATTR_FG_GREEN                       0x40                                // Foreground green character attribute.
#define VMATTR_FG_CYAN                        0x50                                // Foreground cyan character attribute.
#define VMATTR_FG_YELLOW                      0x60                                // Foreground yellow character attribute.
#define VMATTR_FG_WHITE                       0x70                                // Foreground white character attribute.
#define VMATTR_FG_MASKOUT                     0x8F                                // Mask to filter out foreground attribute.
#define VMATTR_FG_MASKIN                      0x70                                // Mask to filter out foreground attribute.
#define VMATTR_BG_BLACK                       0x00                                // Background black character attribute.
#define VMATTR_BG_BLUE                        0x01                                // Background blue character attribute.
#define VMATTR_BG_RED                         0x02                                // Background red character attribute.
#define VMATTR_BG_PURPLE                      0x03                                // Background purple character attribute.
#define VMATTR_BG_GREEN                       0x04                                // Background green character attribute.
#define VMATTR_BG_CYAN                        0x05                                // Background cyan character attribute.
#define VMATTR_BG_YELLOW                      0x06                                // Background yellow character attribute.
#define VMATTR_BG_WHITE                       0x07                                // Background white character attribute.
#define VMATTR_BG_MASKOUT                     0xF8                                // Mask to filter out background attribute.
#define VMATTR_BG_MASKIN                      0x07                                // Mask to filter out background attribute.

// Sharp MZ-80A/700 constants.
//
#define MBADDR_KEYPA                          0xE000                              // Mainboard 8255 Port A
#define MBADDR_KEYPB                          0xE001                              // Mainboard 8255 Port B
#define MBADDR_KEYPC                          0xE002                              // Mainboard 8255 Port C
#define MBADDR_KEYPF                          0xE003                              // Mainboard 8255 Mode Control
#define MBADDR_CSTR                           0xE002                              // Mainboard 8255 Port C
#define MBADDR_CSTPT                          0xE003                              // Mainboard 8255 Mode Control
#define MBADDR_CONT0                          0xE004                              // Mainboard 8253 Counter 0
#define MBADDR_CONT1                          0xE005                              // Mainboard 8253 Counter 1
#define MBADDR_CONT2                          0xE006                              // Mainboard 8253 Counter 1
#define MBADDR_CONTF                          0xE007                              // Mainboard 8253 Mode Control
#define MBADDR_SUNDG                          0xE008                              // Register for reading the tempo timer status (cursor flash). horizontal blank and switching sound on/off.
#define MBADDR_TEMP                           0xE008                              // As above, different name used in original source when writing.
#define MBADDR_MEMSW                          0xE00C                              // Memory swap, 0000->C000, C000->0000
#define MBADDR_MEMSWR                         0xE010                              // Reset memory swap.
#define MBADDR_NRMDSP                         0xE014                              // Return display to normal.
#define MBADDR_INVDSP                         0xE015                              // Invert display.
#define MBADDR_SCLDSP                         0xE200                              // Hardware scroll, a read to each location adds 8 to the start of the video access address therefore creating hardware scroll. 00 - reset to power up
#define MBADDR_SCLBASE                        0xE2                                // High byte scroll base.
#define MBADDR_DSPCTL                         0xDFFF                              // Display 40/80 select register (bit 7)

// Sharp MZ-2000 constants.
#define MBADDR_FDC                            0x0D8                               // MB8866 IO Region 0D8h - 0DBh
#define MBADDR_FDC_CR                         MBADDR_FDC + 0x00                   // Command Register
#define MBADDR_FDC_STR                        MBADDR_FDC + 0x00                   // Status Register
#define MBADDR_FDC_TR                         MBADDR_FDC + 0x01                   // Track Register
#define MBADDR_FDC_SCR                        MBADDR_FDC + 0x02                   // Sector Register
#define MBADDR_FDC_DR                         MBADDR_FDC + 0x03                   // Data Register
#define MBADDR_FDC_MOTOR                      MBADDR_FDC + 0x04                   // DS[0-3] and Motor control. 4 drives  DS= BIT 0 -> Bit 2 = Drive number, 2=1,1=0,0=0 DS0, 2=1,1=0,0=1 DS1 etc
                                                                                  //  bit 7 = 1 MOTOR ON LOW (Active)
#define MBADDR_FDC_SIDE                       MBADDR_FDC + 0x05                   // Side select, Bit 0 when set = SIDE SELECT LOW, 
#define MBADDR_FDC_DDEN                       MBADDR_FDC + 0x06                   // Double density enable, 0 = double density, 1 = single density disks.
#define MBADDR_PPIA                           0x0E0                               // 8255 Port A
#define MBADDR_PPIB                           0x0E1                               // 8255 Port B
#define MBADDR_PPIC                           0x0E2                               // 8255 Port C
#define MBADDR_PPICTL                         0x0E3                               // 8255 Control Port
#define MBADDR_PIOA                           0x0E8                               // Z80 PIO Port A
#define MBADDR_PIOCTLA                        0x0E9                               // Z80 PIO Port A Control Port
#define MBADDR_PIOB                           0x0EA                               // Z80 PIO Port B
#define MBADDR_PIOCTLB                        0x0EB                               // Z80 PIO Port B Control Port
#define MBADDR_CRTBKCOLR                      0x0F4                               // Configure external CRT background colour.
#define MBADDR_CRTGRPHPRIO                    0x0F5                               // Graphics priority register, character or a graphics colour has front display priority.
#define MBADDR_CRTGRPHSEL                     0x0F6                               // Graphics output select on CRT or external CRT
#define MBADDR_GRAMCOLRSEL                    0x0F7                               // Graphics RAM colour bank select.
#define MBADDR_GRAMADDRL                      0x0C000                             // Graphics RAM base address.

//Common character definitions.
#define SCROLL                                0x01                                // Set scroll direction UP.
#define BELL                                  0x07
#define ENQ                                   0x05
#define SPACE                                 0x20
#define TAB                                   0x09                                // TAB ACROSS (8 SPACES FOR SD-BOARD)
#define CR                                    0x0D
#define LF                                    0x0A
#define FF                                    0x0C
#define DELETE                                0x7F
#define BACKS                                 0x08
#define SOH                                   0x01                                // For XModem etc.
#define EOT                                   0x04
#define ACK                                   0x06
#define NAK                                   0x15
#define NUL                                   0x00
//#define NULL                                  0x00
#define CTRL_A                                0x01
#define CTRL_B                                0x02
#define CTRL_C                                0x03
#define CTRL_D                                0x04
#define CTRL_E                                0x05
#define CTRL_F                                0x06
#define CTRL_G                                0x07
#define CTRL_H                                0x08
#define CTRL_I                                0x09
#define CTRL_J                                0x0A
#define CTRL_K                                0x0B
#define CTRL_L                                0x0C
#define CTRL_M                                0x0D
#define CTRL_N                                0x0E
#define CTRL_O                                0x0F
#define CTRL_P                                0x10
#define CTRL_Q                                0x11
#define CTRL_R                                0x12
#define CTRL_S                                0x13
#define CTRL_T                                0x14
#define CTRL_U                                0x15
#define CTRL_V                                0x16
#define CTRL_W                                0x17
#define CTRL_X                                0x18
#define CTRL_Y                                0x19
#define CTRL_Z                                0x1A
#define ESC                                   0x1B
#define CTRL_SLASH                            0x1C
#define CTRL_LB                               0x1
#define CTRL_RB                               0x1D
#define CTRL_CAPPA                            0x1E
#define CTRL_UNDSCR                           0x1F
#define CTRL_AT                               0x00
#define FUNC1                                 0x80
#define FUNC2                                 0x81
#define FUNC3                                 0x82
#define FUNC4                                 0x83
#define FUNC5                                 0x84
#define FUNC6                                 0x85
#define FUNC7                                 0x86
#define FUNC8                                 0x87
#define FUNC9                                 0x88
#define FUNC10                                0x89
#define HOTKEY_ORIGINAL                       0xE0
#define HOTKEY_RFS80                          0xE1
#define HOTKEY_RFS40                          0xE2
#define HOTKEY_TZFS                           0xE3
#define HOTKEY_LINUX                          0xE4
#define PAGEUP                                0xE8
#define PAGEDOWN                              0xE9
#define CURHOMEKEY                            0xEA
#define ALPHAGRAPHKEY                         0xEB
#define SHIFTLOCKKEY                          0xEC
#define NOKEY                                 0xF0
#define CURSRIGHT                             0xF1
#define CURSLEFT                              0xF2
#define CURSUP                                0xF3
#define CURSDOWN                              0xF4
#define DBLZERO                               0xF5
#define INSERT                                0xF6
#define CLRKEY                                0xF7
#define HOMEKEY                               0xF8
#define ENDKEY                                0xF9
#define ANSITGLKEY                            0xFA
#define BREAKKEY                              0xFB
#define GRAPHKEY                              0xFC
#define ALPHAKEY                              0xFD
#define DEBUGKEY                              0xFE                                // Special key to enable debug features such as the ANSI emulation.

// Macros.
//
// The read/write hardware macros are created in order to allow this module to be used with the zSoft/zOS platform
// as well as the FusionX platform. The ZPU writes direct to memory, the FusionX sends via SPI.
#define WRITE_HARDWARE(__force__,__addr__,__data__)\
                                     {\
                                         if(!ctrl.suspendIO || __force__ == 1)\
                                         {\
                                             SPI_SEND32((uint32_t)__addr__ << 16 | __data__ << 8 | CPLD_CMD_WRITE_ADDR);\
                                         }\
                                     }
#define READ_HARDWARE_INIT(__force__,__addr__)\
                                     {\
                                         if(!ctrl.suspendIO || __force__ == 1)\
                                         {\
                                             SPI_SEND32((uint32_t)__addr__ << 16 | 0x00 << 8 | CPLD_CMD_READ_ADDR);\
                                             while(CPLD_READY() == 0);\
                                         }\
                                     }
#define READ_HARDWARE()              (\
                                         z80io_PRL_Read8(1)\
                                     )
#define WRITE_HARDWARE_IO(__force__,__addr__,__data__)\
                                     {\
                                         if(!ctrl.suspendIO || __force__ == 1)\
                                         {\
                                             SPI_SEND32((uint32_t)__addr__ << 16 | __data__ << 8 | CPLD_CMD_WRITEIO_ADDR);\
                                         }\
                                     }
#define READ_HARDWARE_IO_INIT(__force__,__addr__)\
                                     {\
                                         if(!ctrl.suspendIO || __force__ == 1)\
                                         {\
                                             SPI_SEND32((uint32_t)__addr__ << 16 | 0x00 << 8 | CPLD_CMD_READIO_ADDR);\
                                             while(CPLD_READY() == 0);\
                                         }\
                                     }
#define READ_HARDWARE_IO()           (\
                                         z80io_PRL_Read8(1)\
                                     )
// Video memory macros, allows for options based on host hardware - All Sharp MZ series are very similar.
#if (TARGET_HOST_MZ2000 == 1)
  // A7 : H 0xD000:0xD7FF or 0xC000:0xFFFF VRAM paged in.
  // A6 : H Select Character VRAM (H) or Graphics VRAM (L)
  // A5 : H Select 80 char mode, 40 char mode = L
  // A4 : L Select all key strobe lines active, for detection of any key press.
  // A3-A0: Keyboard strobe lines
  #define ENABLE_VIDEO()             {\
                                         display.hwVideoMode = (display.hwVideoMode & 0x3F) | 0xC0;\
                                         WRITE_HARDWARE_IO(0, MBADDR_PIOA, display.hwVideoMode);\
                                     }
  #define DISABLE_VIDEO()            {\
                                         display.hwVideoMode = (display.hwVideoMode & 0x3F);\
                                         WRITE_HARDWARE_IO(0, MBADDR_PIOA, display.hwVideoMode);\
                                     }
  #define WRITE_VRAM_CHAR(__addr__,__data__)      WRITE_HARDWARE(0,__addr__,__data__)
  #define WRITE_VRAM_ATTRIBUTE(__addr__,__data__) {}
  #define WRITE_KEYB_STROBE(__data__)\
                                     {\
                                         display.hwVideoMode = (display.hwVideoMode & 0xF0) | 0x10 | (__data__ & 0x0F);\
                                         WRITE_HARDWARE_IO(0, MBADDR_PIOA, display.hwVideoMode );\
                                     }
  #define READ_KEYB_INIT()           READ_HARDWARE_IO_INIT(0, MBADDR_PIOB)
  #define READ_KEYB()                READ_HARDWARE_IO()
#else
  #define ENABLE_VIDEO()             {}
  #define DISABLE_VIDEO()            {}
  #define WRITE_VRAM_CHAR(__addr__,__data__)      WRITE_HARDWARE(0,__addr__,dispCodeMap[__data__].dispCode)
  #define WRITE_VRAM_ATTRIBUTE(__addr__,__data__) WRITE_HARDWARE(0,__addr__,__data__)
  #define WRITE_KEYB_STROBE(__data__)                       WRITE_HARDWARE(0, MBADDR_KEYPA, __data__)
  #define READ_KEYB_INIT()           READ_HARDWARE_INIT(0, MBADDR_KEYPB)
  #define READ_KEYB()                READ_HARDWARE()
#endif



// Cursor flash mechanism control states.
//
enum CURSOR_STATES {
    CURSOR_OFF                       = 0x00,                             // Turn the cursor off.
    CURSOR_ON                        = 0x01,                             // Turn the cursor on.
    CURSOR_RESTORE                   = 0x02,                             // Restore the saved cursor character.
    CURSOR_FLASH                     = 0x03                              // If enabled, flash the cursor.
};

// Cursor positioning states.
enum CURSOR_POSITION {
    CURSOR_UP                        = 0x00,                             // Move the cursor up.
    CURSOR_DOWN                      = 0x01,                             // Move the cursor down.
    CURSOR_LEFT                      = 0x02,                             // Move the cursor left.
    CURSOR_RIGHT                     = 0x03,                             // Move the cursor right.
    CURSOR_COLUMN                    = 0x04,                             // Set cursor column to absolute value.
    CURSOR_NEXT_LINE                 = 0x05,                             // Move the cursor to the beginning of the next line.
    CURSOR_PREV_LINE                 = 0x06,                             // Move the cursor to the beginning of the previous line.
};

// Keyboard operating states according to buttons pressed.
//
enum KEYBOARD_MODES {
    KEYB_LOWERCASE                   = 0x00,                             // Keyboard in lower case mode.
    KEYB_CAPSLOCK                    = 0x01,                             // Keyboard in CAPS lock mode.
    KEYB_SHIFTLOCK                   = 0x02,                             // Keyboard in SHIFT lock mode.
    KEYB_CTRL                        = 0x03,                             // Keyboard in Control mode.
    KEYB_GRAPHMODE                   = 0x04,                             // Keyboard in Graphics mode.
};

// Keyboard dual key modes. This is for hosts whose keyboards dont support the basic key set or when a key needs to have dual functionality.
enum KEYBOARD_DUALMODES {
    KEYB_DUAL_NONE                   = 0x00,                             // No dual key modes active.
    KEYB_DUAL_GRAPH                  = 0x01,                             // MZ-80A, no Alpha key, only Graph, so double function required.
};

// Mapping table from Sharp MZ80A Ascii to real Ascii.
//
typedef struct {
    uint8_t                          asciiCode;
} t_asciiMap;

// Mapping table from Ascii to Sharp MZ display code.
//
typedef struct {
    uint8_t                          dispCode;
} t_dispCodeMap;

// Mapping table from keyboard scan codes to Sharp MZ keys.
//
typedef struct {
    uint8_t                          scanCode[KEY_SCAN_ROWS*8];
} t_scanCodeMap;

// Mapping table of a sharp keycode to an ANSI escape sequence string.
//
typedef struct {
    uint8_t                          key;
    const char*                      ansiKeySequence;
} t_ansiKeyMap;

// Structure to maintain the Sharp MZ display output parameters and data.
//
typedef struct {
    uint8_t                          displayAttr;                        // Attributes for each character in the display.
    uint16_t                         backingRow;                         // Maximum backing RAM row, allows for a larger virtual backing display with the physical display acting as a window.

    // Location on the physical display to output data. displayCol is also used in the backing store.
    uint8_t                          displayRow;
    uint8_t                          displayCol;

    // History and backing display store. The physical display outputs a portion of this backing store.
    uint8_t                          displayCharBuf[VC_DISPLAY_BUFFER_SIZE];
    uint8_t                          displayAttrBuf[VC_DISPLAY_BUFFER_SIZE];

    // Maxims, dynamic to allow for future changes.
    uint8_t                          maxBackingRow;
    uint8_t                          maxDisplayRow;
    uint8_t                          maxBackingCol;

    // Features.
    uint8_t                          lineWrap;                           // Wrap line at display edge (1) else stop printing at display edge.
    uint8_t                          useAnsiTerm;                        // Enable (1) Ansi Terminal Emulator, (0) disable.
    uint8_t                          inDebug;                            // Prevent recursion when outputting debug information.

    // Working variables.
    uint8_t                          hwVideoMode;                        // Physical configuration of the video control register.
} t_displayBuffer;

// Structure for maintaining the Sharp MZ keyboard parameters and data. Used to retrieve and map a key along with associated
// attributes such as cursor flashing.
//
typedef struct {
    uint8_t                          scanbuf[2][KEY_SCAN_ROWS];
    uint8_t                          keydown[KEY_SCAN_ROWS];
    uint8_t                          keyup[KEY_SCAN_ROWS];
    uint8_t                          keyhold[KEY_SCAN_ROWS];
    uint32_t                         holdTimer;
    uint8_t                          breakKey;                           // Break key pressed.
    uint8_t                          ctrlKey;                            // Ctrl key pressed.
    uint8_t                          shiftKey;                           // Shift key pressed.
    uint8_t                          repeatKey;
    uint8_t                          autorepeat;
    enum KEYBOARD_MODES              mode;                               // Keyboard mode and index into mapping table for a specific map set.
    enum KEYBOARD_DUALMODES          dualmode;                           // Keyboard dual key override modes.
    uint8_t                          keyBuf[MAX_KEYB_BUFFER_SIZE];       // Keyboard buffer.
    uint8_t                          keyBufPtr;                          // Pointer into the keyboard buffer for stored key,
    uint8_t                          cursorOn;                           // Flag to indicate Cursor is switched on.
    uint8_t                          displayCursor;                      // Cursor being displayed = 1
    uint32_t                         flashTimer;                         // Timer to indicate next flash time for cursor.
} t_keyboard;

// Structure for maintaining the Sharp MZ Audio parameters and data.
typedef struct {
    uint32_t                         audioStopTimer;                     // Timer to disable audio once elapsed period has expired. Time in ms.
} t_audio;

// Structure for module control parameters.
typedef struct {
    uint8_t                          suspendIO;                          // Suspend physical I/O.
    uint8_t                          debug;                              // Enable debugging features.
} t_control;

// Structure to maintain the Ansi Terminal Emulator state and parameters.
//
typedef struct {
	enum {
		ANSITERM_ESC,
		ANSITERM_BRACKET,
		ANSITERM_PARSE,
	}                                state;                              // States and current state of the FSM parser.
    uint8_t                          charcnt;                            // Number of characters read into the buffer.
    uint8_t                          paramcnt;                           // Number of parameters parsed and stored.
    uint8_t                          setDisplayMode;                     // Display mode command detected.
    uint8_t                          setExtendedMode;                    // Extended mode command detected.
    uint8_t                          charbuf[80];                        // Storage for the parameter characters as they are received.
    uint16_t                         param[10];                          // Parsed parameters.
    uint8_t                          saveRow;                            // Store the current row when requested.
    uint8_t                          saveCol;                            // Store the current column when requested.
    uint8_t                          saveDisplayRow;                     // Store the current display buffer row when requested.
} t_AnsiTerm;

// Application execution constants.
//

// Prototypes.
//
uint8_t                              mzInitMBHardware(void);
uint8_t                              mzInit(void);
void                                 mzBeep(uint32_t, uint32_t);
uint8_t                              mzMoveCursor(enum CURSOR_POSITION, uint8_t);
uint8_t                              mzSetCursor(uint8_t, uint8_t);    
int                                  mzPutChar(char);
int                                  mzPutRaw(char);
uint8_t                              mzSetAnsiAttribute(uint8_t);
int                                  mzAnsiTerm(char);
int                                  mzPrintChar(char);
uint8_t                              mzFlashCursor(enum CURSOR_STATES);
uint8_t                              mzPushKey(char *);
int                                  mzGetKey(uint8_t);
int                                  mzGetChar(void);
void                                 mzClearDisplay(uint8_t, uint8_t);
void                                 mzClearLine(int, int, int, uint8_t);
uint8_t                              mzGetDisplayWidth(void);
uint8_t                              mzSetDisplayWidth(uint8_t);
uint8_t                              mzSetMachineVideoMode(uint8_t);
void                                 mzRefreshDisplay(void);
uint8_t                              mzScrollUp(uint8_t, uint8_t, uint8_t);
uint8_t                              mzScrollDown(uint8_t);
void                                 mzDebugOut(uint8_t, uint8_t);
void                                 mzSuspendIO(void);
void                                 mzResumeIO(void);
void                                 mzWriteString(uint8_t, uint8_t, char *, int);
void                                 mzService(void);

// Getter/Setter methods!

#ifdef __cplusplus
}
#endif
#endif // SHARPMZ_H
