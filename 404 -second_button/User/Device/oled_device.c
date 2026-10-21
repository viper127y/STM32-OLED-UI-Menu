/**
 *
 *
 * @file       oled_device.c
 * @author     [Ns单片机实验室--yzq]
 * @version    v1.0.01
 * @date       2026-10-13
 * @brief      OLED 显示驱动（软件模拟I2C）
 *
 * 调用链总览（写一个字节命令为例）：
 *   应用层 → s_vOledWriteOneByteCmd()
 *            ├─ s_vOledI2cSatrt()      ─┐
 *            ├─ s_vOledI2cSendByte()   ─┤ 时序层：反复调用引脚层
 *            ├─ s_vOledI2cAck()         │ s_vOledSclSet/Reset()
 *            └─ s_vOledI2cStop()       ─┘ s_vOledSdaSet/Reset()
 *
 * 数据流：画图函数 → 显存 aucOledBuffer（RAM，快）→ vOLedRefreshFromBuffer
 *         → I2C 突发写 → SSD1306 GRAM → 屏幕显示
 *
 /// @line STM32单片机开发架构

 *  <h2><center>&copy;文件版权归属【Ns单片机实验室】所有.</center></h2>
 */
#include "oled_device.h"


/********************** 库函数 ****************/
#include "string.h"


/// @line STM32单片机开发架构

/// @brief  设备管理结构体数组
/// @param  astOLedDeviceParam[OLED_DEV_NUM]  OLED 设备管理结构体数组
/// @note    OLED 设备管理结构体数组 ( 0号为主OLED 1-8号为从OLED )
///          静态数组实现"设备池"：下标即设备号，无需 malloc
///
stOLedDeviceParamTdf astOLedDeviceParam[OLED_DEV_NUM];



/* ================================================================
 *                          引脚操作层
 *  BSRR 原理（32位寄存器一分为二）：
 *    低16位 BS ：某位写1 → 对应引脚 SET（拉高），写0无影响
 *    高16位 BR ：某位写1 → 对应引脚 RESET（拉低），写0无影响
 *  所以：直接写引脚掩码 = 拉高；掩码 <<16u = 搬到高半场 = 拉低。
 *  一次赋值只动目标引脚、原子操作，比读改写 ODR 安全且快。
 * ================================================================ */

const stOLedDeviceParamTdf* c_pstGetOLedDeviceParam(emOLedDeviceNumTdf DevNum)
{
	 return &astOLedDeviceParam[DevNum];
}


/// @brief  模拟I2C的SDA拉高
/// @param  emDevNum 设备号
/// @note   通过静态参数里注册的引脚操作
///         当前用 HAL 版（可读性好）；OLED 整帧刷新超 10ms 时
///         换下方寄存器版提速（BSRR 低16位写1 = SET）
static void s_vOledSdaSet(emOLedDeviceNumTdf emDevNum)
{
    HAL_GPIO_WritePin(astOLedDeviceParam[emDevNum].stStaticParam.pstSdaGpioBase,
                      astOLedDeviceParam[emDevNum].stStaticParam.ulSdaGpioPin,
                      GPIO_PIN_SET);

//	 寄存器版（等价，更快）：
//	 astOLedDeviceParam[emDevNum].stStaticParam.pstSdaGpioBase->BSRR =  astOLedDeviceParam[emDevNum].stStaticParam.ulSdaGpioPin;

}

/// @brief  模拟I2C的SDA拉低
/// @param  emDevNum 设备号
/// @note   寄存器版 = 掩码 <<16u 进 BSRR 高16位（BR区）= RESET
static void s_vOledSdaReset(emOLedDeviceNumTdf emDevNum)
{
    HAL_GPIO_WritePin(astOLedDeviceParam[emDevNum].stStaticParam.pstSdaGpioBase,
                      astOLedDeviceParam[emDevNum].stStaticParam.ulSdaGpioPin,
                      GPIO_PIN_RESET);

//	 寄存器版（等价，更快）：
//	 astOLedDeviceParam[emDevNum].stStaticParam.pstSdaGpioBase->BSRR =   (uint32_t)astOLedDeviceParam[emDevNum].stStaticParam.ulSdaGpioPin << 16u;

}


