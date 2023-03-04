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
// History:         v1.0  Feb 2023 - Initial write of the Sharp MZ series hardware interface software.
//                  v1.01 Mar 2023 - Bug fixes and additional ESC sequence processing.
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
#include <linux/ctype.h>
#include <asm/io.h>
#include <infinity2m/gpio.h>
#include <infinity2m/registers.h>
#include "z80io.h"
#include "sharpmz.h"

// --------------------------------------------------------------------------------------------------------------
// Static data declarations.
// --------------------------------------------------------------------------------------------------------------

// Global scope variables.
//

// Mapping table to map Sharp MZ80A Ascii to Standard ASCII.
//
//static t_asciiMap asciiMap[] = {
//    { 0x00 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x00 }, { 0x20 }, { 0x20 }, // 0x0F
//    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0x1F
//    { 0x20 }, { 0x21 }, { 0x22 }, { 0x23 }, { 0x24 }, { 0x25 }, { 0x26 }, { 0x27 }, { 0x28 }, { 0x29 }, { 0x2A }, { 0x2B }, { 0x2C }, { 0x2D }, { 0x2E }, { 0x2F }, // 0x2F
//    { 0x30 }, { 0x31 }, { 0x32 }, { 0x33 }, { 0x34 }, { 0x35 }, { 0x36 }, { 0x37 }, { 0x38 }, { 0x39 }, { 0x3A }, { 0x3B }, { 0x3C }, { 0x3D }, { 0x3E }, { 0x3F }, // 0x3F
//    { 0x40 }, { 0x41 }, { 0x42 }, { 0x43 }, { 0x44 }, { 0x45 }, { 0x46 }, { 0x47 }, { 0x48 }, { 0x49 }, { 0x4A }, { 0x4B }, { 0x4C }, { 0x4D }, { 0x4E }, { 0x4F }, // 0x4F
//    { 0x50 }, { 0x51 }, { 0x52 }, { 0x53 }, { 0x54 }, { 0x55 }, { 0x56 }, { 0x57 }, { 0x58 }, { 0x59 }, { 0x5A }, { 0x5B }, { 0x5C }, { 0x5D }, { 0x5E }, { 0x5F }, // 0x5F
//    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0x6F
//    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0x7F
//    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0x8F
//    { 0x20 }, { 0x20 }, { 0x65 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x74 }, { 0x67 }, { 0x68 }, { 0x20 }, { 0x62 }, { 0x78 }, { 0x64 }, { 0x72 }, { 0x70 }, { 0x63 }, // 0x9F
//    { 0x71 }, { 0x61 }, { 0x7A }, { 0x77 }, { 0x73 }, { 0x75 }, { 0x69 }, { 0x20 }, { 0x4F }, { 0x6B }, { 0x66 }, { 0x76 }, { 0x20 }, { 0x75 }, { 0x42 }, { 0x6A }, // 0XAF
//    { 0x6E }, { 0x20 }, { 0x55 }, { 0x6D }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x6F }, { 0x6C }, { 0x41 }, { 0x6F }, { 0x61 }, { 0x20 }, { 0x79 }, { 0x20 }, { 0x20 }, // 0xBF
//    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0XCF
//    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0XDF
//    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, // 0XEF
//    { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }  // 0XFF
//};
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

