/**
  * @file    bsp_lcd.c
  * @brief   LCD 驱动实现 - 包含所有绘图、文本、格式转换、DMA2D 加速功能
  * @author  Your Name
  * @version V1.2
  * @date    2026-06-23
  * 
  * @details 本文件实现头文件中声明的所有函数，核心设计要点：
  *          1. 坐标系统：应用层使用逻辑坐标（相对于当前旋转），通过 LCD_MapPixel/LCD_MapWindow 映射到物理坐标。
  *          2. 颜色管理：所有颜色输入为 RGB565，通过 LCD_ConvertColor 转为当前格式，统一使用 uint32_t 存储。
  *          3. 硬件加速：利用 DMA2D 实现高速矩形填充（清屏、实心矩形等），超时保护。
  *          4. 绘图算法：Bresenham 画线、中点圆算法（空心/实心）。
  *          5. 字符绘制：逐行扫描点阵，支持背景透明，光标自动换行。
  *          6. 数值转换：十进制、十六进制、浮点数（四舍五入）转为字符串后绘制。
  *          7. 测试函数演示了旋转、格式切换、图形绘制、文本输出等全部功能。
  * 
  * @note   依赖外部 DMA2D 句柄 hdma2d，需在 main 或 HAL 中初始化。
  *         依赖 Error_Handler()，需用户实现（通常为死循环）。
  */

#include "bsp_lcd.h"
#include "fonts/fonts.h"
#include "main.h"
#include "stm32f4xx_hal_dma2d.h"
#include "ltdc.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>       /* 用于 roundf，浮点数四舍五入 */

/* 外部 DMA2D 句柄（由 HAL 初始化） */
extern DMA2D_HandleTypeDef hdma2d;

/* 外部 LTDC 句柄（由 HAL 初始化） */
extern LTDC_HandleTypeDef hltdc;

/*=============================================================================
 *                               全局变量定义及初始值
 *============================================================================*/
LCD_Layer_t currentLayer = LCD_LAYER_FG;        /* 默认前景层（便于用户绘图） */
uint16_t lcd_cursor_x = 0U;                     /* 光标初始位置 (0,0) */
uint16_t lcd_cursor_y = 0U;
uint16_t lcd_fgColor = COLOR_WHITE;             /* 默认白色前景 */
uint16_t lcd_bgColor = COLOR_BLACK;             /* 默认黑色背景 */
uint8_t  lcd_bg_transparent = 0;                /* 默认不透明（绘制背景色） */
sFONT*   lcd_font = &Font24;                    /* 默认 24 点阵字体 */
LCD_Window_t lcd_bg_window = {0U, 0U, LCD_PHYS_WIDTH, LCD_PHYS_HEIGHT}; /* 全屏窗口 */
LCD_Window_t lcd_fg_window = {0U, 0U, LCD_PHYS_WIDTH, LCD_PHYS_HEIGHT};

uint16_t lcd_orientation = 0;                   /* 初始 0°（横屏） */
LCD_PixelFormat_t lcd_pixel_format = LCD_PIXEL_FORMAT_RGB565;
uint8_t lcd_pixel_bytes = 2;                    /* RGB565 占 2 字节 */
uint32_t lcd_dma2d_output_fmt = DMA2D_OUTPUT_RGB565;

/*=============================================================================
 *                               内部辅助函数
 *============================================================================*/

/**
  * @brief  将 RGB565 颜色转换为当前像素格式的颜色值
  * @param  rgb565 16位 RGB565 颜色
  * @retval 转换后的颜色值（uint32_t，实际有效位数取决于格式）
  * @note   对于 RGB888，返回 24 位值（低 24 位有效）；
  *         对于 ARGB8888，返回 32 位值（Alpha 固定为 0xFF 不透明）。
  *         此转换保证所有颜色输入统一，用户无需关心当前格式。
  */
static uint32_t LCD_ConvertColor(uint16_t rgb565)
{
    /* 从 RGB565 中提取各分量并扩展至 8 位 */
    uint8_t r = (rgb565 >> 11) & 0x1F;   /* 高5位红色 */
    uint8_t g = (rgb565 >> 5)  & 0x3F;   /* 中间6位绿色 */
    uint8_t b = rgb565 & 0x1F;           /* 低5位蓝色 */
    /* 更精确的扩展算法 */
    uint32_t R = (r * 255 + 15) / 31;    /* 5->8 bits: (r * 255)/31 */
    uint32_t G = (g * 255 + 31) / 63;    /* 6->8 bits: (g * 255)/63 */
    uint32_t B = (b * 255 + 15) / 31;    /* 5->8 bits: (b * 255)/31 */

    switch (lcd_pixel_format) {
        case LCD_PIXEL_FORMAT_RGB565:
            return rgb565;   /* 直接返回原始 16 位值 */
        case LCD_PIXEL_FORMAT_RGB888:
            return (R << 16) | (G << 8) | B;  /* 24 位 RGB */
        case LCD_PIXEL_FORMAT_ARGB8888:
            return (0xFF << 24) | (R << 16) | (G << 8) | B; /* Alpha=0xFF 不透明 */
        default:
            return rgb565;
    }
}

