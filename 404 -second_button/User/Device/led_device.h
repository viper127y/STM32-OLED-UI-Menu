/**
 *
 *
 * @file       [test.c/h]
 * @author     [Ns电子实验室--yzq]
 * @version    v1.0.01
 * @date       2026--7--24
 * @brief      led 驱动测试代码
 *
 
   
 *  <h2><center>&copy;此文件版权归【Ns电子实验室】所有.</center></h2>
 */

#ifndef _LED_DEVICE_H_
#define _LED_DEVICE_H_

#include "stm32f1xx_hal.h"

#include "project_config.h"
#define PI 3.141592653

typedef enum 
{
    emLedDevNum0    = 0 ,
    emLedDevNum1        ,
    emLedDevNum2        ,
    emLedDevNum3        ,
    emLedDevNum4        ,
    emLedDevNum5        ,
    emLedDevNum6        ,
    emLedDevNum7        ,
    emLedDevNum8        ,
    
}
emLedDeviceNumTdf;

typedef enum
{
    emLedMode_static     = 0 ,       //静态模式
    emLedMode_Blink      = 1 ,       //闪烁模式
    emLedMode_Breath      = 2 ,      //呼吸灯模式
}
emLedModeTdf;


///@breif          LED 点亮时的电平
typedef enum 
{
    emLedOnLevel_Low       = 0 ,
    emLedOnLevel_Hight         ,
}
emLedOnLeveltdf;


///@breif          LED 状态枚举变量
typedef enum 
{
    emLedState_Off      = 0 ,
    emLedState_On           ,
}
emLedStateTdf;



/// @brief           静态参数定义
///  
/// @note            
///
typedef struct 
{
    GPIO_TypeDef        *pstGpioBase;           //使用的GPIOx
    uint16_t             usGpioPin;             //使用的GPIO_PIN_x
    emLedOnLeveltdf      emLedOnLevel;          //LED 点亮时的电平
 
}
stLedStaticParamTdf;

/// @brief           运行动态参数定义
///  
/// @note            
///
typedef struct 
{
 
    emLedStateTdf       emLedCurrentState;             //LED 状态
    emLedModeTdf        emLedMode;         
    uint32_t            ulLedCurrentCount;               //当前计数
    uint32_t            ulOnCountThreshold;               //亮灯阈值
    uint32_t            ulOffCountThreshold;               //灭灯阈值

    uint32_t            ulBreathCountPeriod;             //breath 计数周期
    uint32_t            ulBreathCurrentCount;            //breath 当前台阶计数（0 ~ 呼吸周期内闪烁次数）
}
stLedRuningParamTdf;

/// @brief              设备参数结构体(静态参数 + 运行参数)
///  
/// @note            
///
typedef struct 
{
    stLedStaticParamTdf  stStaticParam;
    stLedRuningParamTdf  stRuningParam;
	
}
stLedDeviceParamTdf;











void Led_on(emLedDeviceNumTdf emDevNum);
void Led_off(emLedDeviceNumTdf emDevNum);
void vLedDeviceInit(stLedStaticParamTdf *pstInit,emLedDeviceNumTdf emDevNum);
const stLedDeviceParamTdf* c_pstGetLedDeviceParam(emLedDeviceNumTdf DevNum);
void LedToggle(emLedDeviceNumTdf emDevNum);
void vLedDeviceBlinkExecute(emLedDeviceNumTdf emDevNum);
void vLedDevicePeriodExecute(emLedDeviceNumTdf emDevNum);
void vLedDeviceRuningParamInit(stLedRuningParamTdf  *pstInit,emLedDeviceNumTdf emDevNum);







 

#endif