/// @brief  模拟I2C的SCL拉高
/// @param  emDevNum 设备号
/// @note   寄存器直写版：BSRR 低16位写1 → 引脚 SET（拉高）
static void s_vOledSclSet(emOLedDeviceNumTdf emDevNum)
{
	 astOLedDeviceParam[emDevNum].stStaticParam.pstSclGpioBase->BSRR =  astOLedDeviceParam[emDevNum].stStaticParam.ulSclGpioPin;

}

/// @brief  模拟I2C的SCL拉低
/// @param  emDevNum 设备号
/// @note   寄存器直写版：掩码 <<16u 进 BSRR 高16位（BR区）→ 引脚 RESET（拉低）
static void s_vOledSclReset(emOLedDeviceNumTdf emDevNum)
{
	astOLedDeviceParam[emDevNum].stStaticParam.pstSclGpioBase->BSRR =  (uint32_t)astOLedDeviceParam[emDevNum].stStaticParam.ulSclGpioPin << 16u;

}


/* ================================================================
 *                        I2C 时序层
 *  核心规则：SDA 变化只能发生在 SCL 低电平期间；
 *            SCL 高电平期间 SDA 必须稳定（从机采样时刻）。
 *  例外：起始/停止条件——恰好是 SCL 高电平期间让 SDA 跳变。
 * ================================================================ */


/// @brief  I2C起始条件
/// @param  emDevNum 设备号
/// @note   口诀：SCL高电平期间，SDA由高变低
static void s_vOledI2cSatrt(emOLedDeviceNumTdf emDevNum)
{
    //1.SCL、SDA都置高（确保总线处于空闲态）
    s_vOledSclSet(emDevNum);
    s_vOledSdaSet(emDevNum);
    //2.SDA下跳，产生起始条件
    s_vOledSdaReset(emDevNum);
    //3.SCL拉低，准备发送第一位
    s_vOledSclReset(emDevNum);
}


/// @brief  I2C停止条件
/// @param  emDevNum 设备号
/// @note   口诀：SCL高电平期间，SDA由低变高
static void s_vOledI2cStop(emOLedDeviceNumTdf emDevNum)
{
    //1.SCL拉高，SDA先拉低（为上跳做准备）
    s_vOledSclSet(emDevNum);
    s_vOledSdaReset(emDevNum);
    //2.SDA上跳，产生停止条件，总线回到空闲态
    s_vOledSdaSet(emDevNum);

}


/// @brief  I2C第9个应答时钟（不校验ACK）
/// @param  emDevNum 设备号
/// @note   写OLED不需要判断从机应答，但必须先释放SDA给从机让位
static void s_vOledI2cAck(emOLedDeviceNumTdf emDevNum)
{
    //1.释放SDA（开漏下靠上拉变高），主机不再驱动总线，从机才有机会拉低应答
    s_vOledSdaSet(emDevNum);
    //2.打第9个时钟脉冲，高电平期间从机拉低SDA表示ACK
    s_vOledSclSet(emDevNum);
    s_vOledSclReset(emDevNum);

}


/// @brief  I2C发送一个字节（MSB高位先行）
/// @param  ucData   要发送的字节
/// @param  emDevNum 设备号
/// @note   基本节奏：SCL低电平期间改SDA，SCL高电平期间从机采样
static void s_vOledI2cSendByte(uint8_t ucData,emOLedDeviceNumTdf emDevNum)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        //1.SCL拉低，允许改变SDA
        s_vOledSclReset(emDevNum);
        //2.把当前最高位放到SDA上（高位先发，掩码用0x80）
        if ((ucData & 0x80) != 0)
        {
            s_vOledSdaSet(emDevNum);
        }
        else
        {
            s_vOledSdaReset(emDevNum);
        }
        //3.SCL拉高，从机在此采样
        s_vOledSclSet(emDevNum);
        s_vOledSclReset(emDevNum);
        //4.左移一位，准备发送下一位，高位先发
        ucData <<= 1;
    }

}


