/**
 *
 *
 * @file       Ui_Task.h
 * @author     [Ns单片机实验室--yzq]
 * @version    v1.0.01
 * @date       2026-10-18
 * @brief      UI 任务对外接口: 单行文本类型 + 任务入口
 *
 *  <h2><center>&copy;文件版权归属【Ns单片机实验室】所有.</center></h2>
 */

 #ifndef   _UI_TASK_H
 #define   _UI_TASK_H

#include "stdint.h"
#include "oled_device.h"

/// @brief 单行文本结构体: 只显示, 不交互 (区别于可点按的 stButtonTdf)
 typedef struct
 {
    uint32_t             x; 				//起始x坐标 (像素)
    uint32_t             y; 				//y坐标 (像素)
    uint8_t*             ucData;			//要显示的字符串
    emOLedFontSizeTdf    emOLedFontSize;	//字体大小 (枚举值 = 字符像素高)

 }
 stSingleLineTxetTdf;

 #endif