/**
  * @brief  将逻辑坐标映射为物理坐标（依据当前旋转角度）
  * @param  x 逻辑 X 坐标指针（输入输出）
  * @param  y 逻辑 Y 坐标指针（输入输出）
  * @note   映射公式：
  *         0°:   (x,y) -> (x,y)
  *         90°:  (x,y) -> (y, PHY_H - 1 - x)
  *         180°: (x,y) -> (PHY_W - 1 - x, PHY_H - 1 - y)
  *         270°: (x,y) -> (PHY_W - 1 - y, x)
  *         结果保证在 [0, PHY_W-1] 和 [0, PHY_H-1] 范围内（输入已校验）。
  */
static void LCD_MapPixel(uint16_t *x, uint16_t *y)
{
    uint16_t tmp;
    switch (lcd_orientation) {
        case 0:   /* 无旋转 */
            break;
        case 90:  /* 顺时针 90° */
            tmp = *x;
            *x = *y;
            *y = LCD_PHYS_HEIGHT - 1 - tmp;
            break;
        case 180: /* 旋转 180° */
            *x = LCD_PHYS_WIDTH - 1 - *x;
            *y = LCD_PHYS_HEIGHT - 1 - *y;
            break;
        case 270: /* 逆时针 90°（顺时针 270°） */
            tmp = *x;
            *x = LCD_PHYS_WIDTH - 1 - *y;
            *y = tmp;
            break;
        default:
            /* 不支持的旋转角，忽略不做映射 */
            break;
    }
}

/**
  * @brief  将逻辑窗口映射为物理窗口（依据当前旋转角度）
  * @param  w 窗口结构指针（输入输出）
  * @note   映射后会对窗口进行裁剪，确保不超出物理屏幕边界。
  *         窗口的宽度和高度也会相应交换（90°/270° 时）。
  */
static void LCD_MapWindow(LCD_Window_t *w)
{
    uint16_t x0 = w->x, y0 = w->y, wdt = w->width, hgt = w->height;
    switch (lcd_orientation) {
        case 0:
            break;
        case 90:
            /* 逻辑窗口 (x,y,w,h) -> 物理窗口 (y, PHY_H - x - w, h, w) */
            w->x = y0;
            w->y = LCD_PHYS_HEIGHT - x0 - wdt;
            w->width = hgt;
            w->height = wdt;
            break;
        case 180:
            w->x = LCD_PHYS_WIDTH - x0 - wdt;
            w->y = LCD_PHYS_HEIGHT - y0 - hgt;
            break;
        case 270:
            w->x = LCD_PHYS_WIDTH - y0 - hgt;
            w->y = x0;
            w->width = hgt;
            w->height = wdt;
            break;
        default:
            break;
    }
    /* 裁剪以防越界 */
    if (w->x + w->width > LCD_PHYS_WIDTH)  w->width = LCD_PHYS_WIDTH - w->x;
    if (w->y + w->height > LCD_PHYS_HEIGHT) w->height = LCD_PHYS_HEIGHT - w->y;
}

/*=============================================================================
 *                               公共 API 实现
 *============================================================================*/

/**
  * @brief  设置屏幕旋转角度
  * @param  angle 角度（0,90,180,270）
  * @note   只接受 90 的倍数，其他值忽略。
  */
void BSP_LCD_SetOrientation(uint16_t angle)
{
    if (angle % 90 == 0) {
        lcd_orientation = angle % 360;   /* 确保在 0~359 范围内 */
    }
}

/**
  * @brief  设置像素格式
  * @param  format 格式枚举
  * @note   同步更新像素字节数、DMA2D 输出格式和 LTDC 图层像素格式。
  */
void BSP_LCD_SetPixelFormat(LCD_PixelFormat_t format)
{
    lcd_pixel_format = format;
    
    /* 需要同步更新 LTDC 图层配置，否则显存数据解析会出错 */
    uint32_t ltdc_pixel_format;
    
    switch (format) {
        case LCD_PIXEL_FORMAT_RGB565:
            lcd_pixel_bytes = 2;
            lcd_dma2d_output_fmt = DMA2D_OUTPUT_RGB565;
            ltdc_pixel_format = LTDC_PIXEL_FORMAT_RGB565;
            break;
        case LCD_PIXEL_FORMAT_RGB888:
            lcd_pixel_bytes = 3;
            lcd_dma2d_output_fmt = DMA2D_OUTPUT_RGB888;
            ltdc_pixel_format = LTDC_PIXEL_FORMAT_RGB888;
            break;
        case LCD_PIXEL_FORMAT_ARGB8888:
            lcd_pixel_bytes = 4;
            lcd_dma2d_output_fmt = DMA2D_OUTPUT_ARGB8888;
            ltdc_pixel_format = LTDC_PIXEL_FORMAT_ARGB8888;
            break;
        default:
            return;
    }
    
    /* 更新图层0（背景层）的像素格式 */
    HAL_LTDC_ConfigLayer(&hltdc, &(LTDC_LayerCfgTypeDef){
        .WindowX0 = 0,
        .WindowX1 = LCD_PHYS_WIDTH-1,
        .WindowY0 = 0,
        .WindowY1 = LCD_PHYS_HEIGHT-1,
        .PixelFormat = ltdc_pixel_format,
        .Alpha = 0xFF,
        .Alpha0 = 0,
        .BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA,
        .BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA,
        .FBStartAdress = LCD_BG_LAYER_ADDR,
        .ImageWidth = LCD_PHYS_WIDTH,
        .ImageHeight = LCD_PHYS_HEIGHT,
        .Backcolor = {0, 0, 0}
    }, 0);
    
    /* 更新图层1（前景层）的像素格式 */
    HAL_LTDC_ConfigLayer(&hltdc, &(LTDC_LayerCfgTypeDef){
        .WindowX0 = 0,
        .WindowX1 = LCD_PHYS_WIDTH-1,
        .WindowY0 = 0,
        .WindowY1 = LCD_PHYS_HEIGHT-1,
        .PixelFormat = ltdc_pixel_format,
        .Alpha = 0xFF,
        .Alpha0 = 0,
        .BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA,
        .BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA,
        .FBStartAdress = LCD_FG_LAYER_ADDR,
        .ImageWidth = LCD_PHYS_WIDTH,
        .ImageHeight = LCD_PHYS_HEIGHT,
        .Backcolor = {0, 0, 0}
    }, 1);
    
}

