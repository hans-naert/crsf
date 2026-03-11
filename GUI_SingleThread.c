
#include "cmsis_os2.h"
#include "GUI.h"
#include "DIALOG.h"
#include "main.h"
#include <stdio.h>
#include "crsf_parser.h"

extern crsf_channel_data_t channels;

#define ID_FRAMEWIN_0 (GUI_ID_USER + 0x00)
#define ID_BUTTON_0 (GUI_ID_USER + 0x01)
#define ID_CHECKBOX_0 (GUI_ID_USER + 0x02)
#define ID_TEXT_0 (GUI_ID_USER + 0x03)
#define ID_PROGBAR_0 (GUI_ID_USER + 0x05)
#define ID_PROGBAR_1 (GUI_ID_USER + 0x06)
#define ID_PROGBAR_2 (GUI_ID_USER + 0x07)
#define ID_PROGBAR_3 (GUI_ID_USER + 0x08)

#define PROGBAR_ABS_MAX (2000)

extern int  GUI_VNC_X_StartServer(int, int);

static int _ClampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static int _DrawCenteredProgbarSkin(const WIDGET_ITEM_DRAW_INFO * pDrawItemInfo) {
  if ((pDrawItemInfo->Cmd == WIDGET_ITEM_DRAW) ||
      (pDrawItemInfo->Cmd == WIDGET_ITEM_DRAW_BACKGROUND) ||
      (pDrawItemInfo->Cmd == WIDGET_ITEM_DRAW_FRAME) ||
      (pDrawItemInfo->Cmd == WIDGET_ITEM_DRAW_TEXT)) {
    PROGBAR_Handle hObj;
    int minValue;
    int maxValue;
    int value;
    int x0;
    int y0;
    int x1;
    int y1;
    int width;
    int halfWidth;
    int centerX;
    int centerValue;
    int fill;

    hObj = (PROGBAR_Handle)pDrawItemInfo->hWin;
    PROGBAR_GetMinMax(hObj, &minValue, &maxValue);
    value = _ClampInt(PROGBAR_GetValue(hObj), minValue, maxValue);

    x0 = 0;
    y0 = 0;
    x1 = WM_GetWindowSizeX(pDrawItemInfo->hWin) - 1;
    y1 = WM_GetWindowSizeY(pDrawItemInfo->hWin) - 1;

    width = x1 - x0 + 1;
    halfWidth = width / 2;
    centerX = x0 + halfWidth;
    centerValue = minValue + ((maxValue - minValue) / 2);

    GUI_SetColor(GUI_WHITE);
    GUI_FillRect(x0, y0, x1, y1);

    if ((value < centerValue) && (centerValue > minValue)) {
      fill = ((centerValue - value) * halfWidth) / (centerValue - minValue);
      if (fill > 0) {
        GUI_SetColor(GUI_RED);
        GUI_FillRect(centerX - fill, y0 + 1, centerX - 1, y1 - 1);
      }
    } else if ((value > centerValue) && (maxValue > centerValue)) {
      fill = ((value - centerValue) * halfWidth) / (maxValue - centerValue);
      if (fill > 0) {
        GUI_SetColor(GUI_GREEN);
        GUI_FillRect(centerX, y0 + 1, centerX + fill - 1, y1 - 1);
      }
    }

    GUI_SetColor(GUI_GRAY);
    GUI_DrawVLine(centerX, y0 + 1, y1 - 1);
    GUI_SetColor(GUI_BLACK);
    GUI_DrawRect(x0, y0, x1, y1);
    return 0;
  }

  return 0;
}

/*----------------------------------------------------------------------------
 *      GUIThread: GUI Thread for Single-Task Execution Model
 *---------------------------------------------------------------------------*/
#define GUI_THREAD_STK_SZ    (4096U)

static void         GUIThread (void *argument);         /* thread function */
static osThreadId_t GUIThread_tid;                      /* thread id */
static uint64_t     GUIThread_stk[GUI_THREAD_STK_SZ/8]; /* thread stack */

static const osThreadAttr_t GUIThread_attr = {
  .stack_mem  = &GUIThread_stk[0],
  .stack_size = sizeof(GUIThread_stk),
  .priority   = osPriorityNormal
};

int Init_GUIThread (void) {

  GUIThread_tid = osThreadNew(GUIThread, NULL, &GUIThread_attr);
  if (GUIThread_tid == NULL) {
    return(-1);
  }

  return(0);
}

extern WM_HWIN CreateFramewin(void);

__NO_RETURN static void GUIThread (void *argument) {
  (void)argument;

  GUI_Init();           /* Initialize the Graphics Component */
  WM_MULTIBUF_Enable(1);
  WM_SetCreateFlags(WM_CF_MEMDEV);
	
	GUI_VNC_X_StartServer(0,0);

  /* Add GUI setup code here */
	WM_HWIN hWin = CreateFramewin();
	
	WM_HWIN hItem = WM_GetDialogItem(hWin, ID_TEXT_0);
  WM_HWIN hProg0 = WM_GetDialogItem(hWin, ID_PROGBAR_0);
  WM_HWIN hProg1 = WM_GetDialogItem(hWin, ID_PROGBAR_1);
  WM_HWIN hProg2 = WM_GetDialogItem(hWin, ID_PROGBAR_2);
  WM_HWIN hProg3 = WM_GetDialogItem(hWin, ID_PROGBAR_3);
  PROGBAR_SetSkin(hProg0, _DrawCenteredProgbarSkin);
  PROGBAR_SetSkin(hProg1, _DrawCenteredProgbarSkin);
  PROGBAR_SetSkin(hProg2, _DrawCenteredProgbarSkin);
  PROGBAR_SetSkin(hProg3, _DrawCenteredProgbarSkin);
  PROGBAR_SetMinMax(hProg0, 0, PROGBAR_ABS_MAX);
  PROGBAR_SetMinMax(hProg1, 0, PROGBAR_ABS_MAX);
  PROGBAR_SetMinMax(hProg2, 0, PROGBAR_ABS_MAX);
  PROGBAR_SetMinMax(hProg3, 0, PROGBAR_ABS_MAX);
  PROGBAR_SetValue(hProg0, 100);
  PROGBAR_SetValue(hProg1, 800);
  PROGBAR_SetValue(hProg2, 1200);
  PROGBAR_SetValue(hProg3, 1900);
  
	int time=0;
	  
  while (1) {
		if(HAL_GetTick() /1000 != time)
		{
			time=HAL_GetTick()/1000;
			char buffer[50];
			sprintf(buffer,"%d",time);
			TEXT_SetText(hItem,buffer);
		};
		
		PROGBAR_SetValue(hProg0, channels.channel[0]);
		PROGBAR_SetValue(hProg1, channels.channel[1]-500);
		PROGBAR_SetValue(hProg2, channels.channel[2]+500);
		PROGBAR_SetValue(hProg3, channels.channel[3]+1000);
    
    /* All GUI related activities might only be called from here */
    GUI_TOUCH_Exec();             /* Execute Touchscreen support */
    GUI_Exec();         /* Execute all GUI jobs ... Return 0 if nothing was done. */
    GUI_X_ExecIdle();   /* Nothing left to do for the moment ... Idle processing */
  }
}
