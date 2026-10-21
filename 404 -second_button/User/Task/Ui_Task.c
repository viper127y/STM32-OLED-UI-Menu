/**
 *
 *
 * @file       Ui_Task.c
 * @author     [Ns单片机实验室--yzq]
 * @version    v1.0.01
 * @date       2026-10-18
 * @brief      UI 任务: 在 OLED 上绘制单行文本 + 按钮, 用物理按键切换选中/触发
 *
 * 文件结构 (自上而下):
 *   ① 类型定义   : 按钮的状态枚举 + 按钮结构体
 *   ② 数据实例   : 三个按钮 (On/Off/Toggle) + 单行文本
 *   ③ 绘制逻辑   : 把文本/按钮画进显存
 *   ④ 按键逻辑   : 扫描触发 -> 执行回调 -> 切换选中
 *   ⑤ 主循环     : 每帧 清屏 -> 画文本 -> 处理按键 -> 画按钮 -> 刷新
 *
 *  <h2><center>&copy;文件版权归属【Ns单片机实验室】所有.</center></h2>
 */


#include "FreeRtos.h"
#include "task.h"
#include "Ui_Task.h"
#include "oled_device.h"
#include "led_device.h"
#include "button_device.h"
/* ------------------------------ 库函数 ------------------------------ */
#include <string.h>

/* 回调函数先声明后使用: 按钮实例的初始化列表里要取它们的地址 */
void vButtonCallBack_On(void);
void vButtonCallBack_Toggle(void);
void vButtonCallBack_Off(void);


/* ============================ ① 类型定义 ============================ */

/// @brief 按钮回调函数类型: 每个按钮触发时执行的动作
typedef void vfButtonCallBackTdf(void);


/// @brief 按钮选中状态: 被光标选中的按钮才画方框
typedef enum
{
	emButtonSelectStatus_IsNotSelected  = 0 ,
	emButtonSelectStatus_IsSelected		= 1 ,
}
emButtonSelectStatusTdf;


/// @brief 按钮触发状态: 本帧是否检测到 "新按下" (边沿事件, 只在按下瞬间为真)
typedef enum
{
	emButtonTriggerStatus_IsNotTrigger  = 0,
	emButtonTriggerStatus_IsTrigger  = 1,
}
emButtonTriggerStatusTdf;


/// @brief 按钮按下状态: ENTER 键按下时置位, 计数超时后自动复位 (模拟松手)
typedef enum
{
	emButtonPressStatus_IsNotPressed  = 0 ,
	emButtonPressStatus_IsPressed		= 1 ,
}
emButtonPressStatusTdf;

/// @brief 按钮结构体: 外观 (坐标/文本/字号) + 状态 (选中/按下) + 计数 + 回调
 typedef struct
 {
    uint32_t             			x; 						//起始x坐标
    uint32_t             			y; 						//y坐标 (单位: "行", 实际 = 行数 * 字高)
    uint8_t*             			ucData;					//要显示的字符串
    emOLedFontSizeTdf    			emOLedFontSize;			//字体大小 (枚举值 = 字符像素高)
	emButtonPressStatusTdf			emButtonPressStatusLast;	//按钮上次按下状态 (用于边沿检测)
	emButtonPressStatusTdf			emButtonPressStatusCurrent;	//按钮当前按下状态
	emButtonSelectStatusTdf			emButtonSelectStatus;	//按钮选中状态
	uint32_t						ulPressedCount;			//按钮按下保持帧计数
	uint32_t						ulPressedCountThreshold;//按下计数阈值 (超过自动复位)
	vfButtonCallBackTdf				*vpfButtonCallBack;		//按钮回调函数

 }
 stButtonTdf;


/* ============================ ② 数据实例 ============================ */

/* ---- 三个 UI 按钮 ----
 * y 用 "第几行字" 描述位置 (y = 行数 * 字号), 换字体时布局比例不乱;
 * 字符宽 = 字高 / 2 (如 8x16: 宽 8 高 16) */

static uint8_t s_aucButton_On[] = "On";