/*-------------------- 图层操作 --------------------*/

/**
  * @brief  获取指定图层的基地址
  */
uint32_t BSP_LCD_GetLayerAddr(LCD_Layer_t layer)
{
    return (layer == LCD_LAYER_BG) ? LCD_BG_LAYER_ADDR : LCD_FG_LAYER_ADDR;
}

/**
  * @brief  获取当前激活图层
  */
LCD_Layer_t BSP_LCD_GetActiveLayer(void)
{
    return currentLayer;
}

/**
  * @brief  设置当前激活图层
  */
void BSP_LCD_SetActiveLayer(LCD_Layer_t layer)
{
    if (layer == LCD_LAYER_BG || layer == LCD_LAYER_FG) {
        currentLayer = layer;
    }
}

/**
  * @brief  设置图层的逻辑窗口（裁剪区域）
  * @param  layer 目标图层
  * @param  window 窗口结构（逻辑坐标）
  * @retval HAL_OK 成功，HAL_ERROR 窗口无效或超出物理范围
  * @note   窗口用于裁剪后续绘制，超出窗口的像素被忽略。
  */
HAL_StatusTypeDef BSP_LCD_SetLayerWindow(LCD_Layer_t layer, LCD_Window_t window)
{
    /* 校验窗口是否在逻辑尺寸范围内（逻辑尺寸等于物理尺寸） */
    if ((window.x + window.width > LCD_PHYS_WIDTH) || (window.y + window.height > LCD_PHYS_HEIGHT) ||
        (window.width == 0U) || (window.height == 0U)) {
        return HAL_ERROR;
    }

    if (layer == LCD_LAYER_BG) {
        memcpy(&lcd_bg_window, &window, sizeof(LCD_Window_t));
    } else if (layer == LCD_LAYER_FG) {
        memcpy(&lcd_fg_window, &window, sizeof(LCD_Window_t));
    } else {
        return HAL_ERROR;
    }
    return HAL_OK;
}

/**
  * @brief  获取指定图层的逻辑窗口
  */
LCD_Window_t BSP_LCD_GetLayerWindow(LCD_Layer_t layer)
{
    return (layer == LCD_LAYER_BG) ? lcd_bg_window : lcd_fg_window;
}

/*-------------------- DMA2D 填充（硬件加速） --------------------*/

/**
  * @brief  使用 DMA2D 以指定颜色填充矩形区域（物理坐标）
  * @param  addr  目标显存起始地址（物理地址）
  * @param  width 填充宽度（像素，物理坐标）
  * @param  height 填充高度（像素，物理坐标）
  * @param  color 填充颜色（已转换为当前格式的 uint32_t 值）
  * @retval HAL_OK 成功，HAL_ERROR 参数无效，HAL_TIMEOUT 超时
  * @note   行偏移设置为 (LCD_PHYS_WIDTH - width)，确保跨行连续。
  *         超时后会停止 DMA2D 并清除状态，避免挂死。
  *         此函数是底层加速核心，清屏和实心矩形均依赖它。
  */
