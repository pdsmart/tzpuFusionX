/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            z80io.h
// Created:         Oct 2022
// Author(s):       Philip Smart
// Description:     Z80 IO Interface
//                  This file contains the declarations used in interfacing the SOM to the Z80 socket 
//                  and host hardware via a CPLD.
// Credits:         
// Copyright:       (c) 2019-2023 Philip Smart <philip.smart@net2net.org>
//
// History:         Oct 2022 v1.0 - Initial write of the z80 kernel driver software.
//                  Jan 2023 v1.1 - Numerous new tries at increasing throughput to the CPLD failed.
//                                  Maximum read throughput of an 8bit byte due to the SSD202 GPIO
//                                  structure is approx 2MB/s - or 512K/s for a needed 32bit word.
//                                  Write is slower as you have to clock the data so sticking with SPI.
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
#ifndef Z80IO_H
#define Z80IO_H

#ifdef __cplusplus
    extern "C" {
#endif

// Definitions to control compilation.
#define INCLUDE_TEST_METHODS                  1

// CPLD Commands.
#define CPLD_CMD_FETCH_ADDR                   0x10
#define CPLD_CMD_FETCH_ADDR_P1                0x11
#define CPLD_CMD_FETCH_ADDR_P2                0x12
#define CPLD_CMD_FETCH_ADDR_P3                0x13
#define CPLD_CMD_FETCH_ADDR_P4                0x14
#define CPLD_CMD_FETCH_ADDR_P5                0x15
#define CPLD_CMD_FETCH_ADDR_P6                0x16
#define CPLD_CMD_FETCH_ADDR_P7                0x17
#define CPLD_CMD_WRITE_ADDR                   0x18
#define CPLD_CMD_WRITE_ADDR_P1                0x19
#define CPLD_CMD_WRITE_ADDR_P2                0x1A
#define CPLD_CMD_WRITE_ADDR_P3                0x1B
#define CPLD_CMD_WRITE_ADDR_P4                0x1C
#define CPLD_CMD_WRITE_ADDR_P5                0x1D
#define CPLD_CMD_WRITE_ADDR_P6                0x1E
#define CPLD_CMD_WRITE_ADDR_P7                0x1F
#define CPLD_CMD_READ_ADDR                    0x20
#define CPLD_CMD_READ_ADDR_P1                 0x21
#define CPLD_CMD_READ_ADDR_P2                 0x22
#define CPLD_CMD_READ_ADDR_P3                 0x23
#define CPLD_CMD_READ_ADDR_P4                 0x24
#define CPLD_CMD_READ_ADDR_P5                 0x25
#define CPLD_CMD_READ_ADDR_P6                 0x26
#define CPLD_CMD_READ_ADDR_P7                 0x27
#define CPLD_CMD_WRITEIO_ADDR                 0x28
#define CPLD_CMD_WRITEIO_ADDR_P1              0x29
#define CPLD_CMD_WRITEIO_ADDR_P2              0x2A
#define CPLD_CMD_WRITEIO_ADDR_P3              0x2B
#define CPLD_CMD_WRITEIO_ADDR_P4              0x2C
#define CPLD_CMD_WRITEIO_ADDR_P5              0x2D
#define CPLD_CMD_WRITEIO_ADDR_P6              0x2E
#define CPLD_CMD_WRITEIO_ADDR_P7              0x2F
#define CPLD_CMD_READIO_ADDR                  0x30
#define CPLD_CMD_READIO_ADDR_P1               0x31
#define CPLD_CMD_READIO_ADDR_P2               0x32
#define CPLD_CMD_READIO_ADDR_P3               0x33
#define CPLD_CMD_READIO_ADDR_P4               0x34
#define CPLD_CMD_READIO_ADDR_P5               0x35
#define CPLD_CMD_READIO_ADDR_P6               0x36
#define CPLD_CMD_READIO_ADDR_P7               0x37
#define CPLD_CMD_HALT                         0x50
#define CPLD_CMD_REFRESH                      0x51
#define CPLD_CMD_SET_SIGROUP1                 0xF0
#define CPLD_CMD_SET_AUTO_REFRESH             0xF1
#define CPLD_CMD_CLEAR_AUTO_REFRESH           0xF2
#define CPLD_CMD_SET_SPI_LOOPBACK             0xFE
#define CPLD_CMD_NOP1                         0x00
#define CPLD_CMD_NOP2                         0xFF


// Pad numbers for using the MHal GPIO library.
#define PAD_Z80IO_IN_DATA_0                  PAD_GPIO0
#define PAD_Z80IO_IN_DATA_1                  PAD_GPIO1
#define PAD_Z80IO_IN_DATA_2                  PAD_GPIO2
#define PAD_Z80IO_IN_DATA_3                  PAD_GPIO3
#define PAD_Z80IO_IN_DATA_4                  PAD_GPIO4
#define PAD_Z80IO_IN_DATA_5                  PAD_GPIO5
#define PAD_Z80IO_IN_DATA_6                  PAD_GPIO6
#define PAD_Z80IO_IN_DATA_7                  PAD_GPIO7
#define PAD_SPIO_0                           PAD_GPIO8
#define PAD_SPIO_1                           PAD_GPIO9
#define PAD_SPIO_2                           PAD_GPIO10
#define PAD_SPIO_3                           PAD_GPIO11
#define PAD_Z80IO_HIGH_BYTE                  PAD_SAR_GPIO2 // Byte requiured, 0 = Low Byte, 1 = High Byte.
#define PAD_Z80IO_READY                      PAD_GPIO12
#define PAD_Z80IO_LTSTATE                    PAD_UART0_RX  // GPIO47
#define PAD_Z80IO_BUSRQ                      PAD_GPIO13
#define PAD_Z80IO_BUSACK                     PAD_GPIO14
#define PAD_Z80IO_INT                        PAD_PM_IRIN   // IRIN
#define PAD_Z80IO_NMI                        PAD_UART0_TX  // GPIO48
#define PAD_Z80IO_WAIT                       PAD_HSYNC_OUT // GPIO85
#define PAD_Z80IO_RESET                      PAD_VSYNC_OUT // GPIO86
#define PAD_Z80IO_RSV1                       PAD_SATA_GPIO // GPIO90

// Physical register addresses.
#define PAD_Z80IO_IN_DATA_0_ADDR             0x103C00
#define PAD_Z80IO_IN_DATA_1_ADDR             0x103C02
#define PAD_Z80IO_IN_DATA_2_ADDR             0x103C04
#define PAD_Z80IO_IN_DATA_3_ADDR             0x103C06
#define PAD_Z80IO_IN_DATA_4_ADDR             0x103C08
#define PAD_Z80IO_IN_DATA_5_ADDR             0x103C0A
#define PAD_Z80IO_IN_DATA_6_ADDR             0x103C0C
#define PAD_Z80IO_IN_DATA_7_ADDR             0x103C0E
#define PAD_SPIO_0_ADDR                      0x103C10
#define PAD_SPIO_1_ADDR                      0x103C12
#define PAD_SPIO_2_ADDR                      0x103C14
#define PAD_SPIO_3_ADDR                      0x103C16
#define PAD_Z80IO_HIGH_BYTE_ADDR             0x1425 
#define PAD_Z80IO_READY_ADDR                 0x103C18
#define PAD_Z80IO_LTSTATE_ADDR               0x103C30      // GPIO47
#define PAD_Z80IO_BUSRQ_ADDR                 0x103C1A
#define PAD_Z80IO_BUSACK_ADDR                0x103C1C
#define PAD_Z80IO_INT_ADDR                   0xF28         // IRIN
#define PAD_Z80IO_NMI_ADDR                   0x103C32      // GPIO48
#define PAD_Z80IO_WAIT_ADDR                  0x103C80      // GPIO85
#define PAD_Z80IO_RESET_ADDR                 0x103C82      // GPIO86
#define PAD_Z80IO_RSV1_ADDR                  0x103C8A      // GPIO90

#ifdef NOTNEEDED
#define PAD_Z80IO_OUT_DATA_0                 PAD_GPIO12
#define PAD_Z80IO_OUT_DATA_1                 PAD_GPIO13
#define PAD_Z80IO_OUT_DATA_2                 PAD_GPIO14
#define PAD_Z80IO_OUT_DATA_3                 PAD_UART0_RX  // GPIO47
#define PAD_Z80IO_OUT_DATA_4                 PAD_UART0_TX  // GPIO48
#define PAD_Z80IO_OUT_DATA_5                 PAD_HSYNC_OUT // GPIO85
#define PAD_Z80IO_OUT_DATA_6                 PAD_VSYNC_OUT // GPIO86
#define PAD_Z80IO_OUT_DATA_7                 PAD_SATA_GPIO // GPIO90
#define PAD_Z80IO_WRITE                      PAD_PM_IRIN   // Write data clock.
#endif

//-------------------------------------------------------------------------------------------------
// The definitions below come from SigmaStar kernel drivers. No header file exists hence the
// duplication.
//-------------------------------------------------------------------------------------------------

#define SUPPORT_SPI_1                        0
#define MAX_SUPPORT_BITS                     16

#define BANK_TO_ADDR32(b)                    (b<<9)
#define BANK_SIZE                            0x200

#define MS_BASE_REG_RIU_PA                   0x1F000000
#define gChipBaseAddr                        0xFD203C00
#define gPmSleepBaseAddr                     0xFD001C00
#define gSarBaseAddr                         0xFD002800
#define gRIUBaseAddr                         0xFD000000
#define gMOVDMAAddr                          0xFD201600
#define gClkBaseAddr                         0xFD207000
#define gMspBaseAddr                         0xfd222000

#define MHal_CHIPTOP_REG(addr)               (*(volatile U8*)((gChipBaseAddr) + (((addr) & ~1)<<1) + (addr & 1)))
#define MHal_PM_SLEEP_REG(addr)              (*(volatile U8*)((gPmSleepBaseAddr) + (((addr) & ~1)<<1) + (addr & 1)))
#define MHal_SAR_GPIO_REG(addr)              (*(volatile U8*)((gSarBaseAddr) + (((addr) & ~1)<<1) + (addr & 1)))
#define MHal_RIU_REG(addr)                   (*(volatile U8*)((gRIUBaseAddr) + (((addr) & ~1)<<1) + (addr & 1)))


#define MSPI0_BANK_ADDR                      0x1110
#define MSPI1_BANK_ADDR                      0x1111
#define CLK__BANK_ADDR                       0x1038
#define CHIPTOP_BANK_ADDR                    0x101E
#define MOVDMA_BANK_ADDR                     0x100B

#define BASE_REG_MSPI0_ADDR                  MSPI0_BANK_ADDR*0x200 //GET_BASE_ADDR_BY_BANK(IO_ADDRESS(MS_BASE_REG_RIU_PA), 0x111000)
#define BASE_REG_MSPI1_ADDR                  MSPI1_BANK_ADDR*0x200 //GET_BASE_ADDR_BY_BANK(IO_ADDRESS(MS_BASE_REG_RIU_PA), 0x111100)
#define BASE_REG_CLK_ADDR                    CLK__BANK_ADDR*0x200 //GET_BASE_ADDR_BY_BANK(IO_ADDRESS(MS_BASE_REG_RIU_PA), 0x103800)
#define BASE_REG_CHIPTOP_ADDR                CHIPTOP_BANK_ADDR*0x200 //GET_BASE_ADDR_BY_BANK(IO_ADDRESS(MS_BASE_REG_RIU_PA), 0x101E00)

//-------------------------------------------------------------------------------------------------
//  Hardware Register Capability
//-------------------------------------------------------------------------------------------------
#define MSPI_WRITE_BUF_OFFSET                0x40
#define MSPI_READ_BUF_OFFSET                 0x44
#define MSPI_WBF_SIZE_OFFSET                 0x48
#define MSPI_RBF_SIZE_OFFSET                 0x48
            // read/ write buffer size
#define     MSPI_RWSIZE_MASK                 0xFF
#define     MSPI_RSIZE_BIT_OFFSET            0x8
#define     MAX_READ_BUF_SIZE                0x8
#define     MAX_WRITE_BUF_SIZE               0x8
// CLK config
#define MSPI_CTRL_OFFSET                     0x49
#define MSPI_CLK_CLOCK_OFFSET                0x49
#define     MSPI_CLK_CLOCK_BIT_OFFSET        0x08
#define     MSPI_CLK_CLOCK_MASK              0xFF
#define     MSPI_CLK_PHASE_MASK              0x40
#define     MSPI_CLK_PHASE_BIT_OFFSET        0x06
#define     MSPI_CLK_POLARITY_MASK           0x80
#define     MSPI_CLK_POLARITY_BIT_OFFSET     0x07
#define     MSPI_CLK_PHASE_MAX               0x1
#define     MSPI_CLK_POLARITY_MAX            0x1
#define     MSPI_CLK_CLOCK_MAX               0x7
#define     MSPI_CTRL_CPOL_LOW               0x00
#define     MSPI_CTRL_CPOL_HIGH              0x80
#define     MSPI_CTRL_CPHA_LOW               0x00
#define     MSPI_CTRL_CPHA_HIGH              0x40
#define     MSPI_CTRL_3WIRE                  0x10
#define     MSPI_CTRL_INTEN                  0x04
#define     MSPI_CTRL_RESET                  0x02
#define     MSPI_CTRL_ENABLE_SPI             0x01
// DC config
#define MSPI_DC_MASK                         0xFF
#define MSPI_DC_BIT_OFFSET                   0x08
#define MSPI_DC_TR_START_OFFSET              0x4A
#define     MSPI_DC_TRSTART_MAX              0xFF
#define MSPI_DC_TR_END_OFFSET                0x4A
#define     MSPI_DC_TREND_MAX                0xFF
#define MSPI_DC_TB_OFFSET                    0x4B
#define     MSPI_DC_TB_MAX                   0xFF
#define MSPI_DC_TRW_OFFSET                   0x4B
#define     MSPI_DC_TRW_MAX                  0xFF
// Frame Config
#define MSPI_FRAME_WBIT_OFFSET               0x4C
#define MSPI_FRAME_RBIT_OFFSET               0x4E
#define     MSPI_FRAME_BIT_MAX               0x07
#define     MSPI_FRAME_BIT_MASK              0x07
#define     MSPI_FRAME_BIT_FIELD             0x03
#define MSPI_LSB_FIRST_OFFSET                0x50
#define MSPI_TRIGGER_OFFSET                  0x5A
#define MSPI_DONE_OFFSET                     0x5B
#define MSPI_DONE_CLEAR_OFFSET               0x5C
#define MSPI_CHIP_SELECT_OFFSET              0x5F
#define     MSPI_CS1_DISABLE                 0x01
#define     MSPI_CS1_ENABLE                  0x00
#define     MSPI_CS2_DISABLE                 0x02
#define     MSPI_CS2_ENABLE                  0x00
#define     MSPI_CS3_DISABLE                 0x04
#define     MSPI_CS3_ENABLE                  0x00
#define     MSPI_CS4_DISABLE                 0x08
#define     MSPI_CS4_ENABLE                  0x00
#define     MSPI_CS5_DISABLE                 0x10
#define     MSPI_CS5_ENABLE                  0x00
#define     MSPI_CS6_DISABLE                 0x20
#define     MSPI_CS6_ENABLE                  0x00
#define     MSPI_CS7_DISABLE                 0x40
#define     MSPI_CS7_ENABLE                  0x00
#define     MSPI_CS8_DISABLE                 0x80
#define     MSPI_CS8_ENABLE                  0x00

#define MSPI_FULL_DEPLUX_RD_CNT              (0x77)
#define MSPI_FULL_DEPLUX_RD00                (0x78)
#define MSPI_FULL_DEPLUX_RD01                (0x78)
#define MSPI_FULL_DEPLUX_RD02                (0x79)
#define MSPI_FULL_DEPLUX_RD03                (0x79)
#define MSPI_FULL_DEPLUX_RD04                (0x7a)
#define MSPI_FULL_DEPLUX_RD05                (0x7a)
#define MSPI_FULL_DEPLUX_RD06                (0x7b)
#define MSPI_FULL_DEPLUX_RD07                (0x7b)

#define MSPI_FULL_DEPLUX_RD08                (0x7c)
#define MSPI_FULL_DEPLUX_RD09                (0x7c)
#define MSPI_FULL_DEPLUX_RD10                (0x7d)
#define MSPI_FULL_DEPLUX_RD11                (0x7d)
#define MSPI_FULL_DEPLUX_RD12                (0x7e)
#define MSPI_FULL_DEPLUX_RD13                (0x7e)
#define MSPI_FULL_DEPLUX_RD14                (0x7f)
#define MSPI_FULL_DEPLUX_RD15                (0x7f)

//chip select bit map
#define     MSPI_CHIP_SELECT_MAX             0x07

// control bit
#define MSPI_DONE_FLAG                       0x01
#define MSPI_TRIGGER                         0x01
#define MSPI_CLEAR_DONE                      0x01
#define MSPI_INT_ENABLE                      0x04
#define MSPI_RESET                           0x02
#define MSPI_ENABLE                          0x01

// clk_mspi0
#define MSPI0_CLK_CFG                        0x33  //bit 2 ~bit 3
#define      MSPI0_CLK_108M                  0x00
#define      MSPI0_CLK_54M                   0x04
#define      MSPI0_CLK_12M                   0x08
#define      MSPI0_CLK_MASK                  0x0F

// clk_mspi1
#define MSPI1_CLK_CFG                        0x33 //bit 10 ~bit 11
#define      MSPI1_CLK_108M                  0x0000
#define      MSPI1_CLK_54M                   0x0400
#define      MSPI1_CLK_12M                   0x0800
#define      MSPI1_CLK_MASK                  0x0F00

// clk_mspi
#define MSPI_CLK_CFG                         0x33
#define     MSPI_SELECT_0                    0x0000
#define     MSPI_SELECT_1                    0x4000
#define     MSPI_CLK_MASK                    0xF000

// Clock settings
#define     MSPI_CPU_CLOCK_1_2               0x0000
#define     MSPI_CPU_CLOCK_1_4               0x0100
#define     MSPI_CPU_CLOCK_1_8               0x0200
#define     MSPI_CPU_CLOCK_1_16              0x0300
#define     MSPI_CPU_CLOCK_1_32              0x0400
#define     MSPI_CPU_CLOCK_1_64              0x0500
#define     MSPI_CPU_CLOCK_1_128             0x0600
#define     MSPI_CPU_CLOCK_1_256             0x0700
    
//CHITOP 101E mspi mode select
#define MSPI0_MODE                           0x0C //bit0~bit1
#define     MSPI0_MODE_MASK                  0x07
#define MSPI1_MODE                           0x0C //bit4~bit5
#define     MSPI1_MODE_MASK                  0x70
#define EJTAG_MODE                           0xF
#define     EJTAG_MODE_1                     0x01
#define     EJTAG_MODE_2                     0x02
#define     EJTAG_MODE_3                     0x03
#define     EJTAG_MODE_MASK                  0x03

//MOVDMA 100B
#define MOV_DMA_SRC_ADDR_L                   0x03
#define MOV_DMA_SRC_ADDR_H                   0x04
#define MOV_DMA_DST_ADDR_L                   0x05
#define MOV_DMA_DST_ADDR_H                   0x06
#define MOV_DMA_BYTE_CNT_L                   0x07
#define MOV_DMA_BYTE_CNT_H                   0x08
#define DMA_MOVE0_IRQ_CLR                    0x28
#define MOV_DMA_IRQ_FINAL_STATUS             0x2A
#define DMA_MOVE0_ENABLE                     0x00
#define DMA_RW                               0x50 //0 for dma write to device, 1 for dma read from device
#define     DMA_READ                         0x01
#define     DMA_WRITE                        0x00
#define DMA_DEVICE_MODE                      0x51
#define DMA_DEVICE_SEL                       0x52

//spi dma
#define MSPI_DMA_DATA_LENGTH_L               0x30
#define MSPI_DMA_DATA_LENGTH_H               0x31
#define MSPI_DMA_ENABLE                      0x32
#define MSPI_DMA_RW_MODE                     0x33
#define     MSPI_DMA_WRITE                   0x00
#define     MSPI_DMA_READ                    0x01

#define MSTAR_SPI_TIMEOUT_MS                 30000
#define MSTAR_SPI_MODE_BITS                  (SPI_CPOL | SPI_CPHA /*| SPI_CS_HIGH | SPI_NO_CS | SPI_LSB_FIRST*/)


//-------------------------------------------------------------------------------------------------
//  Macros
//-------------------------------------------------------------------------------------------------


#define MHal_CHIPTOP_REG(addr)               (*(volatile U8*)((gChipBaseAddr) + (((addr) & ~1)<<1) + (addr & 1)))
#define MHal_PM_SLEEP_REG(addr)              (*(volatile U8*)((gPmSleepBaseAddr) + (((addr) & ~1)<<1) + (addr & 1)))
#define MHal_SAR_GPIO_REG(addr)              (*(volatile U8*)((gSarBaseAddr) + (((addr) & ~1)<<1) + (addr & 1)))
#define MHal_RIU_REG(addr)                   (*(volatile U8*)((gRIUBaseAddr) + (((addr) & ~1)<<1) + (addr & 1)))
#define READ_BYTE(_reg)                      (*(volatile u8*)(_reg))
#define READ_WORD(_reg)                      (*(volatile u16*)(_reg))
#define READ_LONG(_reg)                      (*(volatile u32*)(_reg))
#define WRITE_BYTE(_reg, _val)               {(*((volatile u8*)(_reg))) = (u8)(_val); }
#define WRITE_WORD(_reg, _val)               {(*((volatile u16*)(_reg))) = (u16)(_val); }
#define WRITE_LONG(_reg, _val)               {(*((volatile u32*)(_reg))) = (u32)(_val); }
#define WRITE_WORD_MASK(_reg, _val, _mask)   {(*((volatile u16*)(_reg))) = ((*((volatile u16*)(_reg))) & ~(_mask)) | ((u16)(_val) & (_mask)); }
#define READ_CPLD_DATA_IN()                  ((MHal_RIU_REG(PAD_Z80IO_IN_DATA_7_ADDR) & 0x1) << 7 | (MHal_RIU_REG(PAD_Z80IO_IN_DATA_6_ADDR) & 0x1) << 6 | (MHal_RIU_REG(PAD_Z80IO_IN_DATA_5_ADDR) & 0x1) << 5 | (MHal_RIU_REG(PAD_Z80IO_IN_DATA_4_ADDR) & 0x1) << 4 |\
                                              (MHal_RIU_REG(PAD_Z80IO_IN_DATA_3_ADDR) & 0x1) << 3 | (MHal_RIU_REG(PAD_Z80IO_IN_DATA_2_ADDR) & 0x1) << 2 | (MHal_RIU_REG(PAD_Z80IO_IN_DATA_1_ADDR) & 0x1) << 1 | (MHal_RIU_REG(PAD_Z80IO_IN_DATA_0_ADDR) & 0x1))
