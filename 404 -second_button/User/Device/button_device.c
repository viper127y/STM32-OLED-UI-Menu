/**
  * @file       button_device.c
  * @author     [Ns单片机实验室--yzq]
  * @version    V1.0.1
  * @date       20251203
  * @brief      按键设备驱动 (轮询扫描, 基于 STM32 HAL)
  *
  * 核心思想 (两级流水线, 每帧调一次 vBtnExecute):
  *   ① 电平采集层  s_emBtnReadStatus  : 读 GPIO -> 消抖 -> 得到稳定状态
  *   ② 事件判定层  s_vBtnDetectEvent  : 对比前后状态 -> 生成 单击/双击/长按 事件
  *
  * 事件判定直觉 (按下 = 1, 松开 = 0):
  *   状态发生跳变时: 0->1 按下瞬间  : 清按下计数, 准备计时
  *                    1->0 松开瞬间  : 按住够久 -> 长按; 否则连击数 +1
  *   状态不变时     : 一直是 1      : 按下计数累加 (攒够阈值就是长按)
  *                    一直是 0      : 若已点了 1 击, 等第二击; 等超了报单击;
  *                                    若已点了 2 击, 直接报双击
  *
  * <h2><center>&copy;文件版权归属【Ns单片机实验室】所有.</center></h2>
  */

#include "button_device.h"
#include "string.h"

/// @brief      设备实例池
///
/// @note       每个设备号对应一份 静态参数 + 运行参数, 上电由 emBtnDeviceInit 填好
static stBtnDeviceParamTdf s_astBtnDevice[BTN_DEV_NUM];

/// @brief      拷贝静态参数
///
/// @param      pstInit     : 初始化参数
/// @param      emDevNum    : 设备号
///
/// @note       整块内存一次拷贝, 应用层配好的接线/阈值原样存进设备池
static void s_vBtnStaticParamCopy(stBtnStaticParamTdf *pstInit, emBtnDevNumTdf emDevNum)
{
    memcpy(&s_astBtnDevice[emDevNum].stStaticParam, pstInit, sizeof(stBtnStaticParamTdf) / sizeof(uint8_t));
}

/// @brief      获取运行参数指针
///
/// @param      emDevNum    : 设备号
///
/// @note       返回只读指针, 外面能看不能改, 防止绕过驱动乱动状态机
const stBtnRunningParamTdf *c_pstGetBtnRunningParam(emBtnDevNumTdf emDevNum)
{
    return &s_astBtnDevice[emDevNum].stRunningParam;
}

/// @brief      修改运行参数
///
/// @param      pstInit     : 要设置的运行参数
/// @param      emDevNum    : 设备号
///
/// @note       调试用途: 逐字段替换, 传 0xFFFF / 0xFFFFFFFF 时表示这项不改
///             (对应枚举 emBtnStatus_NotModify), 方便只改感兴趣的项
void vBtnSetRunningParam(stBtnRunningParamTdf *pstInit, emBtnDevNumTdf emDevNum)
{
    if(pstInit->emCurrentStatus != emBtnStatus_NotModify)
    {
        s_astBtnDevice[emDevNum].stRunningParam.emCurrentStatus = pstInit->emCurrentStatus;
    }

    if(pstInit->emLastStatus != emBtnStatus_NotModify)
    {
        s_astBtnDevice[emDevNum].stRunningParam.emLastStatus = pstInit->emLastStatus;
    }

    if(pstInit->ulDebounceCount != 0xFFFFFFFF)
    {
        s_astBtnDevice[emDevNum].stRunningParam.ulDebounceCount = pstInit->ulDebounceCount;
    }

    if(pstInit->ulPressCount != 0xFFFFFFFF)
    {
        s_astBtnDevice[emDevNum].stRunningParam.ulPressCount = pstInit->ulPressCount;
    }

    if(pstInit->ulClickCount != 0xFFFFFFFF)
    {
        s_astBtnDevice[emDevNum].stRunningParam.ulClickCount = pstInit->ulClickCount;
    }

    if(pstInit->ulDoubleClickCount != 0xFFFFFFFF)
    {
        s_astBtnDevice[emDevNum].stRunningParam.ulDoubleClickCount = pstInit->ulDoubleClickCount;
    }
}

/// @brief      获取按键事件
///
/// @param      emDevNum    : 设备号
///
/// @retval     当前按键事件
emBtnEventTypeTdf emBtnGetEvent(emBtnDevNumTdf emDevNum)
{
    return s_astBtnDevice[emDevNum].stRunningParam.emCurrentEvent;
}