/* ================================================================
 *                        OLED 命令层
 *  I2C 字节流格式（SSD1306 规定）：
 *    Start → 从机地址(0x78=0111100+写位0) → 控制字节 → 数据... → Stop
 *  控制字节只有 bit7(Co) 和 bit6(D/C#) 有效：
 *    0x00 = Co=0 D/C#=0 → 后面全是命令流
 *    0x40 = Co=0 D/C#=1 → 后面全是显存数据流
 *  注意：控制字节低6位是忽略位，写 0x0C 碰巧等效 0x00，
 *        但那是埋雷写法，统一写规范值 0x00/0x40
 * ================================================================ */

/// @brief  OLED写一个字节命令
/// @param  ucCmd    命令字节
/// @param  emDevNum 设备号
/// @note   字节流：Start → 从机地址0x78(写) → 控制字节0x00(命令流) → 命令 → Stop
static void s_vOledWriteOneByteCmd(uint8_t ucCmd,emOLedDeviceNumTdf emDevNum)
{

    //1.起始条件
    s_vOledI2cSatrt(emDevNum);
    //2.发从机地址+写位：0x78 = 0111100(地址) + 0(写)
    s_vOledI2cSendByte(0x78,emDevNum);
    s_vOledI2cAck(emDevNum);
    //3.发控制字节：0x00表示后面跟的是命令
    s_vOledI2cSendByte(0x00,emDevNum);
    s_vOledI2cAck(emDevNum);
    //4.发命令字节
    s_vOledI2cSendByte(ucCmd,emDevNum);
    s_vOledI2cAck(emDevNum);
    //5.停止条件
    s_vOledI2cStop(emDevNum);

}

/// @brief  OLED连续发送显存数据
/// @param  pucData  数据首地址
/// @param  ulLength 数据字节数
/// @param  emDevNum 设备号
/// @note   字节流：Start → 地址0x78 → 控制字节0x40(数据流) → 数据0..n → Stop
///         突发写：多个数据连续发，中间不打Stop，效率高
static void s_vOledWriteDataStream(uint8_t* pucData,uint32_t ulLength,emOLedDeviceNumTdf emDevNum)
{

    uint32_t i;
    //1.起始条件
    s_vOledI2cSatrt(emDevNum);
    //2.发从机地址+写位
    s_vOledI2cSendByte(0x78,emDevNum);
    s_vOledI2cAck(emDevNum);
    //3.发控制字节：0x40表示后面跟的是显存数据
    s_vOledI2cSendByte(0x40,emDevNum);
    s_vOledI2cAck(emDevNum);
    //4.循环发送数据，每个字节后跟一个应答时钟
    for (i = 0; i < ulLength; i++)
    {
        s_vOledI2cSendByte(*(pucData + i),emDevNum);
        s_vOledI2cAck(emDevNum);
    }
    //5.停止条件
    s_vOledI2cStop(emDevNum);

}


/* ================================================================
 *                        显存操作层
 *  坐标 → 显存下标的换算（页结构）：
 *    页号   = y / 8（用 y>>3 代替除法，快）
 *    页内位 = y % 8（用 y&0x07 代替取余）
 *    位掩码 = 0x01 << 页内位（段内 D0~D7 从上往下对应 8 行像素）
 *  显存下标 = 页号 * 128 + 列号（显存按页分块，每页 128 字节）
 *  点亮/熄灭都是"读-改-写"：只能动目标位，不能碰邻居位
 * ================================================================ */

/// @brief  清空整块显存（全灭，不刷屏）
/// @param  emDevNum 设备号
/// @note   每帧绘制前调用：把 RAM 显存整块清 0，保证每帧从"白纸"起步。
///         这是"每帧全量重绘"架构的地基——上一帧的残留（如取消选中的线）
///         不会被带进下一帧，彻底消灭闪动、残影、遗留像素这类问题
void vOLedClearBufferToBuffer(emOLedDeviceNumTdf emDevNum)
{

	memset(astOLedDeviceParam[emDevNum].stRuningParam.aucOledBuffer, 0, sizeof(astOLedDeviceParam[emDevNum].stRuningParam.aucOledBuffer));
}



