---------------------------------------------------------------------------------------------------------
--
-- Name:            tzpuFusionX_pkg.vhd
-- Created:         Nov 2022
-- Author(s):       Philip Smart
-- Description:     tzpuFusionX CPLD configuration file.
--                                                     
--                  This module contains parameters for the CPLD in the tzpuFusionX project
--                  which targets the MZ-80A host.
--
-- Credits:         
-- Copyright:       (c) 2018-23 Philip Smart <philip.smart@net2net.org>
--
-- History:         Nov 2022 v1.0 - Snapshot taken from the MZ-2000 version of the tzpuFusionX source.
--
---------------------------------------------------------------------------------------------------------
-- This source file is free software: you can redistribute it and-or modify
-- it under the terms of the GNU General Public License as published
-- by the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This source file is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http:--www.gnu.org-licenses->.
---------------------------------------------------------------------------------------------------------

library ieee;
library pkgs;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;

package tzpuFusionX_pkg is

    ------------------------------------------------------------ 
    -- Constants
    ------------------------------------------------------------ 

    -- Potential logic state constants.
    constant YES                      : std_logic := '1';
    constant NO                       : std_logic := '0';
    constant HI                       : std_logic := '1';
    constant LO                       : std_logic := '0';
    constant ONE                      : std_logic := '1';
    constant ZERO                     : std_logic := '0';
    constant HIZ                      : std_logic := 'Z';

    -- CPLD Command instructions.
    constant CPLD_CMD_RESET_HOST      : integer   := 1;
    constant CPLD_CMD_HOLD_HOST_BUS   : integer   := 2;
    constant CPLD_CMD_RELEASE_HOST_BUS: integer   := 3;

    -- Target hardware modes.
    constant MODE_MZ80K               : integer   := 0;
    constant MODE_MZ80C               : integer   := 1;
    constant MODE_MZ1200              : integer   := 2;
    constant MODE_MZ80A               : integer   := 3;
    constant MODE_MZ700               : integer   := 4;
    constant MODE_MZ800               : integer   := 5;
    constant MODE_MZ80B               : integer   := 6;
    constant MODE_MZ2000              : integer   := 7;

    -- Memory management modes.
    constant TZMM_ORIG                : integer   := 00;                     -- Original Sharp mode, no tranZPUter features are selected except the I/O control registers (default: 0x60-063).
    constant TZMM_BOOT                : integer   := 01;                     -- Original mode but E800-EFFF is mapped to tranZPUter RAM so TZFS can be booted.
    constant TZMM_TZFS                : integer   := 02;                     -- TZFS main memory configuration. all memory is in tranZPUter RAM, E800-FFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected.
    constant TZMM_TZFS2               : integer   := 03;                     -- TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 1.
    constant TZMM_TZFS3               : integer   := 04;                     -- TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 2.
    constant TZMM_TZFS4               : integer   := 05;                     -- TZFS main memory configuration. all memory is in tranZPUter RAM, E800-EFFF is used by TZFS, SA1510 is at 0000-1000 and RAM is 1000-CFFF, 64K Block 0 selected, F000-FFFF is in 64K Block 3.
    constant TZMM_CPM                 : integer   := 06;                     -- CPM main memory configuration, all memory on the tranZPUter board, 64K block 4 selected. Special case for F3C0:F3FF & F7C0:F7FF (floppy disk paging vectors) which resides on the mainboard.
    constant TZMM_CPM2                : integer   := 07;                     -- CPM main memory configuration, F000-FFFF are on the tranZPUter board in block 4, 0040-CFFF and E800-EFFF are in block 5, mainboard for D000-DFFF (video), E000-E800 (Memory control) selected.
                                                                             -- Special case for 0000:003F (interrupt vectors) which resides in block 4, F3FE:F3FF & F7FE:F7FF (floppy disk paging vectors) which resides on the mainboard.
    constant TZMM_COMPAT              : integer   := 08;                     -- Compatibility monitor mode, monitor ROM on mainboard, RAM on tranZPUter in Block 0 1000-CFFF.
    constant TZMM_HOSTACCESS          : integer   := 09;                     -- Monitor ROM 0000-0FFF and Main DRAM 0x1000-0xD000, video and memory mapped I/O are on the host machine, User/Floppy ROM E800-FFFF are in tranZPUter memory. 
    constant TZMM_MZ700_0             : integer   := 10;                     -- MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the mainboard.
    constant TZMM_MZ700_1             : integer   := 11;                     -- MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 0, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the tranZPUter in block 6.
    constant TZMM_MZ700_2             : integer   := 12;                     -- MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is on the tranZPUter in block 6.
    constant TZMM_MZ700_3             : integer   := 13;                     -- MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 0, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is inaccessible.
    constant TZMM_MZ700_4             : integer   := 14;                     -- MZ700 Mode - 0000:0FFF is on the tranZPUter board in block 6, 1000:CFFF is on the tranZPUter board in block 0, D000:FFFF is inaccessible.
    constant TZMM_MZ800               : integer   := 15;                     -- MZ800 Mode - Running on MZ800 hardware, configuration set according to MZ700/MZ800 mode.
    constant TZMM_MZ2000              : integer   := 16;                     -- MZ2000 Mode - Running on MZ2000 hardware, configuration set according to runtime configuration registers.
    constant TZMM_FPGA                : integer   := 21;                     -- Open up access for the K64F to the FPGA resources such as memory. All other access to RAM or mainboard is blocked.
    constant TZMM_TZPUM               : integer   := 22;                     -- Everything in on mainboard, no access to tranZPUter memory.
    constant TZMM_TZPU                : integer   := 23;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 0 is selected.
    constant TZMM_TZPU0               : integer   := 24;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 0 is selected.
    constant TZMM_TZPU1               : integer   := 25;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 1 is selected.
    constant TZMM_TZPU2               : integer   := 26;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 2 is selected.
    constant TZMM_TZPU3               : integer   := 27;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 3 is selected.
    constant TZMM_TZPU4               : integer   := 28;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 4 is selected.
    constant TZMM_TZPU5               : integer   := 29;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 5 is selected.
    constant TZMM_TZPU6               : integer   := 30;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 6 is selected.
    constant TZMM_TZPU7               : integer   := 31;                     -- Everything is in tranZPUter domain, no access to underlying Sharp mainboard unless memory management mode is switched. tranZPUter RAM 64K block 7 is selected.



    ------------------------------------------------------------ 
    -- Configurable parameters.
    ------------------------------------------------------------ 
    -- Target hardware.
    constant CPLD_HOST_HW             : integer  := MODE_MZ700;

    -- Target video hardware.
    constant CPLD_HAS_FPGA_VIDEO      : std_logic := '1';

    -- Version of hdl.
    constant CPLD_VERSION             : integer   := 2;

    -- Clock source for the secondary clock. If a K64F is installed then enable it otherwise use the onboard oscillator.
    --
    constant USE_K64F_CTL_CLOCK       : integer   := 1;

    ------------------------------------------------------------ 
    -- Function prototypes
    ------------------------------------------------------------ 
    -- Find the maximum of two integers.
    function IntMax(a : in integer; b : in integer) return integer;

    -- Find the number of bits required to represent an integer.
    function log2ceil(arg : positive) return natural;

    -- Function to calculate the number of whole 'clock' cycles in a given time period, the period being in ns.
    function clockTicks(period : in integer; clock : in integer) return integer;

    -- Function to reverse the order of the bits in a standard logic vector.
    -- ie. 1010 becomes 0101
    function reverse_vector(slv:std_logic_vector) return std_logic_vector; 

    -- Function to convert an integer (0 or 1) into std_logic.
    --
    function to_std_logic(i : in integer) return std_logic;

    -- Function to return the value of a bit as an integer for array indexing etc.
    function bit_to_integer( s : std_logic ) return natural;    

    ------------------------------------------------------------ 
    -- Records
    ------------------------------------------------------------ 

    ------------------------------------------------------------ 
    -- Components
    ------------------------------------------------------------