static stButtonTdf s_stButton_On =
{
	.x = 20,
	.y = 1.2 * emOLedFontSize_8x16,			//第 1.2 行
	.ucData = s_aucButton_On,
	.emOLedFontSize = emOLedFontSize_8x16,
	.emButtonSelectStatus = emButtonSelectStatus_IsSelected,	//初始光标停在这
	.emButtonPressStatusCurrent = emButtonPressStatus_IsNotPressed,
	.emButtonPressStatusLast = emButtonPressStatus_IsNotPressed,
	.ulPressedCount		= 0,
	.ulPressedCountThreshold = 5,			//按下 5 帧后自动复位
	.vpfButtonCallBack 		= vButtonCallBack_On,
};

static uint8_t s_aucButton_Off[] = "Off";

static stButtonTdf s_stButton_Off =
{
	.x = 80,
	.y = 1.2 * emOLedFontSize_8x16,
	.ucData = s_aucButton_Off,
	.emOLedFontSize = emOLedFontSize_8x16,
	.emButtonSelectStatus = emButtonSelectStatus_IsNotSelected,
	.emButtonPressStatusCurrent = emButtonPressStatus_IsNotPressed,
	.emButtonPressStatusLast = emButtonPressStatus_IsNotPressed,
	.ulPressedCount		= 0,
	.ulPressedCountThreshold = 5,
	.vpfButtonCallBack 		= vButtonCallBack_Off,
};

static uint8_t s_aucButton_Toggle[] = "Toggle";

static stButtonTdf s_stButton_Toggle =
{
	.x = 20,
	.y = 2.5 * emOLedFontSize_8x16,			//第 2.5 行
	.ucData = s_aucButton_Toggle,
	.emOLedFontSize = emOLedFontSize_8x16,
	.emButtonSelectStatus = emButtonSelectStatus_IsNotSelected,
	.emButtonPressStatusCurrent = emButtonPressStatus_IsNotPressed,
	.emButtonPressStatusLast = emButtonPressStatus_IsNotPressed,
	.ulPressedCount		= 0,
	.ulPressedCountThreshold = 5,
	.vpfButtonCallBack 		= vButtonCallBack_Toggle,
};

/* UI 按钮指针数组: 菜单项个数以这里为准 (sizeof 整体 / sizeof 单个元素) */
static stButtonTdf* s_apstButton[] =
{
	&s_stButton_On,
	&s_stButton_Off,
	&s_stButton_Toggle,
};


/* ---- 单行文本 (非交互, 纯显示) ---- */

static uint8_t s_aucLed0 [] = "Led 0";

static stSingleLineTxetTdf	stSingleLineTxetLed0 =
{
	//居中公式: 屏幕 128 中心 - 文本一半宽度; 5 字符 * 字宽 6 / 2 = 15
	.x = 128 / 2 - (sizeof(s_aucLed0) - 1) * (emOLedFontSize_6x12 / 2) / 2,
	//   = 64 - 5 * 6 / 2 = 49 ✓ 居中
	.y = 0,
	.ucData = s_aucLed0,
	.emOLedFontSize = emOLedFontSize_8x16,
};

static stSingleLineTxetTdf* s_apstSingleLineText[] =
{
	&stSingleLineTxetLed0,
};


/* ============================ ③ 绘制逻辑 ============================ */

/// @brief 把所有单行文本写进显存
static void s_vShowLineTextAll(void)
{
	uint8_t i;

	for ( i = 0; i < sizeof(s_apstSingleLineText) / sizeof(s_apstSingleLineText[0]); i++)
	{
		vOLedWriteStringToBuffer(s_apstSingleLineText[i]->x,s_apstSingleLineText[i]->y,s_apstSingleLineText[i]->ucData,s_apstSingleLineText[i]->emOLedFontSize
								,emOLedPixelShowMode_Positive,emOLedDevNum0);
	}

}

