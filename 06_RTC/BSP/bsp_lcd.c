#include "bsp_lcd.h"
#include "stm32f4xx_hal_dma2d.h"
#include "main.h"
#include <stdlib.h>
#include "fonts/fonts.h"
#include <stdarg.h>
#include <stdio.h>

extern DMA2D_HandleTypeDef hdma2d;

// 当前激活的LCD层
static LCD_Layer_t currentLayer = LCD_LAYER_FG;

/**
 * @brief   设置当前激活的LCD层
 * @param   layer: 图层枚举（LCD_LAYER_BG或LCD_LAYER_FG）
 */
void BSP_LCD_SetActiveLayer(LCD_Layer_t layer)
{
    currentLayer = layer;
}

/**
 * @brief   获取当前激活的LCD层
 * @retval  当前图层枚举值
 */
LCD_Layer_t BSP_LCD_GetActiveLayer(void)
{
    return currentLayer;
}

/**
 * @brief   获取当前图层的基地址
 * @retval  当前图层的SDRAM基地址
 */
static uint32_t getCurrentLayerAddr(void)
{
    return (currentLayer == LCD_LAYER_BG) ? LCD_BG_LAYER_ADDR : LCD_FG_LAYER_ADDR;
}

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
 */
void BSP_LCD_FillBackground(uint16_t color)
{
    DMA2D->CR &= ~DMA2D_CR_START;
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
    DMA2D->OOR = 0;
    DMA2D->NLR = (LCD_WIDTH << 16) | LCD_HEIGHT;
    DMA2D->OMAR = LCD_BG_LAYER_ADDR;
    DMA2D->OCOLR = color;
    DMA2D->CR |= DMA2D_CR_START;
    while((DMA2D->ISR & DMA2D_FLAG_TC) == 0);
    DMA2D->IFCR = DMA2D_FLAG_TC;
}

/**
 * @brief   使用DMA2D填充前景层
 * @param   color: RGB565格式的颜色值
 */
void BSP_LCD_FillForeground(uint16_t color)
{
    DMA2D->CR &= ~DMA2D_CR_START;
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
    DMA2D->OOR = 0;
    DMA2D->NLR = (LCD_WIDTH << 16) | LCD_HEIGHT;
    DMA2D->OMAR = LCD_FG_LAYER_ADDR;
    DMA2D->OCOLR = color;
    DMA2D->CR |= DMA2D_CR_START;
    while((DMA2D->ISR & DMA2D_FLAG_TC) == 0);
    DMA2D->IFCR = DMA2D_FLAG_TC;
}

/**
 * @brief   在当前激活层绘制单个像素点
 * @param   x: 像素的X坐标（0-799）
 * @param   y: 像素的Y坐标（0-479）
 * @param   color: RGB565格式的颜色值
 */
void BSP_LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint32_t layerAddr = getCurrentLayerAddr();
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    uint32_t pixelAddr = layerAddr + (y * LCD_WIDTH + x) * LCD_PIXEL_SIZE;
    *((uint16_t*)pixelAddr) = color;
}

/**
 * @brief   使用Bresenham算法画直线
 * @param   x1,y1: 起点坐标
 * @param   x2,y2: 终点坐标
 * @param   color: RGB565颜色值
 */
void BSP_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    while(1) {
        BSP_LCD_DrawPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/**
 * @brief   使用中点圆算法画圆
 * @param   x0,y0: 圆心坐标
 * @param   radius: 半径
 * @param   color: RGB565颜色值
 */
void BSP_LCD_DrawCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        BSP_LCD_DrawPixel(x0 + x, y0 + y, color);
        BSP_LCD_DrawPixel(x0 + y, y0 + x, color);
        BSP_LCD_DrawPixel(x0 - y, y0 + x, color);
        BSP_LCD_DrawPixel(x0 - x, y0 + y, color);
        BSP_LCD_DrawPixel(x0 - x, y0 - y, color);
        BSP_LCD_DrawPixel(x0 - y, y0 - x, color);
        BSP_LCD_DrawPixel(x0 + y, y0 - x, color);
        BSP_LCD_DrawPixel(x0 + x, y0 - y, color);

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
 * @brief   画矩形（空心）
 * @param   x1,y1: 左上角坐标
 * @param   x2,y2: 右下角坐标
 * @param   color: RGB565颜色值
 */
void BSP_LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    if(x1 > x2) { uint16_t tmp = x1; x1 = x2; x2 = tmp; }
    if(y1 > y2) { uint16_t tmp = y1; y1 = y2; y2 = tmp; }

    BSP_LCD_DrawLine(x1, y1, x2, y1, color);
    BSP_LCD_DrawLine(x2, y1, x2, y2, color);
    BSP_LCD_DrawLine(x2, y2, x1, y2, color);
    BSP_LCD_DrawLine(x1, y2, x1, y1, color);
}

