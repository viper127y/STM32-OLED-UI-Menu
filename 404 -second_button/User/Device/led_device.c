/**
 *
 *
 * @file       [test.c/h]
 * @author     [Ns电子实验室--yzq]
 * @version    v1.0.01
 * @date       2026--7--24
 * @brief      led 驱动测试代码
 *
 /// @line STM32面向对象编程结构
   
 *  <h2><center>&copy;此文件版权归【Ns电子实验室】所有.</center></h2>
 */
#include "led_device.h"
#include "string.h"
#include "math.h"

/// @line STM32面向对象编程结构

/// @brief
////  
/// @param  astLedDeviceParam[LED_DEV_NUM]  LED 设备参数结构体数组
/// @note    LED 设备参数结构体数组 ( 0号为板载LED 1-8为外接LED )
///
stLedDeviceParamTdf astLedDeviceParam[9] =
{
    {
        .stStaticParam.pstGpioBase = GPIOC,
        .stStaticParam.usGpioPin = GPIO_PIN_13,
        .stStaticParam.emLedOnLevel= emLedOnLevel_Low,
    },
		
		
};



const stLedDeviceParamTdf* c_pstGetLedDeviceParam(emLedDeviceNumTdf DevNum)
{
	 return &astLedDeviceParam[DevNum];
}


/// @brief      更新 LED 引脚输出电平（逻辑状态 -> 物理电平）
///
/// @param      emDevNum   LED 设备编号（枚举 emLedDeviceNumTdf，0 为板载 LED，1~8 为外接 LED）
///
/// @note       核心算法：ucOutPut = !(emLedOnLevel ^ emLedCurrentState)，即"同或(XNOR)"。
///             - emLedOnLevel     ：硬件点亮有效电平，0 = 低电平点亮，1 = 高电平点亮
///             - emLedCurrentState：期望逻辑状态，    0 = 灭(Off)，  1 = 亮(On)
///             两值相同 -> 输出 1(GPIO_PIN_SET)；两值不同 -> 输出 0(GPIO_PIN_RESET)，
///             从而自动适配"共阳(低电平点亮)/共阴(高电平点亮)"两种硬件接法。
///
/// @par        真值表
/// | emLedOnLevel | emLedCurrentState | 异或XOR | 引脚输出 ucOutPut | 实际效果 |
/// | :----------: | :---------------: | :-----: | :---------------: | :------: |
/// | 0（低电平点亮） | 0（灭）           |   0     | 1（GPIO_PIN_SET） | 灭       |
/// | 0（低电平点亮） | 1（亮）           |   1     | 0（GPIO_PIN_RESET）| 亮       |
/// | 1（高电平点亮） | 0（灭）           |   1     | 0（GPIO_PIN_RESET）| 灭       |
/// | 1（高电平点亮） | 1（亮）           |   0     | 1（GPIO_PIN_SET） | 亮       |
///
/// @warning    调用前必须先给该设备的 emLedCurrentState 赋好期望值，否则引脚状态不确定。
void vLedUpdatePinLevel(emLedDeviceNumTdf emDevNum)
{

    uint8_t ucOutPut;

    ucOutPut = ! (astLedDeviceParam[emDevNum].stStaticParam.emLedOnLevel ^ astLedDeviceParam[emDevNum].stRuningParam.emLedCurrentState);

    HAL_GPIO_WritePin(astLedDeviceParam[emDevNum].stStaticParam.pstGpioBase,astLedDeviceParam[emDevNum].stStaticParam.usGpioPin,(GPIO_PinState)ucOutPut);


}



/// @brief  LED 点亮
/// 
/// @param  emDevNum  LED 设备编号
///
/// @note 
void Led_on(emLedDeviceNumTdf emDevNum)
{

    astLedDeviceParam[emDevNum].stRuningParam.emLedCurrentState = emLedState_On;

    vLedUpdatePinLevel(emDevNum);

}