HAL_StatusTypeDef BSP_LCD_DMA2D_Fill(uint32_t addr, uint16_t width, uint16_t height, uint32_t color)
{
    if ((width == 0U) || (height == 0U) || (addr == 0U)) {
        return HAL_ERROR;
    }

    uint32_t tick_start = HAL_GetTick();

    /* 如果 DMA2D 正在忙，先停止（防止冲突） */
    if (DMA2D->CR & DMA2D_CR_START) {
        DMA2D->CR &= ~DMA2D_CR_START;
        while (DMA2D->CR & DMA2D_CR_START) {
            if ((HAL_GetTick() - tick_start) > DMA2D_TIMEOUT_MS) {
                return HAL_TIMEOUT;
            }
        }
    }

    /* 清除所有中断标志（传输错误、传输完成、配置错误、CTC） */
    DMA2D->IFCR = DMA2D_FLAG_TE | DMA2D_FLAG_TC | DMA2D_FLAG_CE | DMA2D_FLAG_CTC;

    /* 配置 DMA2D 为寄存器到内存模式（R2M） */
    DMA2D->CR = DMA2D_R2M;
    DMA2D->OPFCCR = lcd_dma2d_output_fmt;      /* 输出格式（动态） */
    DMA2D->OOR = (LCD_PHYS_WIDTH - width);     /* 行偏移（物理宽度 - 窗口宽度） */
    DMA2D->NLR = (width << 16) | height;       /* 高16位：每行像素数，低16位：行数 */
    DMA2D->OMAR = addr;                        /* 输出内存地址 */
    DMA2D->OCOLR = color;                      /* 填充颜色值（已转换为当前格式） */

    /* 启动传输 */
    DMA2D->CR |= DMA2D_CR_START;

    /* 等待传输完成标志 TCIF */
    while ((DMA2D->ISR & DMA2D_FLAG_TC) == 0U) {
        if ((HAL_GetTick() - tick_start) > DMA2D_TIMEOUT_MS) {
            /* 超时：停止 DMA2D，清除标志，返回错误 */
            DMA2D->CR &= ~DMA2D_CR_START;
            DMA2D->IFCR = DMA2D_FLAG_TE | DMA2D_FLAG_TC | DMA2D_FLAG_CE | DMA2D_FLAG_CTC;
            return HAL_TIMEOUT;
        }
    }
    /* 清除完成标志 */
    DMA2D->IFCR = DMA2D_FLAG_TC;
    return HAL_OK;
}

/*-------------------- 清屏操作 --------------------*/

/**
  * @brief  清空当前激活图层的逻辑窗口区域（使用当前背景色或指定颜色）
  * @param  color 填充颜色（RGB565）
  * @note   内部将逻辑窗口映射为物理窗口，再调用 DMA2D_Fill 加速。
  *         如果窗口不是全屏，仅填充窗口区域。
  */
void BSP_LCD_ClearActiveLayer(uint16_t color)
{
    LCD_Layer_t active = BSP_LCD_GetActiveLayer();
    LCD_Window_t window = BSP_LCD_GetLayerWindow(active);
    uint32_t layer_addr = BSP_LCD_GetLayerAddr(active);

    /* 逻辑窗口映射到物理窗口 */
    LCD_Window_t phys_window = window;
    LCD_MapWindow(&phys_window);

    /* 计算物理地址偏移：基址 + (y * 物理宽度 + x) * 像素字节 */
    uint32_t window_addr = layer_addr + (phys_window.y * LCD_PHYS_WIDTH + phys_window.x) * lcd_pixel_bytes;

    uint32_t converted_color = LCD_ConvertColor(color);
    (void)BSP_LCD_DMA2D_Fill(window_addr, phys_window.width, phys_window.height, converted_color);
}

/**
  * @brief  填充背景层全屏
  * @note   始终使用物理尺寸填充，不考虑旋转角度。
  *         旋转仅影响单个像素的坐标映射（通过 LCD_MapPixel/LCD_MapWindow），
  *         整层填充应覆盖整个物理显存缓冲区，否则旋转后会出现填充不完整的问题。
  */
void BSP_LCD_FillBackground(uint16_t color)
{
    uint32_t addr = LCD_BG_LAYER_ADDR;
    uint32_t converted = LCD_ConvertColor(color);
    BSP_LCD_DMA2D_Fill(addr, LCD_PHYS_WIDTH, LCD_PHYS_HEIGHT, converted);
}

/**
  * @brief  填充前景层全屏
  * @note   始终使用物理尺寸填充，不考虑旋转角度。
  *         旋转仅影响单个像素的坐标映射（通过 LCD_MapPixel/LCD_MapWindow），
  *         整层填充应覆盖整个物理显存缓冲区，否则旋转后会出现填充不完整的问题。
  */
void BSP_LCD_FillForeground(uint16_t color)
{
    uint32_t addr = LCD_FG_LAYER_ADDR;
    uint32_t converted = LCD_ConvertColor(color);
    BSP_LCD_DMA2D_Fill(addr, LCD_PHYS_WIDTH, LCD_PHYS_HEIGHT, converted);
}

/*-------------------- 基础图形绘制 --------------------*/

/**
  * @brief  绘制一个像素（逻辑坐标）
  * @param  x, y 逻辑坐标
  * @param  color RGB565 颜色
  * @note   自动检查逻辑窗口裁剪，并映射到物理坐标后写入显存。
  *         根据当前像素字节数，使用不同的写入宽度（2/3/4 字节）。
  */
void BSP_LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    /* 逻辑坐标边界检查（逻辑尺寸 = 物理尺寸） */
    if ((x >= LCD_PHYS_WIDTH) || (y >= LCD_PHYS_HEIGHT)) return;

    /* 检查是否在当前图层的逻辑窗口内 */
    LCD_Layer_t active = BSP_LCD_GetActiveLayer();
    LCD_Window_t window = BSP_LCD_GetLayerWindow(active);
    if ((x < window.x) || (x >= (window.x + window.width)) ||
        (y < window.y) || (y >= (window.y + window.height))) {
        return;
    }

    /* 映射到物理坐标 */
    uint16_t phys_x = x, phys_y = y;
    LCD_MapPixel(&phys_x, &phys_y);

    /* 计算物理地址 */
    uint32_t layerAddr = BSP_LCD_GetLayerAddr(active);
    uint32_t pixelAddr = layerAddr + (phys_y * LCD_PHYS_WIDTH + phys_x) * lcd_pixel_bytes;

    uint32_t converted = LCD_ConvertColor(color);
    /* 根据像素字节数写入 */
    switch (lcd_pixel_bytes) {
        case 2:
            *((__IO uint16_t*)pixelAddr) = (uint16_t)converted;
            break;
        case 3:
            /* RGB888 存储顺序 R,G,B (按地址递增) */
            *((__IO uint8_t*)pixelAddr)     = (converted >> 16) & 0xFF; /* R */
            *((__IO uint8_t*)(pixelAddr+1)) = (converted >> 8)  & 0xFF; /* G */
            *((__IO uint8_t*)(pixelAddr+2)) =  converted        & 0xFF; /* B */
            break;
        case 4:
            /* ARGB8888 存储顺序 A,R,G,B (按地址递增) */
            *((__IO uint32_t*)pixelAddr) = converted;
            break;
        default:
            break;
    }
}

