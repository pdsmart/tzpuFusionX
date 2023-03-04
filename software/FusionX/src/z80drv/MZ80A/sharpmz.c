/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            sharpmz.c
// Created:         December 2020
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     The Sharp MZ library.
//                  This file contains methods which allow the ZPU to access and control the Sharp MZ
//                  series computer hardware. The ZPU is instantiated within a physical Sharp MZ machine
//                  or an FPGA hardware emulation and provides either a host CPU running zOS or as an 
//                  I/O processor providing services.
//
//                  NB. This library is NOT yet thread safe.
// Credits:         
// Copyright:       (c) 2019-2020 Philip Smart <philip.smart@net2net.org>
//
// History:         v1.0 Dec 2020  - Initial write of the Sharp MZ series hardware interface software.
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
#include "z80menu.h"

#include <gpio_table.h>
#include <asm/io.h>
#include <infinity2m/gpio.h>
#include <infinity2m/registers.h>
#include "osd.h"
#include "sharpmz.h"

#ifndef __APP__        // Protected methods which should only reside in the kernel.
   
// --------------------------------------------------------------------------------------------------------------
// Static data declarations.
// --------------------------------------------------------------------------------------------------------------

// Global scope variables used within the zOS kernel.
//
uint32_t volatile *ms;
t_z80Control      z80Control;
t_osControl       osControl;
t_svcControl      *svcControl = TZSVC_CMD_STRUCT_ADDR_ZOS;

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

