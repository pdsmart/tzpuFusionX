////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2009 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// ("MStar Confidential Information") by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////

#define SCA_TOOL_VERSION            "SN SCA V3.0.2 "

////////////////////////////////////////////////////////////////////////////////
// DRAM memory map
//
// Every Module Memory Mapping need 4 define,
// and check code in "msAPI_Memory_DumpMemoryMap"
// 1. XXX_AVAILABLE : For get avaialble memory start address
// 2. XXX_ADR       : Real Address with Alignment
// 3. XXX_GAP_CHK   : For Check Memory Gap, for avoid memory waste
// 4. XXX_LEN       : For the Memory size of this Module usage
////////////////////////////////////////////////////////////////////////////////
#define ENABLE_MIU_1                0
#define ENABLE_MIU_2                0
#define MIU_DRAM_LEN                0x0040000000
#define MIU_DRAM_LEN0               0x0040000000
#define MIU_DRAM_LEN1               0x0000000000
#define MIU_DRAM_LEN2               0x0000000000
#define MIU_INTERVAL                0x0040000000
#define CPU_ALIGN                   0x0000001000

////////////////////////////////////////////////////////////////////////////////
//MIU SETTING
////////////////////////////////////////////////////////////////////////////////
#define MIU0_GROUP_SELMIU                        0000:0000:0000:0000:0000:0000
#define MIU0_GROUP_PRIORITY                        1:0:2:3
#define MIU1_GROUP_SELMIU                        5016:0201:1280:80B8:0004:F61F
#define MIU1_GROUP_PRIORITY                        1:0:2:3
#define MIU2_GROUP_SELMIU                        0000:0000:0000:0000:0000:0000
#define MIU2_GROUP_PRIORITY                        0:0:0:0
////////////////////////////////////////////////////////////////////////////////
//MEMORY TYPE
////////////////////////////////////////////////////////////////////////////////
#define MIU0                        (0x0000)
#define MIU1                        (0x0001)
#define MIU2                        (0x0002)

#define TYPE_NONE                   (0x0000 << 2)

#define UNCACHE                     (0x0001 << 2)
#define REMAPPING_TO_USER_SPACE     (0x0002 << 2)
#define CACHE                       (0x0004 << 2)
#define NONCACHE_BUFFERABLE         (0x0008 << 2)


#define CMA                         (0x0010 << 2)
//MIU_0_START
/* E_MMAP_ID_DUMMY2   */
#define E_MMAP_ID_DUMMY2_LAYER                                 0
#define E_MMAP_ID_DUMMY2_AVAILABLE                             0x0000000000
#define E_MMAP_ID_DUMMY2_ADR                                   0x0000000000  //Alignment 0
#define E_MMAP_ID_DUMMY2_GAP_CHK                               0x0000000000
#define E_MMAP_ID_DUMMY2_LEN                                   0x0000400000
#define E_MMAP_ID_DUMMY2_MEMORY_TYPE                           (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_DUMMY2_CMA_HID                               0

/* E_MMAP_ID_CMDQ   */
#define E_MMAP_ID_CMDQ_LAYER                                   1
#define E_MMAP_ID_CMDQ_AVAILABLE                               0x0000000000
#define E_MMAP_ID_CMDQ_ADR                                     0x0000000000  //Alignment 0x01000
#define E_MMAP_ID_CMDQ_GAP_CHK                                 0x0000000000
#define E_MMAP_ID_CMDQ_LEN                                     0x0000100000
#define E_MMAP_ID_CMDQ_MEMORY_TYPE                             (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_CMDQ_CMA_HID                                 0

/* E_MMAP_ID_GFX   */
#define E_MMAP_ID_GFX_LAYER                                    1
#define E_MMAP_ID_GFX_AVAILABLE                                0x0000100000
#define E_MMAP_ID_GFX_ADR                                      0x0000100000  //Alignment 0x01000
#define E_MMAP_ID_GFX_GAP_CHK                                  0x0000000000
#define E_MMAP_ID_GFX_LEN                                      0x0000040000
#define E_MMAP_ID_GFX_MEMORY_TYPE                              (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_GFX_CMA_HID                                  0

