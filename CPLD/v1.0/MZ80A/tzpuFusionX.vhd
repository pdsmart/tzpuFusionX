-------------------------------------------------------------------------------------------------------
--
-- Name:            tzpuFusionX.vhd
-- Version:         MZ-80A
-- Created:         Nov 2022
-- Author(s):       Philip Smart
-- Description:     tzpuFusionX CPLD logic definition file.
--                  This module contains the definition of the tzpuFusionX project plus enhancements
--                  for the MZ-80A.
--
-- Credits:         
-- Copyright:       (c) 2018-23 Philip Smart <philip.smart@net2net.org>
--
-- History:         Nov 2022 v1.0 - Initial write for the MZ-2000, adaption to the MZ-80A.
--                  Feb 2023 v1.1 - Updates, after numerous tests to try and speed up the Z80 transaction
--                                  from SSD202 issuing a command to data being returned. Source now
--                                  different to the MZ-700/MZ-2000 so will need back porting.
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
use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;
use work.tzpuFusionX_pkg.all;

entity cpld512 is
    generic (
        SPI_CLK_POLARITY          : std_logic := '0'
    );
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

        -- SOM Control Signals
        VSOM_SPI_CLK              : in    std_logic;                           -- SOM SPI Channel 0 Clock.
        VSOM_SPI_MOSI             : in    std_logic;                           --                   MOSI Input.
        VSOM_SPI_MISO             : out   std_logic;                           --                   MISO Output.
        VSOM_SPI_CSn              : in    std_logic;                           --                   Enable.

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
end entity;

architecture rtl of cpld512 is

    -- Finite State Machine states.
    type SOMFSMState is
    (
        IdleCycle,
        FetchCycle,
        FetchCycle_11,
        FetchCycle_20,
        FetchCycle_21,
        FetchCycle_30,
        RefreshCycle,
        RefreshCycle_11,
        RefreshCycle_20,
        RefreshCycle_21,
        RefreshCycle_3,
        WriteCycle,
        WriteCycle_11,
        WriteCycle_20,
        WriteCycle_21,
        WriteCycle_30,
        WriteCycle_31,
        ReadCycle,
        ReadCycle_11,
        ReadCycle_20,
        ReadCycle_21,
        ReadCycle_30,
        ReadCycle_31,
        WriteIOCycle,
        WriteIOCycle_11,
        WriteIOCycle_20,
        WriteIOCycle_21,
        WriteIOCycle_30,
        WriteIOCycle_31,
        WriteIOCycle_40,
        WriteIOCycle_41,
        ReadIOCycle,
        ReadIOCycle_11,
        ReadIOCycle_20,
        ReadIOCycle_21,
        ReadIOCycle_30,
        ReadIOCycle_31,
        ReadIOCycle_40,
        ReadIOCycle_41,
        HaltCycle,
        BusReqCycle
    );

    -- CPU Interface internal signals.
    signal Z80_BUSRQni            :       std_logic;
    signal Z80_INTni              :       std_logic;
    signal Z80_IORQni             :       std_logic;
    signal Z80_MREQni             :       std_logic;
    signal Z80_NMIni              :       std_logic;
    signal Z80_RDni               :       std_logic;
    signal Z80_WRni               :       std_logic;
    signal Z80_HALTni             :       std_logic;
    signal Z80_M1ni               :       std_logic;
    signal Z80_RFSHni             :       std_logic;
    signal Z80_DATAi              :       std_logic_vector(7 downto 0);
    signal Z80_BUSRQ_ACKni        :       std_logic;

    -- Internal CPU state control.
    signal CPU_ADDR               :       std_logic_vector(15 downto 0) := (others => '0');
    signal CPU_DATA_IN            :       std_logic_vector(7 downto 0)  := (others => '0');
    signal CPU_DATA_OUT           :       std_logic_vector(7 downto 0)  := (others => '0');
    signal CPU_DATA_EN            :       std_logic;

    -- Clocks.
    signal CLK_25Mi               :       std_logic := '0';

    -- Reset control
    signal PM_RESETi              :       std_logic := '1';
    signal VSOM_RESETni           :       std_logic := '1';

    -- Refresh control.
    signal FSM_STATE              :       SOMFSMState := IdleCycle;
    signal NEW_SPI_CMD            :       std_logic := '0';
    signal VCPU_CS_EDGE           :       std_logic_vector(1 downto 0)  := "11";
    signal AUTOREFRESH_CNT        :       integer range 0 to 7;
    signal FSM_STATUS             :       std_logic := '0';
    signal FSM_CHECK_WAIT         :       std_logic := '0';
    signal FSM_WAIT_ACTIVE        :       std_logic := '0';
    signal RFSH_STATUS            :       std_logic := '0';
    signal REFRESH_ADDR           :       std_logic_vector(7 downto 0);
    signal IPAR                   :       std_logic_vector(7 downto 0);
    signal AUTOREFRESH            :       std_logic;

    -- Clock edge detection and flagging.
    signal Z80_CLKi               :       std_logic;
    signal Z80_CLK_LAST           :       std_logic_vector(1 downto 0);
    signal Z80_CLK_RE             :       std_logic;
    signal Z80_CLK_FE             :       std_logic;
    signal Z80_CLK_TGL            :       std_logic;
    signal CPU_T_STATE_SET        :       integer range 0 to 5;
    signal CPU_LAST_T_STATE       :       std_logic := '0';

    -- SPI Slave interface.
    signal SPI_SHIFT_EN           :       std_logic;
    signal SPI_TX_SREG            :       std_logic_vector(6 downto 0);        -- TX Shift Register
    signal SPI_RX_SREG            :       std_logic_vector(7 downto 0);        -- RX Shift Register
    signal SPI_TX_DATA            :       std_logic_vector(31 downto 0);       -- Data to transmit.
    signal SPI_RX_DATA            :       std_logic_vector(31 downto 0);       -- Data received.
    signal SPI_BIT_CNT            :       integer range 0 to 16;               -- Count of bits tx/rx'd.
    signal SPI_FRAME_CNT          :       integer range 0 to 4;                -- Number of frames received (8bit chunks).

    -- SPI Command interface.
    signal SOM_CMD                :       std_logic_vector(7 downto 0)  := (others => '0');
    signal SPI_NEW_DATA           :       std_logic;
    signal SPI_PROCESSING         :       std_logic;
    signal SPI_CPU_ADDR           :       std_logic_vector(15 downto 0) := (others => '0');
    signal SPI_CPU_DATA           :       std_logic_vector(7 downto 0)  := (others => '0');

    -- Test modes.
    signal SPI_LOOPBACK_TEST      :       std_logic := '0';

    -- Video/Audio control
    signal VIDEO_SRCi             :       std_logic := '0';
    signal MONO_VIDEO_SRCi        :       std_logic := '0';
    signal AUDIO_SRC_Li           :       std_logic := '0';
    signal AUDIO_SRC_Ri           :       std_logic := '0';
    signal VBUS_ENi               :       std_logic := '1';


    function to_std_logic(L: boolean) return std_logic is
    begin
        if L then
            return('1');
        else
            return('0');
        end if;
    end function to_std_logic;