/// @brief 把所有按钮写进显存: 文字 + 选中方框
static void s_vShowButtonAll()
{

	uint8_t	i;

	for (i = 0; i < sizeof(s_apstButton) / sizeof(s_apstButton[0]); i++)
	{


		/* 1. 写按钮文字: 按下反色 (白底黑字), 未按下正色 */
		if (s_apstButton[i]->emButtonPressStatusCurrent == emButtonPressStatus_IsPressed)
		{
			vOLedWriteStringToBuffer(s_apstButton[i]->x,s_apstButton[i]->y
									,s_apstButton[i]->ucData
									,s_apstButton[i]->emOLedFontSize
									,emOLedPixelShowMode_Negative,emOLedDevNum0);
		}
		if (s_apstButton[i]->emButtonPressStatusCurrent == emButtonPressStatus_IsNotPressed)
		{
			vOLedWriteStringToBuffer(s_apstButton[i]->x,s_apstButton[i]->y
									,s_apstButton[i]->ucData
									,s_apstButton[i]->emOLedFontSize
									,emOLedPixelShowMode_Positive,emOLedDevNum0);
		}


		/* 2. 选中的按钮画方框: 4 条线拼一个矩形
		 * 文本像素宽 w = 字符数 * (字高 / 2);  ±2 是留白边距, 让字不贴着框线
		 *
		 *   (x-2,y-2) ────────── (x+2+w, y-2)
		 *      │      "On"          │
		 *   (x-2,y+2+h) ──────── (x+2+w, y+2+h)
		 */
		if(s_apstButton[i]->emButtonSelectStatus == emButtonSelectStatus_IsSelected)
		{
				//左竖线
				vOLedDrawLineToBuffer(s_apstButton[i]->x-2,s_apstButton[i]->y-2,s_apstButton[i]->x-2,s_apstButton[i]->y+2 + s_apstButton[i]->emOLedFontSize,emOLedDevNum0);
				//上横线
				vOLedDrawLineToBuffer(s_apstButton[i]->x-2,s_apstButton[i]->y-2,s_apstButton[i]->x+2 + strlen((const char*)(s_apstButton[i]->ucData))* (s_apstButton[i]->emOLedFontSize / 2),s_apstButton[i]->y-2,emOLedDevNum0);

				//右竖线
				vOLedDrawLineToBuffer(s_apstButton[i]->x+2 + strlen((const char*)(s_apstButton[i]->ucData))* (s_apstButton[i]->emOLedFontSize / 2),s_apstButton[i]->y-2
										,s_apstButton[i]->x+2 + strlen((const char*)(s_apstButton[i]->ucData))* (s_apstButton[i]->emOLedFontSize / 2)
										,s_apstButton[i]->y+2 + s_apstButton[i]->emOLedFontSize,emOLedDevNum0);

				//下横线
				vOLedDrawLineToBuffer(s_apstButton[i]->x-2,s_apstButton[i]->y+2 + s_apstButton[i]->emOLedFontSize,s_apstButton[i]->x+2 + strlen((const char*)(s_apstButton[i]->ucData)) * (s_apstButton[i]->emOLedFontSize / 2),
									  s_apstButton[i]->y+2+ s_apstButton[i]->emOLedFontSize,emOLedDevNum0);
		}


	}

}


/* ============================ ④ 按键逻辑 ============================ */

/// @brief 	按钮触发扫描 (每个按钮每帧调一次)
/// @param   按钮实例首地址
///@return	 本帧是否检测到 "新按下"
/// @note 	三步:
/// 		1. 边沿检测: 当前按下 && 上次未按下 = 触发一次 (只在按下瞬间成立);
/// 		2. 保持计数: 按住期间每帧 +1, 超过阈值自动复位按下状态 (模拟松手);
/// 		3. 滚动更新 Last, 供下一帧做边沿比较
emButtonTriggerStatusTdf emButtonTriggerStatusScan(stButtonTdf* pstButton)
{
	emButtonTriggerStatusTdf emButtonTriggerStatus = emButtonTriggerStatus_IsNotTrigger;

	//1. 边沿检测: 只有 "从松开到按下" 的这一帧才算触发
	if (pstButton->emButtonPressStatusCurrent  && (pstButton->emButtonPressStatusLast == emButtonPressStatus_IsNotPressed ) )
	{
		pstButton->ulPressedCount = 0;
		emButtonTriggerStatus = emButtonTriggerStatus_IsTrigger;
	}

	//2. 按住期间计数, 超过阈值自动复位 (否则 LED 逻辑会一直认为按着)
	pstButton->ulPressedCount++;
	if (pstButton->ulPressedCount >= pstButton->ulPressedCountThreshold)
	{
		pstButton->ulPressedCount = 0;
		pstButton->emButtonPressStatusCurrent = emButtonPressStatus_IsNotPressed;
	}

	//3. 保存为本帧状态, 下一帧的 "上次状态"
	pstButton->emButtonPressStatusLast = pstButton->emButtonPressStatusCurrent;

	return emButtonTriggerStatus;

}