/**
  * @brief  绘制直线（Bresenham 算法）
  * @param  x1,y1 起点（逻辑坐标）
  * @param  x2,y2 终点（逻辑坐标）
  * @param  color RGB565 颜色
  * @note   算法通过误差项决定步进方向，避免浮点运算，效率高。
  */
void BSP_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx = abs((int16_t)(x2 - x1));
    int16_t dy = abs((int16_t)(y2 - y1));
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    while (1) {
        BSP_LCD_DrawPixel(x1, y1, color);
        if ((x1 == x2) && (y1 == y2)) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }
}

/**
  * @brief  绘制空心圆（中点圆算法）
  * @param  x0,y0 圆心（逻辑坐标）
  * @param  radius 半径
  * @param  color RGB565 颜色
  * @note   利用八分对称性，只计算 1/8 圆弧，其余对称复制。
  */
void BSP_LCD_DrawCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    if (radius == 0) {
        BSP_LCD_DrawPixel(x0, y0, color);
        return;
    }

    while (x >= y) {
        /* 八个对称点 */
        BSP_LCD_DrawPixel(x0 + x, y0 + y, color);
        BSP_LCD_DrawPixel(x0 + y, y0 + x, color);
        BSP_LCD_DrawPixel(x0 - y, y0 + x, color);
        BSP_LCD_DrawPixel(x0 - x, y0 + y, color);
        BSP_LCD_DrawPixel(x0 - x, y0 - y, color);
        BSP_LCD_DrawPixel(x0 - y, y0 - x, color);
        BSP_LCD_DrawPixel(x0 + y, y0 - x, color);
        BSP_LCD_DrawPixel(x0 + x, y0 - y, color);

        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

/**
  * @brief  绘制实心圆（利用水平扫描线填充）
  * @param  x0,y0 圆心
  * @param  radius 半径
  * @param  color RGB565 颜色
  * @note   对于每一行，绘制从 (x0 - x) 到 (x0 + x) 的水平线。
  */
void BSP_LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    if (radius == 0) {
        BSP_LCD_DrawPixel(x0, y0, color);
        return;
    }

    while (x >= y) {
        /* 四条水平扫描线（上下对称） */
        BSP_LCD_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        BSP_LCD_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        BSP_LCD_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        BSP_LCD_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);

        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

/**
  * @brief  绘制空心矩形
  * @param  x1,y1 左上角（逻辑坐标）
  * @param  x2,y2 右下角（逻辑坐标）
  * @param  color RGB565 颜色
  * @note   归一化坐标后绘制四条边。
  */
void BSP_LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t x_start = (x1 < x2) ? x1 : x2;
    uint16_t x_end   = (x1 < x2) ? x2 : x1;
    uint16_t y_start = (y1 < y2) ? y1 : y2;
    uint16_t y_end   = (y1 < y2) ? y2 : y1;

    BSP_LCD_DrawLine(x_start, y_start, x_end, y_start, color);
    BSP_LCD_DrawLine(x_end,   y_start, x_end, y_end, color);
    BSP_LCD_DrawLine(x_end,   y_end,   x_start, y_end, color);
    BSP_LCD_DrawLine(x_start, y_end,   x_start, y_start, color);
}

/**
  * @brief  绘制实心矩形（逐行填充）
  * @param  x1,y1 左上角
  * @param  x2,y2 右下角
  * @param  color RGB565 颜色
  * @note   对于每一行调用 DrawLine，可优化为 DMA2D 但此处保持简单。
  */
void BSP_LCD_FillRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t x_start = (x1 < x2) ? x1 : x2;
    uint16_t x_end   = (x1 < x2) ? x2 : x1;
    uint16_t y_start = (y1 < y2) ? y1 : y2;
    uint16_t y_end   = (y1 < y2) ? y2 : y1;

    for (uint16_t y = y_start; y <= y_end; y++) {
        BSP_LCD_DrawLine(x_start, y, x_end, y, color);
    }
}

/*-------------------- 字符/字符串绘制 --------------------*/

/**
  * @brief  在当前光标位置绘制一个 ASCII 字符（32~126）
  * @param  c 待绘制字符
  * @note   从字体表中读取点阵数据，逐行逐位绘制。
  *         若 lcd_bg_transparent=1，则跳过背景像素绘制，实现透明效果。
  *         光标位置不自动更新（由调用者管理）。
  */
