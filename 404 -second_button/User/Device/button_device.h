/**
  * @file       button_device.h
  * @author     [Ns单片机实验室--yzq]
  * @version    V1.0.1
  * @date       20251203
  * @brief      按键设备驱动 (轮询扫描, 基于 STM32 HAL)
  *
  * 分层说明:
  *   本文件只定义 "一个按键长什么样 + 有哪些接口"。
  *   一个按键 = 静态参数 (接线 + 阈值, 上电配一次) + 运行参数 (状态机, 每帧在变),
  *   通过设备号 emBtnDevNumTdf 索引到设备池里的某一实例。
  *
  * 使用流程:
  *   emBtnDeviceInit() 注册参数  ->  每帧调 vBtnExecute() 扫描
  *   ->  emBtnGetEvent() 取事件  ->  vBtnClearEvent() 清事件
  *
  * <h2><center>&copy;文件版权归属【Ns单片机实验室】所有.</center></h2>
  */

#ifndef _BUTTON_DEVICE_H_
#define _BUTTON_DEVICE_H_

#include "project_config.h"
#include STM32_IC_HAL

/// @brief      设备号枚举类型
///
/// @note       0 号为主设备; 设备池长度由 BTN_DEV_NUM 决定
typedef enum
{
    emBtnDevNum0                                       = 0,                            // 0  设备 0
    emBtnDevNum1,
    emBtnDevNum2,
    emBtnDevNum3,
    emBtnDevNum4,
    emBtnDevNum5,
    emBtnDevNum6,
    emBtnDevNum7,
    emBtnDevNum8,
    emBtnDevNum9,
    emBtnDevNum10,
} emBtnDevNumTdf;

/// @brief      按键有效电平枚举类型
///
/// @note       按下按钮时的电平状态: 接地按键配 Low, 接 VCC 按键配 High
typedef enum
{
    emBtnActiveLevel_Low                               = 0,                            // 0  低电平有效
    emBtnActiveLevel_High                              = 1,                            // 1  高电平有效
} emBtnActiveLevelTdf;

/// @brief      按键状态枚举类型
///
/// @note       NotModify 仅用于 vBtnSetRunningParam: 表示 "这项不改"
typedef enum
{
    emBtnStatus_Released                               = 0,                            // 0        释放
    emBtnStatus_Pressed                                = 1,                            // 1        按下
    emBtnStatus_NotModify                              = 0xFFFF,                       // 0xFFFF   不作修改
} emBtnStatusTdf;

/// @brief      按键事件枚举类型
///
/// @note       事件由 s_vBtnDetectEvent 状态机判定, 一次只存最新一个,
///             应用层取走后必须 vBtnClearEvent 清掉, 否则会重复响应
typedef enum
{
    emBtnEvent_None                                    = 0,                            // 0        无事件
    emBtnEvent_Click                                   = 1,                            // 1        单击事件
    emBtnEvent_LongPress                               = 2,                            // 2        长按事件
    emBtnEvent_DoubleClick                             = 3,                            // 3        双击事件
} emBtnEventTypeTdf;

/// @brief      按键函数错误代码枚举类型
///
/// @note       返回值指示相关函数是否异常
typedef enum
{
    emBtnFuncErrorCode_None                            = 0,                            // 0  无错误
} emBtnFuncErrorCodeTdf;

/// @brief      设备静态参数结构体类型
///
/// @note       上面四项描述接线, 下面三项是判定阈值;
///             单位都是 "扫描次数" (本工程 1 tick 扫一次, 即次数 = tick 数):
///             消抖阈值 2 = 连续 2 次读到新电平才算数;
///             长按阈值 = 按住超过 N 次扫描算长按;
///             双击间隔 = 松手后等 N 次扫描没等到第二击, 就先报单击
typedef struct
{
    GPIO_TypeDef                                       *pstGpioBase;                  // 使用的 GPIOx
    uint16_t                                           usGpioPin;                     // 使用的 PINx
    emBtnActiveLevelTdf                                emActiveLevel;                 // 按键有效电平
    uint32_t                                           ulDebounceCountThreshold;      // 消抖计数阈值
    uint32_t                                           ulLongPressCountThreshold;     // 长按计数阈值
    uint32_t                                           ulDoubleClickInterval;         // 双击间隔计数阈值
} stBtnStaticParamTdf;

/// @brief      设备运行参数结构体类型
///
/// @note       状态机的 "活变量", 每帧扫描都在更新:
///             2 个状态 + 1 个事件 + 4 个计数器
typedef struct
{
    emBtnStatusTdf                                     emCurrentStatus;               // 当前状态 (消抖后的)
    emBtnStatusTdf                                     emLastStatus;                  // 上一次状态 (供边沿比较)
    emBtnEventTypeTdf                                  emCurrentEvent;                // 当前事件 (最新判定的)
    uint32_t                                           ulDebounceCount;               // 消抖计数器
    uint32_t                                           ulPressCount;                  // 按下持续计数器 (判长按用)
    uint32_t                                           ulClickCount;                  // 已连击次数计数器 (1=等二击, 2=双击)
    uint32_t                                           ulDoubleClickCount;            // 双击间隔计数器 (等第二击的等待帧数)
} stBtnRunningParamTdf;

/// @brief      设备参数结构体类型
///
/// @note       静态参数和动态参数打包成一个设备实例
typedef struct
{
    stBtnStaticParamTdf                                stStaticParam;                 // 静态参数
    stBtnRunningParamTdf                               stRunningParam;                // 运行参数
} stBtnDeviceParamTdf;

/* ------------------------------ 接口函数 ------------------------------ */
const stBtnRunningParamTdf *c_pstGetBtnRunningParam(emBtnDevNumTdf emDevNum);   /* 只读获取运行参数指针 */
emBtnEventTypeTdf emBtnGetEvent(emBtnDevNumTdf emDevNum);                       /* 取当前事件 */
void vBtnClearEvent(emBtnDevNumTdf emDevNum);                                   /* 清事件 (处理完必须调) */
void vBtnSetRunningParam(stBtnRunningParamTdf *pstInit, emBtnDevNumTdf emDevNum); /* 调试用: 改运行参数 */
void vBtnExecute(emBtnDevNumTdf emDevNum);                                      /* 每帧扫描: 读电平+判事件 */
emBtnFuncErrorCodeTdf emBtnDeviceInit(stBtnStaticParamTdf *pstInit, emBtnDevNumTdf emDevNum); /* 注册设备 */

#endif