/// @brief  指定坐标画点（写显存，不刷屏）
/// @param  x 列号 0~127    y 行号 0~63
/// @param  emDevNum 设备号
/// @note   步骤：算页号 → 算页内位 → 拼掩码 → 读改写置1
void vOLedDrawOnePointToBuffer(uint32_t x,uint32_t y,emOLedDeviceNumTdf emDevNum)
{

	uint16_t ii,jj,kk;

	ii = y >> 3;            //页号 = y/8：该像素落在第几页（第几个显存字节）

	jj = y & 0x07;          //页内位 = y%8：页内竖着的第几位

	kk = 0x01 << jj;        //位掩码：只有目标位是1

	astOLedDeviceParam[emDevNum].stRuningParam.aucOledBuffer[ii * OLED_BUFFER_WIDTH + x] |= kk;   //读-改-写：或1点亮，不影响同字节其他7个像素
}



/// @brief  指定坐标清除点（写显存，不刷屏）
/// @param  x 列号 0~127    y 行号 0~63
/// @param  emDevNum 设备号
/// @note   与画点同一套坐标换算，区别只是"与上取反掩码"清0
void vOLedClearOnePointToBuffer(uint32_t x,uint32_t y,emOLedDeviceNumTdf emDevNum)
{

	uint16_t ii,jj,kk;

	ii = y >> 3;            //页号 = y/8

	jj = y & 0x07;          //页内位 = y%8

	kk = 0x01 << jj;        //位掩码

	astOLedDeviceParam[emDevNum].stRuningParam.aucOledBuffer[ii * OLED_BUFFER_WIDTH + x] &= ~kk;   //读-改-写：与0熄灭，不影响同字节其他7个像素
}



/// @brief  指定坐标翻转点（写显存，不刷屏）
/// @param  x 列号 0~127    y 行号 0~63
/// @param  emDevNum 设备号
/// @note   与画点/清点同一套坐标换算，区别是"异或掩码"：亮变灭、灭变亮。
///         异或是"反色不覆盖"的核心：先画正色文本，再翻转区域，
///         文本像素只是黑白互换，不会被抹掉（选中高亮的原理）
void vOLedReverseOnePointToBuffer(uint32_t x,uint32_t y,emOLedDeviceNumTdf emDevNum)
{

	uint16_t ii,jj,kk;

	ii = y >> 3;            //页号 = y/8

	jj = y & 0x07;          //页内位 = y%8

	kk = 0x01 << jj;        //位掩码

	astOLedDeviceParam[emDevNum].stRuningParam.aucOledBuffer[ii * OLED_BUFFER_WIDTH + x] ^= kk;   //读-改-写：异或翻转目标位，不影响同字节其他7个像素
}



