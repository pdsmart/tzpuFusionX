---------------------------------------------------------------------------------------------------------
--
-- Name:            tzpuFusionX_Toplevel.vhd
-- Version:         MZ-80A
-- Created:         Nov 2022
-- Author(s):       Philip Smart
-- Description:     tzpuFusionX CPLD Top Level module.
--                                                     
--                  This module contains the basic pin definition of the CPLD<->logic needed in the
--                  project which targets the MZ-80A host.
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
-- along with this program.  If not, see <http:--www.gnu.org-licenses->.
---------------------------------------------------------------------------------------------------------
library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;
use work.tzpuFusionX_pkg.all;
library altera;
use altera.altera_syn_attributes.all;

entity tzpuFusionX_MZ80A is
    port (
        -- Z80 Address Bus
        Z80_ADDR                  : inout std_logic_vector(15 downto 0);

        -- Z80 Data Bus
        Z80_DATA                  : inout std_logic_vector(7 downto 0);
        
        -- Z80 Control signals.
        Z80_BUSRQn                : in    std_logic;
        Z80_BUSAKn                : out   std_logic;
        Z80_INTn                  : in    std_logic;
        Z80_IORQn                 : inout std_logic;
        Z80_MREQn                 : inout std_logic;
        Z80_NMIn                  : in    std_logic;
        Z80_RDn                   : inout std_logic;
        Z80_WRn                   : inout std_logic;
        Z80_RESETn                : in    std_logic;                           -- Host CPU Reset signal, also CPLD reset.
        Z80_HALTn                 : out   std_logic;
        Z80_WAITn                 : in    std_logic;
        Z80_M1n                   : inout std_logic;
        Z80_RFSHn                 : inout std_logic;

        -- SOM SPI
        VSOM_SPI_CSn              : in    std_logic;                           -- SPI Slave Select
        VSOM_SPI_CLK              : in    std_logic;                           -- SPI Clock
        VSOM_SPI_MOSI             : in    std_logic;                           -- SPI Master Output Slave Input
        VSOM_SPI_MISO             : out   std_logic;                           -- SPI Master Input Slave Output
     
        -- SOM Parallel Bus.
        VSOM_DATA_OUT             : out   std_logic_vector(7 downto 0);        -- Address/Data bus for CPLD control registers.
        VSOM_HBYTE                : in    std_logic;                           -- Parallel Bus High (1)/Low (0) byte.
        VSOM_READY                : out   std_logic;                           -- FSM Ready (1), Busy (0)
        VSOM_LTSTATE              : out   std_logic;                           -- Last T-State in current cycle, 1 = active.
        VSOM_BUSRQ                : out   std_logic;                           -- Host device requesting Z80 Bus.
        VSOM_BUSACK               : out   std_logic;                           -- Host device granted Z80 Bus
        VSOM_INT                  : out   std_logic;                           -- Z80 INT signal
        VSOM_NMI                  : out   std_logic;                           -- Z80 NMI signal
        VSOM_WAIT                 : out   std_logic;                           -- Z80 WAIT signal
        VSOM_RESET                : out   std_logic;                           -- Z80 RESET signal
        VSOM_RSV                  : out   std_logic_vector(1 downto 1);        -- Reserved pins.

        -- SOM Control Signals
        PM_RESET                  : out   std_logic;                           -- Reset SOM

        -- VGA_Palette Control
        VGA_R                     : in    std_logic_vector(9 downto 7);        -- Signals used for detecting blank or no video output.
        VGA_G                     : in    std_logic_vector(9 downto 7);
        VGA_B                     : in    std_logic_vector(9 downto 8);
        
        -- VGA Control Signals
        VGA_PXL_CLK               : in    std_logic;                           -- VGA Pixel clock for DAC conversion.
        VGA_DISPEN                : in    std_logic;                           -- Displayed Enabled (SOM video output).
        VGA_VSYNCn                : in    std_logic;                           -- SOM VSync.
        VGA_HSYNCn                : in    std_logic;                           -- SOM HSync.
        VGA_COLR                  : out   std_logic;                           -- COLR colour carrier frequency.
        VGA_CSYNCn                : out   std_logic;                           -- VGA Composite Sync.
        VGA_BLANKn                : out   std_logic;                           -- VGA Blank detected.
        
        -- CRT Control Signals
        MONO_PXL_CLK              : out   std_logic;                           -- Mono CRT pixel clock for DAC conversion.
        MONO_BLANKn               : out   std_logic;                           -- Mono CRT Blank (no active pixel) detection.
        MONO_CSYNCn               : out   std_logic;                           -- Mono CRT composite sync.
        MONO_RSV                  : out   std_logic;
        
        -- CRT Lower Chrominance Control
        MONO_R                    : out   std_logic_vector(2 downto 0);        -- Signals to fine tune Red level of monochrome chrominance.
        MONO_G                    : out   std_logic_vector(2 downto 0);        -- Signals to fine tune Green level of monochrome chrominance.
        MONO_B                    : out   std_logic_vector(2 downto 1);        -- Signals to fine tune Blue level of monochrome chrominance.
        
        -- MUX Control Signals
        VIDEO_SRC                 : out   std_logic;                           -- Select video source, Mainboard or SOM.
        MONO_VIDEO_SRC            : out   std_logic;                           -- Select crt video source, Mainboard or SOM.
        AUDIO_SRC_L               : out   std_logic;                           -- Select Audio Source Left Channel, Mainboard or SOM.
        AUDIO_SRC_R               : out   std_logic;                           -- Select Audio Source Right Channel, Mainboard or SOM.
        
        -- Mainboard Reset Signals
        MB_RESETn                 : in    std_logic;                           -- Motherboard Reset pressed.
        MB_IPLn                   : in    std_logic;                           -- Motherboard IPL pressed.
        
        -- USB Power Control
        VBUS_EN                   : out   std_logic;                           -- USB Enable Power Output

        -- Clocks.
        Z80_CLK                   : in    std_logic;                           -- Host CPU Clock
        CLK_50M                   : in    std_logic                            -- 50MHz oscillator.
    );