static t_scanCodeMap scanCodeMap[] = {
    // NO SHIFT
    {
      //  S0   00 - 07
        ESC    ,                                             //  SPARE - Allocate as Escape
        GRAPHKEY ,                                           //  GRAPH
        '_'      ,                                           //  Pound/Down Arrow
        ALPHAKEY ,                                           //  ALPHA
        NOKEY    ,                                           //  NO
        ';'      ,                                           //  +
        ':'      ,                                           //  *
        CR       ,                                           //  CR
        // S1   08 - 0F
        'y'      ,                                           //  y
        'z'      ,                                           //  z
        '@'      ,                                           //  `
        '['      ,                                           //  {
        ']'      ,                                           //  }
        NOKEY    ,                                           //  NULL
        NOKEY    ,                                           //  NULL
        NOKEY    ,                                           //  NULL
        // S2   10 - 17
        'q'      ,                                           //  q
        'r'      ,                                           //  r
        's'      ,                                           //  s
        't'      ,                                           //  t
        'u'      ,                                           //  u
        'v'      ,                                           //  v
        'w'      ,                                           //  w
        'x'      ,                                           //  x
        // S3   18 - 1F
        'i'      ,                                           //  i
        'j'      ,                                           //  j
        'k'      ,                                           //  k
        'l'      ,                                           //  l
        'm'      ,                                           //  m
        'n'      ,                                           //  n
        'o'      ,                                           //  o
        'p'      ,                                           //  p
        // S4   20 - 27
        'a'      ,                                           //  a
        'b'      ,                                           //  b
        'c'      ,                                           //  c
        'd'      ,                                           //  d
        'e'      ,                                           //  e
        'f'      ,                                           //  f
        'g'      ,                                           //  g
        'h'      ,                                           //  h
        // S5   28 - 2F
        '1'      ,                                           //  1
        '2'      ,                                           //  2
        '3'      ,                                           //  3
        '4'      ,                                           //  4
        '5'      ,                                           //  5
        '6'      ,                                           //  6
        '7'      ,                                           //  7
        '8'      ,                                           //  8
        // S6   30 - 37
        '\\'     ,                                           //  Backslash
        CURSUP   ,                                           //  
        '-'      ,                                           //  -
        ' '      ,                                           //  SPACE
        '0'      ,                                           //  0
        '9'      ,                                           //  9
        ','      ,                                           //  ,
        '.'      ,                                           //  .
        // S7   38 - 3F
        INSERT   ,                                           //  INST.
        DELETE   ,                                           //  DEL.
        CURSUP   ,                                           //  CURSOR UP
        CURSDOWN ,                                           //  CURSOR DOWN
        CURSRIGHT,                                           //  CURSOR RIGHT
        CURSLEFT ,                                           //  CURSOR LEFT
        '?'      ,                                           //  Question Mark
        '/'      ,                                           //  Forward Slash
        // S8   40 - 47 - modifier keys.
        BACKS    ,                                           // BREAK - Backspace without modifiers, like standard ascii keyboards.
        NOKEY    ,                                           // CTRL
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,                                           // SHIFT
        // S9   48 - 4F - Function keys.
        FUNC1    ,                                           // Function key F1
        FUNC2    ,                                           // Function key F2
        FUNC3    ,                                           // Function key F3
        FUNC4    ,                                           // Function key F4
        FUNC5    ,                                           // Function key F5
        NOKEY    ,
        NOKEY    ,
        NOKEY    
    },
    // CAPS LOCK
    {
      // S0   00 - 07
        ESC    ,                                             //  SPARE - Allocate as Escape
        GRAPHKEY ,                                           // GRAPH
        0x58     ,                                           // 
        ALPHAKEY ,                                           // ALPHA
        NOKEY    ,                                           // NO
        ':'      ,                                           // ;
        ';'      ,                                           // :
        CR       ,                                           // CR
        // S1   08 - 0F
        'Y'      ,                                           // Y
        'Z'      ,                                           // Z
        '@'      ,                                           // @
        '['      ,                                           // [
        ']'      ,                                           // ]
        NOKEY    ,                                           // NULL
        NOKEY    ,                                           // NULL
        NOKEY    ,                                           // NULL
        // S2   10 - 17
        'Q'      ,                                           // Q
        'R'      ,                                           // R
        'S'      ,                                           // S
        'T'      ,                                           // T
        'U'      ,                                           // U
        'V'      ,                                           // V
        'W'      ,                                           // W
        'X'      ,                                           // X
        // S3   18 - 1F
        'I'      ,                                           // I
        'J'      ,                                           // J
        'K'      ,                                           // K
        'L'      ,                                           // L
        'M'      ,                                           // M
        'N'      ,                                           // N
        'O'      ,                                           // O
        'P'      ,                                           // P
        // S4   20 - 27
        'A'      ,                                           // A
        'B'      ,                                           // B
        'C'      ,                                           // C
        'D'      ,                                           // D
        'E'      ,                                           // E
        'F'      ,                                           // F
        'G'      ,                                           // G
        'H'      ,                                           // H
        // S5   28 - 2F
        '1'      ,                                           // 1
        '2'      ,                                           // 2
        '3'      ,                                           // 3
        '4'      ,                                           // 4
        '5'      ,                                           // 5
        '6'      ,                                           // 6
        '7'      ,                                           // 7
        '8'      ,                                           // 8
        // S6   30 - 37
        '\\'     ,                                           // Backslash
        CURSUP   ,                                           // 
        '-'      ,                                           // -
        ' '      ,                                           // SPACE
        '0'      ,                                           // 0
        '9'      ,                                           // 9
        ','      ,                                           // ,
        '.'      ,                                           // .
        // S7   38 - 3F
        INSERT   ,                                           // INST.
        DELETE   ,                                           // DEL.
        CURSUP   ,                                           // CURSOR UP
        CURSDOWN ,                                           // CURSOR DOWN
        CURSRIGHT,                                           // CURSOR RIGHT
        CURSLEFT ,                                           // CURSOR LEFT
        '?'      ,                                           // ?
        '/'      ,                                           // /
        // S8   40 - 47 - modifier keys.
        BACKS    ,                                           // BREAK - Backspace without modifiers, like standard ascii keyboards.
        NOKEY    ,                                           // CTRL
        NOKEY    ,                                                             
        NOKEY    ,                                                             
        NOKEY    ,                                                             
        NOKEY    ,                                                             
        NOKEY    ,                                                             
        NOKEY    ,                                           // SHIFT
        // S9   48 - 4F - Function keys.                                       
        FUNC1    ,                                           // Function key F1
        FUNC2    ,                                           // Function key F2
        FUNC3    ,                                           // Function key F3
        FUNC4    ,                                           // Function key F4
        FUNC5    ,                                           // Function key F5
        NOKEY    ,
        NOKEY    ,
        NOKEY    
    },
    // SHIFT LOCK.
    {
        // S0   00 - 07
        ESC    ,                                             //  SPARE - Allocate as Escape
        GRAPHKEY ,                                           //  GRAPH
        0x58     ,                                           //  
        ALPHAKEY ,                                           //  ALPHA
        NOKEY    ,                                           //  NO
        '+'      ,                                           //  ;
        '*'      ,                                           //  :
        CR       ,                                           //  CR
        // S1   08 - 0F
        'Y'      ,                                           //  Y
        'Z'      ,                                           //  Z
        '`'      ,                                           //  @
        '{'      ,                                           //  [
        '}'      ,                                           //  ]
        NOKEY    ,                                           //  NULL
        NOKEY    ,                                           //  NULL
        NOKEY    ,                                           //  NULL
        // S2   10 - 17
        'Q'      ,                                           //  Q
        'R'      ,                                           //  R
        'S'      ,                                           //  S
        'T'      ,                                           //  T
        'U'      ,                                           //  U
        'V'      ,                                           //  V
        'W'      ,                                           //  W
        'X'      ,                                           //  X
        // S3   18 - 1F
        'I'      ,                                           //  I
        'J'      ,                                           //  J
        'K'      ,                                           //  K
        'L'      ,                                           //  L
        'M'      ,                                           //  M
        'N'      ,                                           //  N
        'O'      ,                                           //  O
        'P'      ,                                           //  P
        // S4   20 - 27
        'A'      ,                                           //  A
        'B'      ,                                           //  B
        'C'      ,                                           //  C
        'D'      ,                                           //  D
        'E'      ,                                           //  E
        'F'      ,                                           //  F
        'G'      ,                                           //  G
        'H'      ,                                           //  H
        // S5   28 - 2F
        '!'      ,                                           //  !
        '"'      ,                                           //  "
        '#'      ,                                           //  #
        '$'      ,                                           //  $
        '%'      ,                                           //  %
        '&'      ,                                           //  &
        '\''     ,                                           //  '
        '('      ,                                           //  (
        // S6   30 - 37
        '|'      ,                                           //  Backslash
        '~'      ,                                           //  POND MARK
        '='      ,                                           //  YEN
        ' '      ,                                           //  SPACE
        ' '      ,                                           //  ¶
        ')'      ,                                           //  )
        '<'      ,                                           //  <
        '>'      ,                                           //  >
        // S7   38 - 3F
        CLRKEY   ,                                           //  CLR - END. - Clear screen.
        CURHOMEKEY,                                          //  HOME.      - Cursor to home.
        PAGEUP   ,                                           //  PAGE UP    - CURSOR UP
        PAGEDOWN ,                                           //  PAGE DOWN  - CURSOR DOWN
        ENDKEY   ,                                           //  END        - CURSOR RIGHT
        HOMEKEY  ,                                           //  HOME       - CURSOR LEFT
        '?'      ,                                           //  ?          - Question Mark
        '/'      ,                                           //  /          - Forward Slash
        // S8   40 - 47 - modifier keys.
        BREAKKEY ,                                           // BREAK - Shift+BREAK = BREAK
        NOKEY    ,                                           // CTRL
        NOKEY    ,                                                             
        NOKEY    ,                                                             
        NOKEY    ,                                                             
        NOKEY    ,                                                             
        NOKEY    ,                                                             
        NOKEY    ,                                           // SHIFT
        // S9   48 - 4F - Function keys.                                       
        FUNC6    ,                                           // Function key F1
        FUNC7    ,                                           // Function key F2
        FUNC8    ,                                           // Function key F3
        FUNC9    ,                                           // Function key F4
        FUNC10   ,                                           // Function key F5
        NOKEY    ,
        NOKEY    ,
        NOKEY    
    },
    // CONTROL CODE
    {
        // S0   00 - 07
        ESC      ,                                           // SPARE - Allocate as Escape
        DEBUGKEY ,                                           // GRAPH - Enable debugging output.
        CTRL_CAPPA ,                                         // ^      
        ANSITGLKEY,                                          // ALPHA - Toggle Ansi terminal emulator.
        NOKEY    ,                                           // NO
        NOKEY    ,                                           // ;
        NOKEY    ,                                           // :
        NOKEY    ,                                           // CR
        // S1   08 - 0F
        CTRL_Y   ,                                           // ^Y E3
        CTRL_Z   ,                                           // ^Z E4 (CHECKER)
        CTRL_AT  ,                                           // ^@
        CTRL_LB  ,                                           // ^[ EB/E5
        CTRL_RB  ,                                           // ^] EA/E7 
        NOKEY    ,                                           // #NULL
        NOKEY    ,                                           // #NULL
        NOKEY    ,                                           // #NULL
        // S2   10 - 17
        CTRL_Q   ,                                           // ^Q
        CTRL_R   ,                                           // ^R
        CTRL_S   ,                                           // ^S
        CTRL_T   ,                                           // ^T
        CTRL_U   ,                                           // ^U
        CTRL_V   ,                                           // ^V
        CTRL_W   ,                                           // ^W E1
        CTRL_X   ,                                           // ^X E2
        // S3   18 - 1F
        CTRL_I   ,                                           // ^I F9
        CTRL_J   ,                                           // ^J FA
        CTRL_K   ,                                           // ^K FB
        CTRL_L   ,                                           // ^L FC
        CTRL_M   ,                                           // ^M CD
        CTRL_N   ,                                           // ^N FE
        CTRL_O   ,                                           // ^O FF
        CTRL_P   ,                                           // ^P E0
        // S4   20 - 27
        CTRL_A   ,                                           // ^A F1
        CTRL_B   ,                                           // ^B F2
        CTRL_C   ,                                           // ^C F3
        CTRL_D   ,                                           // ^D F4
        CTRL_E   ,                                           // ^E F5
        CTRL_F   ,                                           // ^F F6
        CTRL_G   ,                                           // ^G F7
        CTRL_H   ,                                           // ^H F8
        // S5   28 - 2F
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        // S6   30 - 37 (ERROR? 7 VALUES ONLY!!)
        NOKEY    ,                                          // ^YEN E6
        CTRL_CAPPA ,                                        // ^    EF
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        CTRL_UNDSCR ,                                       // ^,
        NOKEY    ,
        NOKEY    ,
        // S7  - 38 - 3F
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        CTRL_SLASH ,                                        // ^/ EE
        // S8   40 - 47 - modifier keys.
        NOKEY    ,                                          // BREAK - CTRL+BREAK - not yet assigned
        NOKEY    ,                                          // CTRL
        NOKEY    ,                                                            
        NOKEY    ,                                                            
        NOKEY    ,                                                            
        NOKEY    ,                                                            
        NOKEY    ,                                                            
        NOKEY    ,                                          // SHIFT
        // S9   48 - 4F - Function keys.                                      
        FUNC1    ,                                          // Function key F1
        FUNC2    ,                                          // Function key F2
        FUNC3    ,                                          // Function key F3
        FUNC4    ,                                          // Function key F4
        FUNC5    ,                                          // Function key F5
        NOKEY    ,
        NOKEY    ,
        NOKEY    
    },
    // KANA
    {
        // S0   00 - 07
        0xBF     ,                                          //  SPARE
        NOKEY    ,                                          //  GRAPH BUT NULL
        0xCF     ,                                          //  NIKO WH.
        0xC9     ,                                          //  ALPHA
        NOKEY    ,                                          //  NO
        0xB5     ,                                          //  MO
        0x4D     ,                                          //  DAKU TEN
        0xCD     ,                                          //  CR
        // S1   08 - 0F
        0x35     ,                                          //  HA
        0x77     ,                                          //  TA
        0xD7     ,                                          //  WA
        0xB3     ,                                          //  YO
        0xB7     ,                                          //  HANDAKU
        NOKEY    ,
        NOKEY    ,
        NOKEY    ,
        // S2   10 - 17
        0x7C     ,                                          //  KA
        0x70     ,                                          //  KE
        0x41     ,                                          //  SHI
        0x31     ,                                          //  KO
        0x39     ,                                          // HI
        0xA6     ,                                          //  TE
        0x78     ,                                          //  KI
        0xDD     ,                                          //  CHI
        // S3   18 - 1F
        0x3D     ,                                          //  FU
        0x5D     ,                                          //  MI
        0x6C     ,                                          //  MU
        0x56     ,                                          //  ME
        0x1D     ,                                          //  RHI
        0x33     ,                                          //  RA
        0xD5     ,                                          //  HE
        0xB1     ,                                          //  HO
        // S4   20 - 27
        0x46     ,                                          //  SA
        0x6E     ,                                          //  TO
        0xD9     ,                                          //  THU
        0x48     ,                                          //  SU
        0x74     ,                                          //  KU
        0x43     ,                                          //  SE
        0x4C     ,                                          //  SO
        0x73     ,                                          //  MA
        // S5   28 - 2F
        0x3F     ,                                          //  A
        0x36     ,                                          //  I
        0x7E     ,                                          //  U
        0x3B     ,                                          //  E
        0x7A     ,                                          //  O
        0x1E     ,                                          //  NA
        0x5F     ,                                          //  NI
        0xA2     ,                                          //  NU
        // S6   30 - 37
        0xD3     ,                                          //  YO
        0x9F     ,                                          //  YU
        0xD1     ,                                          //  YA
        0x00     ,                                          //  SPACE
        0x9D     ,                                          //  NO
        0xA3     ,                                          //  NE
        0xD0     ,                                          //  RU
        0xB9     ,                                          //  RE
        // S7   38 - 3F
        0xC6     ,                                          //  ?CLR
        0xC5     ,                                          //  ?HOME
        0xC2     ,                                          //  ?CURSOR UP
        0xC1     ,                                          //  ?CURSOR DOWN
        0xC3     ,                                          //  ?CURSOR RIGHT
        0xC4     ,                                          //  ?CURSOR LEFT 
        0xBB     ,                                          //  DASH
        0xBE     ,                                          //  RO
        // S8   40 - 47 - modifier keys.
        NOKEY    ,                                          // BREAK - GRPH+BREAK - not yet assigned
        NOKEY    ,                                          // CTRL
        NOKEY    ,                                                            
        NOKEY    ,                                                            
        NOKEY    ,                                                            
        NOKEY    ,                                                            
        NOKEY    ,                                                            
        NOKEY    ,                                          // SHIFT
        // S9   48 - 4F - Function keys.                                      
        FUNC1    ,                                          // Function key F1
        FUNC2    ,                                          // Function key F2
        FUNC3    ,                                          // Function key F3
        FUNC4    ,                                          // Function key F4
        FUNC5    ,                                          // Function key F5
        NOKEY    ,
        NOKEY    ,
        NOKEY    
    } 
};

// Mapping table of sharp special control keys to ANSI ESCape sequences.
//
static t_ansiKeyMap ansiKeySeq[] = {
    { HOMEKEY    ,    "\x1b[1~"  },                         // HOME - Cursor to home.
    { CURSUP     ,    "\x1b[A"   },                         // CURSOR UP
    { CURSDOWN   ,    "\x1b[B"   },                         // CURSOR DOWN
    { CURSRIGHT  ,    "\x1b[C"   },                         // CURSOR RIGHT
    { CURSLEFT   ,    "\x1b[D"   },                         // CURSOR LEFT
    { FUNC1      ,    "\x1b[10~" },                         // Function key 1
    { FUNC2      ,    "\x1b[11~" },                         // Function key 2
    { FUNC3      ,    "\x1b[12~" },                         // Function key 3
    { FUNC4      ,    "\x1b[13~" },                         // Function key 4
    { FUNC5      ,    "\x1b[14~" },                         // Function key 5
    { FUNC6      ,    "\x1b[15~" },                         // Function key 6
    { FUNC7      ,    "\x1b[17~" },                         // Function key 7
    { FUNC8      ,    "\x1b[18~" },                         // Function key 8
    { FUNC9      ,    "\x1b[19~" },                         // Function key 9
    { FUNC10     ,    "\x1b[20~" },                         // Function key 10 
    { INSERT     ,    "\x1b[2~"  },                         // Insert.
    { DELETE     ,    "\x1b[3~"  },                         // Delete.
    { ENDKEY     ,    "\x1b[F"   },                         // End Key.
    { PAGEUP     ,    "\x1b[5~"  },                         // Page Up.
    { PAGEDOWN   ,    "\x1b[6~"  },                         // Page Down.
};