void BSP_LCD_DrawChar(char c)
{
    /* 仅支持可打印 ASCII */
    if ((c < ' ') || (c > '~') || (lcd_font == NULL)) return;

    uint16_t x = lcd_cursor_x;
    uint16_t y = lcd_cursor_y;
    uint16_t color = lcd_fgColor;
    uint16_t bgColor = lcd_bgColor;
    sFONT* font = lcd_font;

    if ((x >= LCD_PHYS_WIDTH) || (y >= LCD_PHYS_HEIGHT)) return;

    uint16_t charWidth = font->Width;
    uint16_t charHeight = font->Height;
    /* 计算字符点阵在字体表中的偏移量：
       每个字符占 charHeight 行，每行占 (charWidth+7)/8 字节 */
    uint32_t charOffset = (c - ' ') * charHeight * ((charWidth + 7U) / 8U);
    const uint8_t* charData = &font->table[charOffset];

    /* 逐行扫描 */
    for (uint16_t row = 0; row < charHeight; row++) {
        uint16_t pixel_y = y + row;
        if (pixel_y >= LCD_PHYS_HEIGHT) break;

        for (uint16_t byte = 0; byte < ((charWidth + 7U) / 8U); byte++) {
            uint8_t data = charData[row * ((charWidth + 7U) / 8U) + byte];
            /* 每位代表一个像素，高位在前（MSB first） */
            for (uint16_t bit = 0; bit < 8 && (byte * 8 + bit) < charWidth; bit++) {
                uint16_t pixel_x = x + byte * 8 + bit;
                if (pixel_x >= LCD_PHYS_WIDTH) continue;
                if (data & (1U << (7U - bit))) {
                    /* 笔划像素：前景色 */
                    BSP_LCD_DrawPixel(pixel_x, pixel_y, color);
                } else if (!lcd_bg_transparent) {
                    /* 背景像素：绘制背景色（不透明模式） */
                    BSP_LCD_DrawPixel(pixel_x, pixel_y, bgColor);
                }
                /* 透明模式：背景像素保持原样 */
            }
        }
    }
}

/**
  * @brief  绘制字符串（自动处理换行、回车、自动换行）
  * @param  str 以 '\0' 结尾的字符串
  * @note   光标位置会更新，若超出屏幕底部则清屏并重置光标到顶部。
  *         '\n' 换行并回到行首，'\r' 仅回到行首。
  *         自动换行发生在当前行剩余空间不足以容纳下一个字符时。
  */
void BSP_LCD_DrawString(const char* str)
{
    if ((str == NULL) || (lcd_font == NULL)) return;

    uint16_t charWidth = lcd_font->Width;
    uint16_t charHeight = lcd_font->Height;
    uint16_t lineStartX = lcd_cursor_x;   /* 记录当前行起始 X，用于 '\r' 和换行 */

    while (*str) {
        if (*str == '\n') {
            /* 换行：回到行首，Y 增加一行高度加行间距 */
            lcd_cursor_x = lineStartX;
            lcd_cursor_y += charHeight + LCD_LINE_OFFSET;
            if (lcd_cursor_y >= LCD_PHYS_HEIGHT) {
                /* 超出底部：清屏并重置光标到顶部 */
                lcd_cursor_y = 0U;
                BSP_LCD_ClearActiveLayer(lcd_bgColor);
            }
            str++;
            continue;
        }
        if (*str == '\r') {
            /* 回车：仅回到行首 */
            lcd_cursor_x = lineStartX;
            str++;
            continue;
        }
        /* 检查当前行剩余宽度是否足够 */
        if ((lcd_cursor_x + charWidth + LCD_CHAR_SPACE) >= LCD_PHYS_WIDTH) {
            /* 不足：换行 */
            lcd_cursor_x = lineStartX;
            lcd_cursor_y += charHeight + LCD_LINE_OFFSET;
            if (lcd_cursor_y >= LCD_PHYS_HEIGHT) {
                lcd_cursor_y = 0U;
                BSP_LCD_ClearActiveLayer(lcd_bgColor);
            }
        }
        BSP_LCD_DrawChar(*str);
        lcd_cursor_x += charWidth + LCD_CHAR_SPACE;
        str++;
    }
}

/*-------------------- 数值绘制 --------------------*/

/**
  * @brief  绘制带符号十进制整数
  * @param  num 32位有符号整数
  * @note   特殊处理 INT32_MIN 以防溢出。
  *         内部转为字符串后逐个字符绘制，自动换行。
  */
void BSP_LCD_DrawDec(int32_t num)
{
    if (lcd_font == NULL) return;

    /* INT32_MIN 取反会溢出，单独处理 */
    if (num == INT32_MIN) {
        BSP_LCD_DrawString("-2147483648");
        return;
    }

    char buf[16] = {0};
    char* p = buf + 15;
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
    if (isNegative) *(--p) = '-';

    /* 输出 */
    while (*p) {
        if ((lcd_cursor_x + lcd_font->Width + LCD_CHAR_SPACE) >= LCD_PHYS_WIDTH) {
            lcd_cursor_x = 0U;
            lcd_cursor_y += lcd_font->Height + LCD_LINE_OFFSET;
            if (lcd_cursor_y >= LCD_PHYS_HEIGHT) {
                lcd_cursor_y = 0U;
                BSP_LCD_ClearActiveLayer(lcd_bgColor);
            }
        }
        BSP_LCD_DrawChar(*p);
        lcd_cursor_x += lcd_font->Width + LCD_CHAR_SPACE;
        p++;
    }
}