/// @brief On 按钮回调: 触发时点亮 LED
void vButtonCallBack_On()
{
	if (emButtonTriggerStatusScan(&s_stButton_On) == emButtonTriggerStatus_IsTrigger)
	{
		Led_on(emLedDevNum0);
	}

}


/// @brief Off 按钮回调: 触发时熄灭 LED
void vButtonCallBack_Off()
{
	if (emButtonTriggerStatusScan(&s_stButton_Off) == emButtonTriggerStatus_IsTrigger)
	{
		Led_off(emLedDevNum0);
	}

}


/// @brief Toggle 按钮回调: 触发时翻转 LED
void vButtonCallBack_Toggle()
{
	if (emButtonTriggerStatusScan(&s_stButton_Toggle) == emButtonTriggerStatus_IsTrigger)
	{
		LedToggle(emLedDevNum0);
	}

}


/// @brief 遍历执行所有按钮的回调 (触发扫描在回调内部进行)
void	vButtonCallbackExecuteAll()
{
		uint16_t i;
		for(i = 0;i< sizeof(s_apstButton) / sizeof(s_apstButton[0]); i++)
		{
				if(s_apstButton[i]->vpfButtonCallBack == 0)
				{
					continue;		//回调为空 (未注册) 的按钮跳过
				}

				s_apstButton[i]->vpfButtonCallBack();
		}

}


/* ---- 硬件设备初始化 (只在上电时调一次) ---- */

/// @brief OLED 设备注册: 软件 I2C 引脚 SCL=PB7, SDA=PB6
void vOLedInit()
{
	stOLedStaticParamTdf stOLedStaticInit;
	stOLedStaticInit.pstSclGpioBase 	= GPIOB;
	stOLedStaticInit.ulSclGpioPin 		= GPIO_PIN_7;
	stOLedStaticInit.pstSdaGpioBase		= GPIOB;
	stOLedStaticInit.ulSdaGpioPin			= GPIO_PIN_6;

	vOLedDeviceInit(&stOLedStaticInit,emOLedDevNum0);

}


/// @brief LED 设备注册: PC13, 低电平点亮
void vLedInit()
{
	stLedStaticParamTdf	stLedStaticInit;

	stLedStaticInit.pstGpioBase  = GPIOC;
	stLedStaticInit.usGpioPin    = GPIO_PIN_13;
	stLedStaticInit.emLedOnLevel = emLedOnLevel_Low;

	vLedDeviceInit(&stLedStaticInit,emLedDevNum0);

}


/// @brief 物理按键注册: 4 个键 (UP/DOWN/ENTER/CANCEL), 低电平有效 + 消抖
void vPhyButtonInit()
{
	stBtnStaticParamTdf	stBtnStaticInit;
	stBtnStaticInit.pstGpioBase = GPIOA;
	stBtnStaticInit.usGpioPin	= GPIO_PIN_1;
	stBtnStaticInit.emActiveLevel	= emBtnActiveLevel_Low;
	stBtnStaticInit.ulDebounceCountThreshold	= 2;
	stBtnStaticInit.ulDoubleClickInterval 		= 0;
	stBtnStaticInit.ulLongPressCountThreshold	= 100000;

	emBtnDeviceInit(&stBtnStaticInit,BUTTON_UP);

	stBtnStaticInit.pstGpioBase = GPIOA;
	stBtnStaticInit.usGpioPin	= GPIO_PIN_0;
	stBtnStaticInit.emActiveLevel	= emBtnActiveLevel_Low;
	stBtnStaticInit.ulDebounceCountThreshold	= 2;
	stBtnStaticInit.ulDoubleClickInterval 		= 0;
	stBtnStaticInit.ulLongPressCountThreshold	= 100000;

	emBtnDeviceInit(&stBtnStaticInit,BUTTON_DOWN);

	stBtnStaticInit.pstGpioBase = GPIOC;
	stBtnStaticInit.usGpioPin	= GPIO_PIN_15;
	stBtnStaticInit.emActiveLevel	= emBtnActiveLevel_Low;
	stBtnStaticInit.ulDebounceCountThreshold	= 2;
	stBtnStaticInit.ulDoubleClickInterval 		= 0;
	stBtnStaticInit.ulLongPressCountThreshold	= 100000;

	emBtnDeviceInit(&stBtnStaticInit,BUTTON_ENTER);

	stBtnStaticInit.pstGpioBase = GPIOC;
	stBtnStaticInit.usGpioPin	= GPIO_PIN_14;
	stBtnStaticInit.emActiveLevel	= emBtnActiveLevel_Low;
	stBtnStaticInit.ulDebounceCountThreshold	= 2;
	stBtnStaticInit.ulDoubleClickInterval 		= 0;
	stBtnStaticInit.ulLongPressCountThreshold	= 100000;

	emBtnDeviceInit(&stBtnStaticInit,BUTTON_CANCEL);

}