#define SET_CPLD_READ_DATA()                 {MHal_RIU_REG(PAD_Z80IO_HIGH_BYTE_ADDR) |= 0x4;}
#define SET_CPLD_READ_STATUS()               {MHal_RIU_REG(PAD_Z80IO_HIGH_BYTE_ADDR) &= ~0x4;}
#define SET_CPLD_HIGH_BYTE()                 {MHal_RIU_REG(PAD_Z80IO_HIGH_BYTE_ADDR) |= 0x4;}
#define CLEAR_CPLD_HIGH_BYTE()               {MHal_RIU_REG(PAD_Z80IO_HIGH_BYTE_ADDR) &= ~0x4;}
#define CPLD_READY()                         (MHal_RIU_REG(PAD_Z80IO_READY_ADDR) & 0x1)
#define CPLD_RESET()                         (MHal_RIU_REG(PAD_Z80IO_RESET_ADDR) & 0x1)
#define CPLD_LAST_TSTATE()                   (MHal_RIU_REG(PAD_Z80IO_LTSTATE_ADDR) & 0x4)
#define CPLD_Z80_INT()                       (MHal_RIU_REG(PAD_Z80IO_INT_ADDR) & 0x4)
#define CPLD_Z80_NMI()                       (MHal_RIU_REG(PAD_Z80IO_NMI_ADDR) & 0x4)
#define SPI_SEND8(_d_)                       { uint32_t timeout = MAX_CHECK_CNT; \
                                               MSPI_WRITE(MSPI_WRITE_BUF_OFFSET, (uint16_t)(_d_)); \
                                               MSPI_WRITE(MSPI_WBF_SIZE_OFFSET, 1); \
                                               while((MHal_RIU_REG(PAD_Z80IO_READY_ADDR) & 0x1) == 0);\
                                               MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_ENABLE); \
                                               MSPI_WRITE(MSPI_TRIGGER_OFFSET, MSPI_TRIGGER); \
                                               while((MSPI_READ(MSPI_DONE_OFFSET) & MSPI_DONE_FLAG) == 0) { if(--timeout == 0) break; } \
                                               MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_DISABLE); \
                                               MSPI_WRITE(MSPI_DONE_CLEAR_OFFSET, MSPI_CLEAR_DONE);\
                                             }