// Static structures for controlling and managing hardware features.
// Display control structure. Used to manage the display including the Ansi Terminal.
static t_displayBuffer display = { .screenAttr = 0x71, .screenRow = 0, .displayCol = 0, .displayRow = 0, .maxScreenRow = (VC_DISPLAY_BUFFER_SIZE / VC_MAX_COLUMNS),
                                   .maxDisplayRow = VC_MAX_ROWS, .maxScreenCol = VC_MAX_COLUMNS, .useAnsiTerm = 1, .lineWrap = 0, .debug = 0, .inDebug = 0 };
// Keyboard control structure. Used to manage keyboard sweep, mapping and store.
static t_keyboard keyboard = { .holdTimer = 0L, .autorepeat = 0, .mode = KEYB_LOWERCASE, .cursorOn = 1, .flashTimer = 0L, .keyBuf[0] = 0x00, .keyBufPtr = 0 };

// AnsiTerminal control structure. Used to manage the inbuilt Ansi Terminal.
static t_AnsiTerm ansiterm = { .state = ANSITERM_ESC, .charcnt = 0, .paramcnt = 0, .setScreenMode = 0, .setExtendedMode = 0, .saveRow = 0, .saveCol = 0 };
// Colour map for the Ansi Terminal.
const unsigned char ansiColourMap[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };


// --------------------------------------------------------------------------------------------------------------
// Methods
// --------------------------------------------------------------------------------------------------------------

// Method to configure the motherboard hardware after a reset.
//
uint8_t mzInitMBHardware(void)
{
    // From the 1Z-013A monitor code, initialise the 8255 PIO.
    //
    *(volatile uint8_t *)(MBADDR_8BIT_KEYPF) = 0x8A;        // 10001010 CTRL WORD MODE0
    *(volatile uint8_t *)(MBADDR_8BIT_KEYPF) = 0x07;        // PC3=1 M-ON
    *(volatile uint8_t *)(MBADDR_8BIT_KEYPF) = 0x05;        // PC2=1 INTMSK
    *(volatile uint8_t *)(MBADDR_8BIT_KEYPF) = 0x01;        // TZ: Enable VGATE

    // Initialise the 8253 timer.
    *(volatile uint8_t *)(MBADDR_8BIT_CONTF) = 0x74;        // From monitor, according to system clock.
    *(volatile uint8_t *)(MBADDR_8BIT_CONTF) = 0xB0;        //
    // Set timer in seconds, default to 0.
    *(volatile uint8_t *)(MBADDR_8BIT_CONT2) = 0x00;        // Timer 2 is the number of seconds.
    *(volatile uint8_t *)(MBADDR_8BIT_CONT2) = 0x00;        //
    // Set timer in seconds, default to 0.
    *(volatile uint8_t *)(MBADDR_8BIT_CONT1) = 0x0A;        // Timer 1 is set to 640.6uS pulse into timer 2.
    *(volatile uint8_t *)(MBADDR_8BIT_CONT1) = 0x00;        //
    // Set timer timer to run.
    *(volatile uint8_t *)(MBADDR_8BIT_CONTF) = 0x80;        //

    return(0);
}

// Method to initialise the Sharp MZ extensions.
//
uint8_t mzInit(void)
{
    // Initialise Sharp MZ hardware.
    mzInitMBHardware();

    // Clear and setup the screen mode and resolution.
    mzClearScreen(3, 1);
    mzSetMachineVideoMode(VMMODE_MZ700);
    mzSetVGAMode(VMMODE_VGA_640x480);
    mzSetVGABorder(VMBORDER_BLUE);
    mzSetScreenWidth(80);

    return(0);
}

// Method to clear the screen.
// mode: 0 = clear from cursor to end of screen.
//       1 = clear from 0,0 to cursor.
//       2 = clear entire screen
//       3 = clear entire screen and reset scroll buffer.
//
void mzClearScreen(uint8_t mode, uint8_t updPos)
{
    // Locals.
    uint32_t dstVRAMStartAddr;
    uint32_t dstVRAMEndAddr;
    uint32_t dstARAMStartAddr;
    uint32_t startIdx;
    uint32_t endIdx;

    // Sanity checks.
    if(mode > 3)
        return;

    switch(mode)
    {
        // Clear from cursor to end of screen.
        case 0:
            dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR+(display.displayRow*VC_MAX_COLUMNS)+display.displayCol;
            dstVRAMEndAddr   = VIDEO_VRAM_BASE_ADDR + VIDEO_VRAM_SIZE;
            dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR+(display.displayRow*VC_MAX_COLUMNS)+display.displayCol; 
            startIdx = (((display.screenRow < display.maxDisplayRow ? display.displayRow : (display.screenRow - display.maxDisplayRow + display.displayRow))) * display.maxScreenCol) + display.displayCol;
            endIdx   = startIdx + ((display.maxScreenCol*display.maxDisplayRow) - ((display.screenRow < display.maxDisplayRow ? display.displayRow : display.screenRow ) * display.maxScreenCol));
            break;

        // Clear from beginning of screen to cursor.
        case 1:
            dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR;
            dstVRAMEndAddr   = VIDEO_VRAM_BASE_ADDR+(display.displayRow*VC_MAX_COLUMNS)+display.displayCol;
            dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR;
            startIdx = ((display.screenRow < display.maxDisplayRow ? display.screenRow  : (display.screenRow - display.maxDisplayRow)) * display.maxScreenCol);
            endIdx   = ((display.screenRow < display.maxDisplayRow ? display.displayRow : (display.screenRow - display.maxDisplayRow + display.displayRow)) * display.maxScreenCol) + display.displayCol;
            break;

        // Clear entire screen.
        case 2:
            dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR;
            dstVRAMEndAddr   = VIDEO_VRAM_BASE_ADDR + VIDEO_VRAM_SIZE;
            dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR;
            startIdx = ((display.screenRow < display.maxDisplayRow ? display.screenRow  : (display.screenRow - display.maxDisplayRow)) * display.maxScreenCol);
            endIdx   = startIdx + (display.maxScreenCol*display.maxDisplayRow);
            if(updPos)
            {
                display.displayRow = 0;
                display.displayCol = 0;
            }
            break;

        // Clear entire screen including scrollback buffer.
        default:
            dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR;
            dstVRAMEndAddr   = VIDEO_VRAM_BASE_ADDR+VIDEO_VRAM_SIZE;
            dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR;
            startIdx = 0;
            endIdx   = VC_DISPLAY_BUFFER_SIZE;
            // Reset parameters to start of screen.
            if(updPos)
            {
                display.displayRow = 0;
                display.displayCol = 0;
                display.screenRow = 0;
            }
            break;
    }

    // Clear the physical character display and attribute RAM.
    // Select 32bit or 8 bit clear depending on the start/end position.
    //
    if((dstVRAMStartAddr&0x3) == 0 && (dstVRAMEndAddr&0x3) ==0)
    {
        uint32_t screenAttr = display.screenAttr << 24 | display.screenAttr << 16 | display.screenAttr << 8 | display.screenAttr;
        for(uint32_t dstVRAMAddr=dstVRAMStartAddr, dstARAMAddr = dstARAMStartAddr; dstVRAMAddr < dstVRAMEndAddr; dstVRAMAddr+=4, dstARAMAddr+=4)
        {
            *(uint32_t *)(dstVRAMAddr) = 0x00000000;
            *(uint32_t *)(dstARAMAddr) = screenAttr;
        }
    } else
    {
        for(uint32_t dstVRAMAddr=dstVRAMStartAddr, dstARAMAddr = dstARAMStartAddr; dstVRAMAddr <= dstVRAMEndAddr; dstVRAMAddr+=1, dstARAMAddr+=1)
        {
            *(uint8_t *)(dstVRAMAddr) = 0x00;
            *(uint8_t *)(dstARAMAddr) = display.screenAttr;
        }
    }
    // Clear the shadow display scrollback RAM.
    for(uint32_t dstAddr = startIdx; dstAddr < endIdx; dstAddr++)
    {
        display.screenCharBuf[dstAddr] = 0x20;
        display.screenAttrBuf[dstAddr] = display.screenAttr;
    }

    return;
}