/// @brief  指定区域画反色实心矩形（写显存，不刷屏）
/// @param  x,y       矩形左上角像素坐标
/// @param  ulWidth   矩形宽（像素），画正方形时传 宽=高
/// @param  ulHeight  矩形高（像素）
/// @param  emDevNum  设备号
/// @note   用法：先写正色文本，再对本区域翻转 → 文字反色且不被覆盖。
///         逐点调用"翻转点"原语（异或），区域外的像素一律不碰；
///         越界部分自动裁剪，坐标先转有符号数防止无符号减法下溢
void vOLedReverseRectangleToBuffer(uint32_t x,uint32_t y,uint32_t ulWidth,uint32_t ulHeight,emOLedDeviceNumTdf emDevNum)
{

	int32_t i,j;

	//1.先裁剪出屏幕内的有效区域（无符号数直接减会下溢成大数，先转有符号）
	int32_t lXEnd = (int32_t)x + (int32_t)ulWidth - 1;      //矩形最右一列
	int32_t lYEnd = (int32_t)y + (int32_t)ulHeight - 1;     //矩形最下一行
	int32_t lXStart = (int32_t)x;
	int32_t lYStart = (int32_t)y;

	if (lXEnd >= OLED_POINT_WIDTH)      //右边界超出屏幕：裁到最后一列
	{
		lXEnd = OLED_POINT_WIDTH - 1;
	}
	if (lYEnd >= OLED_POINT_HEIGHT)     //下边界超出屏幕：裁到最后一行
	{
		lYEnd = OLED_POINT_HEIGHT - 1;
	}

	//2.整个矩形落在屏幕外：无事可做，直接返回
	if ((lXStart > lXEnd) || (lYStart > lYEnd))
	{
		return;
	}

	//3.逐行逐列翻转区域内的每个像素
	for (j = lYStart; j <= lYEnd; j++)
	{
		for (i = lXStart; i <= lXEnd; i++)
		{
			vOLedReverseOnePointToBuffer(i,j,emDevNum);
		}
	}
}



/// @brief  两点之间画直线（写显存，不刷屏）
/// @param  x0,y0     起点坐标
/// @param  x1,y1     终点坐标
/// @param  emDevNum  设备号
/// @note   Bresenham 算法：全程整数加减，不用乘除和浮点，单片机友好。
///         思路：每走一步累计误差，误差过阈值就往另一轴挪一步，
///         保证像素始终贴着理想直线走。画文字下划线：
///         起点(x, y+字高) 终点(x+文本宽, y+字高)，两点等高即是水平线
void vOLedDrawLineToBuffer(uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,emOLedDeviceNumTdf emDevNum)
{

	int32_t lDx , lDy , lSx , lSy , lErr , lErr2;

	//1.端点都落在屏幕外直接返回（两点在屏内，中间点必然也在屏内，直线不会越界）
	if ((x0 >= OLED_POINT_WIDTH) || (x1 >= OLED_POINT_WIDTH) ||
		(y0 >= OLED_POINT_HEIGHT) || (y1 >= OLED_POINT_HEIGHT))
	{
		return;
	}

	//2.算轴向步进：x/y 各自的差值和方向（-1 或 +1）
	lDx = (int32_t)x1 - (int32_t)x0;
	lDy = (int32_t)y1 - (int32_t)y0;
	lSx = (lDx >= 0) ? 1 : -1;                      //x 步进方向
	lSy = (lDy >= 0) ? 1 : -1;                      //y 步进方向
	lDx = (lDx >= 0) ? lDx : -lDx;                  //取绝对值 |Δx|
	lDy = (lDy >= 0) ? lDy : -lDy;                  //取绝对值 |Δy|

	//3.误差初值：每走一步 x 累计 -Δy、走一步 y 累计 +Δx，哪个先越界就往哪轴走
	lErr = lDx - lDy;

	//4.主循环：从起点逐像素走到终点
	while (1)
	{
		vOLedDrawOnePointToBuffer(x0,y0,emDevNum);      //点亮当前像素

		if ((x0 == x1) && (y0 == y1))                   //走到终点，结束
		{
			break;
		}

		lErr2 = lErr << 1;                              //误差翻倍后判断该拐弯没有

		if (lErr2 > -lDy)                               //误差偏向 x 侧：x 前进一步
		{
			lErr -= lDy;
			x0 += lSx;
		}
		if (lErr2 < lDx)                                //误差偏向 y 侧：y 前进一步
		{
			lErr += lDx;
			y0 += lSy;
		}
	}
}



