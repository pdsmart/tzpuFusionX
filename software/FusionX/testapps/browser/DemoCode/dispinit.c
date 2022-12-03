#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mi_vdec.h"
#include "mi_vdec_datatype.h"
#include "mi_common.h"
#include "mi_common_datatype.h"
#include "mi_sys.h"
#include "mi_sys_datatype.h"
#include "mi_panel_datatype.h"
#include "mi_panel.h"
#include "mi_disp_datatype.h"
#include "mi_disp.h"

#include "SAT070CP50_1024x600.h"

#define STCHECKRESULT(result)                                                  \
  if (result != MI_SUCCESS) {                                                  \
    printf("[%s %d]exec function failed\n", __FUNCTION__, __LINE__);           \
    return 1;                                                                  \
  } else {                                                                     \
    printf("(%s %d)exec function pass\n", __FUNCTION__, __LINE__);             \
  }

#define VDEC_CHN_ID 0

#define VDEC_INPUT_WIDTH  1920
#define VDEC_INPUT_HEIGHT 1080

#define VDEC_OUTPUT_WIDTH   (stPanelParam.u16Width) // 1024
#define VDEC_OUTPUT_HEIGHT  (stPanelParam.u16Height)// 600

#define DISP_INPUT_WIDTH  VDEC_OUTPUT_WIDTH
#define DISP_INPUT_HEIGHT VDEC_OUTPUT_HEIGHT

#define DISP_OUTPUT_X 0
#define DISP_OUTPUT_Y 0
#define DISP_OUTPUT_WIDTH   VDEC_OUTPUT_WIDTH
#define DISP_OUTPUT_HEIGHT  VDEC_OUTPUT_HEIGHT

#define MAKE_YUYV_VALUE(y, u, v) ((y) << 24) | ((u) << 16) | ((y) << 8) | (v)
#define YUYV_BLACK MAKE_YUYV_VALUE(0, 128, 128)

int sdk_Init(void) {
  // init sys
  MI_SYS_Init();

  // init disp
  MI_DISP_PubAttr_t stDispPubAttr;
  stDispPubAttr.eIntfType = E_MI_DISP_INTF_LCD;
  stDispPubAttr.eIntfSync = E_MI_DISP_OUTPUT_USER;
  stDispPubAttr.u32BgColor = YUYV_BLACK;

  stDispPubAttr.stSyncInfo.u16Vact = stPanelParam.u16Height;
  stDispPubAttr.stSyncInfo.u16Vbb = stPanelParam.u16VSyncBackPorch;
  stDispPubAttr.stSyncInfo.u16Vfb =
      stPanelParam.u16VTotal -
      (stPanelParam.u16VSyncWidth + stPanelParam.u16Height +
       stPanelParam.u16VSyncBackPorch);
  stDispPubAttr.stSyncInfo.u16Hact = stPanelParam.u16Width;
  stDispPubAttr.stSyncInfo.u16Hbb = stPanelParam.u16HSyncBackPorch;
  stDispPubAttr.stSyncInfo.u16Hfb =
      stPanelParam.u16HTotal -
      (stPanelParam.u16HSyncWidth + stPanelParam.u16Width +
       stPanelParam.u16HSyncBackPorch);
  stDispPubAttr.stSyncInfo.u16Bvact = 0;
  stDispPubAttr.stSyncInfo.u16Bvbb = 0;
  stDispPubAttr.stSyncInfo.u16Bvfb = 0;
  stDispPubAttr.stSyncInfo.u16Hpw = stPanelParam.u16HSyncWidth;
  stDispPubAttr.stSyncInfo.u16Vpw = stPanelParam.u16VSyncWidth;
  stDispPubAttr.stSyncInfo.u32FrameRate =
      stPanelParam.u16DCLK * 1000000 /
      (stPanelParam.u16HTotal * stPanelParam.u16VTotal);
  stDispPubAttr.eIntfSync = E_MI_DISP_OUTPUT_USER;
  stDispPubAttr.eIntfType = E_MI_DISP_INTF_LCD;

  MI_DISP_SetPubAttr(0, &stDispPubAttr);
  MI_DISP_Enable(0);
  MI_DISP_BindVideoLayer(0, 0);
  MI_DISP_EnableVideoLayer(0);

  // init Panel
  MI_PANEL_LinkType_e eLinkType;
  eLinkType = stPanelParam.eLinkType;
  MI_PANEL_Init(eLinkType);
  MI_PANEL_SetPanelParam(&stPanelParam);
  if (eLinkType == E_MI_PNL_LINK_MIPI_DSI) {
    MI_PANEL_SetMipiDsiConfig(&stMipiDsiConfig);
  }
  return 0;
}

int sdk_DeInit(void) {
  MI_DISP_DisableInputPort(0, 0);
  MI_DISP_DisableVideoLayer(0);
  MI_DISP_UnBindVideoLayer(0, 0);
  MI_DISP_Disable(0);
  MI_PANEL_DeInit();
  MI_SYS_Exit();
  return 0;
}

int main(int argc, char **argv) {
  printf("--------------- init panel & display  ---------------\n");
  sdk_Init();
  while (1) {
    sleep(10.0);
  }
  printf("--------------- exit ---------------\n");
  sdk_DeInit();
  return 0;
}
