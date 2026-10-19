/**
 *
 *
 * @file       Ui_Task.c
 * @author     [Ns单片机实验室--yzq]
 * @version    v1.0.01
 * @date       2026-10-18
 * @brief      Ui任务
 *

 *
 /// @line STM32单片机开发 

 *  <h2><center>&copy;文件版权归属【Ns单片机实验室】所有.</center></h2>
 */
 

#include "FreeRtos.h"
#include "task.h"
#include "Ui_Task.h"
#include "oled_device.h"

static uint8_t s_aucHello [] = "Hello world!!!!";
static uint8_t s_aucLetter[] = "ABCDEFJHI";
static uint8_t s_aucNUmber[] = "1234";


static stSingleLineTxetTdf	stSingleLineTxetHello = 
{
	.x = 0,
	.y = 0,
	.ucData = s_aucHello,
	.emOLedFontSize = emOLedFontSize_6x12 
};

static stSingleLineTxetTdf stSingleLineTextLetter = 
{
	.x = 0,
	.y = 20,
	.ucData = s_aucLetter,
	.emOLedFontSize = emOLedFontSize_8x16

};

static stSingleLineTxetTdf stSingleLineTxetNumber = 
{
	.x = 20,
	.y = 40,
	.ucData = s_aucNUmber,
	.emOLedFontSize = emOLedFontSize_12x24
};

/// @brief 单行文本指针数组  		 
/// @param   
/// @note 
static stSingleLineTxetTdf* s_apstSingleLineText[] = 
{
	&stSingleLineTxetHello,
	// &stSingleLineTextLetter,
	&stSingleLineTxetNumber

};


/// @brief test 测试函数   		 
/// @param   
/// @note 

void vTest(void)
{
	//局部变量
	uint8_t i;

	for ( i = 0; i < sizeof(s_apstSingleLineText) / sizeof(s_apstSingleLineText[0]); i++)
	{
		vOLedWriteStringToBuffer(s_apstSingleLineText[i]->x,s_apstSingleLineText[i]->y,s_apstSingleLineText[i]->ucData,s_apstSingleLineText[i]->emOLedFontSize
								,emOLedPixelShowMode_Positive,emOLedDevNum0); 	
	}

	vOLedRefreshFromBuffer(emOLedDevNum0);
}


/// @brief   		 
/// @param   
/// @note 
void vOLedInit()
{
	stOLedStaticParamTdf stOLedStaticInit;
	stOLedStaticInit.pstSclGpioBase 	= GPIOB;
	stOLedStaticInit.ulSclGpioPin 		= GPIO_PIN_7;
	stOLedStaticInit.pstSdaGpioBase		= GPIOB;
	stOLedStaticInit.ulSdaGpioPin			= GPIO_PIN_6;
	
	vOLedDeviceInit(&stOLedStaticInit,emOLedDevNum0);

}




/// @brief  UI任务 		 
/// @param   
/// @note    
void vUiTask()
{
	
		vOLedInit();
	
		while(1)
		{
			vTest();
			vTaskDelay(1);
		}

}


 