/// @brief  LED 熄灭
/// 
/// @param  emDevNum LED 设备编号
///
/// @note 
void Led_off(emLedDeviceNumTdf emDevNum)
{

    astLedDeviceParam[emDevNum].stRuningParam.emLedCurrentState = emLedState_Off;

    vLedUpdatePinLevel(emDevNum);

}


void LedToggle(emLedDeviceNumTdf emDevNum)
{

	 astLedDeviceParam[emDevNum].stRuningParam.emLedCurrentState =  (emLedStateTdf)!(astLedDeviceParam[emDevNum].stRuningParam.emLedCurrentState);
	 vLedUpdatePinLevel(emDevNum);

}


/// @brief LED 闪烁
///
/// @param emDevNum
///
/// @note   闪烁周期执行函数  
void vLedDeviceBlinkExecute(emLedDeviceNumTdf emDevNum)
{

    astLedDeviceParam[emDevNum].stRuningParam.ulLedCurrentCount++;

    //1. 亮
    if (astLedDeviceParam[emDevNum].stRuningParam.ulLedCurrentCount < astLedDeviceParam[emDevNum].stRuningParam.ulOnCountThreshold)
    {
        Led_on(emDevNum);
    }
    //2. 灭
    else if (astLedDeviceParam[emDevNum].stRuningParam.ulLedCurrentCount <= astLedDeviceParam[emDevNum].stRuningParam.ulOnCountThreshold + astLedDeviceParam[emDevNum].stRuningParam.ulOffCountThreshold)
    {
        Led_off(emDevNum);
    }
    //3.计数值清零
    else
    {
        astLedDeviceParam[emDevNum].stRuningParam.ulLedCurrentCount =	0;
    }


}

/// @brief      LED 呼吸灯执行函数（软件 PWM 呼吸）
///
/// @param      emDevNum   LED 设备编号
///
/// @note       算法原理：用"闪烁"模拟硬件 PWM 实现呼吸灯效果，分为两层：
///             1. 闪烁（vLedDeviceBlinkExecute）充当软件 PWM 发生器：
///                计数器 ulLedCurrentCount 每 1ms 加 1，count < on 亮、count <= on+off 灭，
///                占空比 = on/(on+off)，决定了人眼看到的平均亮度；
///             2. 本函数在"每个闪烁周期结束（count 刚被清零）"的时刻，按 sin^2 包络
///                更新占空比：on = P*sin^2(n*PI/N)，off = P - on，
///                保持周期 P 恒定、闪烁频率不变，只有亮度沿 sin^2 曲线平滑变化；
///             3. sin^2 全程非负且两端斜率趋近 0，使亮度从 0% 平滑升到 100% 再平滑回落，
///                形成"呼吸"感；一个完整呼吸 = 暗->亮->暗（共 N 个亮度台阶）。
///             注意：必须先"检测 count==0 并更新占空比"再调用闪烁，
///                  否则清零瞬间会被 count++ 错过，占空比永远不更新。
void vLedDeviceBreathExecute(emLedDeviceNumTdf emDevNum)
{

    uint32_t ulBreathCountMax;                  //1.呼吸周期内包含的闪烁次数（亮度台阶数 N）
    uint32_t ulBlinkPeriod;                     //2.固定闪烁周期 P = 亮阈值 + 灭阈值（始终保持不变）
    uint32_t ulOnNew;                           //3.本次亮度台阶的亮灯阈值（占空比分子）

    //4.计算固定闪烁周期 P：当前 亮阈值 + 灭阈值（初始 on + off）
    ulBlinkPeriod = astLedDeviceParam[emDevNum].stRuningParam.ulOnCountThreshold + astLedDeviceParam[emDevNum].stRuningParam.ulOffCountThreshold;

    //5.计算亮度台阶数 N = 呼吸总周期 / 闪烁周期
    ulBreathCountMax = astLedDeviceParam[emDevNum].stRuningParam.ulBreathCountPeriod / ulBlinkPeriod;
    if (ulBreathCountMax == 0)                  //6.防御：配置过小时强制为 1，避免后续除零
    {
        ulBreathCountMax = 1;
    }

    //7.仅在新闪烁周期刚开始（上一轮 blink 刚把 count 清零）时更新占空比
    if (astLedDeviceParam[emDevNum].stRuningParam.ulLedCurrentCount == 0)
    {
        //8.占空比沿 sin^2 包络取值：on = P * sin^2(n*PI/N)，n 为当前亮度台阶
        ulOnNew = (uint32_t)(ulBlinkPeriod * sin(PI * astLedDeviceParam[emDevNum].stRuningParam.ulBreathCurrentCount / ulBreathCountMax)
                                   * sin(PI * astLedDeviceParam[emDevNum].stRuningParam.ulBreathCurrentCount / ulBreathCountMax));

        //9.写入新的亮灯阈值（on 越大占空比越高、灯越亮）
        astLedDeviceParam[emDevNum].stRuningParam.ulOnCountThreshold  = ulOnNew;
        //10.灭灯阈值自动补齐，保证 on+off = P 恒定（闪烁频率不变，只有亮度变）
        astLedDeviceParam[emDevNum].stRuningParam.ulOffCountThreshold = ulBlinkPeriod - ulOnNew;

        //11.亮度台阶计数前进，进入下一个台阶
        astLedDeviceParam[emDevNum].stRuningParam.ulBreathCurrentCount++;
        //12.台阶走满 N 个（一次完整呼吸结束），回绕到 0 开始下一轮呼吸
        if (astLedDeviceParam[emDevNum].stRuningParam.ulBreathCurrentCount >= ulBreathCountMax)
        {
            astLedDeviceParam[emDevNum].stRuningParam.ulBreathCurrentCount = 0;
        }
    }

    //13.最后推进闪烁：用刚更新的 on/off 跑完本闪烁周期（必须先检测后执行）
    vLedDeviceBlinkExecute(emDevNum);

}