/**
 * @brief   填充矩形（实心）
 * @param   x1,y1: 左上角坐标
 * @param   x2,y2: 右下角坐标
 * @param   color: RGB565颜色值
 */
void BSP_LCD_FillRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    if(x1 > x2) { uint16_t tmp = x1; x1 = x2; x2 = tmp; }
    if(y1 > y2) { uint16_t tmp = y1; y1 = y2; y2 = tmp; }

    for(uint16_t y = y1; y <= y2; y++) {
        BSP_LCD_DrawLine(x1, y, x2, y, color);
    }
}

/**
 * @brief   填充圆形（实心）
 * @param   x0,y0: 圆心坐标
 * @param   radius: 半径
 * @param   color: RGB565颜色值
 */
void BSP_LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        BSP_LCD_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        BSP_LCD_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        BSP_LCD_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        BSP_LCD_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);

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
 * @brief   在当前激活层绘制单个字符
 * @param   x,y: 起始坐标
 * @param   c: 要显示的字符
 * @param   color: 前景色(RGB565)
 * @param   bgColor: 背景色(RGB565), 0xFFFF表示透明
 * @param   font: 字体指针
 */
void BSP_LCD_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bgColor, sFONT *font)
{
    uint16_t i, j;
    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;
    uint32_t charOffset = (c - ' ') * charHeight * ((charWidth + 7) / 8);
    const uint8_t *charData = &font->table[charOffset];

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }

    for (i = 0; i < charHeight; i++) {
        for (uint16_t byte = 0; byte < ((charWidth + 7) / 8); byte++) {
            uint8_t data = charData[i * ((charWidth + 7) / 8) + byte];
            for (j = 0; j < 8 && (byte * 8 + j) < charWidth; j++) {
                if (data & (1 << (7 - j))) {
                    BSP_LCD_DrawPixel(x + byte * 8 + j, y + i, color);
                } else if (bgColor != 0xFFFF) {
                    BSP_LCD_DrawPixel(x + byte * 8 + j, y + i, bgColor);
                }
            }
        }
    }
}

/**
 * @brief   在当前激活层绘制字符串
 * @param   x,y: 起始坐标
 * @param   str: 要显示的字符串
 * @param   color: 前景色(RGB565)
 * @param   bgColor: 背景色(RGB565), 0xFFFF表示透明
 * @param   font: 字体指针
 */
void BSP_LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgColor, sFONT *font)
{
    uint16_t originalX = x;
    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;

    while (*str) {
        if (*str == '\n') {
            y += charHeight + 0;
            x = originalX;
            str++;
            continue;
        }

        if (y + charHeight > LCD_HEIGHT) {
            break;
        }

        BSP_LCD_DrawChar(x, y, *str, color, bgColor, font);
        x += charWidth + 1;

        if (x + charWidth > LCD_WIDTH) {
            y += charHeight + 0;
            x = originalX;
        }

        str++;
    }
}

/**
 * @brief   显示十进制整数
 * @param   x,y: 起始坐标
 * @param   num: 要显示的整数(32位有符号)
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明)
 * @param   font: 字体指针
 */
void BSP_LCD_DrawDec(uint16_t x, uint16_t y, int32_t num, uint16_t color, uint16_t bgColor, sFONT *font)
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
    
    if (bgColor != 0xFFFF) {
        for (uint16_t i = 0; i < 12; i++) {
            for (uint16_t j = 0; j < charHeight; j++) {
                for (uint16_t k = 0; k < charWidth; k++) {
                    BSP_LCD_DrawPixel(x + i * (charWidth + 1) + k, y + j, bgColor);
                }
            }
        }
    }
    
    while (*p) {
        BSP_LCD_DrawChar(x, y, *p, color, bgColor, font);
        x += charWidth + 1;
        p++;
    }
}