/* E_MMAP_ID_GE_VQ   */
#define E_MMAP_ID_GE_VQ_LAYER                                  1
#define E_MMAP_ID_GE_VQ_AVAILABLE                              0x0000140000
#define E_MMAP_ID_GE_VQ_ADR                                    0x0000140000  //Alignment 0x01000
#define E_MMAP_ID_GE_VQ_GAP_CHK                                0x0000000000
#define E_MMAP_ID_GE_VQ_LEN                                    0x0000020000
#define E_MMAP_ID_GE_VQ_MEMORY_TYPE                            (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_GE_VQ_CMA_HID                                0

/* E_MMAP_ID_AO   */
#define E_MMAP_ID_AO_LAYER                                     1
#define E_MMAP_ID_AO_AVAILABLE                                 0x0000160000
#define E_MMAP_ID_AO_ADR                                       0x0000160000  //Alignment 0x01000
#define E_MMAP_ID_AO_GAP_CHK                                   0x0000000000
#define E_MMAP_ID_AO_LEN                                       0x000004B000
#define E_MMAP_ID_AO_MEMORY_TYPE                               (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_AO_CMA_HID                                   0

/* E_MMAP_ID_AI   */
#define E_MMAP_ID_AI_LAYER                                     1
#define E_MMAP_ID_AI_AVAILABLE                                 0x00001AB000
#define E_MMAP_ID_AI_ADR                                       0x00001AB000  //Alignment 0x01000
#define E_MMAP_ID_AI_GAP_CHK                                   0x0000000000
#define E_MMAP_ID_AI_LEN                                       0x000004B000
#define E_MMAP_ID_AI_MEMORY_TYPE                               (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_AI_CMA_HID                                   0

/* E_MMAP_ID_8051   */
#define E_MMAP_ID_8051_LAYER                                   1
#define E_MMAP_ID_8051_AVAILABLE                               0x00001F6000
#define E_MMAP_ID_8051_ADR                                     0x0000200000  //Alignment 0x10000
#define E_MMAP_ID_8051_GAP_CHK                                 0x000000A000
#define E_MMAP_ID_8051_LEN                                     0x0000010000
#define E_MMAP_ID_8051_MEMORY_TYPE                             (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_8051_CMA_HID                                 0

/* E_MMAP_ID_XC_MLOAD   */
#define E_MMAP_ID_XC_MLOAD_LAYER                               1
#define E_MMAP_ID_XC_MLOAD_AVAILABLE                           0x0000210000
#define E_MMAP_ID_XC_MLOAD_ADR                                 0x0000210000  //Alignment 0x01000
#define E_MMAP_ID_XC_MLOAD_GAP_CHK                             0x0000000000
#define E_MMAP_ID_XC_MLOAD_LEN                                 0x0000040000
#define E_MMAP_ID_XC_MLOAD_MEMORY_TYPE                         (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_XC_MLOAD_CMA_HID                             0

/* E_MMAP_ID_HW_CURSOR   */
#define E_MMAP_ID_HW_CURSOR_LAYER                              1
#define E_MMAP_ID_HW_CURSOR_AVAILABLE                          0x0000250000
#define E_MMAP_ID_HW_CURSOR_ADR                                0x0000250000  //Alignment 0x01000
#define E_MMAP_ID_HW_CURSOR_GAP_CHK                            0x0000000000
#define E_MMAP_ID_HW_CURSOR_LEN                                0x0000020000
#define E_MMAP_ID_HW_CURSOR_MEMORY_TYPE                        (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_HW_CURSOR_CMA_HID                            0

/* E_MMAP_ID_VDEC_CPU   */
#define E_MMAP_ID_VDEC_CPU_LAYER                               0
#define E_MMAP_ID_VDEC_CPU_AVAILABLE                           0x0000400000
#define E_MMAP_ID_VDEC_CPU_ADR                                 0x0000400000  //Alignment 0x01000
#define E_MMAP_ID_VDEC_CPU_GAP_CHK                             0x0000000000
#define E_MMAP_ID_VDEC_CPU_LEN                                 0x0000C00000
#define E_MMAP_ID_VDEC_CPU_MEMORY_TYPE                         (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_VDEC_CPU_CMA_HID                             0