#define SPI_SEND16(_d_)                      { uint32_t timeout = MAX_CHECK_CNT; \
                                               MSPI_WRITE(MSPI_WRITE_BUF_OFFSET, (uint16_t)(_d_)); \
                                               MSPI_WRITE(MSPI_WBF_SIZE_OFFSET, 2); \
                                               while((MHal_RIU_REG(PAD_Z80IO_READY_ADDR) & 0x1) == 0);\
                                               MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_ENABLE); \
                                               MSPI_WRITE(MSPI_TRIGGER_OFFSET, MSPI_TRIGGER); \
                                               while((MSPI_READ(MSPI_DONE_OFFSET) & MSPI_DONE_FLAG) == 0) { if(--timeout == 0) break; } \
                                               MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_DISABLE); \
                                               MSPI_WRITE(MSPI_DONE_CLEAR_OFFSET, MSPI_CLEAR_DONE); \
                                             }
#define SPI_SEND32(_d_)                      { uint32_t timeout = MAX_CHECK_CNT; \
                                               MSPI_WRITE(MSPI_WRITE_BUF_OFFSET,   (uint16_t)(_d_)); \
                                               MSPI_WRITE(MSPI_WRITE_BUF_OFFSET+1, (uint16_t)((_d_) >> 16)); \
                                               MSPI_WRITE(MSPI_WBF_SIZE_OFFSET, 4); \
                                               while((MHal_RIU_REG(PAD_Z80IO_READY_ADDR) & 0x1) == 0);\
                                               MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_ENABLE); \
                                               MSPI_WRITE(MSPI_TRIGGER_OFFSET, MSPI_TRIGGER); \
                                               while((MSPI_READ(MSPI_DONE_OFFSET) & MSPI_DONE_FLAG) == 0) { if(--timeout == 0) break; } \
                                               MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_DISABLE); \
                                               MSPI_WRITE(MSPI_DONE_CLEAR_OFFSET, MSPI_CLEAR_DONE); \
                                             }