/**
 * @brief   显示十六进制整数
 * @param   x,y: 起始坐标
 * @param   num: 要显示的整数(32位无符号)
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明)
 * @param   font: 字体指针
 */
void BSP_LCD_DrawHex(uint16_t x, uint16_t y, uint32_t num, uint16_t color, uint16_t bgColor, sFONT *font)
{
    char buf[12];
    char *p = buf + 11;
    const char hexDigits[] = "0123456789ABCDEF";
    
    *p = '\0';
    
    if (num == 0) {
        *(--p) = '0';
    } else {
        while (num > 0) {
            *(--p) = hexDigits[num & 0x0F];
            num >>= 4;
        }
    }
    
    *(--p) = 'x';
    *(--p) = '0';
    
    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;
    
    if (bgColor != 0xFFFF) {
        for (uint16_t i = 0; i < 10; i++) {
            for (uint16_t j = 0; j < charHeight; j++) {
                for (uint16_t k = 0; k < charWidth; k++) {
                    BSP_LCD_DrawPixel(x + i * (charWidth + 1) + k, y + j, bgColor);
                }
            }
        }
    }
    
    while (*p) {
        BSP_LCD_DrawChar(x, y, *p, color, bgColor, font);
        x += charWidth + 1;
        p++;
    }
}

/**
 * @brief   显示浮点数
 * @param   x,y: 起始坐标
 * @param   fnum: 要显示的浮点数
 * @param   decimals: 小数位数
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明)
 * @param   font: 字体指针
 */