/// @brief 物理按键每帧处理: 扫描 -> 取事件 -> 切换选中 / 触发按下
void vPhyButtonExecuteAll()
{

	uint8_t i = 0;
	static uint8_t ucSelectIndex = 0;	//当前选中的菜单项下标 (跨帧保持)
	//1. 执行一次物理按键扫描 (消抖 + 事件判定)
	for (i = 0; i < BTN_DEV_NUM; i++)
	{
		 vBtnExecute((emBtnDevNumTdf)i);
	}


	//2. 取事件并执行对应操作
	//2.1【UP】键: 选中上移一项
	if (emBtnGetEvent(BUTTON_UP) == emBtnEvent_Click)
	{
		vBtnClearEvent(BUTTON_UP);
		//先取消旧的选中
		s_apstButton[ucSelectIndex]->emButtonSelectStatus = emButtonSelectStatus_IsNotSelected;

		ucSelectIndex--;
		//2.1.1 边界处理: ucSelectIndex 是 uint8_t, 0 再减会回绕成 255
		//     无符号数 + 一条上边界 = 上溢和下溢一起拦住
		//     注意: 边界用 UI 按钮个数 (3), 不是 BTN_DEV_NUM (物理键 4 个)!
		if (ucSelectIndex >=  sizeof(s_apstButton) / sizeof(s_apstButton[0]) )
		{
			ucSelectIndex =  sizeof(s_apstButton) / sizeof(s_apstButton[0]) - 1;	//拉回最后一个下标, 停边
		}
		//再点亮新的选中
		s_apstButton[ucSelectIndex]->emButtonSelectStatus = emButtonSelectStatus_IsSelected;
	}



	//2.2【DOWN】键: 选中下移一项
	if (emBtnGetEvent(BUTTON_DOWN) == emBtnEvent_Click)
	{
		vBtnClearEvent(BUTTON_DOWN);
		s_apstButton[ucSelectIndex]->emButtonSelectStatus = emButtonSelectStatus_IsNotSelected;

		ucSelectIndex++;
		//下溢拉回 0, 循环菜单
		if (ucSelectIndex >= sizeof(s_apstButton) / sizeof(s_apstButton[0]))
		{
			 ucSelectIndex = 0;
		}
		s_apstButton[ucSelectIndex]->emButtonSelectStatus = emButtonSelectStatus_IsSelected;
	}

	//2.3【ENTER】键: 把当前选中按钮置为按下 (回调里扫描到后执行动作)
	if (emBtnGetEvent(BUTTON_ENTER) == emBtnEvent_Click)
	{
		vBtnClearEvent(BUTTON_ENTER);

		s_apstButton[ucSelectIndex]->emButtonPressStatusCurrent = emButtonPressStatus_IsPressed;
	}


}


/* ============================ ⑤ 主循环 ============================ */

/// @brief 一帧的完整流程
/// @note  顺序有讲究: 先处理按键 (改状态), 再画按钮,
///        这样按下/切换选中当帧就能看到, 不用等下一帧
void vTest(void)
{

	vOLedClearBufferToBuffer(emOLedDevNum0);	//1. 清空显存 (全量重绘的地基)
	s_vShowButtonAll();							//2. 画按钮 (用第 3/4 步更新后的状态)
	s_vShowLineTextAll();						//3. 画单行文本

	vButtonCallbackExecuteAll();				//4. 执行 UI 按钮回调 (扫描触发)
	vPhyButtonExecuteAll();						//5. 处理物理按键 (切换选中/按下)

 

	vOLedRefreshFromBuffer(emOLedDevNum0);		//6. 整帧刷到屏幕 (每帧只刷一次)
}


/// @brief UI 任务入口: 初始化三件设备, 然后每 1 tick 重画一帧
void vUiTask()
{

		vOLedInit();
		vLedInit();
		vPhyButtonInit();
		while(1)
		{
			vTest();
			vTaskDelay(1);
		}

}