#if (TARGET_HOST_MZ700 == 1)
static t_scanCodeMap scanCodeMap[] = {
    // NO SHIFT
    {{
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
    }},
    // CAPS LOCK
    {{
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
    }},
    // SHIFT LOCK.
    {{
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
        CLRKEY   ,                                           //  CLR - END. - Clear display.
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
    }},
    // CONTROL CODE
    {{
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
    }},
    // KANA
    {{
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
    }} 
};
#endif
#if (TARGET_HOST_MZ80A == 1)
static t_scanCodeMap scanCodeMap[] = {
    // MZ_80A NO SHIFT
    {{
      //  S0   00 - 07
        NOKEY    ,                                           //  BREAK/CTRL
        NOKEY    ,                                           // 
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        GRAPHKEY ,                                           //  GRAPH
        NOKEY    ,                                           //  SHIFT
        // S1   08 - 0F
        '2'      ,                                           //  2
        '1'      ,                                           //  1
        'w'      ,                                           //  w
        'q'      ,                                           //  q
        'a'      ,                                           //  a
        BACKS    ,                                           //  DELETE
        NOKEY    ,                                           //  NULL
        'z'      ,                                           //  z
        // S2   10 - 17
        '4'      ,                                           //  4
        '3'      ,                                           //  3
        'r'      ,                                           //  r
        'e'      ,                                           //  e
        'd'      ,                                           //  d
        's'      ,                                           //  s
        'x'      ,                                           //  x
        'c'      ,                                           //  c
        // S3   18 - 1F
        '6'      ,                                           //  6
        '5'      ,                                           //  5
        'y'      ,                                           //  y
        't'      ,                                           //  t
        'g'      ,                                           //  g
        'f'      ,                                           //  f
        'v'      ,                                           //  v
        'b'      ,                                           //  b
        // S4   20 - 27
        '8'      ,                                           //  8
        '7'      ,                                           //  7
        'i'      ,                                           //  i
        'u'      ,                                           //  u
        'j'      ,                                           //  j
        'h'      ,                                           //  h
        'n'      ,                                           //  n
        ' '      ,                                           //  SPACE
        // S5   28 - 2F
        '0'      ,                                           //  0
        '9'      ,                                           //  9
        'p'      ,                                           //  p
        'o'      ,                                           //  o
        'l'      ,                                           //  l
        'k'      ,                                           //  k
        ','      ,                                           //  ,
        'm'      ,                                           //  m
        // S6   30 - 37
        '^'      ,                                           //  ^
        '-'      ,                                           //  -
        '['      ,                                           //  [
        '@'      ,                                           //  @
        ':'      ,                                           //  :
        ';'      ,                                           //  ;
        '/'      ,                                           //  /
        '.'      ,                                           //  .
        // S7   38 - 3F
        HOMEKEY  ,                                           //  HOME.
        '\\'     ,                                           //  Backslash
        CURSRIGHT,                                           //  CURSOR RIGHT
        CURSUP   ,                                           //  CURSOR UP
        CR       ,                                           //  CR
        ']'      ,                                           //  ]
        NOKEY    ,                                           //
        '?'      ,                                           //  ?
        // S8   40 - 47 - Keypad keys.
        '8'      ,                                           // Keypad 8
        '7'      ,                                           //        7
        '5'      ,                                           //        5
        '4'      ,                                           //        4
        '2'      ,                                           //        2
        '1'      ,                                           //        1
        DBLZERO  ,                                           //       00 
        '0'      ,                                           //        0
        // S9   48 - 4F - Keypad keys.
        '+'      ,                                           //        +
        '0'      ,                                           //        9
        '-'      ,                                           //        -
        '6'      ,                                           //        6
        NOKEY    ,                                           //
        '3'      ,                                           //        3
        NOKEY    ,
        '.'                                                  //        .
    }},
    // MZ_80A CAPS LOCK
    {{
      //  S0   00 - 07
        NOKEY    ,                                           //  BREAK/CTRL
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        ALPHAKEY ,                                           //  GRAPH
        NOKEY    ,                                           //  SHIFT
        // S1   08 - 0F
        '2'      ,                                           //  2
        '1'      ,                                           //  1
        'W'      ,                                           //  W
        'Q'      ,                                           //  Q
        'A'      ,                                           //  A
        BACKS    ,                                           //  DELETE
        NOKEY    ,                                           //  NULL
        'Z'      ,                                           //  Z
        // S2   10 - 17
        '4'      ,                                           //  4
        '3'      ,                                           //  3
        'R'      ,                                           //  R
        'E'      ,                                           //  E
        'D'      ,                                           //  D
        'S'      ,                                           //  S
        'X'      ,                                           //  X
        'C'      ,                                           //  C
        // S3   18 - 1F
        '6'      ,                                           //  6
        '5'      ,                                           //  5
        'Y'      ,                                           //  Y
        'T'      ,                                           //  T
        'G'      ,                                           //  G
        'F'      ,                                           //  F
        'V'      ,                                           //  V
        'B'      ,                                           //  B
        // S4   20 - 27
        '8'      ,                                           //  8
        '7'      ,                                           //  7
        'I'      ,                                           //  I
        'U'      ,                                           //  U
        'J'      ,                                           //  J
        'H'      ,                                           //  H
        'N'      ,                                           //  N
        ' '      ,                                           //  SPACE
        // S5   28 - 2F
        '0'      ,                                           //  0
        '9'      ,                                           //  9
        'P'      ,                                           //  P
        'O'      ,                                           //  O
        'L'      ,                                           //  L
        'K'      ,                                           //  K
        ','      ,                                           //  ,
        'M'      ,                                           //  M
        // S6   30 - 37
        '^'      ,                                           //  ^
        '-'      ,                                           //  -
        '['      ,                                           //  [
        '@'      ,                                           //  @
        ':'      ,                                           //  :
        ';'      ,                                           //  ;
        '/'      ,                                           //  /
        '.'      ,                                           //  .
        // S7   38 - 3F
        HOMEKEY  ,                                           //  HOME.
        '\\'     ,                                           //  Backslash
        CURSRIGHT,                                           //  CURSOR RIGHT
        CURSUP   ,                                           //  CURSOR UP
        CR       ,                                           //  CR
        ']'      ,                                           //  ]
        NOKEY    ,                                           //
        '?'      ,                                           //  ?
        // S8   40 - 47 - Keypad keys.
        '8'      ,                                           // Keypad 8
        '7'      ,                                           //        7
        '5'      ,                                           //        5
        '4'      ,                                           //        4
        '2'      ,                                           //        2
        '1'      ,                                           //        1
        DBLZERO  ,                                           //       00 
        '0'      ,                                           //        0
        // S9   48 - 4F - Keypad keys.
        '+'      ,                                           //        +
        '0'      ,                                           //        9
        '-'      ,                                           //        -
        '6'      ,                                           //        6
        NOKEY    ,                                           //
        '3'      ,                                           //        3
        NOKEY    ,
        '.'                                                  //        .
    }},
    // MZ_80A SHIFT LOCK.
    {{
      //  S0   00 - 07
        NOKEY    ,                                           //  BREAK/CTRL
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        ALPHAKEY ,                                           //  GRAPH
        NOKEY    ,                                           //  SHIFT
        // S1   08 - 0F
        '"'      ,                                           //  "
        '!'      ,                                           //  !
        'W'      ,                                           //  W
        'Q'      ,                                           //  Q
        'A'      ,                                           //  A
        INSERT   ,                                           //  INSERT
        NOKEY    ,                                           //  NULL
        'Z'      ,                                           //  Z
        // S2   10 - 17
        '$'      ,                                           //  $
        '#'      ,                                           //  #
        'R'      ,                                           //  R
        'E'      ,                                           //  E
        'D'      ,                                           //  D
        'S'      ,                                           //  S
        'X'      ,                                           //  X
        'C'      ,                                           //  C
        // S3   18 - 1F
        '&'      ,                                           //  &
        '%'      ,                                           //  %
        'Y'      ,                                           //  Y
        'T'      ,                                           //  T
        'G'      ,                                           //  G
        'F'      ,                                           //  F
        'V'      ,                                           //  V
        'B'      ,                                           //  B
        // S4   20 - 27
        '('      ,                                           //  (
        '\''     ,                                           //  '
        'I'      ,                                           //  I
        'U'      ,                                           //  U
        'J'      ,                                           //  J
        'H'      ,                                           //  H
        'N'      ,                                           //  N
        ' '      ,                                           //  SPACE
        // S5   28 - 2F
        '_'      ,                                           //  _
        ')'      ,                                           //  )
        'P'      ,                                           //  P
        'O'      ,                                           //  O
        'L'      ,                                           //  L
        'K'      ,                                           //  K
        '<'      ,                                           //  <
        'M'      ,                                           //  M
        // S6   30 - 37
        '~'      ,                                           //  ~
        '='      ,                                           //  =
        '{'      ,                                           //  {
        '`'      ,                                           //  `
        '*'      ,                                           //  *
        '+'      ,                                           //  +
        NOKEY    ,                                           //  
        '>'      ,                                           //  >
        // S7   38 - 3F
        CLRKEY   ,                                           //  CLR.
        '|'      ,                                           //  |
        CURSLEFT ,                                           //  CURSOR LEFT
        CURSDOWN ,                                           //  CURSOR DOWN
        CR       ,                                           //  CR
        '}'      ,                                           //  }
        NOKEY    ,                                           //
        NOKEY    ,                                           // 
        // S8   40 - 47 - Keypad keys.
        '8'      ,                                           // Keypad 8
        '7'      ,                                           //        7
        '5'      ,                                           //        5
        '4'      ,                                           //        4
        '2'      ,                                           //        2
        '1'      ,                                           //        1
        DBLZERO  ,                                           //       00 
        '0'      ,                                           //        0
        // S9   48 - 4F - Keypad keys.
        '+'      ,                                           //        +
        '0'      ,                                           //        9
        '-'      ,                                           //        -
        '6'      ,                                           //        6
        NOKEY    ,                                           //
        '3'      ,                                           //        3
        NOKEY    ,
        '.'                                                  //        .
    }},
    // MZ_80A CONTROL CODE
    {{
      //  S0   00 - 07
        NOKEY    ,                                           //  BREAK/CTRL
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        NOKEY    ,                                           // 
        ALPHAGRAPHKEY,                                       //  GRAPH
        NOKEY    ,                                           //  SHIFT
        // S1   08 - 0F
        NOKEY    ,                                           //  
        NOKEY    ,                                           // 
        CTRL_W   ,                                           //  CTRL_W
        CTRL_Q   ,                                           //  CTRL_Q
        CTRL_A   ,                                           //  CTRL_A
        DELETE   ,                                           //  DELETE
        NOKEY    ,                                           //  NULL
        CTRL_Z   ,                                           //  CTRL_Z
        // S2   10 - 17
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        CTRL_R   ,                                           //  CTRL_R
        CTRL_E   ,                                           //  CTRL_E
        CTRL_D   ,                                           //  CTRL_D
        CTRL_S   ,                                           //  CTRL_S
        CTRL_X   ,                                           //  CTRL_X
        CTRL_C   ,                                           //  CTRL_C
        // S3   18 - 1F
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        CTRL_Y   ,                                           //  CTRL_Y
        CTRL_T   ,                                           //  CTRL_T
        CTRL_G   ,                                           //  CTRL_G
        CTRL_F   ,                                           //  CTRL_F
        CTRL_V   ,                                           //  CTRL_V
        CTRL_B   ,                                           //  CTRL_B
        // S4   20 - 27
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        CTRL_I   ,                                           //  CTRL_I
        CTRL_U   ,                                           //  CTRL_U
        CTRL_J   ,                                           //  CTRL_J
        CTRL_H   ,                                           //  CTRL_H
        CTRL_N   ,                                           //  CTRL_N
        ' '      ,                                           //  SPACE
        // S5   28 - 2F
        CTRL_UNDSCR,                                         //  CTRL+_
        NOKEY    ,                                           //  
        CTRL_P   ,                                           //  CTRL_P
        CTRL_O   ,                                           //  CTRL_O
        CTRL_L   ,                                           //  CTRL_L
        CTRL_K   ,                                           //  CTRL_K
        NOKEY    ,                                           //  
        CTRL_M   ,                                           //  CTRL_M
        // S6   30 - 37
        CTRL_CAPPA,                                          //  CTRL+^
        NOKEY    ,                                           //  
        CTRL_LB  ,                                           //  CTRL+[ 
        CTRL_AT  ,                                           //  CTRL+@
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        CTRL_SLASH,                                          //  CTRL+/
        NOKEY    ,                                           //  
        // S7   38 - 3F
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        NOKEY    ,                                           //  
        CTRL_RB  ,                                           //  CTRL+]
        NOKEY    ,                                           //
        NOKEY    ,                                           // 
        // S8   40 - 47 - Keypad keys.
        '8'      ,                                           // Keypad 8
        '7'      ,                                           //        7
        '5'      ,                                           //        5
        '4'      ,                                           //        4
        '2'      ,                                           //        2
        '1'      ,                                           //        1
        DBLZERO  ,                                           //       00 
        '0'      ,                                           //        0
        // S9   48 - 4F - Keypad keys.
        '+'      ,                                           //        +
        '0'      ,                                           //        9
        '-'      ,                                           //        -
        '6'      ,                                           //        6
        NOKEY    ,                                           //
        '3'      ,                                           //        3
        NOKEY    ,
        '.'                                                  //        .
    }},
    // MZ_80A KANA
    {{
        // S0   00 - 07
        NOKEY    ,                                          //  BREAK/CTRL
        NOKEY    ,                                          //  
        NOKEY    ,                                          // 
        NOKEY    ,                                          //  
        NOKEY    ,                                          // 
        NOKEY    ,                                          //  
        GRAPHKEY ,                                          //  DAKU TEN
        NOKEY    ,                                          //  
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
        // S8   40 - 47 - Keypad keys.
        '8'      ,                                           // Keypad 8
        '7'      ,                                           //        7
        '5'      ,                                           //        5
        '4'      ,                                           //        4
        '2'      ,                                           //        2
        '1'      ,                                           //        1
        DBLZERO  ,                                           //       00 
        '0'      ,                                           //        0
        // S9   48 - 4F - Keypad keys.
        '+'      ,                                           //        +
        '0'      ,                                           //        9
        '-'      ,                                           //        -
        '6'      ,                                           //        6
        NOKEY    ,                                           //
        '3'      ,                                           //        3
        NOKEY    ,
        '.'                                                  //        .
    }} 
};

#endif

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