/// @brief      清除按键事件
///
/// @param      emDevNum    : 设备号
///
/// @note       事件是 "信箱" 模式: 判定层塞一封, 应用层取走后必须清,
///             不清的话下一帧还能取到同一个事件, 会重复响应
void vBtnClearEvent(emBtnDevNumTdf emDevNum)
{
    s_astBtnDevice[emDevNum].stRunningParam.emCurrentEvent = emBtnEvent_None;
}

/// @brief      读电平并消抖, 得到稳定按键状态
///
/// @param      emDevNum    : 设备号
///
/// @retval     消抖后的按键状态 (按下/释放)
///
/// @note       消抖原理 (计数器法):
///             机械触点闭合/断开瞬间会抖出毛刺, 电平忽高忽低,
///             不能读一次就信一次。
///             规则: 新电平和当前稳定状态不一致 -> 计数 +1;
///                   连续不一致攒够阈值 -> 才承认状态真的变了;
///                   中间只要有一次读回一致 -> 计数清零重新攒。
///             相当于要求 "新状态连续站稳 N 帧" 才翻面, 毛刺活不过 N 帧
static emBtnStatusTdf s_emBtnReadStatus(emBtnDevNumTdf emDevNum)
{
    GPIO_PinState emPinLevel;
    emBtnStatusTdf emCurrentLevel;

    // 1. 读取引脚原始电平
    emPinLevel = HAL_GPIO_ReadPin(s_astBtnDevice[emDevNum].stStaticParam.pstGpioBase,
                                 s_astBtnDevice[emDevNum].stStaticParam.usGpioPin);

    // 2. 电平 -> 状态: 和配置的有效电平相等就是按下, 否则释放
    emCurrentLevel = (emPinLevel == s_astBtnDevice[emDevNum].stStaticParam.emActiveLevel) ?
                     emBtnStatus_Pressed : emBtnStatus_Released;

    // 3. 消抖: 新状态连续 "不一致" 攒够阈值才采信, 中途回到一致就清零重来
    if(emCurrentLevel != s_astBtnDevice[emDevNum].stRunningParam.emCurrentStatus)
    {
        s_astBtnDevice[emDevNum].stRunningParam.ulDebounceCount++;
        if(s_astBtnDevice[emDevNum].stRunningParam.ulDebounceCount >=
           s_astBtnDevice[emDevNum].stStaticParam.ulDebounceCountThreshold)
        {
            s_astBtnDevice[emDevNum].stRunningParam.emCurrentStatus = emCurrentLevel;
            s_astBtnDevice[emDevNum].stRunningParam.ulDebounceCount = 0;
        }
    }
    else
    {
        s_astBtnDevice[emDevNum].stRunningParam.ulDebounceCount = 0;
    }

    return s_astBtnDevice[emDevNum].stRunningParam.emCurrentStatus;
}