///@brief LED模式周期执行函数
///
///@param emDevNum
///
///@note 模式周期执行函数
void vLedDevicePeriodExecute(emLedDeviceNumTdf emDevNum)
{

    //1.模式选择
    switch (astLedDeviceParam[emDevNum].stRuningParam.emLedMode)
    {
    case emLedMode_Blink:
        vLedDeviceBlinkExecute(emDevNum);
        break;
    
    case emLedMode_Breath:
            vLedDeviceBreathExecute(emDevNum);
            break;

    default:
        break;
    }



}

/// @brief LED设备动态参数初始化
/// @param pstInit 
/// @param emDevNum 
/// @note  运行参数初始化
void vLedDeviceRuningParamInit(stLedRuningParamTdf  *pstInit,emLedDeviceNumTdf emDevNum)
{
    //1.运行动态参数初始化
    memcpy(&astLedDeviceParam[emDevNum].stRuningParam,pstInit,sizeof(astLedDeviceParam[emDevNum].stRuningParam));
}

void vLedDeviceInit(stLedStaticParamTdf *pstInit,emLedDeviceNumTdf emDevNum)
{
    //1.第一代version版本进行静态参数初始化，效率太低
//    astLedDeviceParam[emDevNum].pstGpioBase = pstInit->pstGpioBase;
//    astLedDeviceParam[emDevNum].usGpioPin = pstInit->usGpioPin;
//    astLedDeviceParam[emDevNum].emLedOnLevel = pstInit->emLedOnLevel;
	
    //2.第二代版本进行静态参数初始化，用memcpy进行内存拷贝
	 memcpy(&astLedDeviceParam[emDevNum].stStaticParam , pstInit ,sizeof(stLedStaticParamTdf) / sizeof(uint8_t));

}











 