#if (TARGET_HOST_MZ700 == 1)
    // Static structures for controlling and managing hardware features.
    // Display control structure. Used to manage the display including the Ansi Terminal.
    const t_displayBuffer displayDefault  = { .displayAttr = 0x71, .backingRow = 0, .displayCol = 0, .displayRow = 0, .maxBackingRow = (VC_DISPLAY_BUFFER_SIZE / VC_MAX_COLUMNS),
                                              .maxDisplayRow = VC_MAX_ROWS, .maxBackingCol = 40, .useAnsiTerm = 1, .lineWrap = 0, .inDebug = 0
                                            };
    // Keyboard control structure. Used to manage keyboard sweep, mapping and store.
    const t_keyboard      keyboardDefault = { .holdTimer = 0L, .autorepeat = 0, .mode = KEYB_LOWERCASE, .cursorOn = 1, .flashTimer = 0L, .keyBuf[0] = 0x00, .keyBufPtr = 0,
                                              .mode = KEYB_LOWERCASE, .dualmode = KEYB_DUAL_NONE 
                                            };

    // Audio control structure. Used to manage audio output.
    const t_audio         audioDefault    = { .audioStopTimer = 0
                                            };
    
    // AnsiTerminal control structure. Used to manage the inbuilt Ansi Terminal.
    const t_AnsiTerm      ansitermDefault = { .state = ANSITERM_ESC, .charcnt = 0, .paramcnt = 0, .setDisplayMode = 0, .setExtendedMode = 0, .saveRow = 0, .saveCol = 0, 
                                            };

    // Module control structure.
    const t_control       ctrlDefault     = { .suspendIO = 0, .debug = 0
                                            };

    // Colour map for the Ansi Terminal.
    const unsigned char ansiColourMap[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
#endif
#if (TARGET_HOST_MZ80A == 1)
    // Static structures for controlling and managing hardware features.
    // Display control structure. Used to manage the display including the Ansi Terminal.
    const t_displayBuffer displayDefault  = { .displayAttr = 0x71, .backingRow = 0, .displayCol = 0, .displayRow = 0, .maxBackingRow = (VC_DISPLAY_BUFFER_SIZE / VC_MAX_COLUMNS),
                                              .maxDisplayRow = VC_MAX_ROWS, .maxBackingCol = 80, .useAnsiTerm = 1, .lineWrap = 0, .inDebug = 0
                                            };

    // Keyboard control structure. Used to manage keyboard sweep, mapping and store.
    const t_keyboard      keyboardDefault = { .holdTimer = 0L, .autorepeat = 0, .mode = KEYB_LOWERCASE, .cursorOn = 1, .flashTimer = 0L, .keyBuf[0] = 0x00, .keyBufPtr = 0,
                                              .mode = KEYB_LOWERCASE, .dualmode = KEYB_DUAL_GRAPH
                                            };

    // Audio control structure. Used to manage audio output.
    const t_audio         audioDefault    = { .audioStopTimer = 0
                                            };
    

    // AnsiTerminal control structure. Used to manage the inbuilt Ansi Terminal.
    const t_AnsiTerm      ansitermDefault = { .state = ANSITERM_ESC, .charcnt = 0, .paramcnt = 0, .setDisplayMode = 0, .setExtendedMode = 0, .saveRow = 0, .saveCol = 0, 
                                            };

    // Module control structure.
    const t_control       ctrlDefault     = { .suspendIO = 0, .debug = 0
                                            };

    // Colour map for the Ansi Terminal.
    const unsigned char ansiColourMap[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
#endif     

    // Set the module control structures to default values according to build target.
    static t_displayBuffer display        = displayDefault;
    static t_keyboard      keyboard       = keyboardDefault;
    static t_audio         audio          = audioDefault;
    static t_AnsiTerm      ansiterm       = ansitermDefault;
    static t_control       ctrl           = ctrlDefault;


// --------------------------------------------------------------------------------------------------------------
// Methods
// --------------------------------------------------------------------------------------------------------------

// Method to configure the motherboard hardware after a reset.
//
uint8_t mzInitMBHardware(void)
{
    // From the 1Z-013A monitor code, initialise the 8255 PIO.
    //
    WRITE_HARDWARE(1, MBADDR_KEYPF, 0x8A);                                       // 10001010 CTRL WORD MODE0
    WRITE_HARDWARE(1, MBADDR_KEYPF, 0x07);                                       // PC3=1 M-ON
    WRITE_HARDWARE(1, MBADDR_KEYPF, 0x05);                                       // PC2=1 INTMSK
    WRITE_HARDWARE(1, MBADDR_KEYPF, 0x01);                                       // TZ: Enable VGATE

    // Initialise the 8253 timer.
    WRITE_HARDWARE(1, MBADDR_CONTF, 0x74);                                       // From monitor, according to system clock.
    WRITE_HARDWARE(1, MBADDR_CONTF, 0xB0);
    // Set timer in seconds, default to 0.
    WRITE_HARDWARE(1, MBADDR_CONT2, 0x00);                                       // Timer 2 is the number of seconds.
    WRITE_HARDWARE(1, MBADDR_CONT2, 0x00);                                       // Timer 2 is the number of seconds.
    // Set timer in seconds, default to 0.
    WRITE_HARDWARE(1, MBADDR_CONT1, 0x0A);                                       // Timer 1 is set to 640.6uS pulse into timer 2.
    WRITE_HARDWARE(1, MBADDR_CONT1, 0x00);  
    // Set timer timer to run.
    WRITE_HARDWARE(1, MBADDR_CONTF, 0x80);  

    return(0);
}

// Method to initialise the Sharp MZ extensions.
//
uint8_t mzInit(void)
{
    // Initialise Sharp MZ hardware.
    mzInitMBHardware();

    // Ensure module is at default configuration.
    display        = displayDefault;
    keyboard       = keyboardDefault;
    audio          = audioDefault;
    ansiterm       = ansitermDefault;
    ctrl           = ctrlDefault;

    // Clear and setup the display mode and resolution.
    mzClearDisplay(3, 1);
  #if (TARGET_HOST_MZ80A == 1)
    mzSetMachineVideoMode(VMMODE_MZ80A);
  #elif (TARGET_HOST_MZ700 == 1)
    mzSetMachineVideoMode(VMMODE_MZ700);
  #elif (TARGET_HOST_MZ2000 == 1)
    mzSetMachineVideoMode(VMMODE_MZ2000);
  #endif
    mzSetDisplayWidth(display.maxBackingCol);

    return(0);
}

// Method to generate a Beep sound on the host via it's hardware.
//
void mzBeep(uint32_t freq, uint32_t timeout)
{
    // Locals.
  #if (TARGET_HOST_MZ80A == 1)
    uint16_t   freqDiv = TIMER_8253_MZ80A_FREQ/(freq*2);
  #else
    uint16_t   freqDiv = TIMER_8253_MZ700_FREQ/freq;
  #endif

    // Setup the 8253 Timer 0 to output a sound, enable output to amplifier and set timeout.
    WRITE_HARDWARE(0, MBADDR_CONTF, 0x34               );       // Timer 0 to square wave generator, load LSB first.
    WRITE_HARDWARE(0, MBADDR_CONT0, (freqDiv&0xff)     );  
    WRITE_HARDWARE(0, MBADDR_CONT0, (freqDiv & 0xff00) );       // Timer 1 to 300Hz
    WRITE_HARDWARE(0, MBADDR_SUNDG, 0x01               );       // Enable sound.

    // Set a 500ms timeout on the sound to create beep effect.
    audio.audioStopTimer = timeout == 0 ? 11 : (timeout/10)+1;  // Each unit is 10ms, valid range 1..n
    return;
}

// Method to clear the display.
// mode: 0 = clear from cursor to end of display.
//       1 = clear from 0,0 to cursor.
//       2 = clear entire display
//       3 = clear entire display and reset scroll buffer.
//
void mzClearDisplay(uint8_t mode, uint8_t updPos)
{
    // Locals.
    uint32_t dstVRAMStartAddr;
    uint32_t dstVRAMEndAddr;
    uint32_t dstARAMStartAddr;
    uint32_t startIdx;
    uint32_t endIdx;
    uint32_t dstAddr;
    uint32_t dstARAMAddr;
    uint32_t dstVRAMAddr;

    // Sanity checks.
    if(mode > 3)
        return;

    switch(mode)
    {
        // Clear from cursor to end of display.
        case 0:
            dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR+(display.displayRow*display.maxBackingCol)+display.displayCol;
            dstVRAMEndAddr   = VIDEO_VRAM_BASE_ADDR + VIDEO_VRAM_SIZE;
            dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR+(display.displayRow*display.maxBackingCol)+display.displayCol; 
            startIdx = (((display.backingRow < display.maxDisplayRow ? display.displayRow : (display.backingRow - display.maxDisplayRow + display.displayRow))) * display.maxBackingCol) + display.displayCol;
            endIdx   = startIdx + ((display.maxBackingCol*display.maxDisplayRow) - ((display.backingRow < display.maxDisplayRow ? display.displayRow : display.backingRow ) * display.maxBackingCol));
            break;

        // Clear from beginning of display to cursor.
        case 1:
            dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR;
            dstVRAMEndAddr   = VIDEO_VRAM_BASE_ADDR+(display.displayRow*display.maxBackingCol)+display.displayCol;
            dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR;
            startIdx = ((display.backingRow < display.maxDisplayRow ? display.backingRow  : (display.backingRow - display.maxDisplayRow)) * display.maxBackingCol);
            endIdx   = ((display.backingRow < display.maxDisplayRow ? display.displayRow : (display.backingRow - display.maxDisplayRow + display.displayRow)) * display.maxBackingCol) + display.displayCol;
            break;

        // Clear entire display.
        case 2:
            dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR;
            dstVRAMEndAddr   = VIDEO_VRAM_BASE_ADDR + VIDEO_VRAM_SIZE;
            dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR;
            startIdx = ((display.backingRow < display.maxDisplayRow ? display.backingRow  : (display.backingRow - display.maxDisplayRow)) * display.maxBackingCol);
            endIdx   = startIdx + (display.maxBackingCol*display.maxDisplayRow);
            if(updPos)
            {
                display.displayRow = 0;
                display.displayCol = 0;
            }
            break;

        // Clear entire display including scrollback buffer.
        default:
            dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR;
            dstVRAMEndAddr   = VIDEO_VRAM_BASE_ADDR+VIDEO_VRAM_SIZE;
            dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR;
            startIdx = 0;
            endIdx   = VC_DISPLAY_BUFFER_SIZE;
            // Reset parameters to start of display.
            if(updPos)
            {
                display.displayRow = 0;
                display.displayCol = 0;
                display.backingRow = 0;
            }
            break;
    }

    // Clear the physical character display and attribute RAM.
    for(dstVRAMAddr=dstVRAMStartAddr, dstARAMAddr = dstARAMStartAddr; dstVRAMAddr <= dstVRAMEndAddr; dstVRAMAddr+=1, dstARAMAddr+=1)
    {
        // Clear both Video and Attribute RAM.
        WRITE_HARDWARE(0, dstVRAMAddr, 0x00);
        WRITE_HARDWARE(0, dstARAMAddr, display.displayAttr);
    }
    // Clear the shadow display scrollback RAM.
    for(dstAddr = startIdx; dstAddr < endIdx; dstAddr++)
    {
        display.displayCharBuf[dstAddr] = 0x20;
        display.displayAttrBuf[dstAddr] = display.displayAttr;
    }

    return;
}

// Method to clear a line, starting at a given column.
void mzClearLine(int row, int colStart, int colEnd, uint8_t updPos)
{
    // Locals.
    uint8_t  newRow            = row;
    uint8_t  newColStart       = colStart;
    uint8_t  newColEnd         = colEnd;
    uint32_t displayAttr;
    uint32_t dstVRAMStartAddr;
    uint32_t dstVRAMEndAddr;
    uint32_t dstARAMStartAddr;
    uint32_t dstAddr;
    uint32_t startIdx;
    uint32_t dstARAMAddr;
    uint32_t dstVRAMAddr;

    // Adjust the parameters, -1 indicates use current position.
    if(row == -1)      newRow = display.displayRow;
    if(colStart == -1) newColStart = 0;
    if(colEnd == -1)   newColEnd = display.maxBackingCol-1;

    // Sanity checks.
    if(newRow > display.maxDisplayRow || newColStart > display.maxBackingCol || newColEnd > display.maxBackingCol || newColEnd <= newColStart)
        return;
   
    // Clear the physical character display and attribute RAM.
    displayAttr = display.displayAttr << 24 | display.displayAttr << 16 | display.displayAttr << 8 | display.displayAttr;
    dstVRAMStartAddr = VIDEO_VRAM_BASE_ADDR+(newRow*display.maxBackingCol)+newColStart;
    dstVRAMEndAddr   = dstVRAMStartAddr + newColEnd;
    dstARAMStartAddr = VIDEO_ARAM_BASE_ADDR+(newRow*display.maxBackingCol)+newColStart; 

    // Select 32bit or 8 bit clear depending on the start/end position.
    //
    for(dstVRAMAddr=dstVRAMStartAddr, dstARAMAddr = dstARAMStartAddr; dstVRAMAddr <= dstVRAMEndAddr; dstVRAMAddr+=1, dstARAMAddr+=1)
    {
        WRITE_HARDWARE(0, dstVRAMAddr, 0x00);
        WRITE_HARDWARE(0, dstARAMAddr, display.displayAttr);
    }

    // Clear the shadow display scrollback RAM.
    startIdx = (((display.backingRow < display.maxDisplayRow ? newRow : (display.backingRow - display.maxDisplayRow + newRow))) * display.maxBackingCol) + newColStart;
    for(dstAddr = startIdx; dstAddr <= startIdx + newColEnd; dstAddr++)
    {
        display.displayCharBuf[dstAddr] = 0x20;
        display.displayAttrBuf[dstAddr] = display.displayAttr;
    }

    // Update the display pointer if needed.
    if(updPos)
    {
        display.displayRow = newRow;
        display.displayCol = newColEnd;
    }
    return;
}

// Method to set the display mode.
//
uint8_t mzSetMachineVideoMode(uint8_t vmode)
{
    // Locals.

    // Sanity check parameters.
    if(vmode != VMMODE_MZ80K && vmode != VMMODE_MZ80C && vmode != VMMODE_MZ1200 && vmode != VMMODE_MZ80A && vmode != VMMODE_MZ700 && vmode != VMMODE_MZ1500 && vmode != VMMODE_MZ800 && vmode != VMMODE_MZ80B && vmode != VMMODE_MZ2000 && vmode != VMMODE_MZ2200 && vmode != VMMODE_MZ2500) 
        return(1);
    
    // Set the hardware video mode.

    return(0);
}

// Method to return the character based display width.
//
uint8_t mzGetDisplayWidth(void)
{
    return(display.maxBackingCol);
}

// Method to setup the character based display width.
//
uint8_t mzSetDisplayWidth(uint8_t width)
{
    // Locals.

    // Sanity check parameters.
    if(width != 40 && width != 80)
        return(1);
    
    // Toggle the 40/80 bit according to requirements.
    if(width == 40)
    {
        // Dummy read to enable access to control register.
        READ_HARDWARE_INIT(0, MBADDR_DSPCTL);
        WRITE_HARDWARE(0, MBADDR_DSPCTL, 0x00);
        display.maxBackingCol = 40;
    }
    else
    {
        // Dummy read to enable access to control register.
        READ_HARDWARE_INIT(0, MBADDR_DSPCTL);
        WRITE_HARDWARE(0, MBADDR_DSPCTL, VMMODE_80CHAR);
        display.maxBackingCol = 80;
    }

    return(0);
}

// Method to refresh the display from the scrollback buffer contents.
void mzRefreshDisplay(void)
{
    // Locals
    uint32_t srcIdx;
    uint32_t startIdx;
    uint32_t dstARAMAddr;
    uint32_t dstVRAMAddr;

    // Refresh the display with buffer window contents
    startIdx = (display.backingRow < display.maxDisplayRow ? 0 : (display.backingRow - display.maxDisplayRow)+1) * display.maxBackingCol;
    for(srcIdx = startIdx, dstVRAMAddr = VIDEO_VRAM_BASE_ADDR, dstARAMAddr = VIDEO_ARAM_BASE_ADDR; srcIdx < startIdx+(display.maxDisplayRow*display.maxBackingCol); srcIdx++, dstVRAMAddr++, dstARAMAddr++)
    {
        WRITE_HARDWARE(0, dstVRAMAddr, dispCodeMap[display.displayCharBuf[srcIdx]].dispCode);
        WRITE_HARDWARE(0, dstARAMAddr, display.displayAttrBuf[srcIdx]);
    }
}

// Method to scroll the display contents upwards, either because new data is being added to the bottom or for scrollback.
//
uint8_t mzScrollUp(uint8_t lines, uint8_t clear, uint8_t useBacking)
{
    // Locals.
    uint32_t srcAddr;
    uint32_t dstAddr;
    uint32_t startIdx;
    uint32_t endIdx;

    // Sanity check.
    if(lines > display.maxDisplayRow)
        return(1);
  
    // Restore cursor character before scrolling.
    mzFlashCursor(CURSOR_RESTORE);

    // Add the lines to the current row address. If the row exceeds the maximum then scroll the display up.
    display.backingRow += lines;
    display.displayRow += lines;
    if(display.displayRow >= display.maxDisplayRow)
    {
        display.displayRow = display.maxDisplayRow - 1;
    }

    // At end of buffer or not using the backing scroll region? Shift up.
    if(display.backingRow >= display.maxBackingRow || (useBacking == 0 && display.backingRow >= display.maxDisplayRow))
    {
        display.backingRow = useBacking == 0 ? display.maxDisplayRow-1 : display.maxBackingRow-1;
        srcAddr = (lines * display.maxBackingCol);
        dstAddr = 0;
        for(; srcAddr < VC_DISPLAY_BUFFER_SIZE; srcAddr++, dstAddr++)
        {
            display.displayCharBuf[dstAddr] = display.displayCharBuf[srcAddr];
            display.displayAttrBuf[dstAddr] = display.displayAttrBuf[srcAddr];
        }

        // When in Ansi mode the backing memory outside of the display isnt used.
        if(useBacking == 0)
        {
            dstAddr = (display.backingRow - lines + 1) * display.maxBackingCol;
        }
        for(; dstAddr < VC_DISPLAY_BUFFER_SIZE; dstAddr++)
        {
            display.displayCharBuf[dstAddr] = 0x20;
            display.displayAttrBuf[dstAddr] = display.displayAttr;
        }
    }
    // If we havent scrolled at the end of the buffer then clear the lines scrolled if requested.
    else if(clear && display.displayRow == display.maxDisplayRow - 1)
    {
        startIdx = (display.backingRow - lines + 1) * display.maxBackingCol;
        endIdx   = startIdx + (lines * display.maxBackingCol);

        // Clear the shadow display scrollback RAM.
        for(dstAddr = startIdx; dstAddr < endIdx; dstAddr++)
        {
            display.displayCharBuf[dstAddr] = 0x20;
            display.displayAttrBuf[dstAddr] = display.displayAttr;
        }
    }

    // Refresh the display with buffer window contents
    mzRefreshDisplay();
    return(0);
}

// Method to scroll the display contents downwards for scrollback purposes.
uint8_t mzScrollDown(uint8_t lines)
{
    // Sanity check.
    if(lines > display.maxDisplayRow)
        return(1);

    // Restore cursor character before scrolling.
    mzFlashCursor(CURSOR_RESTORE);

    // Subtract the lines from the current row address. If the row falls below zero then scroll the display down.
    if((((int)display.backingRow) - lines) < 0) { display.backingRow = 0; } 
    else if( display.backingRow < display.maxDisplayRow ) { display.backingRow = display.maxDisplayRow -1; }
    else { display.backingRow -= lines; }

    // Same for the physical row pointer.
    if((((int)display.displayRow) - lines) < 0) { display.displayRow = 0; } 
    else if( display.displayRow < display.maxDisplayRow ) { display.displayRow = display.maxDisplayRow -1; }
    else { display.displayRow -= lines; }

    // Refresh display.
    mzRefreshDisplay();
    return(0);
}

// Method to delete lines from the display buffer, scrolling as necessary.
//
uint8_t mzDeleteLines(uint8_t lines)
{
    // Locals.
    uint32_t srcAddr;
    uint32_t dstAddr;
static char testbuf[VC_DISPLAY_BUFFER_SIZE*8];

    // Sanity check.
    if(lines == 0 || lines > display.maxDisplayRow)
        return(1);

    // Restore cursor character before scrolling.
    mzFlashCursor(CURSOR_RESTORE);

    for(srcAddr=0; srcAddr < VC_DISPLAY_BUFFER_SIZE; srcAddr++)
    {
        if(srcAddr % display.maxBackingCol == 0)
        {
            if(srcAddr % 0x320 == 0)
            {
                pr_info("%s\n", testbuf);
                testbuf[0] = 0x00;
            }
            sprintf(&testbuf[strlen(testbuf)], "\n%04x ", srcAddr);
        }
        sprintf(&testbuf[strlen(testbuf)], "%c", display.displayCharBuf[srcAddr]);
    }
    pr_info("%s\n", testbuf);

    // Starting line based on the current display row.
    srcAddr = ((display.displayRow+lines) * display.maxBackingCol);
    dstAddr = (display.displayRow * display.maxBackingCol);

    // Move data up by 'lines'.
    for(; srcAddr < VC_DISPLAY_BUFFER_SIZE; srcAddr++, dstAddr++)
    {
        display.displayCharBuf[dstAddr] = display.displayCharBuf[srcAddr];
        display.displayAttrBuf[dstAddr] = display.displayAttrBuf[srcAddr];
    }
pr_info("SrcAddr=%04x, DstAddr=%04x\n", srcAddr, dstAddr);
    // Fill end of buffer with space.
    for(; dstAddr < VC_DISPLAY_BUFFER_SIZE; dstAddr++)
    {
        display.displayCharBuf[dstAddr] = 0x20;
        display.displayAttrBuf[dstAddr] = display.displayAttr;
    }

    for(srcAddr=0; srcAddr < VC_DISPLAY_BUFFER_SIZE; srcAddr++)
    {
        if(srcAddr % display.maxBackingCol == 0)
        {
            if(srcAddr % 0x320 == 0)
            {
                pr_info("%s\n", testbuf);
                testbuf[0] = 0x00;
            }
            sprintf(&testbuf[strlen(testbuf)], "\n%04x ", srcAddr);
        }
        sprintf(&testbuf[strlen(testbuf)], "%c", display.displayCharBuf[srcAddr]);
    }
    pr_info("%s\n", testbuf);

    // Refresh the display with buffer window contents
    mzRefreshDisplay();
    return(0);
}

// Method to insert lines in the display buffer, scrolling as necessary.
//
uint8_t mzInsertLines(uint8_t lines)
{
    // Locals.
    uint32_t srcAddr;
    uint32_t dstAddr;

    // Sanity check.
    if(lines == 0 || lines > display.maxDisplayRow)
        return(1);

    // Restore cursor character before scrolling.
    mzFlashCursor(CURSOR_RESTORE);

    // Source is the last line - 'lines' in the buffer 
    srcAddr = ((display.maxDisplayRow - lines) * display.maxBackingCol);
    dstAddr = ((display.maxDisplayRow)         * display.maxBackingCol);

    // Move data down by 'lines'.
    for(; dstAddr > ((display.displayRow + lines) * display.maxBackingCol)-1; srcAddr--, dstAddr--)
    {
        display.displayCharBuf[dstAddr] = display.displayCharBuf[srcAddr];
        display.displayAttrBuf[dstAddr] = display.displayAttrBuf[srcAddr];
    }
    // Fill new lines with space.
    do {
        display.displayCharBuf[dstAddr] = 0x20;
        display.displayAttrBuf[dstAddr] = display.displayAttr;
        if(dstAddr-- == 0) break;
    } while(dstAddr >= (display.displayRow * display.maxBackingCol));

    // Refresh the display with buffer window contents
    mzRefreshDisplay();
    return(0);
}

// Method to move the cursor within the physical display buffer.
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
            display.displayCol = (int)(display.displayCol + cnt) >= display.maxBackingCol ? display.maxBackingCol -1 : display.displayCol+cnt;
            break;
        case CURSOR_COLUMN:
            if(cnt < display.maxBackingCol)
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
    // Restore character under cursor before change of co-ordinates.
    mzFlashCursor(CURSOR_RESTORE);

    // Backing row tracks display row in direct cursor positioning.
    display.backingRow = y >= display.maxDisplayRow ? display.maxDisplayRow-1 : y;
    display.displayRow = y >= display.maxDisplayRow ? display.maxDisplayRow-1 : y;
    display.displayCol = x >= display.maxBackingCol ? display.maxBackingCol-1 : x;
    return(0);
}

// Stream method to output a character to the display.
//
int mzPutChar(char c)
{
    // Variables and locals.
    uint8_t  idx;
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
            mzScrollUp(1, 1, 1);
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
            mzPutChar(SPACE);
            display.displayCol = display.displayCol == 0 ? 0 : display.displayCol -1;
            output = 0;
            break;

        // Tab - expand by printing whitespace.
        case TAB:
            for(idx=0; idx < 4; idx++)
                mzPutChar(SPACE);
            output = 0;
            break;

        // Scroll display up.
        case SCROLL:
            mzScrollUp(1, 0, 1);
            output = 0;
            break;
    }

    // Output to display if flag set.
    if(output)
    {
        // Output character using default attributes.
        dispMemAddr = VIDEO_VRAM_BASE_ADDR + (display.displayRow * display.maxBackingCol) + display.displayCol;
        WRITE_HARDWARE(0, dispMemAddr, (char)dispCodeMap[(int)c].dispCode);
        display.displayCharBuf[(display.backingRow * display.maxBackingCol) + display.displayCol] = c;
        //
        dispMemAddr = VIDEO_ARAM_BASE_ADDR + (display.displayRow * display.maxBackingCol) + display.displayCol;
        WRITE_HARDWARE(0, dispMemAddr, display.displayAttr);
        display.displayAttrBuf[(display.backingRow * display.maxBackingCol) + display.displayCol] = display.displayAttr;
        if(++display.displayCol >= display.maxBackingCol)
        {
            if(display.lineWrap)
            {
                // Increment line and scroll if necessary.
                display.displayCol = 0;
                mzScrollUp(1, 1, 1);
            } else
            {
                display.displayCol = display.maxBackingCol-1;
            }
        }
    }

    if(ctrl.debug && !display.inDebug) mzDebugOut(3, c);
    return(0);
}