// Method to clear a line, starting at a given column.
void mzClearLine(int row, int colStart, int colEnd, uint8_t updPos)
{
    // Locals.
    uint8_t newRow      = row;
    uint8_t newColStart = colStart;
    uint8_t newColEnd   = colEnd;

    // Adjust the parameters, -1 indicates use current position.
    if(row == -1)      newRow = display.displayRow;
    if(colStart == -1) newColStart = 0;
    if(colEnd == -1)   newColEnd = display.maxScreenCol-1;

    // Sanity checks.
    if(newRow > display.maxDisplayRow || newColStart > display.maxScreenCol || newColEnd > display.maxScreenCol || newColEnd <= newColStart)
        return;
   
    // Clear the physical character display and attribute RAM.
    uint32_t screenAttr = display.screenAttr << 24 | display.screenAttr << 16 | display.screenAttr << 8 | display.screenAttr;
    uint32_t dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR+(newRow*VC_MAX_COLUMNS)+newColStart;
    uint32_t dstVRAMEndAddr   = dstVRAMStartAddr + newColEnd;
    uint32_t dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR+(newRow*VC_MAX_COLUMNS)+newColStart; 

    // Select 32bit or 8 bit clear depending on the start/end position.
    //
    if((dstVRAMStartAddr&0x3) == 0 && (dstVRAMEndAddr&0x3) ==0)
    {
        uint32_t screenAttr = display.screenAttr << 24 | display.screenAttr << 16 | display.screenAttr << 8 | display.screenAttr;
        for(uint32_t dstVRAMAddr=dstVRAMStartAddr, dstARAMAddr = dstARAMStartAddr; dstVRAMAddr < dstVRAMEndAddr; dstVRAMAddr+=4, dstARAMAddr+=4)
        {
            *(uint32_t *)(dstVRAMAddr) = 0x00000000;
            *(uint32_t *)(dstARAMAddr) = screenAttr;
        }
    } else
    {
        for(uint32_t dstVRAMAddr=dstVRAMStartAddr, dstARAMAddr = dstARAMStartAddr; dstVRAMAddr <= dstVRAMEndAddr; dstVRAMAddr+=1, dstARAMAddr+=1)
        {
            *(uint8_t *)(dstVRAMAddr) = 0x00;
            *(uint8_t *)(dstARAMAddr) = display.screenAttr;
        }
    }

    // Clear the shadow display scrollback RAM.
    uint32_t startIdx = (((display.screenRow < display.maxDisplayRow ? newRow : (display.screenRow - display.maxDisplayRow + newRow))) * display.maxScreenCol) + newColStart;
    for(uint32_t dstAddr = startIdx; dstAddr <= startIdx + newColEnd; dstAddr++)
    {
        display.screenCharBuf[dstAddr] = 0x20;
        display.screenAttrBuf[dstAddr] = display.screenAttr;
    }

    // Update the screen pointer if needed.
    if(updPos)
    {
        display.displayRow = newRow;
        display.displayCol = newColEnd;
    }
    return;
}

// Method to set the VGA Border colour when running in a VGA mode where the output display doesnt match the VGA display leaving blank pixels.
uint8_t mzSetVGABorder(uint8_t vborder)
{
    // Locals.
    uint8_t mode = (uint8_t)*(volatile uint32_t *)(VCADDR_32BIT_VMVGATTR) & VMBORDER_MASK;
   
    // Sanity check parameters.
    if(vborder != VMBORDER_BLACK && vborder != VMBORDER_BLUE && vborder != VMBORDER_RED && vborder != VMBORDER_PURPLE && vborder != VMBORDER_GREEN && vborder != VMBORDER_CYAN && vborder != VMBORDER_YELLOW && vborder != VMBORDER_WHITE)
        return(1);
   
    // Set the VGA Border.
    *(volatile uint8_t *)(VCADDR_8BIT_VMVGATTR) = mode | vborder;

    return(0);
}

// Method to set the VGA mode.
uint8_t mzSetVGAMode(uint8_t vgamode)
{
    // Locals.
    uint8_t mode = (uint8_t)*(volatile uint32_t *)(VCADDR_32BIT_VMVGAMODE) & VMMODE_VGA_MASK;

    // Sanity check parameters.
    if(vgamode != VMMODE_VGA_OFF && vgamode != VMMODE_VGA_640x480 && vgamode != VMMODE_VGA_800x600)
        return(1);
    
    // Set the VGA mode.
    *(volatile uint8_t *)(VCADDR_8BIT_VMVGAMODE) = mode | vgamode;

    return(0);
}

// Method to set the screen mode, ie. machine video being emulated.
//
uint8_t mzSetMachineVideoMode(uint8_t vmode)
{
    // Locals.
    uint8_t mode = (uint8_t)*(volatile uint32_t *)(VCADDR_32BIT_VMCTRL) & VMMODE_MASK;

    // Sanity check parameters.
    if(vmode != VMMODE_MZ80K && vmode != VMMODE_MZ80C && vmode != VMMODE_MZ1200 && vmode != VMMODE_MZ80A && vmode != VMMODE_MZ700 && vmode != VMMODE_MZ1500 && vmode != VMMODE_MZ800 && vmode != VMMODE_MZ80B && vmode != VMMODE_MZ2000 && vmode != VMMODE_MZ2200 && vmode != VMMODE_MZ2500) 
        return(1);
    
    // Set the hardware video mode.
    *(volatile uint8_t *)(VCADDR_8BIT_VMCTRL) = mode | hwmode;

    return(0);
}

// Method to return the character based screen width.
//
uint8_t mzGetScreenWidth(void)
{
    return(display.maxScreenCol);
}

// Method to setup the character based screen width.
//
uint8_t mzSetScreenWidth(uint8_t width)
{
    // Locals.
    uint8_t mode = (uint8_t)*(volatile uint32_t *)(VCADDR_32BIT_VMCTRL) & VMMODE_80CHAR_MASK;

    // Sanity check parameters.
    if(width != 40 && width != 80)
        return(1);
    
    // Toggle the 40/80 bit according to requirements.
    if(width == 40)
    {
        *(volatile uint8_t *)(VCADDR_8BIT_VMCTRL) = mode;
        display.maxScreenCol = 40;
    }
    else
    {
        *(volatile uint8_t *)(VCADDR_8BIT_VMCTRL) = mode | VMMODE_80CHAR;
        display.maxScreenCol = 80;
    }

    return(0);
}

// Method to refresh the screen from the scrollback buffer contents.
void mzRefreshScreen(void)
{
    // Refresh the screen with buffer window contents
    uint32_t startIdx = (display.screenRow < display.maxDisplayRow ? 0 : (display.screenRow - display.maxDisplayRow)+1) * display.maxScreenCol;
    for(uint32_t srcIdx = startIdx, dstVRAMAddr = VIDEO_VRAM_BASE_ADDR, dstARAMAddr = VIDEO_ARAM_BASE_ADDR; srcIdx < startIdx+(display.maxDisplayRow*display.maxScreenCol); srcIdx++)
    {
        *(uint8_t *)(dstVRAMAddr++) = dispCodeMap[display.screenCharBuf[srcIdx]].dispCode;
        *(uint8_t *)(dstARAMAddr++) = display.screenAttrBuf[srcIdx];
    }
}

// Method to scroll the screen contents upwards, either because new data is being added to the bottom or for scrollback.
//
uint8_t mzScrollUp(uint8_t lines, uint8_t clear)
{
    // Sanity check.
    if(lines > display.maxDisplayRow)
        return(1);
  
    // Restore cursor character before scrolling.
    mzFlashCursor(CURSOR_RESTORE);

    // Add the lines to the current row address. If the row exceeds the maximum then scroll the screen up.
    display.screenRow += lines;
    display.displayRow += lines;
    if(display.displayRow >= display.maxDisplayRow)
    {
        display.displayRow = display.maxDisplayRow - 1;
    }

    // At end of buffer? Shift up.
    if(display.screenRow >= display.maxScreenRow)
    {
        uint32_t srcAddr = (lines * display.maxScreenCol);
        uint32_t dstAddr = 0;
        for(; srcAddr < VC_DISPLAY_BUFFER_SIZE; srcAddr++, dstAddr++)
        {
            display.screenCharBuf[dstAddr] = display.screenCharBuf[srcAddr];
            display.screenAttrBuf[dstAddr] = display.screenAttrBuf[srcAddr];
        }
        for(; dstAddr < VC_DISPLAY_BUFFER_SIZE; dstAddr++)
        {
            display.screenCharBuf[dstAddr] = 0x20;
            display.screenAttrBuf[dstAddr] = display.screenAttr;
        }
        display.screenRow = display.maxScreenRow-1;
    }
    // If we havent scrolled at the end of the buffer then clear the lines scrolled if requested.
    else if(clear && display.displayRow == display.maxDisplayRow - 1)
    {
        uint32_t startIdx = (display.screenRow - lines + 1) * display.maxScreenCol;
        uint32_t endIdx   = startIdx + (lines * display.maxScreenCol);

        // Clear the shadow display scrollback RAM.
        for(uint32_t dstAddr = startIdx; dstAddr < endIdx; dstAddr++)
        {
            display.screenCharBuf[dstAddr] = 0x20;
            display.screenAttrBuf[dstAddr] = display.screenAttr;
        }
    }
       
    // Refresh the screen with buffer window contents
    mzRefreshScreen();
    return(0);
}

// Method to scroll the screen contents downwards for scrollback purposes.
uint8_t mzScrollDown(uint8_t lines)
{
    // Sanity check.
    if(lines > display.maxDisplayRow)
        return(1);

    // Restore cursor character before scrolling.
    mzFlashCursor(CURSOR_RESTORE);

    // Subtract the lines from the current row address. If the row falls below zero then scroll the screen down.
    if((((int)display.screenRow) - lines) < 0) { display.screenRow = 0; } 
    else if( display.screenRow < display.maxDisplayRow ) { display.screenRow = display.maxDisplayRow -1; }
    else { display.screenRow -= lines; }

    // Same for the physical row pointer.
    if((((int)display.displayRow) - lines) < 0) { display.displayRow = 0; } 
    else if( display.displayRow < display.maxDisplayRow ) { display.displayRow = display.maxDisplayRow -1; }
    else { display.displayRow -= lines; }

    // Refresh screen.
    mzRefreshScreen();
    return(0);
}

// Method to move the cursor within the phsyical screen buffer.
//
uint8_t mzMoveCursor(enum CURSOR_POSITION pos, uint8_t cnt)
{
    // Process according to command.
    //
    switch(pos)
    {
        case CURSOR_UP:
            display.displayRow = (int)(display.displayRow - cnt) < 0 ? 0 : display.displayRow-cnt;
            break;
        case CURSOR_DOWN:
            display.displayRow = (int)(display.displayRow + cnt) >= display.maxDisplayRow ? display.maxDisplayRow -1 : display.displayRow+cnt;
            break;
        case CURSOR_LEFT:
            display.displayCol = (int)(display.displayCol - cnt) < 0 ? 0 : display.displayCol-cnt;
            break;
        case CURSOR_RIGHT:
            display.displayCol = (int)(display.displayCol + cnt) >= display.maxScreenCol ? display.maxScreenCol -1 : display.displayCol+cnt;
            break;
        case CURSOR_COLUMN:
            if(cnt < display.maxScreenCol)
                display.displayCol = cnt;
            break;
        case CURSOR_NEXT_LINE:
            display.displayCol = 0;
            if(display.displayRow < display.maxDisplayRow-1)
                display.displayRow += 1;
            break;
        case CURSOR_PREV_LINE:
            display.displayCol = 0;
            if(display.displayRow > 0)
                display.displayRow -= 1;
            break;
        default:
            break;
    }
    return(0);
}

