#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "stm32f4xx_hal.h"
#include "fonts/fonts.h"

// LCD屏幕参数
#define LCD_WIDTH       800
#define LCD_HEIGHT      480
#define LCD_PIXEL_SIZE  2  // RGB565每个像素占2字节

// 层地址定义
#define LCD_BG_LAYER_ADDR  0xD0000000
#define LCD_FG_LAYER_ADDR  0xD0200000

// 颜色定义(RGB565)
#define RGB565(r,g,b)  ((((r)&0xF8)<<8) | (((g)&0xFC)<<3) | ((b)>>3))


// 常用颜色定义(RGB565)
#define COLOR_WHITE       RGB565(255, 255, 255)
#define COLOR_BLACK       RGB565(0, 0, 0)
#define COLOR_RED         RGB565(255, 0, 0)
#define COLOR_GREEN       RGB565(0, 255, 0)
#define COLOR_BLUE        RGB565(0, 0, 255)
#define COLOR_YELLOW      RGB565(255, 255, 0)
#define COLOR_CYAN        RGB565(0, 255, 255)
#define COLOR_MAGENTA     RGB565(255, 0, 255)
#define COLOR_ORANGE      RGB565(255, 165, 0)
#define COLOR_PURPLE      RGB565(128, 0, 128)
#define COLOR_PINK        RGB565(255, 192, 203)
#define COLOR_BROWN       RGB565(165, 42, 42)
#define COLOR_GRAY        RGB565(128, 128, 128)
#define COLOR_LIGHT_GRAY  RGB565(192, 192, 192)
#define COLOR_DARK_RED    RGB565(128, 0, 0)
#define COLOR_DARK_GREEN  RGB565(0, 128, 0)
#define COLOR_DARK_BLUE   RGB565(0, 0, 128)

// 函数声明
void BSP_LCD_Init(void);
void BSP_LCD_Test(void);
void BSP_LCD_FillBackground(uint16_t color);
void BSP_LCD_FillForeground(uint16_t color);
void BSP_LCD_DrawPixel(uint32_t layerAddr, uint16_t x, uint16_t y, uint16_t color);
void BSP_LCD_DrawLine(uint32_t layerAddr, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void BSP_LCD_DrawCircle(uint32_t layerAddr, uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void BSP_LCD_DrawRectangle(uint32_t layerAddr, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void BSP_LCD_FillRectangle(uint32_t layerAddr, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void BSP_LCD_FillCircle(uint32_t layerAddr, uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);

// 字体绘制函数
void BSP_LCD_DrawChar(uint32_t layerAddr, uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bgColor, sFONT *font);
void BSP_LCD_DrawString(uint32_t layerAddr, uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgColor, sFONT *font);

/**
 * @brief   显示十进制整数
 * @param   layerAddr: 层地址
 * @param   x,y: 起始坐标
 * @param   num: 要显示的整数
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明)
 * @param   font: 字体指针
 */
void BSP_LCD_DrawDec(uint32_t layerAddr, uint16_t x, uint16_t y, int32_t num, uint16_t color, uint16_t bgColor, sFONT *font);

/**
 * @brief   显示十六进制整数
 * @param   layerAddr: 层地址
 * @param   x,y: 起始坐标 
 * @param   num: 要显示的整数
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明)
 * @param   font: 字体指针
 */
void BSP_LCD_DrawHex(uint32_t layerAddr, uint16_t x, uint16_t y, uint32_t num, uint16_t color, uint16_t bgColor, sFONT *font);

/**
 * @brief   显示浮点数
 * @param   layerAddr: 层地址
 * @param   x,y: 起始坐标
 * @param   fnum: 要显示的浮点数
 * @param   decimals: 小数位数
 * @param   color: 前景色
 * @param   bgColor: 背景色(0xFFFF透明) 
 * @param   font: 字体指针
 */
void BSP_LCD_DrawFloat(uint32_t layerAddr, uint16_t x, uint16_t y, float fnum, uint8_t decimals, uint16_t color, uint16_t bgColor, sFONT *font);

#endif /* __BSP_LCD_H */