// Method to output a character to the display.
//
int mzPrintChar(char c)
{
    // If the ansi terminal emulator is enabled, parse the character through the emulator.
    if(display.useAnsiTerm)
    {
        mzAnsiTerm(c);
    } else
    {
        mzPutChar(c);
    }
    return(0);
}

// Method to put a character onto the display without character interpretation.
//
int mzPutRaw(char c)
{
    // Variables and locals.
    uint32_t dispMemAddr;

    // Output character using default attributes.
    dispMemAddr = VIDEO_VRAM_BASE_ADDR + (display.displayRow * display.maxBackingCol) + display.displayCol;
    WRITE_HARDWARE(0, dispMemAddr, (char)dispCodeMap[(int)c].dispCode);
    display.displayCharBuf[(display.backingRow * display.maxBackingCol) + display.displayCol] = c;
    //
    dispMemAddr = VIDEO_ARAM_BASE_ADDR + (display.displayRow * display.maxBackingCol) + display.displayCol;
    WRITE_HARDWARE(0, dispMemAddr, display.displayAttr);
    display.displayAttrBuf[(display.backingRow * display.maxBackingCol) + display.displayCol] = display.displayAttr;
    if(++display.displayCol >= display.maxBackingCol)
    {
        if(display.lineWrap)
        {
            // Increment line and scroll if necessary.
            display.displayCol = 0;
            mzScrollUp(1, 0, 1);
        } else
        {
            display.displayCol = display.maxBackingCol-1;
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
            display.displayAttr = VMATTR_FG_WHITE | VMATTR_BG_BLUE;
            break;
           
        // Invert FG/BG.
        case 7:
            // If background goes to white with default colours, it is not so readable so change foreground colour.
            if((display.displayAttr & VMATTR_FG_MASKIN) == VMATTR_FG_WHITE)
            {
                display.displayAttr = VMATTR_FG_WHITE | VMATTR_BG_RED;
            } else
            {
                display.displayAttr = (display.displayAttr & VMATTR_FG_MASKIN) >> 4 | (display.displayAttr & VMATTR_BG_MASKIN) << 4;
            }
            break;

        // Foreground black.
        case 30:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_BLACK;
            break;
            
        // Foreground red.
        case 31:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_RED;
            break;
           
        // Foreground green.
        case 32:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_GREEN;
            break;
            
        // Foreground yellow.
        case 33:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_YELLOW;
            break;
          
        // Foreground blue.
        case 34:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_BLUE;
            break;
            
        // Foreground magenta.
        case 35:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_PURPLE;
            break;
           
        // Foreground cyan.
        case 36:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_CYAN;
            break;
            
        // Foreground white.
        case 37:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_WHITE;
            break;
           
        // Default Foreground colour.
        case 39:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_WHITE;
            break;
           
        // Background black.
        case 40:
            display.displayAttr = (display.displayAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_BLACK;
            break;
            
        // Background red.
        case 41:
            display.displayAttr = (display.displayAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_RED;
            break;
           
        // Background green.
        case 42:
            display.displayAttr = (display.displayAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_GREEN;
            break;
            
        // Background yellow.
        case 43:
            display.displayAttr = (display.displayAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_YELLOW;
            break;
          
        // Background blue.
        case 44:
            display.displayAttr = (display.displayAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_BLUE;
            break;
            
        // Background magenta.
        case 45:
            display.displayAttr = (display.displayAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_PURPLE;
            break;
           
        // Background cyan.
        case 46:
            display.displayAttr = (display.displayAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_CYAN;
            break;
            
        // Background white.
        case 47:
            display.displayAttr = (display.displayAttr & VMATTR_BG_MASKOUT) | VMATTR_BG_WHITE;
            break;
           
        // Default Background colour.
        case 49:
            display.displayAttr = (display.displayAttr & VMATTR_FG_MASKOUT) | VMATTR_FG_BLUE;
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
    uint8_t idx;
    char    *ptr;
    int     result;

  #if (TARGET_HOST_MZ80A == 1)
    if(ctrl.debug && !display.inDebug) pr_info("(%02x, \'%c\')\n", c, c > 0x1f ? c : ' ');
  #endif
    //pr_info("(%02x, \'%c\')\n", c, c > 0x1f ? c : ' ');

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
                    ansiterm.setDisplayMode = 0;
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
                    mzScrollUp(1, 0, 0);
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
                    for(idx=0; idx < 4; idx++)
                        mzPutRaw(SPACE);
                    break;

                // ENQuire - send back answerbackString.
                case ENQ: {
                    char response[MAX_KEYB_BUFFER_SIZE];

                    // Build response and push onto keyboard buffer.
                    sprintf(response, "SharpMZ TTY FusionX\n");
                    mzPushKey(response);
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Enquire data:%s\n", response);
                  #endif
                    } break;

                // BELL - sound a short bell.
                case BELL:
                    mzBeep(500, 100);
                    break;

                default:
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("ESC Raw(%02x, \'%c\')\n", c, c > 0x1f ? c : ' ');
                  #endif
                    mzPutRaw(c);
                    break;
            }
			break;

        // Escape found, now look for next in the ESCape sequence or abort the state if a character is received which is not expected, print the character.
		case ANSITERM_BRACKET:
          #if (TARGET_HOST_MZ80A == 1)
            if(ctrl.debug && !display.inDebug) pr_info("Bracket(%02x, \'%c\')\n", c, c > 0x1f ? c : ' ');
          #endif
            switch(c)
            {
                case ESC:
                    break;

                case '[':
                    ansiterm.state = ANSITERM_PARSE;
                    break;

                // Back Index
                case '6':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Back Index\n");
                  #endif
                    break;

                // Save the current cursor position.
                case '7':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Save position\n");
                  #endif
                    ansiterm.saveRow = display.displayRow;
                    ansiterm.saveCol = display.displayCol;
                    ansiterm.saveDisplayRow = display.backingRow;
                    ansiterm.state = ANSITERM_ESC;
                    break;

                // Restore the current cursor position.
                case '8':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Restore position\n");
                  #endif
                    display.displayRow = ansiterm.saveRow;
                    display.displayCol = ansiterm.saveCol;
                    display.backingRow  = ansiterm.saveDisplayRow;
                    ansiterm.state = ANSITERM_ESC;
                    break;
                   
                // Forward Index
                case '9':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Forward Index\n");
                  #endif
                    break;
                   
                // Select application keypad
                case '=':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Select application keypad\n");
                  #endif
                    break;

                // Select normal keypad
                case '>':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Select normal keypad\n");
                  #endif
                    break;
                   
                // Index
                case 'D':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Index\n");
                  #endif
                    break;
                   
                // Next Line
                case 'E':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Next Line\n");
                  #endif
                    mzPutChar(LF);
                    break;
                   
                // Cursor to lower left corner of display.
                case 'F':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Cursor to lower left corner.\n");
                  #endif
                    mzSetCursor(0, display.maxDisplayRow);
                    break;
                    
                // Tab Set
                case 'H':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Tab Set\n");
                  #endif
                    break;
                   
                // Reverse Index
                case 'M':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Reverse Index\n");
                  #endif
                    break;
                   
                // Full reset.
                case 'c':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Full reset.\n");
                  #endif
                    // Clear display.
                    mzClearDisplay(3, 1);

                    // Ensure module is at default configuration.
                    display        = displayDefault;
                    keyboard       = keyboardDefault;
                    audio          = audioDefault;
                    break;
                   
                // Memory lock above cursor.
                case 'l':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Memory lock above cursor.\n");
                  #endif
                    break;
                   
                // Memory unlock above cursor.
                case 'm':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Memory unlock above cursor.\n");
                  #endif
                    break;

                // Change character set.
                case 'n':
                case 'o':
                case '|':
                case '}':
                case '~':
                  #if (TARGET_HOST_MZ80A == 1)
                    if(ctrl.debug && !display.inDebug) pr_info("Change character set - not yet supported.\n");
                  #endif
                    break;

                default: {
                    // Output debug message so the escape sequence can be seen.
                    pr_info("Unhandled Escape Sequence: ESC %c\n", c);

                    ansiterm.state = ANSITERM_ESC;
                    mzPutRaw(c);
                    } break;
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
              #if (TARGET_HOST_MZ80A == 1)
                if(ctrl.debug && !display.inDebug) pr_info("IsDigit(%02x, \'%c\')\n", c, c > 0x1f ? c : ' ');
              #endif
                ansiterm.charbuf[ansiterm.charcnt++] = c;
                ansiterm.charbuf[ansiterm.charcnt] = 0x00;
            }
            else if(c == ';')
            {
                ptr = (char *)&ansiterm.charbuf;
                if(kstrtoint(ptr, 10, &result) != 0)
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
                ansiterm.setDisplayMode = 1;
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
                    if(kstrtoint(ptr, 10, &result) == 0)
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
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Set Cursor (%d,%d)\n", (uint8_t)ansiterm.param[1]-1, (uint8_t)ansiterm.param[0]-1);
                          #endif
                            mzSetCursor((uint8_t)ansiterm.param[1]-1, (uint8_t)ansiterm.param[0]-1);
                        } 
                        // Home cursor.
                        else if(ansiterm.paramcnt == 0)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Set Cursor (%d,%d)\n", 0, 0);
                          #endif
                            mzSetCursor(0, 0);
                        }
                        break;

                    // Move cursor up.
                    case 'A':
                        if(ansiterm.paramcnt > 0)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor Up (%d)\n", (uint8_t)ansiterm.param[0]);
                          #endif
                            mzMoveCursor(CURSOR_UP, (uint8_t)ansiterm.param[0]);
                        } else
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor Up (%d)\n", 1);
                          #endif
                            mzMoveCursor(CURSOR_UP, 1);
                        }
                        break;

                    // Move cursor down.
                    case 'B':
                        if(ansiterm.paramcnt > 0)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor Down (%d)\n", (uint8_t)ansiterm.param[0]);
                          #endif
                            mzMoveCursor(CURSOR_DOWN, (uint8_t)ansiterm.param[0]);
                        } else
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor Down (%d)\n", 1);
                          #endif
                            mzMoveCursor(CURSOR_DOWN, 1);
                        }
                        break;

                    // Move cursor right.
                    case 'C':
                        if(ansiterm.paramcnt > 0)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor Right (%d)\n", (uint8_t)ansiterm.param[0]);
                          #endif
                            mzMoveCursor(CURSOR_RIGHT, (uint8_t)ansiterm.param[0]);
                        } else
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor Right (%d)\n", 1);
                          #endif
                            mzMoveCursor(CURSOR_RIGHT, 1);
                        }
                        break;

                    // Move cursor left.
                    case 'D':
                        if(ansiterm.paramcnt > 0)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor Left (%d)\n", (uint8_t)ansiterm.param[0]);
                          #endif
                            mzMoveCursor(CURSOR_LEFT, (uint8_t)ansiterm.param[0]);
                        } else
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor Left (%d)\n", 1);
                          #endif
                            mzMoveCursor(CURSOR_LEFT, 1);
                        }
                        break;

                    // Move cursor to start of next line.
                    case 'E':
                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Cursor Next Line\n");
                      #endif
                        mzMoveCursor(CURSOR_NEXT_LINE, 0);
                        break;

                    // Move cursor to start of previous line.
                    case 'F':
                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Cursor Start Previous Line\n");
                      #endif
                        mzMoveCursor(CURSOR_PREV_LINE, 0);
                        break;

                    // Move cursor to absolute column position.
                    case 'G':
                        if(ansiterm.paramcnt > 0)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor ABS Position (%d)\n", (uint8_t)ansiterm.param[0]-1);
                          #endif
                            mzMoveCursor(CURSOR_COLUMN, (uint8_t)ansiterm.param[0]-1);
                        } else
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Cursor ABS Position (%d)\n", 0);
                          #endif
                            mzMoveCursor(CURSOR_COLUMN, 0);
                        }
                        break;

                    // Insert lines.
                    case 'L': {
                        uint8_t linesToInsert = 1;

                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Insert Line\n");
                      #endif
                        if(ansiterm.paramcnt > 0)
                        {
                            linesToInsert = ansiterm.param[0];
                        }
                        mzInsertLines(linesToInsert);
                        } break;
                             
                    // Delete lines.
                    case 'M': {
                      uint8_t linesToDelete = 1;

                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Delete Line\n");
                      #endif
                        if(ansiterm.paramcnt > 0)
                        {
                            linesToDelete = ansiterm.param[0];
                        }
                        mzDeleteLines(linesToDelete);
                        } break;

                    // Scroll up.
                    case 'S':
                        if(ansiterm.paramcnt > 0)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Scroll Up (%d)\n", (uint8_t)ansiterm.param[0]);
                          #endif
                            mzScrollUp((uint8_t)ansiterm.param[0], 0, 0);
                        } else
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Scroll Up (%d)\n", 1);
                          #endif
                            mzScrollUp(1, 0, 0);
                        }
                        break;

                    // Scroll down.
                    case 'T':
                        if(ansiterm.paramcnt > 0)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Scroll Down (%d)\n", (uint8_t)ansiterm.param[0]);
                          #endif
                            mzScrollDown((uint8_t)ansiterm.param[0]);
                        } else
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Scroll Down (%d)\n", 1);
                          #endif
                            mzScrollDown(1);
                        }
                        break;

                    // Report Cursor.
                    case 'R':
                        pr_info("Report Cursor:");
                        for(idx=0; idx < ansiterm.paramcnt; idx++)
                            pr_info("%d,", ansiterm.param[idx]);
                        pr_info("\n");
                        break;

                    // Save the current cursor position.
                    case 's':
                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Save current cursor position\n");
                      #endif
                        ansiterm.saveRow = display.displayRow;
                        ansiterm.saveCol = display.displayCol;
                        ansiterm.saveDisplayRow = display.backingRow;
                        break;

                    // Restore the current cursor position.
                    case 'u':
                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Restore current cursor position\n");
                      #endif
                        display.displayRow = ansiterm.saveRow;
                        display.displayCol = ansiterm.saveCol;
                        display.backingRow  = ansiterm.saveDisplayRow;
                        break;

                    // Report data.
                    case 'n': {
                        char response[MAX_KEYB_BUFFER_SIZE];

                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Report data\n");
                      #endif

                        // Report current cursor position?
                        //
                        if(ansiterm.paramcnt > 0)
                        {
                            switch(ansiterm.param[0])
                            {
                                case 0:
                                    // Build response and push onto keyboard buffer.
                                    sprintf(response, "OK");
                                    break;
                                case 5:
                                    // Build response and push onto keyboard buffer.
                                    sprintf(response, "SharpMZ TTY OK");
                                    break;
                                case 6:
                                default:
                                    // Build response and push onto keyboard buffer.
                                    sprintf(response, "%c[%d;%dR", ESC, display.displayRow+1, display.displayCol+1);
                                    break;
                            }
                            mzPushKey(response);
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Report data:%02x,%d,%dR\n", ESC, display.displayRow+1, display.displayCol+1);
                          #endif
                        }
                        } break;

                    // Clear display or block of display.
                    case 'J': {
                        // Default is to clear the complete display but not scrollback buffer.
                        int clearMode = 0;
                        if(ansiterm.paramcnt > 0 && ansiterm.param[0] < 4)
                        {
                            clearMode = ansiterm.param[0];
                        }
                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Clear:%d\n", clearMode);
                      #endif
                        mzClearDisplay(clearMode, 1);
                        } break;

                    // Clear line.
                    case 'K': {
                        int clearRow      = -1;
                        int clearColStart = display.displayCol;
                        int clearColEnd   = display.maxBackingCol-1;
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
                            else if(ansiterm.param[0] == 2)
                            {
                                clearColEnd   = 0;
                            }
                        }
                        // Clear current row from Start to End.
                        mzClearLine(clearRow, clearColStart, clearColEnd, 0);
                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Clear Line:%d, %d, %d\n", clearRow, clearColStart, clearColEnd);
                      #endif
                        } break;

                    // Set Display Attributes.
                    case 'm': {
                        uint8_t idx;

                        // Process all the attributes.
                        for(idx=0; idx < ansiterm.paramcnt; idx++)
                        {
                          #if (TARGET_HOST_MZ80A == 1)
                            if(ctrl.debug && !display.inDebug) pr_info("Set attribute:%d\n", ansiterm.param[idx]);
                          #endif
                            mzSetAnsiAttribute(ansiterm.param[idx]);
                        }
                        } break;

                    // Show cursor?
                    case 'h':
                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Show cursor\n");
                      #endif
                        // VT220/Ansi show cursor command?
                        if(ansiterm.paramcnt > 0 && ansiterm.param[0] == 25)
                        {
                            mzFlashCursor(CURSOR_ON);
                        }
                        break;

                    // Hide cursor?
                    case 'l':
                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Hide cursor\n");
                      #endif
                        if(ansiterm.paramcnt > 0 && ansiterm.param[0] == 25)
                        {
                            mzFlashCursor(CURSOR_OFF);
                        }
                        break;

                    // Set scrolling region
                    case 'r': {
                        char buf[80];

                      #if (TARGET_HOST_MZ80A == 1)
                        if(ctrl.debug && !display.inDebug) pr_info("Hide cursor\n");
                      #endif
                        sprintf(buf, "Set Scrolling Region: ESC [ ");
                        for(idx=0; idx < ansiterm.paramcnt; idx++)
                        {
                            sprintf(&buf[strlen(buf)], "%d ", ansiterm.param[idx]);
                        }
                        sprintf(&buf[strlen(buf)], "%c", c);
                        pr_info("%s, X=%d,Y=%d\n", buf, display.displayCol, display.displayRow);
                        } break;

                    default: {
                        uint8_t idx;
                        char    buf[80];

                        // Output debug message so the escape sequence can be seen.
                        sprintf(buf, "Unhandled Escape Sequence: ESC [ ");
                        for(idx=0; idx < ansiterm.paramcnt; idx++)
                        {
                            sprintf(&buf[strlen(buf)], "%d ", ansiterm.param[idx]);
                        }
                        sprintf(&buf[strlen(buf)], "%c", c);
                        pr_info("%s\n", buf);

                        mzPutRaw(c);
                        ansiterm.state = ANSITERM_ESC;
                        } break;
                }
                ansiterm.state = ANSITERM_ESC;

                if(ctrl.debug && !display.inDebug) mzDebugOut(1, c);
            }
            break;
	}
	return(0) ;
}