/**
  * @brief  绘制十六进制整数（自动添加 "0x" 前缀）
  * @param  num 32位无符号整数
  * @note   输出格式如 "0x12AB"，字母大写。
  */
void BSP_LCD_DrawHex(uint32_t num)
{
    if (lcd_font == NULL) return;

    char buf[12] = {0};
    char* p = buf + 11;
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

    while (*p) {
        if ((lcd_cursor_x + lcd_font->Width + LCD_CHAR_SPACE) >= LCD_PHYS_WIDTH) {
            lcd_cursor_x = 0U;
            lcd_cursor_y += lcd_font->Height + LCD_LINE_OFFSET;
            if (lcd_cursor_y >= LCD_PHYS_HEIGHT) {
                lcd_cursor_y = 0U;
                BSP_LCD_ClearActiveLayer(lcd_bgColor);
            }
        }
        BSP_LCD_DrawChar(*p);
        lcd_cursor_x += lcd_font->Width + LCD_CHAR_SPACE;
        p++;
    }
}

/**
  * @brief  绘制浮点数（四舍五入到指定小数位数）
  * @param  fnum 浮点数
  * @param  decimals 小数位数（0~15）
  * @note   内部使用 roundf 进行四舍五入，防止精度误差。
  *         负数、整数部分、小数部分分别处理。
  */
void BSP_LCD_DrawFloat(float fnum, uint8_t decimals)
{
    if (lcd_font == NULL || decimals > 15) return;

    char buf[32] = {0};
    char* p = buf;
    uint8_t isNegative = 0;
    int32_t intPart;
    uint32_t fracPart = 0;
    uint32_t multiplier = 1;

    decimals = (decimals > 15) ? 15 : decimals;
    for (uint8_t i = 0; i < decimals; i++) multiplier *= 10;

    if (fnum < 0.0f) {
        isNegative = 1;
        fnum = -fnum;
    }

    intPart = (int32_t)fnum;
    /* 小数部分 = (fnum - intPart) * multiplier，加 0.5 四舍五入 */
    fracPart = (uint32_t)((fnum - (float)intPart) * (float)multiplier + 0.5f);
    if (fracPart >= multiplier) {
        fracPart = 0;
        intPart++;
    }

    if (isNegative) *p++ = '-';

    /* 整数部分 */
    if (intPart == 0) {
        *p++ = '0';
    } else {
        char intBuf[16] = {0};
        char* pi = intBuf + 15;
        *pi = '\0';
        int32_t temp = intPart;
        while (temp > 0) {
            *(--pi) = '0' + (temp % 10);
            temp /= 10;
        }
        while (*pi) *p++ = *pi++;
    }

    /* 小数部分 */
    if (decimals > 0) {
        *p++ = '.';
        char fracBuf[16] = {0};
        char* pf = fracBuf + 15;
        *pf = '\0';
        uint32_t temp = fracPart;
        uint8_t digits = 0;
        while (temp > 0 && digits < decimals) {
            *(--pf) = '0' + (temp % 10);
            temp /= 10;
            digits++;
        }
        while (digits < decimals) {
            *(--pf) = '0';
            digits++;
        }
        while (*pf) *p++ = *pf++;
    }

    /* 输出 */
    p = buf;
    while (*p) {
        if ((lcd_cursor_x + lcd_font->Width + LCD_CHAR_SPACE) >= LCD_PHYS_WIDTH) {
            lcd_cursor_x = 0U;
            lcd_cursor_y += lcd_font->Height + LCD_LINE_OFFSET;
            if (lcd_cursor_y >= LCD_PHYS_HEIGHT) {
                lcd_cursor_y = 0U;
                BSP_LCD_ClearActiveLayer(lcd_bgColor);
            }
        }
        BSP_LCD_DrawChar(*p);
        lcd_cursor_x += lcd_font->Width + LCD_CHAR_SPACE;
        p++;
    }
}

/*-------------------- 格式化输出 --------------------*/

/**
  * @brief  类似 printf 的格式化输出到 LCD
  * @param  fmt 格式化字符串
  * @param  ... 可变参数
  * @note   使用 vsnprintf 将结果格式化到缓冲区，再调用 DrawString。
  *         缓冲区大小 256 字节，注意避免溢出。
  */
void BSP_LCD_Printf(const char* fmt, ...)
{
    if (fmt == NULL) return;
    char buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    BSP_LCD_DrawString(buf);
}

/*-------------------- Setter 函数 --------------------*/

void BSP_LCD_SetCursor(uint16_t x, uint16_t y)
{
    lcd_cursor_x = (x < LCD_PHYS_WIDTH) ? x : LCD_PHYS_WIDTH - 1;
    lcd_cursor_y = (y < LCD_PHYS_HEIGHT) ? y : LCD_PHYS_HEIGHT - 1;
}