#define SPI_SEND32i(_d_)                      { uint32_t timeout = MAX_CHECK_CNT; \
                                               MSPI_WRITE(MSPI_WRITE_BUF_OFFSET,   (uint16_t)(_d_)); \
                                               MSPI_WRITE(MSPI_WRITE_BUF_OFFSET+1, (uint16_t)((_d_) >> 16)); \
                                               MSPI_WRITE(MSPI_WBF_SIZE_OFFSET, 4); \
    pr_info("Stage 0");\
                                               while((MHal_RIU_REG(PAD_Z80IO_READY_ADDR) & 0x1) == 0);\
    pr_info("Stage 1");\
                                               MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_ENABLE); \
                                               MSPI_WRITE(MSPI_TRIGGER_OFFSET, MSPI_TRIGGER); \
    pr_info("Stage 2");\
                                               timeout = MAX_CHECK_CNT; \
                                               while((MSPI_READ(MSPI_DONE_OFFSET) & MSPI_DONE_FLAG) == 0) { if(--timeout == 0) break; }; \
    pr_info("Stage 3");\
                                               MSPI_WRITE(MSPI_CHIP_SELECT_OFFSET, MSPI_CS8_DISABLE | MSPI_CS7_DISABLE | MSPI_CS6_DISABLE | MSPI_CS5_DISABLE | MSPI_CS4_DISABLE | MSPI_CS3_DISABLE | MSPI_CS2_DISABLE | MSPI_CS1_DISABLE); \
                                               MSPI_WRITE(MSPI_DONE_CLEAR_OFFSET, MSPI_CLEAR_DONE); \
                                             }

                                              // while((MHal_RIU_REG(PAD_Z80IO_READY_ADDR) & 0x1) == 0) { if(--timeout == 0) break; };