/// @brief      按键事件判定 (状态机)
///
/// @param      emDevNum    : 设备号
///
/// @note       输入: 本帧消抖后的 emCurrentStatus vs 上一帧的 emLastStatus
///             输出: 往 emCurrentEvent 信箱里塞事件
///
///             长按判定: 松手那刻看按下持续计数, 攒够长按阈值 -> 长按
///             单击判定: 松手后连击数=1, 进入等二击窗口 (间隔计数累加),
///                       窗口内没等到第二击 -> 先报单击 (间隔配 0 = 立即报)
///             双击判定: 第二击松手后连击数变成 2 -> 直接报双击
static void s_vBtnDetectEvent(emBtnDevNumTdf emDevNum)
{
    emBtnStatusTdf emCurrentStatus = s_astBtnDevice[emDevNum].stRunningParam.emCurrentStatus;
    emBtnStatusTdf emLastStatus = s_astBtnDevice[emDevNum].stRunningParam.emLastStatus;

    // 1. 状态跳变分支: 只在按下/松开的那一帧进来
    if(emCurrentStatus != emLastStatus)
    {
        // 1.1 按下跳变: 从零开始计按下时长
        if(emCurrentStatus == emBtnStatus_Pressed)
        {
            s_astBtnDevice[emDevNum].stRunningParam.ulPressCount = 0;
        }
        // 1.2 松开跳变: 结算这一轮按了多久
        else
        {
            // 按住时长攒够阈值 -> 长按事件, 连击流程作废
            if(s_astBtnDevice[emDevNum].stRunningParam.ulPressCount >=
               s_astBtnDevice[emDevNum].stStaticParam.ulLongPressCountThreshold)
            {
                s_astBtnDevice[emDevNum].stRunningParam.emCurrentEvent = emBtnEvent_LongPress;
                s_astBtnDevice[emDevNum].stRunningParam.ulClickCount = 0;
            }
            // 没够长按 -> 记为一次 "击", 连击数 +1 (第 1 击或第 2 击)
            else
            {
                s_astBtnDevice[emDevNum].stRunningParam.ulClickCount++;
                s_astBtnDevice[emDevNum].stRunningParam.ulDoubleClickCount = 0;
            }
        }
    }
    // 2. 状态不变分支: 一直按着, 或者一直松着
    else
    {
        // 2.1 持续按下: 累加按下时长 (松手时拿去和长按阈值比)
        if(emCurrentStatus == emBtnStatus_Pressed)
        {
            s_astBtnDevice[emDevNum].stRunningParam.ulPressCount++;
        }
        // 2.2 持续松开: 处理连击等待
        else
        {
            // 已击 1 次: 在等第二击, 等待计数累加, 超过间隔还没等到 -> 报单击
            if(s_astBtnDevice[emDevNum].stRunningParam.ulClickCount == 1)
            {
                s_astBtnDevice[emDevNum].stRunningParam.ulDoubleClickCount++;
                if(s_astBtnDevice[emDevNum].stRunningParam.ulDoubleClickCount >=
                   s_astBtnDevice[emDevNum].stStaticParam.ulDoubleClickInterval)
                {
                    s_astBtnDevice[emDevNum].stRunningParam.emCurrentEvent = emBtnEvent_Click;
                    s_astBtnDevice[emDevNum].stRunningParam.ulClickCount = 0;
                }
            }
            // 已击 2 次: 不用等了, 直接报双击
            else if(s_astBtnDevice[emDevNum].stRunningParam.ulClickCount == 2)
            {
                s_astBtnDevice[emDevNum].stRunningParam.emCurrentEvent = emBtnEvent_DoubleClick;
                s_astBtnDevice[emDevNum].stRunningParam.ulClickCount = 0;
            }
        }
    }

    // 3. 滚动保存本帧状态, 供下一帧做边沿比较
    s_astBtnDevice[emDevNum].stRunningParam.emLastStatus = emCurrentStatus;
}

/// @brief      按键执行 (每帧对每个设备调一次)
///
/// @param      emDevNum    : 设备号
///
/// @note       两级流水线: 先采电平消抖, 再基于新状态判事件
void vBtnExecute(emBtnDevNumTdf emDevNum)
{
    // 1. 采集按键状态 (含消抖)
    s_emBtnReadStatus(emDevNum);

    // 2. 判定按键事件
    s_vBtnDetectEvent(emDevNum);
}

/// @brief      设备初始化
///
/// @param      pstInit     : 初始化结构体实例指针
/// @param      emDevNum    : 设备号
///
/// @retval     错误是否成功, 见 emBtnFuncErrorCodeTdf 枚举
///
/// @note       注册静态参数 + 把状态机所有变量归零
emBtnFuncErrorCodeTdf emBtnDeviceInit(stBtnStaticParamTdf *pstInit, emBtnDevNumTdf emDevNum)
{
    // 1. 拷贝静态参数
    s_vBtnStaticParamCopy(pstInit, emDevNum);

    // 2. 初始化运行参数: 初始状态 = 释放, 无事件, 计数全清零
    s_astBtnDevice[emDevNum].stRunningParam.emCurrentStatus = emBtnStatus_Released;
    s_astBtnDevice[emDevNum].stRunningParam.emLastStatus = emBtnStatus_Released;
    s_astBtnDevice[emDevNum].stRunningParam.emCurrentEvent = emBtnEvent_None;
    s_astBtnDevice[emDevNum].stRunningParam.ulDebounceCount = 0;
    s_astBtnDevice[emDevNum].stRunningParam.ulPressCount = 0;
    s_astBtnDevice[emDevNum].stRunningParam.ulClickCount = 0;
    s_astBtnDevice[emDevNum].stRunningParam.ulDoubleClickCount = 0;

    // 3. 返回错误码
    return emBtnFuncErrorCode_None;
}