/* E_LX_MEM   */
#define E_LX_MEM_LAYER                                         0
#define E_LX_MEM_AVAILABLE                                     0x0001000000
#define E_LX_MEM_ADR                                           0x0001000000  //Alignment 0x01000
#define E_LX_MEM_GAP_CHK                                       0x0000000000
#define E_LX_MEM_LEN                                           0x003E000000
#define E_LX_MEM_MEMORY_TYPE                                   (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_LX_MEM_CMA_HID                                       0

/* E_EMAC_MEM   */
#define E_EMAC_MEM_LAYER                                       0
#define E_EMAC_MEM_AVAILABLE                                   0x003F000000
#define E_EMAC_MEM_ADR                                         0x003F000000  //Alignment 0x01000
#define E_EMAC_MEM_GAP_CHK                                     0x0000000000
#define E_EMAC_MEM_LEN                                         0x0000000000
#define E_EMAC_MEM_MEMORY_TYPE                                 (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_EMAC_MEM_CMA_HID                                     0

/* E_MMAP_ID_XC_MAIN_FB   */
#define E_MMAP_ID_XC_MAIN_FB_LAYER                             0
#define E_MMAP_ID_XC_MAIN_FB_AVAILABLE                         0x003F000000
#define E_MMAP_ID_XC_MAIN_FB_ADR                               0x003F000000  //Alignment 0x01000
#define E_MMAP_ID_XC_MAIN_FB_GAP_CHK                           0x0000000000
#define E_MMAP_ID_XC_MAIN_FB_LEN                               0x0000000000
#define E_MMAP_ID_XC_MAIN_FB_MEMORY_TYPE                       (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_XC_MAIN_FB_CMA_HID                           0

/* E_MMAP_ID_FB   */
#define E_MMAP_ID_FB_LAYER                                     0
#define E_MMAP_ID_FB_AVAILABLE                                 0x003F000000
#define E_MMAP_ID_FB_ADR                                       0x003F000000  //Alignment 0x01000
#define E_MMAP_ID_FB_GAP_CHK                                   0x0000000000
#define E_MMAP_ID_FB_LEN                                       0x0000800000
#define E_MMAP_ID_FB_MEMORY_TYPE                               (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_FB_CMA_HID                                   0

/* E_MMAP_ID_IVE   */
#define E_MMAP_ID_IVE_LAYER                                    0
#define E_MMAP_ID_IVE_AVAILABLE                                0x003F800000
#define E_MMAP_ID_IVE_ADR                                      0x003F800000  //Alignment 0x01000
#define E_MMAP_ID_IVE_GAP_CHK                                  0x0000000000
#define E_MMAP_ID_IVE_LEN                                      0x0000200000
#define E_MMAP_ID_IVE_MEMORY_TYPE                              (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_IVE_CMA_HID                                  0

/* E_MMAP_ID_VPE_MLOAD   */
#define E_MMAP_ID_VPE_MLOAD_LAYER                              0
#define E_MMAP_ID_VPE_MLOAD_AVAILABLE                          0x003FA00000
#define E_MMAP_ID_VPE_MLOAD_ADR                                0x003FA00000  //Alignment 0x01000
#define E_MMAP_ID_VPE_MLOAD_GAP_CHK                            0x0000000000
#define E_MMAP_ID_VPE_MLOAD_LEN                                0x0000100000
#define E_MMAP_ID_VPE_MLOAD_MEMORY_TYPE                        (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_MMAP_ID_VPE_MLOAD_CMA_HID                            0

/* E_EMAC_ID_CEVA   */
#define E_EMAC_ID_CEVA_LAYER                                   0
#define E_EMAC_ID_CEVA_AVAILABLE                               0x003FB00000
#define E_EMAC_ID_CEVA_ADR                                     0x003FB00000  //Alignment 0x01000
#define E_EMAC_ID_CEVA_GAP_CHK                                 0x0000000000
#define E_EMAC_ID_CEVA_LEN                                     0x0000500000
#define E_EMAC_ID_CEVA_MEMORY_TYPE                             (MIU0 | TYPE_NONE | UNCACHE | TYPE_NONE)
#define E_EMAC_ID_CEVA_CMA_HID                                 0

//MIU_1_START

#define MIU0_END_ADR                                           0x0040000000
#define MMAP_COUNT                                             0x0000000011

#define TEST_4K_ALIGN                   1

/* CHK_VALUE = 1718323879 */