// read 2 byte
#define MSPI_READ(_reg_)                     READ_WORD(gMspBaseAddr + ((_reg_)<<2))
// write 2 byte
//#define MSPI_WRITE(_reg_, _val_)             {pr_info("PDS: MSPI_WRITE(0x%x, 0x%x, 0x%x)\n", _reg_, _val_, gMspBaseAddr + ((_reg_)<<2));  WRITE_WORD(gMspBaseAddr + ((_reg_)<<2), (_val_)); }
#define MSPI_WRITE(_reg_, _val_)             WRITE_WORD(gMspBaseAddr + ((_reg_)<<2), (_val_));
//write 2 byte mask
//#define MSPI_WRITE_MASK(_reg_, _val_, mask)  {pr_info("PDS: WRITE_LONG(0x%x, 0x%x, mask=0x%x)\n", _reg_, _val_, mask); WRITE_WORD_MASK(gMspBaseAddr + ((_reg_)<<2), (_val_), (mask)); }
#define MSPI_WRITE_MASK(_reg_, _val_, mask)  WRITE_WORD_MASK(gMspBaseAddr + ((_reg_)<<2), (_val_), (mask));

#define CLK_READ(_reg_)                      READ_WORD(gClkBaseAddr + ((_reg_)<<2))
//#define CLK_WRITE(_reg_, _val_)              {pr_info("PDS: CLK_WRITE(0x%x, 0x%x)\n", _reg_, _val_); WRITE_WORD(gClkBaseAddr + ((_reg_)<<2), (_val_)); }
#define CLK_WRITE(_reg_, _val_)              WRITE_WORD(gClkBaseAddr + ((_reg_)<<2), (_val_));

