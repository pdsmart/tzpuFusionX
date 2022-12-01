/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80menu.h
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 User Interface Menu
//                  This file contains the declarations required to provide a menu system allowing a
//                  user to configure and load/save applications/data.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Oct 2022 - Initial write of the z80 kernel driver software.
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
#ifndef Z80MENU_H
#define Z80MENU_H

#ifdef __cplusplus
    extern "C" {
#endif

// Function definitions.
//
void           z80menu(void);

#ifdef __cplusplus
}
#endif
#endif // Z80MENU_H
