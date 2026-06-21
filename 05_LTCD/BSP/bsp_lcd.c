#include "bsp_lcd.h"
#include "stm32f4xx_hal_dma2d.h"
#include "main.h"
#include <stdlib.h>
#include "fonts/fonts.h"

extern DMA2D_HandleTypeDef hdma2d;



void BSP_LCD_Init(void)
{
    // DMA2D时钟使能
    __HAL_RCC_DMA2D_CLK_ENABLE();
    
    // 配置DMA2D
    hdma2d.Instance = DMA2D;
    hdma2d.Init.Mode = DMA2D_R2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;
    
    if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
    {
        // 初始化错误处理
        Error_Handler();
    }
}

/**
 * @brief   使用DMA2D填充背景层
 * @param   color: RGB565格式的颜色值
 *           例如: 0xF800(红色), 0x07E0(绿色), 0x001F(蓝色)
 * @note    DMA2D配置说明:
 *           1. DMA2D->CR: 控制寄存器，先清除START位停止传输
 *           2. DMA2D->OPFCCR: 输出像素格式配置寄存器，设置为RGB565
 *           3. DMA2D->OOR: 输出行偏移寄存器，设置为0（连续输出）
 *           4. DMA2D->NLR: 行列数寄存器，高16位为宽度，低16位为高度
 *           5. DMA2D->OMAR: 输出内存地址寄存器，设置为背景层基地址
 *           6. DMA2D->OCOLR: 输出颜色寄存器，设置为填充颜色值
 *           7. 设置START位启动DMA2D传输
 *           8. 轮询传输完成标志位TC
 *           9. 清除中断标志位
 */
void BSP_LCD_FillBackground(uint16_t color)
{
    /* 停止当前DMA2D传输（如果正在传输） */
    DMA2D->CR &= ~DMA2D_CR_START;
    /* 配置输出像素格式为RGB565 */
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
    /* 设置输出行偏移为0 */
    DMA2D->OOR = 0;
    /* 设置像素行列数: 高16位 = 宽度800, 低16位 = 高度480 */
    DMA2D->NLR = (LCD_WIDTH << 16) | LCD_HEIGHT;
    /* 设置输出内存地址为背景层SDRAM地址 0xD0000000 */
    DMA2D->OMAR = LCD_BG_LAYER_ADDR;
    /* 设置填充颜色值（RGB565格式） */
    DMA2D->OCOLR = color;
    /* 启动DMA2D传输 */
    DMA2D->CR |= DMA2D_CR_START;
    /* 等待传输完成 */
    while((DMA2D->ISR & DMA2D_FLAG_TC) == 0);
    /* 清除传输完成标志位 */
    DMA2D->IFCR = DMA2D_FLAG_TC;
}

/**
 * @brief   使用DMA2D填充前景层
 * @param   color: RGB565格式的颜色值
 *           例如: 0xF800(红色), 0x07E0(绿色), 0x001F(蓝色)
 * @note    DMA2D配置说明:
 *           1. DMA2D->CR: 控制寄存器，先清除START位停止传输
 *           2. DMA2D->OPFCCR: 输出像素格式配置寄存器，设置为RGB565
 *           3. DMA2D->OOR: 输出行偏移寄存器，设置为0（连续输出）
 *           4. DMA2D->NLR: 行列数寄存器，高16位为宽度，低16位为高度
 *           5. DMA2D->OMAR: 输出内存地址寄存器，设置为前景层基地址
 *           6. DMA2D->OCOLR: 输出颜色寄存器，设置为填充颜色值
 *           7. 设置START位启动DMA2D传输
 *           8. 轮询传输完成标志位TC
 *           9. 清除中断标志位
 */