void BSP_LCD_DrawFloat(uint16_t x, uint16_t y, float fnum, uint8_t decimals, uint16_t color, uint16_t bgColor, sFONT *font)
{
    char buf[32];
    char *p = buf;
    uint8_t isNegative = 0;
    int32_t intPart;
    uint32_t fracPart;
    int32_t multiplier = 1;
    
    for (uint8_t i = 0; i < decimals; i++) {
        multiplier *= 10;
    }
    
    if (fnum < 0) {
        isNegative = 1;
        fnum = -fnum;
    }
    
    intPart = (int32_t)fnum;
    fracPart = (uint32_t)((fnum - intPart) * multiplier + 0.5f);
    
    if (fracPart >= (uint32_t)multiplier) {
        fracPart = 0;
        intPart++;
    }
    
    if (isNegative) {
        *p++ = '-';
    }
    
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
    
    if (decimals > 0) {
        *p++ = '.';
        
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
    
    if (bgColor != 0xFFFF) {
        for (uint16_t i = 0; i < 32; i++) {
            for (uint16_t j = 0; j < charHeight; j++) {
                for (uint16_t k = 0; k < charWidth; k++) {
                    BSP_LCD_DrawPixel(x + i * (charWidth + 1) + k, y + j, bgColor);
                }
            }
        }
    }
    
    p = buf;
    while (*p) {
        BSP_LCD_DrawChar(x, y, *p, color, bgColor, font);
        x += charWidth + 1;
        p++;
    }
}

/**
 * @brief   在当前激活层显示格式化字符串（类似printf功能）
 * @param   x: 起始X坐标（0-799）
 * @param   y: 起始Y坐标（0-479）
 * @param   color: 前景色(RGB565格式)
 * @param   bgColor: 背景色(RGB565格式), 0xFFFF表示透明
 * @param   font: 字体指针(&Font8, &Font12, &Font16, &Font20, &Font24)
 * @param   fmt: 格式化字符串，支持%d、%s、%x等标准printf格式
 * @param   ...: 可变参数，与fmt中的格式说明符对应
 */
void BSP_LCD_Printf(uint16_t x, uint16_t y, uint16_t color, uint16_t bgColor, sFONT *font, const char *fmt, ...)
{
    // 1. 定义字符缓冲区，最多存255个字符 + 字符串结束符'\0'
    char buf[256];
    // 2. 可变参数操作句柄，用来读取...里的参数
    va_list args;
    // 3. 初始化可变参数列表：从fmt之后开始取参数
    va_start(args, fmt);
    // 4. 核心：把fmt+可变参数格式化输出到buf数组里
    // vsnprintf = 安全版sprintf，限制最大输出长度为sizeof(buf)防止数组溢出
    vsnprintf(buf, sizeof(buf), fmt, args);
    // 5. 结束可变参数读取，释放资源
    va_end(args);
    // 6. 调用底层字符串绘制函数，把格式化好的buf打印到LCD屏幕
    BSP_LCD_DrawString(x, y, buf, color, bgColor, font);
}

/**
 * @brief   测试函数，用于测试LCD功能
* @param  None
* @retval None
 */
void BSP_LCD_Test(void)
{
    BSP_LCD_Init();

    BSP_LCD_FillBackground(COLOR_RED);
    HAL_Delay(1000);

    BSP_LCD_FillForeground(COLOR_BLUE);
    HAL_Delay(1000);

    BSP_LCD_SetActiveLayer(LCD_LAYER_FG);
    BSP_LCD_DrawPixel(LCD_WIDTH/2, LCD_HEIGHT/2, COLOR_GREEN);
    HAL_Delay(1000);

    for(uint16_t i = 0; i < LCD_WIDTH; i++) {
        BSP_LCD_DrawPixel(i, LCD_HEIGHT/2, COLOR_YELLOW);
    }
    for(uint16_t i = 0; i < LCD_HEIGHT; i++) {
        BSP_LCD_DrawPixel(LCD_WIDTH/2, i, COLOR_YELLOW);
    }
    HAL_Delay(1000);

    BSP_LCD_DrawLine(100, 100, 700, 100, COLOR_RED);
    BSP_LCD_DrawLine(100, 150, 100, 400, COLOR_GREEN);
    BSP_LCD_DrawLine(700, 150, 100, 400, COLOR_BLUE);
    HAL_Delay(1000);

    BSP_LCD_DrawCircle(200, 200, 50, COLOR_YELLOW);
    BSP_LCD_DrawCircle(600, 300, 80, COLOR_RED);
    HAL_Delay(1000);

    BSP_LCD_DrawRectangle(300, 100, 500, 200, COLOR_GREEN);
    BSP_LCD_DrawRectangle(50, 300, 300, 400, COLOR_BLUE);
    HAL_Delay(1000);

    BSP_LCD_FillRectangle(350, 250, 450, 350, COLOR_YELLOW);
    BSP_LCD_FillRectangle(500, 350, 700, 450, COLOR_RED);
    HAL_Delay(1000);

    BSP_LCD_FillCircle(400, 400, 50, COLOR_GREEN);
    BSP_LCD_FillCircle(150, 150, 30, COLOR_BLUE);
    HAL_Delay(1000);

    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_DrawChar(100, 50, 'A', COLOR_RED, COLOR_BLACK, &Font24);
    HAL_Delay(1000);

    BSP_LCD_DrawString(50, 100, "Hello World!", COLOR_GREEN, COLOR_BLACK, &Font24);
    HAL_Delay(1000);

    BSP_LCD_DrawString(50, 200, "STM32F429\nLTDC+DMA2D\nRGB565 800x480", COLOR_YELLOW, COLOR_BLACK, &Font16);
    HAL_Delay(1000);

    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_DrawDec(50, 50, 12345, COLOR_RED, COLOR_BLACK, &Font24);
    BSP_LCD_DrawDec(50, 100, -6789, COLOR_GREEN, COLOR_BLACK, &Font24);
    BSP_LCD_DrawDec(50, 150, 0, COLOR_BLUE, COLOR_BLACK, &Font24);
    HAL_Delay(1000);

    BSP_LCD_DrawHex(50, 200, 255, COLOR_YELLOW, COLOR_BLACK, &Font24);
    BSP_LCD_DrawHex(50, 250, 0xABCD, COLOR_CYAN, COLOR_BLACK, &Font24);
    BSP_LCD_DrawHex(50, 300, 0, COLOR_MAGENTA, COLOR_BLACK, &Font24);
    HAL_Delay(1000);

    BSP_LCD_DrawFloat(200, 50, 3.14159f, 5, COLOR_ORANGE, COLOR_BLACK, &Font24);
    BSP_LCD_DrawFloat(200, 100, -1.59f, 1, COLOR_PINK, COLOR_BLACK, &Font24);
    BSP_LCD_DrawFloat(200, 150, 0.0f, 3, COLOR_PURPLE, COLOR_BLACK, &Font24);
    HAL_Delay(1000);
}