// Method to output debug data to track issues with the display or ansi emulator.
//
void mzDebugOut(uint8_t set, uint8_t data1)
{
    // Locals.
    uint8_t idx;
    char    buf[80];
    // Save current coordinates.
    uint8_t sr = display.displayRow;
    uint8_t scr= display.backingRow;
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
            display.backingRow  = 0;

            // Output required data.
            sprintf(buf, "D:%d-%d-%d:%c:%d,%d,%d:", sr,sc,scr,data1,ansiterm.paramcnt, ansiterm.setDisplayMode, ansiterm.setExtendedMode);
            for(idx=0; idx < ansiterm.paramcnt; idx++)
                sprintf(&buf[strlen(buf)], "%d,", ansiterm.param[idx]);

            pr_info("%s\n", buf);
            } break;

        case 2: {
            // Set location where debug data should be displayed.
            display.displayRow = 1;
            display.displayCol = 40;
            display.backingRow  = 1;

            sprintf(buf, "K:%d:", strlen(keyboard.keyBuf));
            for(idx=0; idx < strlen(keyboard.keyBuf); idx++)
                sprintf(&buf[strlen(buf)], "%02x,", keyboard.keyBuf[idx]);

            pr_info("%s\n", buf);
            } break;

        case 3: {
            // Set location where debug data should be displayed.
            display.displayRow = 2;
            display.displayCol = 40;
            display.backingRow  = 2;

            sprintf(buf, "X:%d,%d,%d,%d,%d,%d:%02x", sr, sc, scr, display.maxBackingRow, display.maxDisplayRow, display.maxBackingCol, data1);

            pr_info("%s\n", buf);
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
    display.backingRow = scr;
    display.displayCol = sc;
    return;
}