void BSP_LCD_FillForeground(uint16_t color)
{
    /* 停止当前DMA2D传输（如果正在传输） */
    DMA2D->CR &= ~DMA2D_CR_START;
    /* 配置输出像素格式为RGB565 */
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
    /* 设置输出行偏移为0 */
    DMA2D->OOR = 0;
    /* 设置像素行列数: 高16位 = 宽度800, 低16位 = 高度480 */
    DMA2D->NLR = (LCD_WIDTH << 16) | LCD_HEIGHT;
    /* 设置输出内存地址为前景层SDRAM地址 0xD0200000 */
    DMA2D->OMAR = LCD_FG_LAYER_ADDR;
    /* 设置填充颜色值（RGB565格式） */
    DMA2D->OCOLR = color;
    /* 启动DMA2D传输 */
    DMA2D->CR |= DMA2D_CR_START;
    /* 等待传输完成 */
    while((DMA2D->ISR & DMA2D_FLAG_TC) == 0);
    /* 清除传输完成标志位 */
    DMA2D->IFCR = DMA2D_FLAG_TC;
}

/**
 * @brief   在指定层绘制单个像素点
 * @param   layerAddr: 层基地址（LCD_BG_LAYER_ADDR或LCD_FG_LAYER_ADDR）
 * @param   x: 像素的X坐标（0-799）
 * @param   y: 像素的Y坐标（0-479）
 * @param   color: RGB565格式的颜色值
 * @note    此函数直接向SDRAM写入颜色值，不使用DMA2D
 *           像素地址 = 层基地址 + (y * 屏幕宽度 + x) * 每像素字节数
 *           对于RGB565: 每像素占2字节
 */
void BSP_LCD_DrawPixel(uint32_t layerAddr, uint16_t x, uint16_t y, uint16_t color)
{
    /* 检查坐标是否在屏幕范围内 */
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    /* 计算像素在SDRAM中的地址 */
    uint32_t pixelAddr = layerAddr + (y * LCD_WIDTH + x) * LCD_PIXEL_SIZE;
    /* 向该地址写入RGB565颜色值 */
    *((uint16_t*)pixelAddr) = color;
}

/**
 * @brief   使用Bresenham算法画直线
 * @param   layerAddr: 层基地址
 * @param   x1,y1: 起点坐标
 * @param   x2,y2: 终点坐标
 * @param   color: RGB565颜色值
 * @note    Bresenham算法使用整数运算，通过误差累积判断最佳逼近像素点
 *           1. 计算x和y方向的增量dx、dy
 *           2. 根据坐标大小确定步进方向sx、sy
 *           3. 使用误差变量err控制绘制路径
 *           4. 在循环中绘制像素并更新误差
 */