#define CHIPTOP_READ(_reg_)                  READ_WORD(gChipBaseAddr + ((_reg_)<<2))
//#define CHIPTOP_WRITE(_reg_, _val_)          {pr_info("PDS: CHIPTOP_WRITE(0x%x, 0x%x)\n", _reg_, _val_); WRITE_WORD(gChipBaseAddr + ((_reg_)<<2), (_val_)); }
#define CHIPTOP_WRITE(_reg_, _val_)          WRITE_WORD(gChipBaseAddr + ((_reg_)<<2), (_val_));

#define MOVDMA_READ(_reg_)                   READ_WORD(gMOVDMAAddr + ((_reg_)<<2))
//#define MOVDMA_WRITE(_reg_, _val_)           {pr_info("PDS: MOVDMA_WRITE(0x%x, 0x%x)\n", _reg_, _val_); WRITE_WORD(gMOVDMAAddr + ((_reg_)<<2), (_val_)); }
#define MOVDMA_WRITE(_reg_, _val_)           WRITE_WORD(gMOVDMAAddr + ((_reg_)<<2), (_val_));

#define _HAL_MSPI_ClearDone()                MSPI_WRITE(MSPI_DONE_CLEAR_OFFSET,MSPI_CLEAR_DONE)
#define MAX_CHECK_CNT                        5000