// Method to flash a cursor at current X/Y location on the physical display.
uint8_t mzFlashCursor(enum CURSOR_STATES state)
{
    // Locals.
    uint32_t dispMemAddr = VIDEO_VRAM_BASE_ADDR + (display.displayRow * display.maxBackingCol) + display.displayCol;
    uint16_t srcIdx = (display.backingRow * display.maxBackingCol) + display.displayCol;

    // Action according to request.
    switch(state)
    {
        // Disable the cursor flash mechanism.
        case CURSOR_OFF:
        default:
            // Only restore character if it had been previously saved and active.
            if(keyboard.cursorOn == 1 && keyboard.displayCursor == 1)
            {
                WRITE_HARDWARE(0, dispMemAddr, dispCodeMap[display.displayCharBuf[srcIdx]].dispCode);
            }
            keyboard.cursorOn = 0;
            keyboard.displayCursor = 0;
            break;

        // Enable the cursor mechanism.
        case CURSOR_ON:
            keyboard.cursorOn = 1;
            keyboard.displayCursor = 0;
            break;

        // Restore the character under the cursor.
        case CURSOR_RESTORE:
            if(keyboard.displayCursor == 1)
            {
                WRITE_HARDWARE(0, dispMemAddr, dispCodeMap[display.displayCharBuf[srcIdx]].dispCode);
                keyboard.displayCursor = 0;
            }
            break;

        // If enabled and timer expired, flash cursor.
        case CURSOR_FLASH:
            if(keyboard.cursorOn == 1 && (keyboard.flashTimer == 0L || keyboard.flashTimer + KEYB_FLASH_TIME < (ktime_get_ns()/1000000)))
            {
                keyboard.displayCursor = keyboard.displayCursor == 1 ? 0 : 1;
                keyboard.flashTimer = (ktime_get_ns()/1000000);
                if(keyboard.displayCursor == 1)
                {
                    switch(keyboard.mode)
                    {
                        case KEYB_LOWERCASE:
                            WRITE_HARDWARE(0, dispMemAddr, CURSOR_UNDERLINE);
                            break;
                        case KEYB_CAPSLOCK:
                            WRITE_HARDWARE(0, dispMemAddr, CURSOR_BLOCK);
                            break;
                        case KEYB_SHIFTLOCK:
                        default:
                            WRITE_HARDWARE(0, dispMemAddr, CURSOR_THICK_BLOCK);
                            break;
                    }
                } else
                {
                    WRITE_HARDWARE(0, dispMemAddr, dispCodeMap[display.displayCharBuf[srcIdx]].dispCode);
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
    // Locals.
    uint8_t   strobe;
    uint8_t   strobeIdx;
    uint16_t  keyIdx;
    uint64_t  delay;

    // Sequence through the strobe lines and read back the scan data into the buffer.
    for(strobe=0xF0; strobe < 0xFA; strobe++)
    {
        // Output the keyboard strobe.
        WRITE_HARDWARE(0, MBADDR_KEYPA, strobe);

        // Slight delay to allow for bounce.
        delay = ktime_get_ns(); while((ktime_get_ns() - delay) < 1000000);

        // Read the scan lines.
        READ_HARDWARE_INIT(0, MBADDR_KEYPB);
        keyboard.scanbuf[0][strobe-0xF0] = ctrl.suspendIO == 0 ? READ_HARDWARE() : 0xFF;
    }

    // Now look for active keys.
    for(strobeIdx=0; strobeIdx < 10; strobeIdx++)
    {
        // Skip over modifier keys.
        //if(strobeIdx == 8) continue;

        // To cater for multiple keys being pressed on the same row, we need to process per bit.
        for(keyIdx=1; keyIdx < 256; keyIdx <<= 1)
        {
            if((keyboard.scanbuf[0][strobeIdx] & keyIdx) == 0)
            {
                keyboard.keydown[strobeIdx] = keyboard.keydown[strobeIdx] & ~keyIdx;
            } else
            {
                keyboard.keydown[strobeIdx] = keyboard.keydown[strobeIdx] | keyIdx;
            }
            if((keyboard.scanbuf[1][strobeIdx] & keyIdx) != (keyboard.scanbuf[0][strobeIdx] & keyIdx) && (keyboard.scanbuf[1][strobeIdx] & keyIdx) == 0)
            {
                keyboard.keyup[strobeIdx] = keyboard.keyup[strobeIdx] & ~keyIdx;
            } else
            {
                keyboard.keyup[strobeIdx] = keyboard.keyup[strobeIdx] | keyIdx;
            }
        }

        // Additional key added to the same row then reset hold state.
        if(keyboard.scanbuf[0][strobeIdx] != 0xFF && (keyboard.scanbuf[0][strobeIdx]  != keyboard.scanbuf[1][strobeIdx]))
        {
            keyboard.keyhold[strobeIdx] = 0;
        } else

        // No change in key scans but keys being held, increment hold status.
        if(keyboard.scanbuf[0][strobeIdx] != 0xFF && (keyboard.scanbuf[0][strobeIdx] == keyboard.scanbuf[1][strobeIdx]))
        {
            keyboard.keyhold[strobeIdx]++;
        } else

        // Keys released, reset.
        if(keyboard.scanbuf[0][strobeIdx] == 0xFF && keyboard.scanbuf[1][strobeIdx] == 0xFF)
        {
            keyboard.keyhold[strobeIdx] = 0;
            keyboard.keydown[strobeIdx] = 0xFF;
            keyboard.keyup[strobeIdx] = 0xFF;
        }
        keyboard.scanbuf[1][strobeIdx] = keyboard.scanbuf[0][strobeIdx];
    }

  #if (TARGET_HOST_MZ700 == 1)
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
  #endif
  #if (TARGET_HOST_MZ80A == 1)
    // Check for modifiers.
    //
    if((keyboard.scanbuf[0][0] & 0x01) == 0)
    {
        keyboard.shiftKey = 1;
    } else
    {
        keyboard.shiftKey = 0;
    }
    if((keyboard.scanbuf[0][0] & 0x80) == 0 && keyboard.shiftKey == 0)
    {
        keyboard.ctrlKey = 1;
    } else
    {
        keyboard.ctrlKey = 0;
    }
    if((keyboard.scanbuf[0][0] & 0x80) == 0 && keyboard.shiftKey == 1)
    {
        keyboard.breakKey = 1;
    } else
    {
        keyboard.breakKey = 0;
    }
  #endif

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
    int     retcode = -1;
    uint8_t idx;
    uint8_t strobeIdx;
    uint8_t keyIdx;
    uint8_t key;
    uint8_t modifiedMode;

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
            for(strobeIdx=0; strobeIdx < 10; strobeIdx++)
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
                    keyIdx = 0;
                    key = keyboard.keydown[strobeIdx];
                    modifiedMode = keyboard.ctrlKey == 1                                     ? KEYB_CTRL : 
                                   keyboard.mode == KEYB_LOWERCASE && keyboard.shiftKey == 1 ? KEYB_SHIFTLOCK : 
                                   keyboard.mode == KEYB_SHIFTLOCK && keyboard.shiftKey == 1 ? KEYB_CAPSLOCK  : 
                                   keyboard.mode == KEYB_CAPSLOCK  && keyboard.shiftKey == 1 ? KEYB_LOWERCASE : 
                                   keyboard.mode;

                    // MZ-80A has control keys and data key on one strobe line, either we store the previous keydown map and compare or
                    // make special exceptions.
                  #if TARGET_HOST_MZ80A == 1
                    if(strobeIdx == 0 && keyboard.ctrlKey == 1 && (keyboard.keydown[strobeIdx] & 0x7f) != 0x7f) { keyIdx = 1; key <<= 1; }
                  #endif

                    // Look for the key which was pressed.
                    for(; keyIdx < 8 && key & 0x80; keyIdx++, key=key << 1);
                    retcode = scanCodeMap[modifiedMode].scanCode[(strobeIdx*8)+keyIdx];

                    // Setup auto repeat.
                    keyboard.repeatKey = retcode;
                    keyboard.holdTimer = (ktime_get_ns()/1000000);
                } else
                if(keyboard.keydown[strobeIdx] != 0xFF && keyboard.keyhold[strobeIdx] > 1 && keyboard.holdTimer + KEYB_AUTOREPEAT_INITIAL_TIME < (ktime_get_ns()/1000000))
                {
                    keyboard.autorepeat = 1;
                    keyboard.holdTimer = (ktime_get_ns()/1000000);
                } else
                if(keyboard.keydown[strobeIdx] != 0xFF && keyboard.keyhold[strobeIdx] > 1 && keyboard.autorepeat == 1 && keyboard.holdTimer + KEYB_AUTOREPEAT_TIME < (ktime_get_ns()/1000000))
                {
                    keyboard.holdTimer = (ktime_get_ns()/1000000);
                    retcode = keyboard.repeatKey;
                }
            }
          
            // Override an internal control key if DualKey is set.
            if(retcode == GRAPHKEY && (keyboard.dualmode & KEYB_DUAL_GRAPH) != 0) retcode = ALPHAKEY;

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
                    keyboard.mode = (keyboard.mode == KEYB_GRAPHMODE ? KEYB_CAPSLOCK : KEYB_GRAPHMODE);
                    retcode = -1;
                    break;
                  
                // Some machines, such as the MZ-80A dont have seperate Alpha/Graph keys, only one is available, so need to be able to use this one key to activate multiple modes.
                case ALPHAGRAPHKEY:
                    keyboard.dualmode = (keyboard.dualmode & KEYB_DUAL_GRAPH) != 0 ? (keyboard.dualmode & ~KEYB_DUAL_GRAPH) : (keyboard.dualmode | KEYB_DUAL_GRAPH);
                    retcode = -1;
                    break;

                // If the debug key is pressed, toggle the debugging output on/off.
                case DEBUGKEY:
                    ctrl.debug = (ctrl.debug == 0 ? 1 : 0);
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

                // Clear display.
                case CLRKEY:
                    mzClearDisplay(3, 1);
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
            for(idx=0; idx < (sizeof(ansiKeySeq)/sizeof(*ansiKeySeq)); idx++)
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
            if(ctrl.debug && !display.inDebug) mzDebugOut(2, retcode);
        }
    }

    return(retcode);
}