END entity;

architecture rtl of tzpuFusionX_MZ80A is

begin

    cpldl512Toplevel : entity work.cpld512
    generic map (
        SPI_CLK_POLARITY          => '0'
    )    
    port map
    (    
        -- Z80 Address Bus
        Z80_ADDR                  => Z80_ADDR,                                       

        -- Z80 Data Bus
        Z80_DATA                  => Z80_DATA,                                       
        
        -- Z80 Control signals.
        Z80_BUSRQn                => Z80_BUSRQn,                                       
        Z80_BUSAKn                => Z80_BUSAKn,                                       
        Z80_INTn                  => Z80_INTn,                                       
        Z80_IORQn                 => Z80_IORQn,
        Z80_MREQn                 => Z80_MREQn,
        Z80_NMIn                  => Z80_NMIn,
        Z80_RDn                   => Z80_RDn,
        Z80_WRn                   => Z80_WRn,
        Z80_RESETn                => Z80_RESETn,
        Z80_HALTn                 => Z80_HALTn,
        Z80_WAITn                 => Z80_WAITn,
        Z80_M1n                   => Z80_M1n,
        Z80_RFSHn                 => Z80_RFSHn,

        -- SOM SPI
        VSOM_SPI_CSn              => VSOM_SPI_CSn,                             -- SPI Slave Select
        VSOM_SPI_CLK              => VSOM_SPI_CLK,                             -- SPI Clock
        VSOM_SPI_MOSI             => VSOM_SPI_MOSI,                            -- SPI Master Output Slave Input
        VSOM_SPI_MISO             => VSOM_SPI_MISO,                            -- SPI Master Input Slave Output
     
        -- SOM Parallel Bus.
        VSOM_DATA_OUT             => VSOM_DATA_OUT,                            -- Address/Data bus for CPLD control registers.
        VSOM_HBYTE                => VSOM_HBYTE,                               -- Parallel Bus High (1)/Low (0) byte.
        VSOM_READY                => VSOM_READY,                               -- FSM Ready (1), Busy (0)
        VSOM_LTSTATE              => VSOM_LTSTATE,                             -- Last T-State in current cycle.
        VSOM_BUSRQ                => VSOM_BUSRQ,                               -- Host device requesting Z80 Bus.
        VSOM_BUSACK               => VSOM_BUSACK,                              -- Host device granted Z80 Bus
        VSOM_INT                  => VSOM_INT,                                 -- Z80 INT signal
        VSOM_NMI                  => VSOM_NMI,                                 -- Z80 NMI signal
        VSOM_WAIT                 => VSOM_WAIT,                                -- Z80 WAIT signal
        VSOM_RESET                => VSOM_RESET,                               -- Z80 RESET signal
        VSOM_RSV                  => VSOM_RSV,                                 -- Reserved pins.

        -- SOM Control Signals
        PM_RESET                  => PM_RESET,                                 -- Reset SOM

        -- VGA_Palette Control
        VGA_R                     => VGA_R,                                    -- Signals used for detecting blank or no video output.
        VGA_G                     => VGA_G,                                       
        VGA_B                     => VGA_B,                                    
        
        -- VGA Control Signals
        VGA_PXL_CLK               => VGA_PXL_CLK,                              -- VGA Pixel clock for DAC conversion.
        VGA_DISPEN                => VGA_DISPEN,                               -- Displayed Enabled (SOM video output).
        VGA_VSYNCn                => VGA_VSYNCn,                               -- SOM VSync.
        VGA_HSYNCn                => VGA_HSYNCn,                               -- SOM HSync.
        VGA_COLR                  => VGA_COLR,                                 -- COLR colour carrier frequency.
        VGA_CSYNCn                => VGA_CSYNCn,                               -- VGA Composite Sync.
        VGA_BLANKn                => VGA_BLANKn,                               -- VGA Blank detected.
        
        -- CRT Control Signals
        MONO_PXL_CLK              => MONO_PXL_CLK,                             -- Mono CRT pixel clock for DAC conversion.
        MONO_BLANKn               => MONO_BLANKn,                              -- Mono CRT Blank (no active pixel) detection.
        MONO_CSYNCn               => MONO_CSYNCn,                              -- Mono CRT composite sync.
        MONO_RSV                  => MONO_RSV,
        
        -- CRT Lower Chrominance Control
        MONO_R                    => MONO_R,                                   -- Signals to fine tune Red level of monochrome chrominance.
        MONO_G                    => MONO_G,                                   -- Signals to fine tune Green level of monochrome chrominance.
        MONO_B                    => MONO_B,                                   -- Signals to fine tune Blue level of monochrome chrominance.
        
        -- MUX Control Signals
        VIDEO_SRC                 => VIDEO_SRC,                                -- Select video source, Mainboard or SOM.
        MONO_VIDEO_SRC            => MONO_VIDEO_SRC,                           -- Select crt video source, Mainboard or SOM.
        AUDIO_SRC_L               => AUDIO_SRC_L,                              -- Select Audio Source Left Channel, Mainboard or SOM.
        AUDIO_SRC_R               => AUDIO_SRC_R,                              -- Select Audio Source Right Channel, Mainboard or SOM.
        
        -- Mainboard Reset Signals=> MONO_R,
        MB_RESETn                 => MB_RESETn,                                -- Motherboard Reset pressed.
        MB_IPLn                   => MB_IPLn,                                  -- Motherboard IPL pressed.
        
        -- USB Power Control
        VBUS_EN                   => VBUS_EN,                                  -- USB Enable Power Output

        -- Clocks.
        Z80_CLK                   => Z80_CLK,                                  -- Host CPU Clock
        CLK_50M                   => CLK_50M                                   -- 50MHz oscillator.
    );

end architecture;