/// @brief  把显存整帧刷新到屏幕
/// @param  emDevNum 设备号
/// @note   页寻址模式刷屏循环（每页三步）：
///         ① 0xB0+i 设页地址：命令码 0xB0 的低3位 = 页号，一条搞定
///         ② 0x00/0x10 归零列地址：低4位命令+高4位命令，数据都是0
///            （每页写满128字节后列指针绕回本页起始列，页指针不会自动跳页，
///             所以换页必须手动把列扳回0，否则画面错位）
///         ③ 突发灌128字节数据：本页显存 &buf[i*128]，一次 I2C 事务发完
void vOLedRefreshFromBuffer(emOLedDeviceNumTdf emDevNum)
{

    uint8_t i;
    for (i = 0; i < OLED_BUFFER_HEIGHT; i++)       //OLED_BUFFER_HEIGHT = 8 页
    {
        //设置行起始地址（页地址，低3位=页号）
        s_vOledWriteOneByteCmd(0xB0 + i,emDevNum);
        //设置列起始地址低4位（=0）
        s_vOledWriteOneByteCmd(0x00 , emDevNum);
        //设置列起始地址高4位（=0）
        s_vOledWriteOneByteCmd(0x10 , emDevNum);
        //写连续数据流：第 i 页显存块的 128 字节一次性突发发出
        s_vOledWriteDataStream(&astOLedDeviceParam[emDevNum].stRuningParam.aucOledBuffer[i * OLED_BUFFER_WIDTH],
                                OLED_BUFFER_WIDTH,emDevNum);   //aucOledBuffer[i * 128] 就是按"第 i 页"切块的
    }

}



/// @brief  写一个字符进显存（不刷屏）
/// @param  x,y        字符左上角像素坐标
/// @param  ucChar     要显示的 ASCII 字符（' '~'~'）
/// @param  emFontSize 字体枚举（值=字高，宽=高/2）
/// @param  Mode       显示模式：正常(字模1=亮) / 反色(字模1=灭)
/// @param  emDevNum   设备号
/// @note   思路：定位字模 → 逐字节取字模 → 逐位扫描 → 按模式画/清点
///         关键：字符是"竖着扫描"的——先写完一列（y 方向）再换下一列（x 方向）
void vOLedWriteOneCharToBuffer(uint32_t x,uint32_t y,uint8_t ucChar,emOLedFontSizeTdf emFontSize,emOLedPixelShowModeTdf Mode,emOLedDeviceNumTdf emDevNum)
{
    uint32_t i,j;
    uint16_t y0;
    uint32_t ulSizeTemp , ulCharIndex;
    const uint8_t* p;
    uint8_t ucData;

    //1.计算显示一个字符需要多少byte（字模数组里一个字符占的长度）
    //  公式：字节数 = (高/8 不足则进1) × 宽。如 8x16：(16/8)×8 = 16字节；
    //  12x12：高12 → (12/8=1 余4 → 2页) × 宽6 = 12字节
    ulSizeTemp = ((emFontSize >> 3) + ((emFontSize & 0x07 ) ? 1 : 0 ))* (emFontSize >> 1);

    //2.计算要显示的字符，在字模数组中的索引
    //  字库从空格' '开始排（前32个ASCII控制字符没有字模），所以减 ' '
    ulCharIndex = ucChar - ' ';

    //3.初始化y0坐标（记住列起点，写完一列后要回来）
    y0 = y;


    //4.1 根据字体选择字库（字模数组首地址）
    if (emFontSize == emOLedFontSize_6x12)
    {
        p = (const uint8_t*) c_a2ucOledFont_Ascii_1206;
    }
    else  if (emFontSize == emOLedFontSize_8x16)
    {
        p = (const uint8_t*) c_a2ucOledFont_Ascii_1608;
    }
    else  if (emFontSize == emOLedFontSize_12x24)
    {
        p = (const uint8_t*) c_a2ucOledFont_Ascii_2412;
    }
    else
    {
        return;     //未知字体直接退出，防止野指针
    }



    //4.2按字模将数据写入到显存
    for (i = 0; i < ulSizeTemp; i++)
    {
        //4.2.1 取字模的一个字节：字模首地址 + 字符索引×单字符长度 + 字节偏移
        ucData = p[ulCharIndex * ulSizeTemp + i];

        //4.2.2按位写入缓存（高位在前，掩码0x80）
        for (j = 0; j < 8; j++)
        {
            //字模位=1：正常模式画点 / 反色模式清点
            if ((ucData & 0x80) != 0)
            {
                if (Mode == emOLedPixelShowMode_Positive)
                    {
                        vOLedDrawOnePointToBuffer(x,y,emDevNum);
                    }
                else
                    {
                        vOLedClearOnePointToBuffer(x,y,emDevNum);
                    }
            }
            //字模位=0：正常模式清点 / 反色模式画点（两种模式正好相反）
            else
            {
                if (Mode == emOLedPixelShowMode_Negative)
                {
                    vOLedDrawOnePointToBuffer(x,y,emDevNum);
                }
                    else
                    {
                        vOLedClearOnePointToBuffer(x,y,emDevNum);
                    }
            }
            ucData <<= 1;       //移到下一位
            y++;                //竖着扫描：y 先往下走
            if ((y -y0) == emFontSize)
            {
                //此列的8位已经写完：y 回到列起点，x 换下一列
                //（字符是按列组织的：先写完一竖条，再写右边一竖条）
                y = y0;
                x++;
                break;
            }

        }
    }


}