end tzpuFusionX_pkg;

------------------------------------------------------------ 
-- Function definitions.
------------------------------------------------------------ 
package body tzpuFusionX_pkg is
    
    -- Find the maximum of two integers.
    function IntMax(a : in integer; b : in integer) return integer is
    begin
        if a > b then
            return a;
        else
            return b;
        end if;
        return a;
    end function IntMax;

    -- Find the number of bits required to represent an integer.
    function log2ceil(arg : positive) return natural is
        variable tmp : positive     := 1;
        variable log : natural      := 0;
    begin
        if arg = 1 then
            return 0;
        end if;

        while arg > tmp loop
            tmp := tmp * 2;
            log := log + 1;
        end loop;
        return log;
    end function;

    -- Function to calculate the number of whole 'clock' cycles in a given time period, the period being in ns.
    function clockTicks(period : in integer; clock : in integer) return integer is
        variable ticks         : real;
        variable fracTicks     : real;
    begin
        ticks         := (Real(period) * Real(clock)) / 1000000000.0;
        fracTicks     := ticks - CEIL(ticks);
        if fracTicks > 0.0001 then
            return Integer(CEIL(ticks + 1.0));
        else
            return Integer(CEIL(ticks));
        end if;
    end function;

    function reverse_vector(slv:std_logic_vector) return std_logic_vector is 
       variable target : std_logic_vector(slv'high downto slv'low); 
    begin 
      for idx in slv'high downto slv'low loop 
        target(idx) := slv(slv'low + (slv'high-idx)); 
      end loop; 
      return target; 
    end reverse_vector;

    function to_std_logic(i : in integer) return std_logic is
    begin
      if i = 0 then
        return '0';
      end if;
      return '1';
    end function;

    -- Function to return the value of a bit as an integer for array indexing etc.
    function bit_to_integer( s : std_logic ) return natural is
    begin
        if s = '1' then
            return 1;
        else
            return 0;
        end if;
    end function;
end package body;