void BSP_LCD_SetColor(uint16_t color) { lcd_fgColor = color; }
void BSP_LCD_SetBgColor(uint16_t color) { lcd_bgColor = color; }
void BSP_LCD_SetBgTransparent(uint8_t enable) { lcd_bg_transparent = enable ? 1 : 0; }
void BSP_LCD_SetFont(sFONT* font) { if (font) lcd_font = font; }

/*-------------------- 初始化 --------------------*/

/**
  * @brief  LCD 底层初始化
  * @note   使能 DMA2D 时钟，初始化 DMA2D 句柄，设置默认格式和旋转，清屏。
  *         若 HAL_DMA2D_Init 失败则调用 Error_Handler()。
  *         用户需确保 LTDC 已正确初始化且 SDRAM 可用。
  */
void BSP_LCD_Init(void)
{
    /* 使能 DMA2D 时钟 */
    __HAL_RCC_DMA2D_CLK_ENABLE();

    /* 初始化 DMA2D 句柄（使用默认 R2M 模式） */
    hdma2d.Instance = DMA2D;
    hdma2d.Init.Mode = DMA2D_R2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;  /* 初始值，后续动态更新 */
    hdma2d.Init.OutputOffset = 0U;

    if (HAL_DMA2D_Init(&hdma2d) != HAL_OK) {
        Error_Handler();   /* 用户需实现此函数 */
    }

    /* 设置默认格式（RGB565）和旋转（0°） */
    BSP_LCD_SetPixelFormat(LCD_PIXEL_FORMAT_RGB565);
    BSP_LCD_SetOrientation(0);

    /* 清空两层为黑色 */
    BSP_LCD_FillBackground(COLOR_BLACK);
    BSP_LCD_FillForeground(COLOR_BLACK);
}

/*-------------------- 测试函数 --------------------*/

/**
  * @brief  完整的 LCD 功能测试
  * @note   依次演示：
  *         - 基本图形（线、圆、矩形）
  *         - 四种旋转角度切换
  *         - 三种像素格式切换
  *         - 字符、字符串、数值、浮点数、printf 输出
  *         每步延时 2 秒以便观察，最终清屏。
  */
void BSP_LCD_Test(void)
{
    BSP_LCD_Init();

    /* 1. 横屏 0° RGB565 基本图形 */
    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_SetActiveLayer(LCD_LAYER_FG);
    BSP_LCD_SetColor(COLOR_GREEN);
    BSP_LCD_DrawLine(100, 100, 700, 400, COLOR_RED);
    BSP_LCD_DrawCircle(400, 240, 100, COLOR_YELLOW);
    BSP_LCD_DrawRectangle(200, 150, 600, 350, COLOR_BLUE);
    HAL_Delay(2000);

    /* 2. 90° 竖屏 */
    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_SetOrientation(90);
    BSP_LCD_DrawLine(100, 100, 300, 400, COLOR_RED);
    BSP_LCD_DrawCircle(300, 200, 80, COLOR_YELLOW);
    BSP_LCD_DrawRectangle(150, 100, 450, 300, COLOR_BLUE);
    HAL_Delay(2000);

    /* 3. 180° */
    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_SetOrientation(180);
    BSP_LCD_DrawLine(100, 100, 700, 400, COLOR_GREEN);
    BSP_LCD_DrawCircle(400, 240, 120, COLOR_MAGENTA);
    HAL_Delay(2000);

    /* 4. 270° */
    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_SetOrientation(270);
    BSP_LCD_DrawLine(200, 50, 500, 430, COLOR_CYAN);
    BSP_LCD_DrawCircle(600, 300, 70, COLOR_ORANGE);
    HAL_Delay(2000);

    /* 5. 回到 0°，切换 RGB888 */
    BSP_LCD_SetOrientation(0);
    BSP_LCD_SetPixelFormat(LCD_PIXEL_FORMAT_RGB888);
    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_SetColor(COLOR_RED);
    BSP_LCD_DrawString("RGB888 Test");
    BSP_LCD_SetCursor(0, 50);
    BSP_LCD_DrawCircle(200, 200, 80, COLOR_BLUE);
    HAL_Delay(2000);

    /* 6. 切换 ARGB8888 */
    BSP_LCD_SetPixelFormat(LCD_PIXEL_FORMAT_ARGB8888);
    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_SetColor(COLOR_GREEN);
    BSP_LCD_DrawString("ARGB8888 Test");
    BSP_LCD_SetCursor(0, 50);
    BSP_LCD_DrawRectangle(50, 100, 350, 300, COLOR_YELLOW);
    HAL_Delay(2000);

    /* 7. 回到 RGB565，显示当前配置 */
    BSP_LCD_SetPixelFormat(LCD_PIXEL_FORMAT_RGB565);
    BSP_LCD_SetOrientation(0);
    BSP_LCD_FillForeground(COLOR_BLACK);
    BSP_LCD_SetCursor(20, 20);
    BSP_LCD_SetColor(COLOR_WHITE);
    BSP_LCD_Printf("Orientation: %d", lcd_orientation);
    BSP_LCD_SetCursor(20, 60);
    BSP_LCD_Printf("Pixel Format: %d", lcd_pixel_format);
    BSP_LCD_SetCursor(20, 100);
    BSP_LCD_Printf("Test completed!");
    HAL_Delay(3000);

    /* 最终清屏 */
    BSP_LCD_FillForeground(COLOR_BLACK);
}