#define MSPI_READ_INDEX                      0x0
#define MSPI_WRITE_INDEX                     0x1

#define SPI_MIU0_BUS_BASE                    0x20000000
#define SPI_MIU1_BUS_BASE                    0xFFFFFFFF


// Function definitions.
//
int            z80io_init(void);
uint8_t        z80io_SPI_Send8(uint8_t   txData, uint8_t *rxData);
uint8_t        z80io_SPI_Send16(uint16_t txData, uint16_t *rxData);
uint8_t        z80io_SPI_Send32(uint32_t txData, uint32_t *rxData);
#ifdef NOTNEEDED
uint8_t        z80io_PRL_Send8(uint8_t  txData);
uint8_t        z680io_PRL_Send16(uint16_t txData);
#endif
uint8_t        z80io_PRL_Read(void);
uint8_t        z80io_PRL_Read8(uint8_t dataFlag);
uint16_t       z80io_PRL_Read16(void);
uint8_t        z80io_SPI_Test(void);
uint8_t        z80io_PRL_Test(void);
uint8_t        z80io_Z80_TestMemory(void);

extern void    MHal_GPIO_Init(void);
extern void    MHal_GPIO_Pad_Set(uint8_t u8IndexGPIO);
extern int     MHal_GPIO_PadGroupMode_Set(uint32_t u32PadMode);
extern int     MHal_GPIO_PadVal_Set(uint8_t u8IndexGPIO, uint32_t u32PadMode);
extern void    MHal_GPIO_Pad_Oen(uint8_t u8IndexGPIO);
extern void    MHal_GPIO_Pad_Odn(uint8_t u8IndexGPIO);
extern uint8_t MHal_GPIO_Pad_Level(uint8_t u8IndexGPIO);
extern uint8_t MHal_GPIO_Pad_InOut(uint8_t u8IndexGPIO);
extern void    MHal_GPIO_Pull_High(uint8_t u8IndexGPIO);
extern void    MHal_GPIO_Pull_Low(uint8_t u8IndexGPIO);
extern void    MHal_GPIO_Set_High(uint8_t u8IndexGPIO);
extern void    MHal_GPIO_Set_Low(uint8_t u8IndexGPIO);
extern void    MHal_Enable_GPIO_INT(uint8_t u8IndexGPIO);
extern int     MHal_GPIO_To_Irq(uint8_t u8IndexGPIO);
extern void    MHal_GPIO_Set_POLARITY(uint8_t u8IndexGPIO, uint8_t reverse);
extern void    MHal_GPIO_Set_Driving(uint8_t u8IndexGPIO, uint8_t setHigh);
extern void    MHal_GPIO_PAD_32K_OUT(uint8_t u8Enable);

#ifdef __cplusplus
}
#endif
#endif // Z80IO_H