/// @brief  写字符串进显存（不刷屏）
/// @param  x,y        首字符左上角坐标
/// @param  c_pucChar  字符串首地址（'\0' 结尾）
/// @param  emFontSize 字体枚举
/// @param  emMode     显示模式
/// @param  emDevNum   设备号
/// @note   步骤：逐字符调"写单字符" → x 步进一个字宽 → 越界自动换行
///         字符范围限定 ' '~'~'（可打印 ASCII），遇到其他字符即停止
void vOLedWriteStringToBuffer(uint16_t x, uint16_t y, const uint8_t *c_pucChar, emOLedFontSizeTdf emFontSize, emOLedPixelShowModeTdf emMode, emOLedDeviceNumTdf emDevNum)
{
    while((*c_pucChar >= ' ') && (*c_pucChar <= '~'))           // 限定字符范围
    {
         vOLedWriteOneCharToBuffer(x, y, *c_pucChar, emFontSize, emMode, emDevNum);

        x += emFontSize >> 1;    //步进一个字宽：宽度 = 字高的一半（emFontSize>>1）

        if(x > (OLED_POINT_WIDTH - emFontSize))              // 超出行宽：换行
        {
            x  = 0;
            y += emFontSize;     //y 下移一行字高
        }

        c_pucChar++;
    }
}