// Method to set the physical X/Y location of the cursor.
//
uint8_t mzSetCursor(uint8_t x, uint8_t y)
{
    display.screenRow  = y >= display.maxDisplayRow ? display.maxDisplayRow-1 : y;
    display.displayRow = y >= display.maxDisplayRow ? display.maxDisplayRow-1 : y;
    display.displayCol = x >= display.maxScreenCol  ? display.maxScreenCol-1 : x;
    return(0);
}


// Stream method to output a character to the display.
//
int mzPutChar(char c, FILE *stream)
{
    // Variables and locals.
    uint8_t  output = 1;
    uint32_t dispMemAddr;

    // Restore character under cursor before printing.
    mzFlashCursor(CURSOR_RESTORE);

    // Pre-process special characters.
    switch(c)
    {
        // Return to start of line?
        case CR:
            display.displayCol = 0;
            output = 0;
            break;

        // New line?
        case LF:
            // Increment line and scroll if necessary.
            mzScrollUp(1, 1);
            display.displayCol = 0;
            output = 0;
            break;

        // Backspace.
        case BACKS:
            display.displayCol = display.displayCol == 0 ? 0 : display.displayCol -1;
            output = 0;
            break;

        // Delete.
        case DELETE:
            display.displayCol = display.displayCol == 0 ? 0 : display.displayCol -1;
            mzPutChar(SPACE, stream);
            display.displayCol = display.displayCol == 0 ? 0 : display.displayCol -1;
            output = 0;
            break;

        // Tab - expand by printing whitespace.
        case TAB:
            for(uint8_t idx=0; idx < 4; idx++)
                mzPutChar(SPACE, stream);
            output = 0;
            break;

        // Scroll screen up.
        case SCROLL:
            mzScrollUp(1, 0);
            output = 0;
            break;
    }

    // Output to screen if flag set.
    if(output)
    {
        // Output character using default attributes.
        dispMemAddr = VIDEO_VRAM_BASE_ADDR + (display.displayRow * display.maxScreenCol) + display.displayCol;
        *(uint8_t *)(dispMemAddr) = (char)dispCodeMap[c].dispCode;
        display.screenCharBuf[(display.screenRow * display.maxScreenCol) + display.displayCol] = c;
        //
        dispMemAddr = VIDEO_ARAM_BASE_ADDR + (display.displayRow * display.maxScreenCol) + display.displayCol;
        *(uint8_t *)(dispMemAddr) = display.screenAttr;
        display.screenAttrBuf[(display.screenRow * display.maxScreenCol) + display.displayCol] = display.screenAttr;
        if(++display.displayCol >= display.maxScreenCol)
        {
            if(display.lineWrap)
            {
                // Increment line and scroll if necessary.
                display.displayCol = 0;
                mzScrollUp(1, 1);
            } else
            {
                display.displayCol = display.maxScreenCol-1;
            }
        }
    }

    if(display.debug && !display.inDebug) mzDebugOut(3, c);
    return(0);
}

// Stream method to output a character to the display. This method becomes the defacto output for system calls (ie. printf).
//
int mzPrintChar(char c, FILE *stream)
{
    // If the ansi terminal emulator is enabled, parse the character through the emulator.
    if(display.useAnsiTerm)
    {
        mzAnsiTerm(c);
    } else
    {
        mzPutChar(c, stream);
    }
    return(0);
}

// Method to put a character onto the screen without character interpretation.
//
int mzPutRaw(char c)
{
    // Variables and locals.
    uint32_t dispMemAddr;

    // Output character using default attributes.
    dispMemAddr = VIDEO_VRAM_BASE_ADDR + (display.displayRow * display.maxScreenCol) + display.displayCol;
    *(uint8_t *)(dispMemAddr) = (char)dispCodeMap[c].dispCode;
    display.screenCharBuf[(display.screenRow * display.maxScreenCol) + display.displayCol] = c;
    //
    dispMemAddr = VIDEO_ARAM_BASE_ADDR + (display.displayRow * display.maxScreenCol) + display.displayCol;
    *(uint8_t *)(dispMemAddr) = display.screenAttr;
    display.screenAttrBuf[(display.screenRow * display.maxScreenCol) + display.displayCol] = display.screenAttr;
    if(++display.displayCol >= display.maxScreenCol)
    {
        if(display.lineWrap)
        {
            // Increment line and scroll if necessary.
            display.displayCol = 0;
            mzScrollUp(1, 0);
        } else
        {
            display.displayCol = display.maxScreenCol-1;
        }
    }
    return(0);
}