// File stream method to get a key from the keyboard.
//
int mzGetChar(void)
{
    return(mzGetKey(1));
}

// Method to suspend the physical I/O which updates the host framebuffer. All operations
// continue but only into backing RAM.
void mzSuspendIO(void)
{
    // Disable IO.
    ctrl.suspendIO = 1;
    return;
}

// Method to resume the physical I/O updating the host framebuffer. A display refresh is
// required to update the physical framebuffer with the state of the backing RAM.
void mzResumeIO(void)
{
    // Locals.

    // Re-enable IO.
    ctrl.suspendIO = 0;

    // Ensure hardware is in a defined state.
    mzInitMBHardware();

    // Ensure the display is in the correct resolution.
    mzSetDisplayWidth(display.maxBackingCol);

    // Refresh display.
    mzRefreshDisplay();
    return;
}

// Method to write a string into the frame buffer at given co-ordinates with optional
// attribute override.
void mzWriteString(uint8_t x, uint8_t y, char *str, int attr)
{
    // Locals.
    //
    uint8_t       *ptr;

    // Locate the cursor pre-write.
    mzSetCursor(x, y);

    // If attribute provided, update prior to write.
    if(attr >= 0)
    {
        display.displayAttr = (uint8_t)attr;
    }

    // Output the string.
    for(ptr=str; *ptr != 0x00; ptr++)
    {
        mzPrintChar((*ptr));
    }
    
    return;
}

// Method to schedule and service any time based requirements of the Sharp MZ driver.
// This method is called every 10ms.
//
void mzService(void)
{
    // If an audio output down counter is running, when it gets to 1, disable the audio output.
    if(audio.audioStopTimer > 0)
    {
        if(--audio.audioStopTimer == 1)
        {
            audio.audioStopTimer = 0;

            // Disable the hardware sound output.
            WRITE_HARDWARE(0, MBADDR_SUNDG, 0x00);                           // Disable sound.
        }
    }
    return;
}

//////////////////////////////////////////////////////////////
// End of Sharp MZ i/f methods                              //
//////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