/// @brief  SSD1306 初始化命令序列（上电调用一次）
/// @param  emDevNum 设备号
/// @note   每条命令一个字节（参数命令 = 命令码+参数两条），
///         分组：基础显示 → 对比度/扫描方向 → 时序 → 布局 → 电荷泵 → 开屏
void vOledCmdInit(emOLedDeviceNumTdf emDevNum)
{
    /* ---- 基础显示设置 ---- */
    s_vOledWriteOneByteCmd(0xAE, emDevNum);//--turn off oled panel（关屏，配完再开）
    s_vOledWriteOneByteCmd(0x00, emDevNum);//--set low column address（列地址低4位=0）
    s_vOledWriteOneByteCmd(0x10, emDevNum);//--set high column address（列地址高4位=0）
    s_vOledWriteOneByteCmd(0x40, emDevNum);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)

    /* ---- 对比度 ---- */
    s_vOledWriteOneByteCmd(0x81, emDevNum);//--set contrast control register（双字节命令：下一条是参数）
    s_vOledWriteOneByteCmd(0xCF, emDevNum);// Set SEG Output Current Brightness

    /* ---- 扫描方向（决定画面上下左右是否镜像） ---- */
    s_vOledWriteOneByteCmd(0xA1, emDevNum);//--Set SEG/Column Mapping  0xa0左右反置 0xa1正常
    s_vOledWriteOneByteCmd(0xC8, emDevNum);//Set COM/Row Scan Direction  0xc0上下反置 0xc8正常

    /* ---- 占空比与显示偏移 ---- */
    s_vOledWriteOneByteCmd(0xA6, emDevNum);//--set normal display（正常显示，非反色）
    s_vOledWriteOneByteCmd(0xA8, emDevNum);//--set multiplex ratio(1 to 64)（双字节命令）
    s_vOledWriteOneByteCmd(0x3f, emDevNum);//--1/64 duty（64行全用）
    s_vOledWriteOneByteCmd(0xD3, emDevNum);//--set display offset  Shift Mapping RAM Counter (0x00~0x3F)（双字节命令）
    s_vOledWriteOneByteCmd(0x00, emDevNum);//-not offset

    /* ---- 时序参数 ---- */
    s_vOledWriteOneByteCmd(0xD5, emDevNum);//--set display clock divide ratio/oscillator frequency（双字节命令）
    s_vOledWriteOneByteCmd(0x80, emDevNum);//--set divide ratio, Set Clock as 100 Frames/Sec
    s_vOledWriteOneByteCmd(0xD9, emDevNum);//--set pre-charge period（双字节命令）
    s_vOledWriteOneByteCmd(0xF1, emDevNum);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock

    /* ---- 引脚布局 ---- */
    s_vOledWriteOneByteCmd(0xDA, emDevNum);//--set com pins hardware configuration（双字节命令）
    s_vOledWriteOneByteCmd(0x12, emDevNum);
    s_vOledWriteOneByteCmd(0xDB, emDevNum);//--set vcomh（双字节命令）
    s_vOledWriteOneByteCmd(0x40, emDevNum);// Set VCOM Deselect Level

    /* ---- 寻址模式 ---- */
    s_vOledWriteOneByteCmd(0x20, emDevNum);//-Set Page Addressing Mode（双字节命令，本驱动依赖页模式！）
    s_vOledWriteOneByteCmd(0x02, emDevNum);//--02 = 页寻址模式（0x00/0x01/0x02）

    /* ---- 电荷泵（OLED 没有外部 VCC，靠内部泵压） ---- */
    s_vOledWriteOneByteCmd(0x8D, emDevNum);//--set Charge Pump enable/disable（双字节命令）
    s_vOledWriteOneByteCmd(0x14, emDevNum);//--enable charge pump（0x10 = disable）

    /* ---- 显示恢复默认 + 开屏 ---- */
    s_vOledWriteOneByteCmd(0xA4, emDevNum);// Disable Entire Display On：显示跟随 GRAM（0xA5 全亮测屏用）
    s_vOledWriteOneByteCmd(0xA6, emDevNum);// Disable Inverse Display On：正常色（0xA7 反色）
    s_vOledWriteOneByteCmd(0xAF, emDevNum);//--turn on oled panel（全配完，开屏）
}



/* ================================================================
 *                        设备注册层
 *  两个 Init 都是"整块 memcpy"式注册：应用层填好局部结构体 → 拷入设备池。
 *  好处：成员再多也不用逐个赋值，新增字段不用改注册代码
 * ================================================================ */

/// @brief OLED设备运行参数初始化
/// @param pstInit  运行参数结构体指针
/// @param emDevNum 设备号
/// @note  把运行参数整块拷入对应设备的管理结构体
void vOLedDeviceRuningParamInit(stOLedRuningParamTdf  *pstInit,emOLedDeviceNumTdf emDevNum)
{
    memcpy(&astOLedDeviceParam[emDevNum].stRuningParam,pstInit,sizeof(astOLedDeviceParam[emDevNum].stRuningParam));
}

/// @brief OLED设备静态参数初始化
/// @param pstInit  静态参数结构体指针（引脚地址 + 引脚编号）
/// @param emDevNum 设备号
/// @note  把静态参数整块拷入对应设备的管理结构体（整块拷贝，避免逐个成员赋值），
///        拷完立即发初始化命令序列，设备即可用
void vOLedDeviceInit(stOLedStaticParamTdf *pstInit,emOLedDeviceNumTdf emDevNum)
{
    memcpy(&astOLedDeviceParam[emDevNum].stStaticParam , pstInit ,sizeof(stOLedStaticParamTdf));
    vOledCmdInit(emDevNum);
}