void BSP_LCD_DrawLine(uint32_t layerAddr, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    /* 计算x和y方向的绝对值差 */
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    /* 确定x和y的步进方向：正向(+1)或反向(-1) */
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    /* 初始化误差值，用于判断每一步应该走x方向还是y方向 */
    int16_t err = dx - dy;
    int16_t e2;

    /* 循环绘制直到到达终点 */
    while(1) {
        /* 在当前坐标位置绘制一个像素 */
        BSP_LCD_DrawPixel(layerAddr, x1, y1, color);
        /* 到达终点坐标则退出循环 */
        if (x1 == x2 && y1 == y2) break;
        /* 计算2倍误差，用于判断下一步方向 */
        e2 = 2 * err;
        /* 如果误差大于-dy，则沿x方向步进 */
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        /* 如果误差小于dx，则沿y方向步进 */
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/**
 * @brief   使用中点圆算法画圆
 * @param   layerAddr: 层基地址
 * @param   x0,y0: 圆心坐标
 * @param   radius: 半径
 * @param   color: RGB565颜色值
 * @note    中点圆算法利用圆的八分对称性：
 *           1. 从(0,R)开始，利用判别式err决定下一个像素位置
 *           2. 每次迭代同时绘制8个对称点
 *           3. 当x>=y时循环结束
 */
void BSP_LCD_DrawCircle(uint32_t layerAddr, uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    /* 初始化x为半径，y为0 */
    int16_t x = radius;
    int16_t y = 0;
    /* 误差判别变量，用于决定下一步选择哪个像素 */
    int16_t err = 0;

    /* 利用圆的八分对称性，当x>=y时循环绘制 */
    while (x >= y) {
        /* 利用八分对称性同时绘制8个像素点 */
        BSP_LCD_DrawPixel(layerAddr, x0 + x, y0 + y, color); /* 第一象限 上 */
        BSP_LCD_DrawPixel(layerAddr, x0 + y, y0 + x, color); /* 第一象限 下 */
        BSP_LCD_DrawPixel(layerAddr, x0 - y, y0 + x, color); /* 第二象限 上 */
        BSP_LCD_DrawPixel(layerAddr, x0 - x, y0 + y, color); /* 第二象限 下 */
        BSP_LCD_DrawPixel(layerAddr, x0 - x, y0 - y, color); /* 第三象限 上 */
        BSP_LCD_DrawPixel(layerAddr, x0 - y, y0 - x, color); /* 第三象限 下 */
        BSP_LCD_DrawPixel(layerAddr, x0 + y, y0 - x, color); /* 第四象限 上 */
        BSP_LCD_DrawPixel(layerAddr, x0 + x, y0 - y, color); /* 第四象限 下 */

        /* 根据误差判断下一个像素位置 */
        if (err <= 0) {
            /* 选择右侧像素 */
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            /* 选择右下像素 */
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

/**
 * @brief   画矩形（空心）
 * @param   layerAddr: 层基地址
 * @param   x1,y1: 左上角坐标
 * @param   x2,y2: 右下角坐标
 * @param   color: RGB565颜色值
 * @note    通过绘制四条直线组成矩形边框：
 *           1. 上边：从(x1,y1)到(x2,y1)
 *           2. 右边：从(x2,y1)到(x2,y2)
 *           3. 下边：从(x2,y2)到(x1,y2)
 *           4. 左边：从(x1,y2)到(x1,y1)
 */
void BSP_LCD_DrawRectangle(uint32_t layerAddr, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    /* 确保x1<x2, y1<y2，如果坐标顺序颠倒则交换 */
    if(x1 > x2) { uint16_t tmp = x1; x1 = x2; x2 = tmp; }
    if(y1 > y2) { uint16_t tmp = y1; y1 = y2; y2 = tmp; }

    /* 画四条边 */
    BSP_LCD_DrawLine(layerAddr, x1, y1, x2, y1, color); // 上边
    BSP_LCD_DrawLine(layerAddr, x2, y1, x2, y2, color); // 右边
    BSP_LCD_DrawLine(layerAddr, x2, y2, x1, y2, color); // 下边
    BSP_LCD_DrawLine(layerAddr, x1, y2, x1, y1, color); // 左边
}

/**
 * @brief   填充矩形（实心）
 * @param   layerAddr: 层基地址
 * @param   x1,y1: 左上角坐标
 * @param   x2,y2: 右下角坐标
 * @param   color: RGB565颜色值
 * @note    从顶部到底部逐行填充：
 *           1. 先确保x1<x2, y1<y2
 *           2. 从y1到y2循环
 *           3. 每行用DrawLine绘制水平线
 */
void BSP_LCD_FillRectangle(uint32_t layerAddr, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    /* 确保x1<x2, y1<y2，如果坐标顺序颠倒则交换 */
    if(x1 > x2) { uint16_t tmp = x1; x1 = x2; x2 = tmp; }
    if(y1 > y2) { uint16_t tmp = y1; y1 = y2; y2 = tmp; }

    /* 从顶部到底部逐行绘制水平线进行填充 */
    for(uint16_t y = y1; y <= y2; y++) {
        BSP_LCD_DrawLine(layerAddr, x1, y, x2, y, color);
    }
}

/**
 * @brief   填充圆形（实心）
 * @param   layerAddr: 层基地址
 * @param   x0,y0: 圆心坐标
 * @param   radius: 半径
 * @param   color: RGB565颜色值
 * @note    利用圆的对称性，通过画水平线填充：
 *           1. 使用中点圆算法遍历
 *           2. 在每个y位置画出对称的水平线
 *           3. 覆盖圆形的所有水平截面
 */
void BSP_LCD_FillCircle(uint32_t layerAddr, uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    /* 初始化x为半径，y为0 */
    int16_t x = radius;
    int16_t y = 0;
    /* 误差判别变量 */
    int16_t err = 0;

    /* 遍历圆的八分之一并填充水平线 */
    while (x >= y) {
        /* 画水平线填充：利用对称性同时填充四个象限的水平截面 */
        BSP_LCD_DrawLine(layerAddr, x0 - x, y0 + y, x0 + x, y0 + y, color); /* 上方水平线 */
        BSP_LCD_DrawLine(layerAddr, x0 - y, y0 + x, x0 + y, y0 + x, color); /* 上方水平线 */
        BSP_LCD_DrawLine(layerAddr, x0 - x, y0 - y, x0 + x, y0 - y, color); /* 下方水平线 */
        BSP_LCD_DrawLine(layerAddr, x0 - y, y0 - x, x0 + y, y0 - x, color); /* 下方水平线 */

        /* 根据误差更新下一个x和y位置 */
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

/**
 * @brief   在指定层绘制单个字符
 * @param   layerAddr: 层基地址
 * @param   x,y: 起始坐标
 * @param   c: 要显示的字符
 * @param   color: 前景色(RGB565)
 * @param   bgColor: 背景色(RGB565), 0xFFFF表示透明
 * @param   font: 字体指针
 * @note    根据字模数据逐行扫描绘制字符：
 *           1. 计算字符在字模表中的偏移地址
 *           2. 逐行读取字模数据
 *           3. 逐位判断：1绘制前景色，0可绘制背景色或透明
 */
void BSP_LCD_DrawChar(uint32_t layerAddr, uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bgColor, sFONT *font)
{
    uint16_t i, j;
    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;
    // 计算字符在字模表中的偏移量
    uint32_t charOffset = (c - ' ') * charHeight * ((charWidth + 7) / 8);
    const uint8_t *charData = &font->table[charOffset];

    // 检查坐标是否超出屏幕范围
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }

    // 逐行绘制字符
    for (i = 0; i < charHeight; i++) {
        // 每行可能有多个字节(当宽度>8时)
        for (uint16_t byte = 0; byte < ((charWidth + 7) / 8); byte++) {
            uint8_t data = charData[i * ((charWidth + 7) / 8) + byte];
            // 逐位绘制(最多8位)
            for (j = 0; j < 8 && (byte * 8 + j) < charWidth; j++) {
                if (data & (1 << (7 - j))) {
                    // 绘制前景色
                    BSP_LCD_DrawPixel(layerAddr, x + byte * 8 + j, y + i, color);
                } else if (bgColor != 0xFFFF) {
                    // 绘制背景色
                    BSP_LCD_DrawPixel(layerAddr, x + byte * 8 + j, y + i, bgColor);
                }
            }
        }
    }
}

/**
 * @brief   在指定层绘制字符串
 * @param   layerAddr: 层基地址
 * @param   x,y: 起始坐标
 * @param   str: 要显示的字符串
 * @param   color: 前景色(RGB565)
 * @param   bgColor: 背景色(RGB565), 0xFFFF表示透明
 * @param   font: 字体指针
 * @note    逐个字符绘制并自动换行：
 *           1. 遍历字符串中的每个字符
 *           2. 处理换行符\n换行
 *           3. 超出屏幕右侧自动换行
 *           4. 字符间距1像素，行间距2像素
 */
void BSP_LCD_DrawString(uint32_t layerAddr, uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgColor, sFONT *font)
{
    uint16_t originalX = x;
    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;

    // 逐个字符绘制
    while (*str) {
        // 处理换行符
        if (*str == '\n') {
            y += charHeight + 0; // 行间距0像素
            x = originalX;
            str++;
            continue;
        }

        // 检查是否超出屏幕底部
        if (y + charHeight > LCD_HEIGHT) {
            break;
        }

        // 绘制当前字符
        BSP_LCD_DrawChar(layerAddr, x, y, *str, color, bgColor, font);

        // 移动到下一个字符位置
        x += charWidth + 1; // 字符间距1像素

        // 检查是否超出屏幕右侧
        if (x + charWidth > LCD_WIDTH) {
            y += charHeight + 0; // 行间距0像素
            x = originalX;
        }

        str++;
    }
}

/**
 * @brief   显示十进制整数
 * @param   layerAddr: 层基地址
 * @param   x,y: 起始坐标
 * @param   num: 要显示的整数(32位有符号)
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明)
 * @param   font: 字体指针
 * @note    将整数转换为字符串并显示
 *           支持负数和0值
 */
void BSP_LCD_DrawDec(uint32_t layerAddr, uint16_t x, uint16_t y, int32_t num, uint16_t color, uint16_t bgColor, sFONT *font)
{
    char buf[16];
    char *p = buf + 15;
    uint8_t isNegative = 0;
    
    *p = '\0';
    
    if (num < 0) {
        isNegative = 1;
        num = -num;
    }
    
    if (num == 0) {
        *(--p) = '0';
    } else {
        while (num > 0) {
            *(--p) = '0' + (num % 10);
            num /= 10;
        }
    }
    
    if (isNegative) {
        *(--p) = '-';
    }
    
    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;
    
    // 先清除数字显示区域(最大支持11个字符: -2147483648)
    if (bgColor != 0xFFFF) {
        for (uint16_t i = 0; i < 11; i++) {
            for (uint16_t j = 0; j < charHeight; j++) {
                for (uint16_t k = 0; k < charWidth; k++) {
                    BSP_LCD_DrawPixel(layerAddr, x + i * (charWidth + 1) + k, y + j, bgColor);
                }
            }
        }
    }
    
    // 绘制数字
    while (*p) {
        BSP_LCD_DrawChar(layerAddr, x, y, *p, color, bgColor, font);
        x += charWidth + 1;
        p++;
    }
}

/**
 * @brief   显示十六进制整数
 * @param   layerAddr: 层基地址
 * @param   x,y: 起始坐标
 * @param   num: 要显示的整数(32位无符号)
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明)
 * @param   font: 字体指针
 * @note    显示格式为 "0x" + 十六进制值
 *           例如: 0xFF, 0xABCD
 */
void BSP_LCD_DrawHex(uint32_t layerAddr, uint16_t x, uint16_t y, uint32_t num, uint16_t color, uint16_t bgColor, sFONT *font)
{
    char buf[12];
    char *p = buf + 11;
    const char hexDigits[] = "0123456789ABCDEF";
    
    *p = '\0';
    
    // 处理0值
    if (num == 0) {
        *(--p) = '0';
    } else {
        while (num > 0) {
            *(--p) = hexDigits[num & 0x0F];
            num >>= 4;
        }
    }
    
    // 添加0x前缀
    *(--p) = 'x';
    *(--p) = '0';
    
    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;
    
    // 先清除十六进制显示区域(最大支持10个字符: 0xFFFFFFFF)
    if (bgColor != 0xFFFF) {
        for (uint16_t i = 0; i < 10; i++) {
            for (uint16_t j = 0; j < charHeight; j++) {
                for (uint16_t k = 0; k < charWidth; k++) {
                    BSP_LCD_DrawPixel(layerAddr, x + i * (charWidth + 1) + k, y + j, bgColor);
                }
            }
        }
    }
    
    // 绘制十六进制数字
    while (*p) {
        BSP_LCD_DrawChar(layerAddr, x, y, *p, color, bgColor, font);
        x += charWidth + 1;
        p++;
    }
}

/**
 * @brief   显示浮点数
 * @param   layerAddr: 层基地址
 * @param   x,y: 起始坐标
 * @param   fnum: 要显示的浮点数
 * @param   decimals: 小数位数
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明)
 * @param   font: 字体指针
 * @note    支持负数和指定小数位数
 *           例如: 3.14(decimals=2), -1.5(decimals=1)
 */
void BSP_LCD_DrawFloat(uint32_t layerAddr, uint16_t x, uint16_t y, float fnum, uint8_t decimals, uint16_t color, uint16_t bgColor, sFONT *font)
{
    char buf[32];
    char *p = buf;
    uint8_t isNegative = 0;
    int32_t intPart;
    uint32_t fracPart;
    int32_t multiplier = 1;
    
    // 计算乘数用于获取小数部分
    for (uint8_t i = 0; i < decimals; i++) {
        multiplier *= 10;
    }
    
    // 处理负数
    if (fnum < 0) {
        isNegative = 1;
        fnum = -fnum;
    }
    
    // 提取整数部分和小数部分
    intPart = (int32_t)fnum;
    fracPart = (uint32_t)((fnum - intPart) * multiplier + 0.5f); // 四舍五入
    
    // 处理进位(小数部分进位到整数部分)
    if (fracPart >= (uint32_t)multiplier) {
        fracPart = 0;
        intPart++;
    }
    
    // 处理负号
    if (isNegative) {
        *p++ = '-';
    }
    
    // 转换整数部分
    if (intPart == 0) {
        *p++ = '0';
    } else {
        char intBuf[16];
        char *pi = intBuf + 15;
        *pi = '\0';
        int32_t temp = intPart;
        while (temp > 0) {
            *(--pi) = '0' + (temp % 10);
            temp /= 10;
        }
        while (*pi) {
            *p++ = *pi++;
        }
    }
    
    // 添加小数点
    if (decimals > 0) {
        *p++ = '.';
        
        // 转换小数部分(补零)
        uint32_t temp = fracPart;
        int8_t digits = 0;
        char fracBuf[16];
        char *pf = fracBuf + 15;
        *pf = '\0';
        
        while (temp > 0) {
            *(--pf) = '0' + (temp % 10);
            temp /= 10;
            digits++;
        }
        
        // 补足前导零
        while (digits < decimals) {
            *(--pf) = '0';
            digits++;
        }
        
        while (*pf) {
            *p++ = *pf++;
        }
    }
    
    *p = '\0';
    
    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;
    
    // 先清除浮点数显示区域(最大32个字符)
    if (bgColor != 0xFFFF) {
        for (uint16_t i = 0; i < 32; i++) {
            for (uint16_t j = 0; j < charHeight; j++) {
                for (uint16_t k = 0; k < charWidth; k++) {
                    BSP_LCD_DrawPixel(layerAddr, x + i * (charWidth + 1) + k, y + j, bgColor);
                }
            }
        }
    }
    
    // 显示完整字符串
    p = buf;
    while (*p) {
        BSP_LCD_DrawChar(layerAddr, x, y, *p, color, bgColor, font);
        x += charWidth + 1;
        p++;
    }
}

/**
 * @brief   测试函数，用于测试LCD功能
* @param  None
* @retval None
 */
void BSP_LCD_Test(void)
{
    // 初始化DMA2D
    BSP_LCD_Init();

    // 测试1: 填充背景层为红色
    BSP_LCD_FillBackground(COLOR_RED);
    HAL_Delay(1000); // 延时2秒

    // 测试2: 填充前景层为蓝色(半透明)
    BSP_LCD_FillForeground(COLOR_BLUE);
    HAL_Delay(1000); // 延时2秒

    // 测试3: 在前景层中心绘制绿色像素点
    BSP_LCD_DrawPixel(LCD_FG_LAYER_ADDR, LCD_WIDTH/2, LCD_HEIGHT/2, COLOR_GREEN);
    HAL_Delay(1000); // 延时2秒

    // 测试4: 绘制十字线
    for(uint16_t i = 0; i < LCD_WIDTH; i++) {
        BSP_LCD_DrawPixel(LCD_FG_LAYER_ADDR, i, LCD_HEIGHT/2, COLOR_YELLOW);
    }
    for(uint16_t i = 0; i < LCD_HEIGHT; i++) {
        BSP_LCD_DrawPixel(LCD_FG_LAYER_ADDR, LCD_WIDTH/2, i, COLOR_YELLOW);
    }
    HAL_Delay(1000); // 延时2秒

    // 测试5: 画直线
    BSP_LCD_DrawLine(LCD_FG_LAYER_ADDR, 100, 100, 700, 100, COLOR_RED);  // 水平线
    BSP_LCD_DrawLine(LCD_FG_LAYER_ADDR, 100, 150, 100, 400, COLOR_GREEN); // 垂直线
    BSP_LCD_DrawLine(LCD_FG_LAYER_ADDR, 700, 150, 100, 400, COLOR_BLUE);  // 斜线
    HAL_Delay(1000); // 延时2秒

    // 测试6: 画圆
    BSP_LCD_DrawCircle(LCD_FG_LAYER_ADDR, 200, 200, 50, COLOR_YELLOW);
    BSP_LCD_DrawCircle(LCD_FG_LAYER_ADDR, 600, 300, 80, COLOR_RED);
    HAL_Delay(1000); // 延时2秒

    // 测试7: 画矩形
    BSP_LCD_DrawRectangle(LCD_FG_LAYER_ADDR, 300, 100, 500, 200, COLOR_GREEN);
    BSP_LCD_DrawRectangle(LCD_FG_LAYER_ADDR, 50, 300, 300, 400, COLOR_BLUE);
    HAL_Delay(1000); // 延时2秒

    // 测试8: 填充矩形
    BSP_LCD_FillRectangle(LCD_FG_LAYER_ADDR, 350, 250, 450, 350, COLOR_YELLOW);
    BSP_LCD_FillRectangle(LCD_FG_LAYER_ADDR, 500, 350, 700, 450, COLOR_RED);
    HAL_Delay(1000); // 延时2秒

    // 测试9: 填充圆形
    BSP_LCD_FillCircle(LCD_FG_LAYER_ADDR, 400, 400, 50, COLOR_GREEN);
    BSP_LCD_FillCircle(LCD_FG_LAYER_ADDR, 150, 150, 30, COLOR_BLUE);
    HAL_Delay(1000); // 延时2秒

    // 测试10: 显示字符和字符串
    BSP_LCD_FillForeground(COLOR_BLACK);
    // 显示单个字符
    BSP_LCD_DrawChar(LCD_FG_LAYER_ADDR, 100, 50, 'A', COLOR_RED, COLOR_BLACK, &Font24);
    HAL_Delay(1000); // 延时2秒

    // 显示字符串
    BSP_LCD_DrawString(LCD_FG_LAYER_ADDR, 50, 100, "Hello World!", COLOR_GREEN, COLOR_BLACK, &Font24);
    HAL_Delay(1000); // 延时2秒

    // 显示多行字符串
    BSP_LCD_DrawString(LCD_FG_LAYER_ADDR, 50, 200, "STM32F429\nLTDC+DMA2D\nRGB565 800x480", COLOR_YELLOW, COLOR_BLACK, &Font16);
    HAL_Delay(1000); // 延时2秒

    // 测试11: 显示数字
    BSP_LCD_FillForeground(COLOR_BLACK);
    // 显示十进制整数
    BSP_LCD_DrawDec(LCD_FG_LAYER_ADDR, 50, 50, 12345, COLOR_RED, COLOR_BLACK, &Font24);
    BSP_LCD_DrawDec(LCD_FG_LAYER_ADDR, 50, 100, -6789, COLOR_GREEN, COLOR_BLACK, &Font24);
    BSP_LCD_DrawDec(LCD_FG_LAYER_ADDR, 50, 150, 0, COLOR_BLUE, COLOR_BLACK, &Font24);
    HAL_Delay(1000); // 延时3秒

    // 显示十六进制整数
    BSP_LCD_DrawHex(LCD_FG_LAYER_ADDR, 50, 200, 255, COLOR_YELLOW, COLOR_BLACK, &Font24);
    BSP_LCD_DrawHex(LCD_FG_LAYER_ADDR, 50, 250, 0xABCD, COLOR_CYAN, COLOR_BLACK, &Font24);
    BSP_LCD_DrawHex(LCD_FG_LAYER_ADDR, 50, 300, 0, COLOR_MAGENTA, COLOR_BLACK, &Font24);
    HAL_Delay(1000); // 延时3秒

    // 显示浮点数
    BSP_LCD_DrawFloat(LCD_FG_LAYER_ADDR, 200, 50, 3.14159f, 5, COLOR_ORANGE, COLOR_BLACK, &Font24);
    BSP_LCD_DrawFloat(LCD_FG_LAYER_ADDR, 200, 100, -1.59f, 1, COLOR_PINK, COLOR_BLACK, &Font24);
    BSP_LCD_DrawFloat(LCD_FG_LAYER_ADDR, 200, 150, 0.0f, 3, COLOR_PURPLE, COLOR_BLACK, &Font24);
    HAL_Delay(1000); // 延时3秒
}
