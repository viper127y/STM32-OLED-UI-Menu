/**
 *
 *
 * @file       oled_device.h
 * @author     [Ns单片机实验室--yzq]
 * @version    v1.0.01
 * @date       2026-10-13
 * @brief      OLED 显示驱动头文件（软件模拟I2C）
 *
 * 驱动分层架构（自底向上）：
 *   ① 引脚层    ：SCL/SDA 拉高拉低（BSRR 寄存器原子操作）
 *   ② I2C时序层 ：起始 / 停止 / 应答 / 发送字节
 *   ③ OLED命令层：写字节命令 / 连续写显存数据
 *   ④ 显存操作层：画点 / 清点 / 写字符 / 写字符串（只改 RAM 显存）
 *   ⑤ 刷新层    ：把整块显存一次性刷到屏幕
 *
 *  <h2><center>&copy;文件版权归属【Ns单片机实验室】所有.</center></h2>
 */

#ifndef OLED_DEVICE_H
#define OLED_DEVICE_H

#include "stm32f1xx_hal.h"

#include "project_config.h"
#include "OLED_Font.h"

/* ------------------------------ 设备号枚举 ------------------------------ */
/* 0 号为主设备，1~8 为从设备；每个设备独立占用一组引脚参数 + 一块显存 */
typedef enum
{
    emOLedDevNum0    = 0 ,
    emOLedDevNum1        ,
    emOLedDevNum2        ,
    emOLedDevNum3        ,
    emOLedDevNum4        ,
    emOLedDevNum5        ,
    emOLedDevNum6        ,
    emOLedDevNum7        ,
    emOLedDevNum8        ,

}
emOLedDeviceNumTdf;


/* ------------------------------ 字体大小枚举 ------------------------------ */
/* 枚举值 = 字符高度（像素）。字符宽度 = 高度的一半（如 8x16：宽8 高16）
 * 例：emOLedFontSize_8x16 = 16 → 高 16，宽 16/2 = 8 */
typedef enum
{

	emOLedFontSize_6x12 		  =12,
	emOLedFontSize_8x16			  =16,
	emOLedFontSize_12x24			=24,


}
emOLedFontSizeTdf;



/* ------------------------------ 像素显示模式枚举 ------------------------------ */
/* Positive（正常）：字模位为 1 → 画点（亮）
 * Negative（反色）：字模位为 1 → 清点（灭），效果是"黑底白字"变"白底黑字" */
typedef enum
{

emOLedPixelShowMode_Positive  = 0,
emOLedPixelShowMode_Negative  = 1,

}
emOLedPixelShowModeTdf;


/// @brief           静态参数结构体
///
/// @note            SDA/SCL 引脚基地址 + 引脚编号，应用层填好后交给 vOLedDeviceInit 注册。
///                  属于"硬件接线描述"：上电配一次不再改变
///
typedef struct
{
  //SDA 引脚
  GPIO_TypeDef*   pstSdaGpioBase;    //SDA GPIO基地址（如 GPIOB）
  uint32_t        ulSdaGpioPin;      //SDA 引脚号（如 GPIO_PIN_6）

  //SCL 引脚
  GPIO_TypeDef*   pstSclGpioBase;    //SCL GPIO基地址
  uint32_t        ulSclGpioPin;      //SCL 引脚号

}
stOLedStaticParamTdf;

/// @brief           运行参数结构体
///
/// @note            显存（帧缓冲）：1 字节对应屏上 8 个像素点（1 页 = 1 行字节）。
///                  所有画图操作先改这块 RAM 副本，最后由 vOLedRefreshFromBuffer
///                  整帧刷到 SSD1306——草稿纸随便涂改，考卷整页誊写
///
typedef struct
{

      uint8_t         aucOledBuffer[OLED_BUFFER_HEIGHT * OLED_BUFFER_WIDTH];  //显存
}
stOLedRuningParamTdf;

/// @brief              设备管理结构体(静态参数 + 运行参数)
///
/// @note            每个 OLED 设备一份：静态参数记录"接在哪些引脚"，
///                  运行参数记录"当前画面内容"，通过设备号索引访问
///
typedef struct
{

    stOLedStaticParamTdf  stStaticParam;
    stOLedRuningParamTdf  stRuningParam;

}
stOLedDeviceParamTdf;



/* ------------------------------ 接口函数 ------------------------------ */

/* 设备注册（初始化阶段调用一次） */
void vOLedDeviceRuningParamInit(stOLedRuningParamTdf  *pstInit,emOLedDeviceNumTdf emDevNum);   /* 拷入运行参数（显存初值） */
void vOLedDeviceInit(stOLedStaticParamTdf *pstInit,emOLedDeviceNumTdf emDevNum);               /* 拷入静态参数 + 发初始化命令序列 */

/* 设备参数查询 */
const stOLedDeviceParamTdf* c_pstGetOLedDeviceParam(emOLedDeviceNumTdf DevNum);

/* 显存操作层：只改 RAM 副本，不碰屏幕，速度快（纳秒级） */
void vOLedRefreshFromBuffer(emOLedDeviceNumTdf emDevNum);                                      /* 把显存整帧刷到屏幕（I2C，慢，一次） */
void vOLedDrawOnePointToBuffer(uint32_t x,uint32_t y,emOLedDeviceNumTdf emDevNum);             /* 显存中点亮一个像素（读-改-写置1） */
void vOLedClearOnePointToBuffer(uint32_t x,uint32_t y,emOLedDeviceNumTdf emDevNum);            /* 显存中熄灭一个像素（读-改-写清0） */
void vOLedWriteOneCharToBuffer(uint32_t x,uint32_t y,uint8_t ucChar,emOLedFontSizeTdf emFontSize,emOLedPixelShowModeTdf Mode,emOLedDeviceNumTdf emDevNum);   /* 写一个字符进显存 */
void vOLedWriteStringToBuffer(uint16_t x, uint16_t y, const uint8_t *c_pucChar, emOLedFontSizeTdf emFontSize, emOLedPixelShowModeTdf emMode, emOLedDeviceNumTdf emDevNum);  /* 写字符串进显存（自动换行） */

#endif