// Method to process an ANSI ESCape Set Attribute value into a Sharp MZ attribute setting.
//
uint8_t mzSetAnsiAttribute(uint8_t attr)
{
    // Process attribute command byte and set the corresponding Sharp setting.
    switch(attr)
    {
        // Reset to default.
        case 0:
            display.screenAttr = VMATTR_FG_WHITE | VMATTR_BG_BLUE;
            break;
           
        // Invert FG/BG.
        case 7:
            // If background goes to white with default colours, it is not so readable so change foreground colour.
            if((display.screenAttr & VMATTR_FG_MASKIN) == VMATTR_FG_WHITE)
            {
                display.screenAttr = VMATTR_FG_WHITE | VMATTR_BG_RED;
            } else
            {
                display.screenAttr = (display.screenAttr & VMATTR_FG_MASKIN) >> 4 | (display.screenAttr & VMATTR_BG_MASKIN) << 4;
            }
            break;

        // Foreground black.
        case 30:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_BLACK;
            break;
            
        // Foreground red.
        case 31:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_RED;
            break;
           
        // Foreground green.
        case 32:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_GREEN;
            break;
            
        // Foreground yellow.
        case 33:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_YELLOW;
            break;
          
        // Foreground blue.
        case 34:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_BLUE;
            break;
            
        // Foreground magenta.
        case 35:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_PURPLE;
            break;
           
        // Foreground cyan.
        case 36:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_CYAN;
            break;
            
        // Foreground white.
        case 37:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_WHITE;
            break;
           
        // Default Foreground colour.
        case 39:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_WHITE;
            break;
           
        // Background black.
        case 40:
            display.screenAttr = (display.screenAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_BLACK;
            break;
            
        // Background red.
        case 41:
            display.screenAttr = (display.screenAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_RED;
            break;
           
        // Background green.
        case 42:
            display.screenAttr = (display.screenAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_GREEN;
            break;
            
        // Background yellow.
        case 43:
            display.screenAttr = (display.screenAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_YELLOW;
            break;
          
        // Background blue.
        case 44:
            display.screenAttr = (display.screenAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_BLUE;
            break;
            
        // Background magenta.
        case 45:
            display.screenAttr = (display.screenAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_PURPLE;
            break;
           
        // Background cyan.
        case 46:
            display.screenAttr = (display.screenAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_CYAN;
            break;
            
        // Background white.
        case 47:
            display.screenAttr = (display.screenAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_WHITE;
            break;
           
        // Default Background colour.
        case 49:
            display.screenAttr = (display.screenAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_BLUE;
            break;

        // Not supported.
        default:
            break;
    }
    return(0);
}

// Simple Ansi Terminal escape sequence parser. This translates ANSI Escape sequences output by programs such as the Kilo Editor
// into actual display updates.
//
int mzAnsiTerm(char c)
{
    // Locals.
    char *ptr;
    long result;

    // State machine to process the incoming character stream. Look for ANSI escape sequences and process else display the character.
	switch (ansiterm.state)
    {
        // Looking for an ESC character, if one is found move to the next state else just process the character for output.
		case ANSITERM_ESC:
        default:
            switch(c)
            {
                // Enhanced escape sequence start.
                case ESC:
                    ansiterm.charcnt = 0;
                    ansiterm.paramcnt = 0;
                    ansiterm.setScreenMode = 0;
                    ansiterm.setExtendedMode = 0;
                    ansiterm.state = ANSITERM_BRACKET;
                    break;
                   
                // Return to start of line?
                case CR:
                    display.displayCol = 0;
                    break;
                   
                // New line?
                case LF:
                    // Increment line and scroll if necessary.
                    mzScrollUp(1, 1);
                    display.displayCol = 0;
                    break;
                   
                // Backspace.
                case BACKS:
                    mzFlashCursor(CURSOR_RESTORE);
                    display.displayCol = display.displayCol == 0 ? 0 : display.displayCol -1;
                    break;

                // Delete.
                case DELETE:
                    mzFlashCursor(CURSOR_RESTORE);
                    display.displayCol = display.displayCol == 0 ? 0 : display.displayCol -1;
                    mzPutRaw(SPACE);
                    display.displayCol = display.displayCol == 0 ? 0 : display.displayCol -1;
                    break;

                // Tab - expand by printing whitespace.
                case TAB:
                    mzFlashCursor(CURSOR_RESTORE);
                    for(uint8_t idx=0; idx < 4; idx++)
                        mzPutRaw(SPACE);
                    break;

                default:
                    mzPutRaw(c);
                    break;
            }
			break;

        // Escape found, no look for next in the ESCape sequence or abort the state if a character is received which is not expected, print the character.
		case ANSITERM_BRACKET:
            switch(c)
            {
                case ESC:
                    break;

                case '[':
                    ansiterm.state = ANSITERM_PARSE;
                    break;

                case '7':
                    // Save the current cursor position.
                    ansiterm.saveRow = display.displayRow;
                    ansiterm.saveCol = display.displayCol;
                    ansiterm.saveScreenRow = display.screenRow;
                    ansiterm.state = ANSITERM_ESC;
                    break;

                case '8':
                    // Restore the current cursor position.
                    display.displayRow = ansiterm.saveRow;
                    display.displayCol = ansiterm.saveCol;
                    display.screenRow  = ansiterm.saveScreenRow;
                    ansiterm.state = ANSITERM_ESC;
                    break;

                default:
                    ansiterm.state = ANSITERM_ESC;
                    mzPutRaw(c);
                    break;
			}
			break;

        // Parse the ESCAPE sequence. If digits come in then these are parameters so store and parse into the parameter array. When a terminating command is received, execute the desired command.
        case ANSITERM_PARSE:
            // Multiple Escapes, or incomplete sequences. If an escape occurs then go back to looking for a bracket or similar as we are on a
            // different escape sequence.
            if(c == ESC)
            {
                ansiterm.state = ANSITERM_BRACKET;
            }
            else if(isdigit(c))
            {
                ansiterm.charbuf[ansiterm.charcnt++] = c;
                ansiterm.charbuf[ansiterm.charcnt] = 0x00;
            }
            else if(c == ';')
            {
                ptr = (char *)&ansiterm.charbuf;
                if(!xatoi(&ptr, &result))
                {
                    ansiterm.state = ANSITERM_ESC;
                } else
                {
                    ansiterm.param[ansiterm.paramcnt++] = (uint16_t)result;
                }
                ansiterm.charcnt = 0;
            }
            else if(c == '=')
            {
                ansiterm.setScreenMode = 1;
            }
            else if(c == '?')
            {
                ansiterm.setExtendedMode = 1;
            }
            else
            {
                // No semicolon so attempt to get the next parameter before processing command.
                if(ansiterm.charcnt > 0)
                {
                    ptr = (char *)&ansiterm.charbuf;
                    if(xatoi(&ptr, &result))
                    {
                        ansiterm.param[ansiterm.paramcnt++] = (uint16_t)result;
                    }
                }

                // Process the command now parameters have been retrieved.
                switch(c)
                {
                    // Position cursor.
                    case 'H':
                        // Set the cursor to given co-ordinates.
                        if(ansiterm.paramcnt >= 2)
                        {
                            mzSetCursor((uint8_t)ansiterm.param[1]-1, (uint8_t)ansiterm.param[0]-1);
                        } 
                        // Home cursor.
                        else if(ansiterm.paramcnt == 0)
                        {
                            mzSetCursor(0, 0);
                        }
                        break;

                    // Move cursor up.
                    case 'A':
                        if(ansiterm.paramcnt > 0)
                        {
                            mzMoveCursor(CURSOR_UP, (uint8_t)ansiterm.param[0]);
                        } else
                        {
                            mzMoveCursor(CURSOR_UP, 1);
                        }
                        break;

                    // Move cursor down.
                    case 'B':
                        if(ansiterm.paramcnt > 0)
                        {
                            mzMoveCursor(CURSOR_DOWN, (uint8_t)ansiterm.param[0]);
                        } else
                        {
                            mzMoveCursor(CURSOR_DOWN, 1);
                        }
                        break;

                    // Move cursor right.
                    case 'C':
                        if(ansiterm.paramcnt > 0)
                        {
                            mzMoveCursor(CURSOR_RIGHT, (uint8_t)ansiterm.param[0]);
                        } else
                        {
                            mzMoveCursor(CURSOR_RIGHT, 1);
                        }
                        break;

                    // Move cursor left.
                    case 'D':
                        if(ansiterm.paramcnt > 0)
                        {
                            mzMoveCursor(CURSOR_LEFT, (uint8_t)ansiterm.param[0]);
                        } else
                        {
                            mzMoveCursor(CURSOR_LEFT, 1);
                        }
                        break;

                    // Move cursor to start of next line.
                    case 'E':
                        mzMoveCursor(CURSOR_NEXT_LINE, 0);
                        break;

                    // Move cursor to start of previous line.
                    case 'F':
                        mzMoveCursor(CURSOR_PREV_LINE, 0);
                        break;

                    // Move cursor to absolute column position.
                    case 'G':
                        if(ansiterm.paramcnt > 0)
                        {
                            mzMoveCursor(CURSOR_COLUMN, (uint8_t)ansiterm.param[0]-1);
                        } else
                        {
                            mzMoveCursor(CURSOR_COLUMN, 0);
                        }
                        break;

                    // Scroll up.
                    case 'S':
                        if(ansiterm.paramcnt > 0)
                        {
                            mzScrollUp((uint8_t)ansiterm.param[0], 0);
                        } else
                        {
                            mzScrollUp(1, 0);
                        }
                        break;

                    // Scroll down.
                    case 'T':
                        if(ansiterm.paramcnt > 0)
                        {
                            mzScrollDown((uint8_t)ansiterm.param[0]);
                        } else
                        {
                            mzScrollDown(1);
                        }
                        break;

                    case 'R':
                        printf("Report Cursor:");
                        for(uint8_t idx=0; idx < ansiterm.paramcnt; idx++)
                            printf("%d,", ansiterm.param[idx]);
                        printf("\n");
                        break;

                    case 's':
                        // Save the current cursor position.
                        ansiterm.saveRow = display.displayRow;
                        ansiterm.saveCol = display.displayCol;
                        ansiterm.saveScreenRow = display.screenRow;
                        break;

                    case 'u':
                        // Restore the current cursor position.
                        display.displayRow = ansiterm.saveRow;
                        display.displayCol = ansiterm.saveCol;
                        display.screenRow  = ansiterm.saveScreenRow;
                        break;

                    // Report data.
                    case 'n': {
                        char response[MAX_KEYB_BUFFER_SIZE];

                        // Report current cursor position?
                        //
                        if(ansiterm.paramcnt > 0 && ansiterm.param[0] == 6)
                        {
                            // Build response and push onto keyboard buffer.
                            sprintf(response, "%c[%d;%dR", ESC, display.displayRow+1, display.displayCol+1);
                            mzPushKey(response);
                        }
                        } break;

                    // Clear Screen or block of screen.
                    case 'J': {
                        // Default is to clear the complete display but not scrollback buffer.
                        int clearMode = 2;
                        if(ansiterm.paramcnt > 0 && ansiterm.param[0] < 4)
                        {
                            clearMode = ansiterm.param[0];
                        }
                        mzClearScreen(clearMode, 1);
                        } break;

                    // Clear line.
                    case 'K': {
                        int clearRow      = -1;
                        int clearColStart = 0;
                        int clearColEnd   = display.maxScreenCol-1;
                        if(ansiterm.paramcnt > 0)
                        {
                            // 0 = clear cursor to end of line.
                            if(ansiterm.param[0] == 0)
                            {
                                clearColStart = display.displayCol;
                            }
                            // 1 = clear from beginning to cursor.
                            else if(ansiterm.param[0] == 1)
                            {
                                clearColEnd   = display.displayCol;
                            }
                            // 2 = clear whole line.
                        }
                        mzClearLine(clearRow, clearColStart, clearColEnd, 0);
                        } break;

                    // Set Display Attributes.
                    case 'm':
                        // Process all the attributes.
                        for(uint8_t idx=0; idx < ansiterm.paramcnt; idx++)
                        {
                            mzSetAnsiAttribute(ansiterm.param[idx]);
                        }
                        break;

                    case 'h':
                        // Show cursor?
                        if(ansiterm.paramcnt > 0 && ansiterm.param[0] == 25)
                        {
                            mzFlashCursor(CURSOR_ON);
                        }
                        break;

                    case 'l':
                        // Hide cursor?
                        if(ansiterm.paramcnt > 0 && ansiterm.param[0] == 25)
                        {
                            mzFlashCursor(CURSOR_OFF);
                        }
                        break;

                    default:
                        mzPutRaw(c);
                        ansiterm.state = ANSITERM_ESC;
                        break;
                }
                ansiterm.state = ANSITERM_ESC;

                if(display.debug && !display.inDebug) mzDebugOut(1, c);
            }
            break;
	}
	return(0) ;
}

// Method to output debug data to track issues with the display or ansi emulator.
//
void mzDebugOut(uint8_t set, uint8_t data1)
{
    // Save current coordinates.
    uint8_t sr = display.displayRow;
    uint8_t scr= display.screenRow;
    uint8_t sc = display.displayCol;
    uint8_t uat= display.useAnsiTerm;

    // Disable ansi terminal so we dont get recursion issues when we call printf. Disable further debug calls whilst data
    // is being output.
    display.useAnsiTerm = 0;
    display.inDebug = 1;

    switch(set)
    {
        // Debug data to show the escape sequence and parameters.
        case 1: {
            // Set location where debug data should be displayed.
            display.displayRow = 0;
            display.displayCol = 40;
            display.screenRow  = 0;

            // Output required data.
            printf("D:%d-%d-%d:%c:%d,%d,%d:", sr,sc,scr,data1,ansiterm.paramcnt, ansiterm.setScreenMode, ansiterm.setExtendedMode);
            for(uint8_t idx=0; idx < ansiterm.paramcnt; idx++)
                printf("%d,", ansiterm.param[idx]);
            printf("        ");

            // Set a delay so that the change can be seen.
            //TIMER_MILLISECONDS_UP = 0; while(TIMER_MILLISECONDS_UP < 50);
            } break;

        case 2: {
            // Set location where debug data should be displayed.
            display.displayRow = 1;
            display.displayCol = 40;
            display.screenRow  = 1;

            printf("K:%d:", strlen(keyboard.keyBuf));
            for(uint8_t idx=0; idx < strlen(keyboard.keyBuf); idx++)
                printf("%02x,", keyboard.keyBuf[idx]);

            // Set a delay so that the change can be seen.
            //TIMER_MILLISECONDS_UP = 0; while(TIMER_MILLISECONDS_UP < 100);
            } break;

        case 3: {
            // Set location where debug data should be displayed.
            display.displayRow = 2;
            display.displayCol = 40;
            display.screenRow  = 2;

            printf("X:%d,%d,%d,%d,%d,%d:%02x", sr, sc, scr, display.maxScreenRow, display.maxDisplayRow, display.maxScreenCol, data1);

            // Set a delay so that the change can be seen.
            //TIMER_MILLISECONDS_UP = 0; while(TIMER_MILLISECONDS_UP < 1000);
            } break;

        // No set defined, illegal call!
        default:
            break;
    }

    // Restore ansi emulation mode and disable further debug calls.
    display.useAnsiTerm = uat;
    display.inDebug = 0;
  
    // Restore coordinates
    display.displayRow = sr;
    display.screenRow  = scr;
    display.displayCol = sc;
    return;
}

// Method to flash a cursor at current X/Y location on the physical screen.
uint8_t mzFlashCursor(enum CURSOR_STATES state)
{
    // Locals.
    uint32_t dispMemAddr = VIDEO_VRAM_BASE_ADDR + (display.displayRow * display.maxScreenCol) + display.displayCol;
    uint16_t srcIdx = (display.screenRow * display.maxScreenCol) + display.displayCol;

    // Action according to request.
    switch(state)
    {
        // Disable the cursor flash mechanism.
        case CURSOR_OFF:
        default:
            // Only restore character if it had been previously saved and active.
            if(keyboard.cursorOn == 1 && keyboard.displayCursor == 1)
            {
                *(uint8_t *)(dispMemAddr) = dispCodeMap[display.screenCharBuf[srcIdx]].dispCode;
            }
            keyboard.cursorOn = 0;
            break;

        // Enable the cursor mechanism.
        case CURSOR_ON:
            keyboard.cursorOn = 1;
            break;

        // Restore the character under the cursor.
        case CURSOR_RESTORE:
            if(keyboard.displayCursor == 1)
            {
                *(uint8_t *)(dispMemAddr) = dispCodeMap[display.screenCharBuf[srcIdx]].dispCode;
            }
            break;

        // If enabled and timer expired, flash cursor.
        case CURSOR_FLASH:
            if(keyboard.cursorOn == 1 && (keyboard.flashTimer == 0L || keyboard.flashTimer + KEYB_FLASH_TIME < RTC_MILLISECONDS_EPOCH))
            {
                keyboard.displayCursor = keyboard.displayCursor == 1 ? 0 : 1;
                keyboard.flashTimer = RTC_MILLISECONDS_EPOCH;
                if(keyboard.displayCursor == 1)
                {
                    switch(keyboard.mode)
                    {
                        case KEYB_LOWERCASE:
                            *(uint8_t *)(dispMemAddr) = CURSOR_UNDERLINE;
                            break;
                        case KEYB_CAPSLOCK:
                            *(uint8_t *)(dispMemAddr) = CURSOR_BLOCK;
                            break;
                        case KEYB_SHIFTLOCK:
                        default:
                            *(uint8_t *)(dispMemAddr) = CURSOR_THICK_BLOCK;
                            break;
                    }
                } else
                {
                    *(uint8_t *)(dispMemAddr) = dispCodeMap[display.screenCharBuf[srcIdx]].dispCode;
                }
            }
            break;
    }
    return(0);
}

// A method to push keys into the keyboard buffer as though they had been pressed. This 
// is necessary for the ANSI Terminal emulation but also a useful feature for applications.
//
uint8_t mzPushKey(char *keySeq)
{
    uint8_t seqSize = strlen(keySeq);

    // Sanity check, cant push more keys then the keyboard buffer will hold.
    //
    if((keyboard.keyBufPtr + seqSize) > MAX_KEYB_BUFFER_SIZE)
        return(1);

    // Concat the key sequence into the keyboard buffer.
    //
    strcat(keyboard.keyBuf, keySeq);

    return(0);
}

// Method to sweep the keyboard and store any active keys. Detect key down, key up and keys being held.
uint8_t mzSweepKeys(void)
{
    // Sequence through the strobe lines and read back the scan data into the buffer.
    for(uint8_t strobe=0xF0; strobe < 0xFA; strobe++)
    {
        // Output the keyboard strobe.
        *(volatile uint8_t *)(MBADDR_8BIT_KEYPA) = strobe;

        // Slight delay to allow for bounce.
        TIMER_MILLISECONDS_UP = 0; while(TIMER_MILLISECONDS_UP < 1);

        // Read the scan lines.
        keyboard.scanbuf[0][strobe-0xF0] = (uint8_t)(*(volatile uint32_t *)(MBADDR_8BIT_KEYPB));
//        keyboard.scanbuf[0][strobe-0xF0] = (uint8_t)(*(volatile uint32_t *)(MBADDR_8BIT_KEYPB));
//printf("R%02x ", (uint8_t)(*(volatile uint32_t *)(MBADDR_8BIT_KEYPB)));

    }

    // Now look for active keys.
    for(uint8_t strobeIdx=0; strobeIdx < 10; strobeIdx++)
    {
        // Skip over modifier keys.
        //if(strobeIdx == 8) continue;

        if(keyboard.scanbuf[0][strobeIdx] != keyboard.scanbuf[1][strobeIdx])
        {
            if(keyboard.scanbuf[0][strobeIdx] != 0xFF)
            {
                keyboard.keydown[strobeIdx] = keyboard.scanbuf[0][strobeIdx];
            } else
            {
                keyboard.keydown[strobeIdx] = 0xFF;
            }

            if(keyboard.scanbuf[1][strobeIdx] != 0xFF)
            {
                keyboard.keyup[strobeIdx] = keyboard.scanbuf[1][strobeIdx];
            } else
            {
                keyboard.keyup[strobeIdx] = 0xFF;
            }
        } else
        {
            if(keyboard.scanbuf[0][strobeIdx] != 0xFF)
            {
                keyboard.keyhold[strobeIdx]++;
            } else
            {
                keyboard.keyhold[strobeIdx] = 0;
                keyboard.keydown[strobeIdx] = 0xFF;
                keyboard.keyup[strobeIdx] = 0xFF;
            }
        }
        keyboard.scanbuf[1][strobeIdx] = keyboard.scanbuf[0][strobeIdx];
    }
//printf("\n");

    // Check for modifiers.
    //
    if((keyboard.scanbuf[0][8] & 0x80) == 0)
    {
        keyboard.breakKey = 1;
    } else
    {
        keyboard.breakKey = 0;
    }
    if((keyboard.scanbuf[0][8] & 0x40) == 0)
    {
        keyboard.ctrlKey = 1;
    } else
    {
        keyboard.ctrlKey = 0;
    }
    if((keyboard.scanbuf[0][8] & 0x01) == 0)
    {
        keyboard.shiftKey = 1;
    } else
    {
        keyboard.shiftKey = 0;
    }

    return(0);
}

// Method to scan the keyboard and return any valid key presses.
// Input:  mode = 0 - No blocking, standard keyboard.
//                1 - blocking, standard keyboard.
//                2 - No blocking, ansi keyboard.
//                3 - blocking, ansi keyboard.
// Return: -1 = no key pressed.
//        ASCII value when key pressed.
int mzGetKey(uint8_t mode)
{
    // Locals.
    int retcode = -1;

    // Return buffered key strokes first, once exhausted scan for more.
    //
    if(keyboard.keyBuf[keyboard.keyBufPtr] != 0x00)
    {
        retcode = (int)keyboard.keyBuf[keyboard.keyBufPtr++];
    }
    else
    {
        // Loop if blocking flag set until a key is pressed else get a key if available and return.
        do {
            // Flash the cursor as needed.
            mzFlashCursor(CURSOR_FLASH);

            // Make a sweep of the keyboard, updating the key map.
            mzSweepKeys();

            // Run through the strobe sequence and identify any pressed keys, mapping to an ASCII value for return.
            for(uint8_t strobeIdx=0; strobeIdx < 10; strobeIdx++)
            {
                // Skip over modifier keys.
                //if(strobeIdx == 8) continue;

                // If a keyboard press is released, cancel autorepeat.
                if((keyboard.keydown[strobeIdx] != 0xFF && keyboard.keyhold[strobeIdx] == 0) || (keyboard.keyup[strobeIdx] != 0xFF && keyboard.keyhold[strobeIdx] > 0))
                {
                    keyboard.autorepeat = 0;
                } else
                if(keyboard.keydown[strobeIdx] != 0xFF && keyboard.keyhold[strobeIdx] == 1)
                {
                    uint8_t keyIdx = 0;
                    uint8_t key = keyboard.keydown[strobeIdx];
                    uint8_t modifiedMode = keyboard.ctrlKey == 1 ? KEYB_CTRL : keyboard.mode == KEYB_LOWERCASE && keyboard.shiftKey == 1 ? KEYB_SHIFTLOCK : keyboard.mode == KEYB_SHIFTLOCK && keyboard.shiftKey == 1 ? KEYB_CAPSLOCK : keyboard.mode == KEYB_CAPSLOCK && keyboard.shiftKey == 1 ? KEYB_LOWERCASE : keyboard.mode;
                    for(; keyIdx < 8 && key & 0x80; keyIdx++, key=key << 1);
                    retcode = scanCodeMap[modifiedMode].scanCode[(strobeIdx*8)+keyIdx];

                    // Setup auto repeat.
                    keyboard.repeatKey = retcode;
                    keyboard.holdTimer = RTC_MILLISECONDS_EPOCH;
                } else
                if(keyboard.keydown[strobeIdx] != 0xFF && keyboard.keyhold[strobeIdx] > 1 && keyboard.holdTimer + KEYB_AUTOREPEAT_INITIAL_TIME < RTC_MILLISECONDS_EPOCH)
                {
                    keyboard.autorepeat = 1;
                    keyboard.holdTimer = RTC_MILLISECONDS_EPOCH;
                } else
                if(keyboard.keydown[strobeIdx] != 0xFF && keyboard.keyhold[strobeIdx] > 1 && keyboard.autorepeat == 1 && keyboard.holdTimer + KEYB_AUTOREPEAT_TIME < RTC_MILLISECONDS_EPOCH)
                {
                    keyboard.holdTimer = RTC_MILLISECONDS_EPOCH;
                    retcode = keyboard.repeatKey;
                }
            }
         
            // Process internal keys, dont return.
            //
            switch(retcode)
            {
                // Toggle through the 3 key locks, lowercase, shiftlock, capslock.
                case ALPHAKEY:
                    keyboard.mode = keyboard.mode == KEYB_LOWERCASE ? KEYB_SHIFTLOCK : keyboard.mode == KEYB_SHIFTLOCK ? KEYB_CAPSLOCK : KEYB_LOWERCASE;
                    retcode = -1;
                    break;

                // Switch to graphics mode character set.
                case GRAPHKEY:
                    keyboard.mode = keyboard.mode == KEYB_GRAPHMODE ? KEYB_CAPSLOCK : KEYB_GRAPHMODE;
                    retcode = -1;
                    break;

                // If the debug key is pressed, toggle the debugging output on/off.
                case DEBUGKEY:
                    display.debug = (display.debug == 0 ? 1 : 0);
                    retcode = -1;
                    break;

                // If the Ansi Toggle Key is pressed, toggle enabling of the Ansi Terminal Emulator (1), or raw Sharp (0).
                case ANSITGLKEY:
                    display.useAnsiTerm = (display.useAnsiTerm == 0 ? 1 : 0);
                    retcode = -1;
                    break;
                    
                // Send cursor to 0,0.
                case CURHOMEKEY:
                    mzSetCursor(0,0);
                    retcode = -1;
                    break;

                // Clear screen.
                case CLRKEY:
                    mzClearScreen(3, 1);
                    retcode = -1;

                // No key assigned.
                case NOKEY:
                    retcode = -1;

                default:
                    break;
            }
          
        } while(retcode == -1 && (mode == 1 || mode == 3));

        // If we are in ANSI terminal mode, certain keys need expansion into ANSI ESCape sequences, detect the key and expand as necessary.
        //
        if((display.useAnsiTerm == 1 || mode == 2 || mode == 3) && retcode != -1)
        {
            for(uint8_t idx=0; idx < (sizeof(ansiKeySeq)/sizeof(*ansiKeySeq)); idx++)
            {
                // On match, copy the escape sequence into the keyboard buffer and set pointer to start of buffer+1.
                if(ansiKeySeq[idx].key == retcode)
                {
                    strcpy(keyboard.keyBuf, ansiKeySeq[idx].ansiKeySequence);
                    keyboard.keyBufPtr = 0;
                    retcode=(int)keyboard.keyBuf[keyboard.keyBufPtr++];
                    break;
                }
            }
            if(display.debug && !display.inDebug) mzDebugOut(2, retcode);
        }
    }

    return(retcode);
}

// File stream method to get a key from the keyboard.
//
int mzGetChar(FILE *stream)
{
    return(mzGetKey(1));
}

// A helper method to wait for a change in the service request status with timeout.
int mzSDGetStatus(uint32_t timeout, uint8_t initStatus)
{
    uint32_t timeoutMS = RTC_MILLISECONDS_EPOCH;
    uint8_t  result;

    // Wait for the status to update, dont read status continuously as a transaction can delay the K64F from requesting the bus to make an update.
    do {
        for(uint32_t timeMS = RTC_MILLISECONDS_EPOCH; timeMS == RTC_MILLISECONDS_EPOCH;);
        result = svcControl->result;
    } while(timeoutMS + timeout > RTC_MILLISECONDS_EPOCH && initStatus == result);

    return(timeoutMS + timeout > RTC_MILLISECONDS ? (int)result : -1);
}

// Method to make a generic service call to the K64F processor.
//
int mzServiceCall(uint8_t cmd)
{
    // Locals.
    uint8_t retries;
    uint8_t result = 0;
    int     status;

    // Place command to be executed in control structure.
    svcControl->cmd        = cmd;

    // Retry the command a number of times when failure occurs to allow for the I/O processor being busy.
    retries = TZSVC_RETRY_COUNT;
    do {
        // Instigate a service request 
        svcControl->result   = TZSVC_STATUS_REQUEST;
        *(volatile uint8_t *)(MBADDR_8BIT_IOW_SVCREQ) = 0x00;
       
        // Error occurred?
        if((status=mzSDGetStatus(TZSVC_TIMEOUT, TZSVC_STATUS_REQUEST)) != -1)
        {
            status=mzSDGetStatus(TZSVC_TIMEOUT, TZSVC_STATUS_PROCESSING);
        }
        retries--;
    } while(retries > 0 && (uint8_t)status != TZSVC_STATUS_OK);

    return(retries == 0 ? -1 : status);
}

// Method to make a service call to the K64F processor. Actual data copy in/out is made outside this method as it only takes care of actually
// making the command request and getting the status back.
//
int mzSDServiceCall(uint8_t drive, uint8_t cmd)
{
    // Locals.
    int     status;
  
    // Setup control structure to request a disk sector from the I/O processor.
    svcControl->fileSector = drive;

    // Make the call.
    status = mzServiceCall(cmd);

    return(status);
}

// Method to initialise an SD card hosted on the I/O processor. This is accomplished by a service request to the I/O processor
// and it takes care of the initialisation.
//
uint8_t mzSDInit(uint8_t drive)
{
    // Locals.
    uint8_t result = 0;
    int     status;

    // Make the initialisation service call.
    status = mzSDServiceCall(drive, TZSVC_CMD_SD_DISKINIT);

    // Did we get a successful service?
    if(status == -1 || (uint8_t)status != TZSVC_STATUS_OK)
    {
        result = 1;
    }
    return(result);
}

// Method to read a sector from the SD card hosted on the I/O processor. This is accomplished by a service request to the I/O processor
// and it will read the required sector and place it into the service control record.
//
uint8_t mzSDRead(uint8_t drive, uint32_t sector, uint32_t buffer)
{
    // Locals.
    uint8_t result = 0;
    int     status;
   
    // Setup control structure to request a disk sector from the I/O processor.
    svcControl->sectorLBA = convBigToLittleEndian(sector);

    // Make the disk read service call.
    status = mzSDServiceCall(drive, TZSVC_CMD_SD_READSECTOR);

    // Did we get a successful service?
    if((uint8_t)status == TZSVC_STATUS_OK)
    {
        // Copy the received sector into the provided buffer.
        for(uint32_t srcAddr=(uint32_t)svcControl->sector, dstAddr=buffer; dstAddr < buffer+TZSVC_SECTOR_SIZE; srcAddr++, dstAddr++)
        {
            *(uint8_t *)(dstAddr) = *(uint8_t *)srcAddr;
        }
    } else
    {
        result = 1;
    }
    return(result);
}

// Method to write a sector to the SD card hosted on the I/O processor. This is accomplished by placing data into the service control record
// and making a service request to the I/O processor which will write the data to the required sector.
//
uint8_t mzSDWrite(uint8_t drive, uint32_t sector, uint32_t buffer)
{
    // Locals.
    uint8_t result = 0;
    int     status;

    // Setup control structure to request writing of a sector to a disk on the I/O processor.
    svcControl->sectorLBA = convBigToLittleEndian(sector);
   
    // Copy the provided buffer into service control record sector buffer.
    for(uint32_t srcAddr=buffer, dstAddr=(uint32_t)svcControl->sector; srcAddr < buffer+TZSVC_SECTOR_SIZE; srcAddr++, dstAddr++)
    {
        *(uint8_t *)(dstAddr) = *(uint8_t *)srcAddr;
    }
 
    // Make the disk write service call.
    status = mzSDServiceCall(drive, TZSVC_CMD_SD_WRITESECTOR);

    // Did we get a successful service?
    if((uint8_t)status != TZSVC_STATUS_OK)
    {
        result = 1;
    }
    return(result);
}

// Method to exit from zOS/ZPU CPU and return control to the host Z80 CPU.
//
void mzSetZ80(void)
{
    // We could write direct to the CPU selection port but the host ROM may well be required as the ZPU uses it's memory. For this reason
    // we make a call to the I/O processor which will load the rom, switch CPU and perform a reset.
    mzServiceCall(TZSVC_CMD_CPU_SETZ80);

    // Loop forever waiting for the CPU switch or a reset.
    while(1);
}

//////////////////////////////////////////////////////////////
// End of Sharp MZ i/f methods for zOS                      //
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
// Temporary test routines                                  //
//////////////////////////////////////////////////////////////

// Dummy function used for testing, it's contents change over time as tests are made on various hardware components.
int mzGetTest()
{
    uint8_t nl = 0;
    while(1)
    {
        mzSweepKeys();
        TIMER_MILLISECONDS_UP = 0; while(TIMER_MILLISECONDS_UP < 250);
        //mzSweepKeys();

        for(uint8_t strobeIdx=0; strobeIdx < 9; strobeIdx++)
        {
            if(keyboard.keyup[strobeIdx] != 0xFF)
            {
                uint8_t keyIdx = 0;
                uint8_t key = keyboard.keyup[strobeIdx];
                for(; keyIdx < 8 && key & 0x80; keyIdx++, key=key << 1);
                printf("Up:%02x %02x", keyboard.keyup[strobeIdx], scanCodeMap[0].scanCode[(strobeIdx*8)+keyIdx]);
                nl = 1;
            }
            if(keyboard.keydown[strobeIdx] != 0xFF)
            {
                uint8_t keyIdx = 0;
                uint8_t key = keyboard.keydown[strobeIdx];
                for(; keyIdx < 8 && key & 0x80; keyIdx++, key=key << 1);
                printf("Dw:%02x %02x", keyboard.keyup[strobeIdx], scanCodeMap[0].scanCode[(strobeIdx*8)+keyIdx]);
                nl = 1;
            }
            if(keyboard.keyhold[strobeIdx] != 0)
            {
                printf("Hd:%02x ", keyboard.keyhold[strobeIdx]);
                nl = 1;
            }
            if(nl == 1)
            {
                printf("\n");
                nl = 0;
            }
        }
      //  TIMER_MILLISECONDS_UP = 0; while(TIMER_MILLISECONDS_UP < 1000);
    }
    return(-1);
}

//// Test routine. Add code here to test an item within the kernel. Anything accessing hardware generally has to call the kernel as it doesnt have real access.
//
void testRoutine(void)
{
    // Locals.
    //
    printf("No test defined.\n");
}
#endif // Protected methods which reside in the kernel.

#if defined __APP__
#endif // __APP__

#ifdef __cplusplus
}
#endif