begin


    -- System RESET.
    --
    -- Multiple reset sources, Z80_RESETn, MB_IPLn, MB_RESETn. On the MZ-80A, MB_RESETn is a full reset to the power on state.
    -- Like the MZ-700, we trigger on Z80_RESETn after having sampled the MB_RESETn state to decide on any extra actions needed within
    -- the CPLD and the signals are forwarded onto the SOM so it too can take the correct reset path.
    --
    -- If the external reset switch is pressed, a Z80_RESETn is invoked sending the signal low for approx 30ms. 
    -- On the first edge the VSOM_RESETn signal is set which allows the SOM to see it and the Z80 application to enter a reset state.
    -- On the second edge, if occurring within 1 second of the first, the PM_RESET signal to the SOM is triggered, held low for 1 second,
    -- forcing the SOM to reboot.
    SYSRESET: process( Z80_CLKi, Z80_RESETn )
        variable timer1        : integer range 0 to 200000 := 0;
        variable timer100      : integer range 0 to 10     := 0;
        variable timerPMReset  : integer range 0 to 10     := 0;
        variable resetCount    : integer range 0 to 3      := 0;
        variable cpuResetEdge  : std_logic                 := '1';
    begin
        -- Synchronous on the HOST Clock.
        if(rising_edge(Z80_CLKi)) then

            -- If the PM Reset timer is active, count down and on expiry release the SOM PM_RESET line.
            if(timerPMReset = 0 and PM_RESETi = '1') then
                PM_RESETi        <= '0';
            end if;

            -- If the VSOM_RESETni is active after reset timer expiry, cancel the RESET state.
            if(timerPMReset = 0 and VSOM_RESETni = '0') then
                VSOM_RESETni     <= '1';
            end if;

            -- Each time the reset button is pressed, count the edges.
            if(Z80_RESETn = '0' and cpuResetEdge = '1' and (resetCount = 0 or timer100 > 5)) then
                resetCount       := resetCount + 1;
                VSOM_RESETni     <= '0';
                timerPMReset     := 5;
                timer100         := 0;

                -- If there are 2 or more reset signals in a given period it means a SOM reset is required.
                if(resetCount >= 2) then
                    PM_RESETi    <= '1';
                    timerPMReset := 10;
                    resetCount   := 0;
                end if;

            end if;

            -- 100ms interval.
            if(timer1 = 200000) then
                timer100         := timer100 + 1;

                if(timer100 >= 10) then
                    timer100     := 0;
                    resetCount   := 0;
                end if;

                if(timerPMReset > 0) then
                    timerPMReset := timerPMReset - 1;
                end if;
            end if;

            timer1               := timer1 - 1;
            cpuResetEdge         := Z80_RESETn;
        end if;
    end process;

    -- Create Mono DAC Clock based on primary clock.
    MONOCLK: process( CLK_50M )
    begin
        if(rising_edge(CLK_50M)) then
            CLK_25Mi             <= not CLK_25Mi;
        end if;
    end process;

    -- SPI Slave input. Receive command and data from the SOM.
    SPI_INPUT : process(VSOM_SPI_CLK)
    begin
        -- SPI_CLK_POLARITY='0' => falling edge; SPI_CLK_POLARITY='1' => rising edge
        if(VSOM_SPI_CLK'event and VSOM_SPI_CLK = SPI_CLK_POLARITY) then
            if(VSOM_SPI_CSn = '0') then
                SPI_RX_SREG      <= SPI_RX_SREG(6 downto 0) & VSOM_SPI_MOSI;

                -- End of frame then store the data prior to next bit arrival.
                -- Convert to Little Endian, same as SOM.
                if(SPI_SHIFT_EN = '1' and SPI_FRAME_CNT = 1 and SPI_BIT_CNT = 0) then
                    SPI_RX_DATA(7 downto 0)   <= SPI_RX_SREG(6 downto 0) & VSOM_SPI_MOSI;  

                elsif(SPI_SHIFT_EN = '1' and SPI_FRAME_CNT = 2 and SPI_BIT_CNT = 0) then
                    SPI_RX_DATA(15 downto 8)  <= SPI_RX_SREG(6 downto 0) & VSOM_SPI_MOSI;

                elsif(SPI_SHIFT_EN = '1' and SPI_FRAME_CNT = 3 and SPI_BIT_CNT = 0) then
                    SPI_RX_DATA(23 downto 16) <= SPI_RX_SREG(6 downto 0) & VSOM_SPI_MOSI;

                elsif(SPI_FRAME_CNT = 4 and SPI_BIT_CNT = 0) then
                    SPI_RX_DATA(31 downto 24) <= SPI_RX_SREG(6 downto 0) & VSOM_SPI_MOSI;  
                end if;
            end if;
        end if;
    end process;

    -- SPI Slave output. Return the current data set as selected by the input signals XACT.
    SPI_OUTPUT : process(VSOM_SPI_CLK,VSOM_SPI_CSn,SPI_TX_DATA)
    begin
        -- Chip Select inactive, disable process and reset control flags.
        if(VSOM_SPI_CSn = '1') then
            SPI_SHIFT_EN         <= '0';
            SPI_BIT_CNT          <= 7;

        -- SPI_CLK_POLARITY='0' => falling edge; SPI_CLK_POLARITY='1' => risinge edge
        elsif(VSOM_SPI_CLK'event and VSOM_SPI_CLK = not SPI_CLK_POLARITY) then
            -- Each clock reset the shift enable and done flag in preparation for the next cycle.
            SPI_SHIFT_EN         <= '1';

            -- Bit count decrements to detect when last bit of byte is sent.
            if(SPI_BIT_CNT > 0) then
                SPI_BIT_CNT      <= SPI_BIT_CNT - 1;
            end if;

            -- Shift out the next bit.
            VSOM_SPI_MISO        <= SPI_TX_SREG(6);
            SPI_TX_SREG          <= SPI_TX_SREG(5 downto 0) & '0';

            -- First clock after CS goes active, load up the data to be sent to the SOM.
            if(SPI_SHIFT_EN = '0' or SPI_BIT_CNT = 0) then

                if(SPI_LOOPBACK_TEST = '1') then
                    VSOM_SPI_MISO<= SPI_RX_SREG(7);
                    SPI_TX_SREG  <= SPI_RX_SREG(6 downto 0);

                elsif(SPI_SHIFT_EN = '0') then
                    SPI_FRAME_CNT<= 1;
                    VSOM_SPI_MISO<= SPI_TX_DATA(7);
                    SPI_TX_SREG  <= SPI_TX_DATA(6 downto 0);
                elsif(SPI_FRAME_CNT = 1) then
                    SPI_FRAME_CNT<= 2;
                    VSOM_SPI_MISO<= SPI_TX_DATA(15);
                    SPI_TX_SREG  <= SPI_TX_DATA(14 downto 8);
                elsif(SPI_FRAME_CNT = 2) then
                    SPI_FRAME_CNT<= 3;
                    VSOM_SPI_MISO<= SPI_TX_DATA(23);
                    SPI_TX_SREG  <= SPI_TX_DATA(22 downto 16);
                else
                    -- Increment frame count for each word received. We handle 8bit (1 frame), 16bit (2 frames) or 32bit (4 frames) reception.
                    SPI_FRAME_CNT<= 4;
                    VSOM_SPI_MISO<= SPI_TX_DATA(31);
                    SPI_TX_SREG  <= SPI_TX_DATA(30 downto 24);
                end if;

                SPI_BIT_CNT  <= 7;
            end if;
        end if;
    end process;

    SPI_REGISTER : process(Z80_RESETn, VSOM_SPI_CSn, SPI_FRAME_CNT)
    begin
        if(Z80_RESETn = '0') then
            VIDEO_SRCi           <= '0';
            VGA_BLANKn           <= '1';
            VBUS_ENi             <= '1';
            MONO_VIDEO_SRCi      <= '1';
            AUDIO_SRC_Li         <= '0';
            AUDIO_SRC_Ri         <= '0';
            AUTOREFRESH          <= '0';
            SPI_LOOPBACK_TEST    <= '0';
            SOM_CMD              <= (others => '0');
            SPI_CPU_ADDR         <= (others => '0');
            SPI_NEW_DATA         <= '0';

        -- On rising edge of SPI CSn a new data packet from the SOM has arrived and in the shift register SPI_RX_SREG.
        -- The variable SPI_FRAME_CNT indicates which byte (frame) in a 32bit word has been transmitted. This allows
        -- for 8bit, 16bit and 32bit transmissions.
        -- The packet is formatted as follows:
        --
        -- < SPI_CPU_ADDR                                > < SPI_CPU_DATA      >< SOM_CMD>
        -- < SPI_FRAME_CNT=4     >< SPI_FRAME=3          > < SPI_FRAME_CNT=2   >< SPI_FRAME_CNT=1>
        -- < 16bit Z80 Address                           > < Z80 Data          ><Command=00..80>
        -- 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
        --
        -- <                                             > < Data              ><Command=F0..FF>
        -- 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
        --
        elsif(VSOM_SPI_CSn'event and VSOM_SPI_CSn = '1') then

            -- Command is always located in the upper byte of frame 1.
            SOM_CMD                  <= SPI_RX_DATA(7 downto 0);

            -- Toggle flag to indicate new data arrived.
            SPI_NEW_DATA             <= not SPI_NEW_DATA;

            -- Process the command. Some commands require the FSM, others can be serviced immediately.
            case SPI_RX_DATA(7 downto 0) is

                -- Z80XACT(0..15): Setup data and address as provided then execute FSM.
                when X"10" | X"11" | X"12" | X"13" | X"14" | X"15" | X"16" | X"17" |    -- Fetch
                     X"18" | X"19" | X"1A" | X"1B" | X"1C" | X"1D" | X"1E" | X"1F" |    -- Write
                     X"20" | X"21" | X"22" | X"23" | X"24" | X"25" | X"26" | X"27" |    -- Read
                     X"28" | X"29" | X"2A" | X"2B" | X"2C" | X"2D" | X"2E" | X"2F" |    -- WriteIO
                     X"30" | X"31" | X"32" | X"33" | X"34" | X"35" | X"36" | X"37" |    -- ReadIO
                     X"38" | X"39" | X"3A" | X"3B" | X"3C" | X"3D" | X"3E" | X"3F" |    -- 
                     X"40" | X"41" | X"42" | X"43" | X"44" | X"45" | X"46" | X"47" |    -- 
                     X"48" | X"49" | X"4A" | X"4B" | X"4C" | X"4D" | X"4E" | X"4F" =>

                    -- Direct address set.
                    if(SPI_FRAME_CNT = 4) then
                        SPI_CPU_ADDR <= SPI_RX_DATA(31 downto 16);
                    else
                    --    if(SPI_CPU_ADDR >= X"D010" and SPI_CPU_ADDR < X"D020") then
                    --      SPI_CPU_ADDR <= std_logic_vector(X"D020" + unsigned(SPI_RX_DATA(2 downto 0)));
                    --    else
                          SPI_CPU_ADDR <= std_logic_vector(unsigned(SPI_CPU_ADDR) + unsigned(SPI_RX_DATA(2 downto 0)));
                    --  end if;
                    end if;

                    if(SPI_FRAME_CNT > 1) then
                        SPI_CPU_DATA <= SPI_RX_DATA(15 downto 8);
                    end if;

                -- SETSIGSET1: Set control lines directly.
                when X"F0" =>
                    VIDEO_SRCi       <= SPI_RX_DATA(8);
                    MONO_VIDEO_SRCi  <= SPI_RX_DATA(9);
                    AUDIO_SRC_Li     <= SPI_RX_DATA(10);
                    AUDIO_SRC_Ri     <= SPI_RX_DATA(11);
                    VBUS_ENi         <= SPI_RX_DATA(12);
                    VGA_BLANKn       <= not SPI_RX_DATA(13);

                -- Enable auto refresh DRAM cycle.
                when X"F1" =>
                    AUTOREFRESH      <= '1';

                -- Disable auto refresh DRAM cycle.
                when X"F2" =>
                    AUTOREFRESH      <= '0';

                -- SETLOOPBACK: Enable loopback test mode.
                when X"FE" =>
                    SPI_LOOPBACK_TEST<= '1';

                -- No action, called to retrieve status.
                when X"00"  | X"FF" => 

                when others =>
            end case;
        end if;
    end process;

    -- Process to detect the Z80 Clock edges. Each edge is used to recreate the Z80 external signals.
    --
    Z80CLK: process( CLK_50M, Z80_CLKi, Z80_RESETn )
    begin
        if(Z80_RESETn = '0') then
            Z80_CLK_RE               <= '1';
            Z80_CLK_FE               <= '1';
            Z80_CLK_TGL              <= '1';

        elsif(rising_edge(CLK_50M)) then

            -- Default is to clear the signals, only active for 1 clock period.
            Z80_CLK_RE               <= '0';
            Z80_CLK_FE               <= '0';
            Z80_CLK_TGL              <= '0';

            -- Rising Edge.
            if(Z80_CLKi = '1' and Z80_CLK_LAST = "00") then
                Z80_CLK_RE           <= '1';

            -- Toggle on rising edge is delayed by one clock to allow time for command to be decoded.
            elsif(Z80_CLKi = '1' and Z80_CLK_LAST = "01") then
                Z80_CLK_TGL          <= '1';

            -- Falling Edge.
            elsif(Z80_CLKi = '0' and Z80_CLK_LAST = "11") then
                Z80_CLK_FE           <= '1';
                Z80_CLK_TGL          <= '1';
            end if;
            Z80_CLK_LAST             <= Z80_CLK_LAST(0) & Z80_CLKi;
        end if;
    end process;

    -- SOM Finite State Machine.
    --
    -- A command processor, based on an FSM concept, to process requested commands, ie. Z80 Write, Z80 Read etc.
    -- The external signal SOM_CMD_EN, when set, indicates a new command available in SOM_CMD.
    --
    SOMFSM: process( CLK_50M, Z80_CLKi, Z80_RESETn )
    begin
        if(Z80_RESETn = '0') then
            Z80_IORQni            <= '1';
            Z80_MREQni            <= '1';
            Z80_RDni              <= '1';
            Z80_WRni              <= '1';
            Z80_HALTni            <= '1';
            Z80_M1ni              <= '1';
            Z80_RFSHni            <= '1';
            Z80_BUSRQ_ACKni       <= '1';
            FSM_CHECK_WAIT        <= '0';
            FSM_WAIT_ACTIVE       <= '0';
            FSM_STATUS            <= '0';
            FSM_STATE             <= IdleCycle;
            RFSH_STATUS           <= '0';
            CPU_DATA_EN           <= '0';
            CPU_DATA_IN           <= (others => '0');
            REFRESH_ADDR          <= (others => '0');
            AUTOREFRESH_CNT       <= 7;
            IPAR                  <= (others => '0');
            NEW_SPI_CMD           <= '0';
            VCPU_CS_EDGE          <= "11";
            SPI_PROCESSING        <= '0';

        elsif(rising_edge(CLK_50M)) then

            -- Bus request mechanism. If an externel Bus Request comes in and the FSM is idle, run the Bus Request command which 
            -- suspends processing and  tri-states the bus.
            if(Z80_BUSRQn = '0' and Z80_BUSRQ_ACKni = '1' and FSM_STATE = IdleCycle) then
                FSM_STATE         <= BusReqCycle;
            end if;
            if(Z80_BUSRQn = '1' and Z80_BUSRQ_ACKni = '0' and FSM_STATE = IdleCycle) then
                Z80_BUSRQ_ACKni   <= '1';
            end if;

            -- New command, set flag as the signal is only 1 clock wide.
            if(SPI_LOOPBACK_TEST = '0' and VSOM_SPI_CSn = '1' and VCPU_CS_EDGE = "01") then
                NEW_SPI_CMD       <= '1';
            end if;

            -- Whenever we return to Idle or just prior to Refresh from a Fetch cycle set all control signals to default.
            if((FSM_STATE = IdleCycle or FSM_STATE = RefreshCycle) and Z80_CLK_TGL = '1') then
                CPU_DATA_EN       <= '0';
                Z80_MREQni        <= '1';
                Z80_IORQni        <= '1';
                Z80_RDni          <= '1';
                Z80_WRni          <= '1';
                Z80_M1ni          <= '1';
                FSM_STATUS        <= '0';
                Z80_RFSHni        <= '1';

                -- Auto DRAM refresh cycles. When enabled, every 7 host clock cycles, a 2 cycle refresh period commences.
                -- This will be overriden if the SPI receives a new command.
                -- 
                if AUTOREFRESH = '1' and FSM_STATE = IdleCycle then
                    AUTOREFRESH_CNT <= AUTOREFRESH_CNT - 1;
                    if(AUTOREFRESH_CNT = 0) then
                        FSM_STATE <= RefreshCycle_3;
                        FSM_STATUS<= '1';
                    end if;
                end if;
            end if;

            -- If new command has been given and the FSM enters idle state, load up new command for processing.
            if(NEW_SPI_CMD = '1' and FSM_STATE = IdleCycle and Z80_CLK_RE = '1') then
                NEW_SPI_CMD       <= '0';

                -- Store new address and data for this command.
                CPU_ADDR          <= SPI_CPU_ADDR;
                if(SPI_CPU_DATA /= CPU_DATA_OUT) then
                    CPU_DATA_OUT  <= SPI_CPU_DATA;
                end if;

                -- Process the SOM command. The SPI_REGISTER executes non FSM commands and stores FSM
                -- data prior to this execution block, which fires 1 cycle later on the same control clock.
                -- If the command is not for the FSM then the READY mechanism is held for one
                -- further cycle before going inactive.
                case SOM_CMD is
                    when X"10" | X"11" | X"12" | X"13" | X"14" | X"15" | X"16" | X"17" =>
                        -- Initiate a Fetch Cycle.
                        FSM_STATE      <= FetchCycle;

                    when X"18" | X"19" | X"1A" | X"1B" | X"1C" | X"1D" | X"1E" | X"1F" =>

                        -- Set the Z80 data bus value and initiate a Write Cycle.
                        FSM_STATE      <= WriteCycle;

                    when X"20" | X"21" | X"22" | X"23" | X"24" | X"25" | X"26" | X"27" =>
                        -- Initiate a Read Cycle.
                        FSM_STATE      <= ReadCycle;

                    when X"28" | X"29" | X"2A" | X"2B" | X"2C" | X"2D" | X"2E" | X"2F" =>
                        -- Set the Z80 data bus value and initiate an IO Write Cycle.
                        -- The SOM should set 15:8 to the B register value.
                        FSM_STATE      <= WriteIOCycle;

                    when X"30" | X"31" | X"32" | X"33" | X"34" | X"35" | X"36" | X"37" =>
                        -- Initiate a Read IO Cycle.
                        FSM_STATE      <= ReadIOCycle;

                    when X"50" =>
                        -- Register a Halt state.
                        FSM_STATE      <= HaltCycle;

                    when X"51" =>
                        -- Initiate a refresh cycle.
                        FSM_STATE      <= RefreshCycle_3;

                    when X"E0" =>
                        -- Initiate a Halt Cycle.
                        FSM_STATE      <= HaltCycle;

                    -- Set the Refresh Address register.
                    when X"E1" =>
                        REFRESH_ADDR   <= CPU_DATA_OUT;

                    -- Set the Interrupt Page Address Register.
                    when X"E2" =>
                        IPAR           <= CPU_DATA_OUT;

                    when others =>
                end case;

                -- Toggle the processing flag to negate the new data flag. Used to indicate device is busy.
                if(SPI_NEW_DATA /= SPI_PROCESSING) then
                    SPI_PROCESSING     <= not SPI_PROCESSING;
                end if;

               -- FSM Status bit. When processing a command it is set, cleared when idle. Used by SOM to determine command completion.
               FSM_STATUS         <= '1';
            end if;

            -- Refresh status bit. Indicates a Refresh cycle is under way.
            if FSM_STATE = RefreshCycle or FSM_STATE = RefreshCycle_11 or FSM_STATE = RefreshCycle_20 or FSM_STATE = RefreshCycle_21 or FSM_STATE = RefreshCycle_3 then
                RFSH_STATUS       <= '1';
            else
                RFSH_STATUS       <= '0';
            end if;

            -- If we are in a WAIT sampling 1/2 cycle and wait goes active, set the state so we repeat the full clock cycle by winding back 2 places.
            if(FSM_CHECK_WAIT = '1' and Z80_WAITn = '0' and Z80_CLK_TGL = '0') then
                FSM_WAIT_ACTIVE   <= '1';
            end if;

            -- On each Z80 edge we advance the FSM to recreate the Z80 external signal transactions.
            if(Z80_CLK_TGL = '1') then

                -- The FSM advances to the next stage on each Z80 edge unless in Idle state.
                if(FSM_STATE /= IdleCycle) then
                    FSM_STATE              <= SOMFSMState'val(SOMFSMState'POS(FSM_STATE)+1);
                end if;

                -- Half cycle expired so we dont check the Z80 wait again.
                FSM_CHECK_WAIT             <= '0';
                FSM_WAIT_ACTIVE            <= '0';

                -- FSM to implement all the required Z80 cycles.
                --
                case FSM_STATE is

                    when IdleCycle =>
                        CPU_LAST_T_STATE   <= '1';
                        FSM_STATUS         <= '0';
                    --    FSM_STATE          <= IdleCycle;

                    -----------------------------
                    -- Z80 Fetch Cycle.
                    -----------------------------
                    when FetchCycle =>
                        Z80_M1ni           <= '0';

                    when FetchCycle_11 =>
                        Z80_M1ni           <= '0';
                        Z80_MREQni         <= '0';
                        Z80_RDni           <= '0';

                    when FetchCycle_20 =>
                        FSM_CHECK_WAIT     <= '1';

                    when FetchCycle_21 =>
                        if(Z80_WAITn = '0' or FSM_WAIT_ACTIVE = '1') then
                            FSM_STATE      <=  FetchCycle_20;
                        end if;

                    when FetchCycle_30 =>
                        -- To meet the timing diagrams, just after Rising edge on T3 clear signals. Data wont be available until
                        -- a short period before the falling edge of T3 (could be an MZ-80A design restriction or the Z80 timing diagrams are a bit out).
                        FSM_STATE          <= RefreshCycle;

                    -----------------------------
                    -- Z80 Refresh Cycle.
                    -----------------------------
                    when RefreshCycle =>
                        -- Latch data from mainboard.
                        CPU_DATA_IN        <= Z80_DATA;
                        Z80_RFSHni         <= '0';

                    when RefreshCycle_11 =>
                        -- Falling edge of T3 activates the MREQ line.
                        Z80_MREQni         <= '0';
                        FSM_STATUS         <= '0';

                    when RefreshCycle_20 =>

                    when RefreshCycle_21 =>
                        Z80_MREQni         <= '1';
                        REFRESH_ADDR(6 downto 0) <= REFRESH_ADDR(6 downto 0) + 1;
                        FSM_STATE          <= IdleCycle;

                    when RefreshCycle_3 =>
                        Z80_RFSHni         <= '0';
                        FSM_STATE          <= RefreshCycle_11;

                    -----------------------------
                    -- Z80 Write Cycle.
                    -----------------------------
                    when WriteCycle =>

                    when WriteCycle_11 =>
                        Z80_MREQni         <= '0';
                        CPU_DATA_EN        <= '1';

                    when WriteCycle_20 =>
                        FSM_CHECK_WAIT     <= '1';

                    when WriteCycle_21 =>
                        Z80_WRni           <= '0';
                        if(Z80_WAITn = '0' or FSM_WAIT_ACTIVE = '1') then
                            FSM_STATE      <=  WriteCycle_20;
                        end if;

                    when WriteCycle_30 =>

                    when WriteCycle_31 =>
                        FSM_STATUS         <= '0';
                        Z80_MREQni         <= '1';
                        Z80_WRni           <= '1';
                        FSM_STATE          <= IdleCycle;

                    -----------------------------
                    -- Z80 Read Cycle.
                    -----------------------------
                    when ReadCycle =>

                    when ReadCycle_11 =>
                        Z80_MREQni         <= '0';
                        Z80_RDni           <= '0';

                    when ReadCycle_20 =>
                        FSM_CHECK_WAIT     <= '1';
                       
                    when ReadCycle_21 =>
                        if(Z80_WAITn = '0' or FSM_WAIT_ACTIVE = '1') then
                            FSM_STATE      <=  ReadCycle_20;
                        end if;

                    when ReadCycle_30 =>

                    when ReadCycle_31 =>
                        -- Latch data from mainboard.
                        CPU_DATA_IN        <= Z80_DATA;
                        FSM_STATUS         <= '0';
                        FSM_STATE          <= IdleCycle;


                    -----------------------------
                    -- Z80 IO Write Cycle.
                    -----------------------------
                    when WriteIOCycle =>

                    when WriteIOCycle_11 =>
                        CPU_DATA_EN        <= '1';

                    when WriteIOCycle_20 =>
                        Z80_IORQni         <= '0';
                        Z80_WRni           <= '0';

                    when WriteIOCycle_21 =>

                    when WriteIOCycle_30 =>
                        FSM_CHECK_WAIT     <= '1';

                    when WriteIOCycle_31 =>
                        if(Z80_WAITn = '0' or FSM_WAIT_ACTIVE = '1') then
                            FSM_STATE      <=  WriteIOCycle_20;
                        end if;

                    when WriteIOCycle_40 =>

                    when WriteIOCycle_41 =>
                        FSM_STATUS         <= '0';
                        Z80_IORQni         <= '1';
                        Z80_WRni           <= '1';
                        FSM_STATE          <= IdleCycle;

                    -----------------------------
                    -- Z80 IO Read Cycle.
                    -----------------------------
                    when ReadIOCycle =>

                    when ReadIOCycle_11 =>

                    when ReadIOCycle_20 =>
                        Z80_IORQni         <= '0';
                        Z80_RDni           <= '0';

                    when ReadIOCycle_21 =>
                        
                    when ReadIOCycle_30 =>
                        FSM_CHECK_WAIT     <= '1';

                    when ReadIOCycle_31 =>
                        if(Z80_WAITn = '0' or FSM_WAIT_ACTIVE = '1') then
                            FSM_STATE      <=  ReadIOCycle_20;
                        end if;
                        
                    when ReadIOCycle_40 =>
                        
                    when ReadIOCycle_41 =>
                        -- Latch data from mainboard.
                        CPU_DATA_IN        <= Z80_DATA;
                        FSM_STATUS         <= '0';

                        -- IORQ/RD are deactivated at idle giving 1 clock to latch the data in.
                        FSM_STATE          <= IdleCycle;

                    -----------------------------
                    -- Halt Request.
                    -----------------------------
                    when HaltCycle =>
                        Z80_HALTni         <= '0';
                        FSM_STATUS         <= '0';
                        FSM_STATE          <= IdleCycle;

                    -----------------------------
                    -- Z80 Bus Request.
                    -----------------------------
                    when BusReqCycle =>
                        Z80_BUSRQ_ACKni    <= '0';
                        FSM_STATE          <= IdleCycle;

                    when others =>
                        FSM_STATE          <= IdleCycle;
                end case;
            end if;

            VCPU_CS_EDGE         <= VCPU_CS_EDGE(0) & VSOM_SPI_CSn;
        end if;
    end process;

    Z80_CLKi                     <= not Z80_CLK;

    -- CPU Interface tri-state control based on acknowledged bus request.
    Z80_ADDR                     <= IPAR & REFRESH_ADDR                    when Z80_RFSHni = '0'
                                    else
                                    CPU_ADDR                               when Z80_BUSRQ_ACKni = '1'
                                    else
                                    (others => 'Z');
    Z80_DATA                     <= CPU_DATA_OUT                           when Z80_BUSRQ_ACKni = '1' and CPU_DATA_EN = '1'
                                    else
                                    (others => 'Z');
  --  Z80_DATAi                    <= Z80_DATA                               when Z80_RDn = '0'
   --                                 else (others => '1');
    Z80_RDn                      <= Z80_RDni                               when Z80_BUSRQ_ACKni = '1'
                                    else 'Z';
    Z80_WRn                      <= Z80_WRni                               when Z80_BUSRQ_ACKni = '1'
                                    else 'Z';
    Z80_M1n                      <= Z80_M1ni                               when Z80_BUSRQ_ACKni = '1'
                                    else 'Z';
    Z80_RFSHn                    <= Z80_RFSHni                             when Z80_BUSRQ_ACKni = '1'
                                    else 'Z';
    Z80_MREQn                    <= Z80_MREQni                             when Z80_BUSRQ_ACKni = '1'
                                    else 'Z';
    Z80_IORQn                    <= Z80_IORQni                             when Z80_BUSRQ_ACKni = '1'
                                    else 'Z';
    Z80_BUSAKn                   <= Z80_BUSRQ_ACKni;

    -- CPU Interface single state output.
    Z80_HALTn                    <= Z80_HALTni;

    -- CPU Interface single state input.
    Z80_NMIni                    <= Z80_NMIn;
    Z80_INTni                    <= Z80_INTn;
    Z80_BUSRQni                  <= Z80_BUSRQn;

    -- SOM Reset.
    PM_RESET                     <= PM_RESETi;

    -- SOM to CPLD Interface.
    VSOM_DATA_OUT                <= CPU_DATA_IN                            when VSOM_HBYTE = '1'
                                    else
                                    FSM_STATUS & RFSH_STATUS & Z80_BUSRQ_ACKni & Z80_BUSRQni & Z80_INTni & Z80_NMIni & Z80_WAITn & Z80_RESETn  when VSOM_HBYTE = '0'
                                    else
                                    (others => '0');

                                    -- Loopback test, echo what was received.
    SPI_TX_DATA                  <= SPI_RX_DATA                            when SPI_LOOPBACK_TEST = '1'
                                    else
                                    --CPU_ADDR & SOM_CMD & FSM_STATUS & RFSH_STATUS & std_logic_vector(to_unsigned(SOMFSMState'POS(FSM_STATE), 6));
                                    CPU_ADDR & CPU_DATA_IN & FSM_STATUS & RFSH_STATUS & Z80_BUSRQ_ACKni & Z80_BUSRQni & Z80_INTni & Z80_NMIni & Z80_WAITn & Z80_RESETn;

    -- Signal mirrors.
    VSOM_READY                   <= '0'                                    when FSM_STATUS='1' or SPI_NEW_DATA /= SPI_PROCESSING
                                     else '1';                                                                       -- FSM Ready (1), Busy (0)
    VSOM_LTSTATE                 <= '1'                                    when CPU_LAST_T_STATE = '1'               -- Last T-State in current cycle.
                                    else '0';
    VSOM_BUSRQ                   <= not Z80_BUSRQn;                                                                  -- Host device requesting Z80 Bus.
    VSOM_BUSACK                  <= not Z80_BUSRQ_ACKni;                                                             -- Host device granted Z80 Bus
    VSOM_INT                     <= not Z80_INTn;                                                                    -- Z80 INT signal
    VSOM_NMI                     <= not Z80_NMIn;                                                                    -- Z80 NMI signal
    VSOM_WAIT                    <= not Z80_WAITn;                                                                   -- Z80 WAIT signal
    VSOM_RESET                   <= not VSOM_RESETni;                                                                -- Z80 RESET signal
    VSOM_RSV                     <= (others => '0');                                                                 -- Reserved pins.    

    -- Video/Audio control signals.
    VIDEO_SRC                    <= VIDEO_SRCi;
    MONO_VIDEO_SRC               <= MONO_VIDEO_SRCi;
    AUDIO_SRC_L                  <= AUDIO_SRC_Li;
    AUDIO_SRC_R                  <= AUDIO_SRC_Ri;

    -- USB Power Supply enable.
    VBUS_EN                      <= VBUS_ENi;

    -- Monochrome output is based on the incoming VGA to give the best chrominance levels.
    MONO_R                       <= VGA_R;
    MONO_G                       <= VGA_G;
    MONO_B                       <= VGA_B;

    -- Blanking is active when all colour signals are at 0. The DAC converts values in range 4v .. 5v to adjust chrominance 
    -- but true off can obly be achieved by bringing the signal value to 0v which is achieved by a Mux activated with this blanking signal.
    MONO_BLANKn                  <= '0'                                    when VGA_R = "000" and VGA_G = "000" and VGA_B = "000"
                                    else '1';

    -- Generate composite sync.
    VGA_CSYNCn                   <= VGA_HSYNCn xor not VGA_VSYNCn;
    MONO_CSYNCn                  <= not VGA_HSYNCn xor not VGA_VSYNCn;

    -- DAC clocks.
    --VGA_PXL_CLK           <= CLK_50M;
    MONO_PXL_CLK                 <= VGA_PXL_CLK;

    -- Currently unassigned.
    VGA_COLR                     <= '0';
    MONO_RSV                     <= '0';
                      
end architecture;
