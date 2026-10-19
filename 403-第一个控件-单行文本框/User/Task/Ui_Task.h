/**
 *
 *
 * @file       Ui_Task.h
 * @author     [Ns单片机实验室--yzq]
 * @version    v1.0.01
 * @date       2026-10-18
 * @brief      Ui任务
 *

 *
 /// @line STM32单片机开发 

 *  <h2><center>&copy;文件版权归属【Ns单片机实验室】所有.</center></h2>
 */
 
 #ifndef   _UI_TASK_H
 #define   _UI_TASK_H
 
#include "stdint.h"
#include "oled_device.h"
 
 typedef struct 
 {
    uint32_t             x; //起始x坐标
    uint32_t             y; //y坐标
    uint8_t*             ucData;
    emOLedFontSizeTdf    emOLedFontSize;
    
 }
 stSingleLineTxetTdf;
 
 
 
 
 
 
 
 
 #endif
 
 
 
 
 
 
 


