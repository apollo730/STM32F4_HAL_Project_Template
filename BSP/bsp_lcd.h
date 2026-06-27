/**
  * @file    bsp_lcd.h
  * @brief   LCD 驱动头文件 - 基于 HAL 库，支持 STM32F429 LTDC + DMA2D，双图层，动态旋转，多种像素格式
  * @author  Your Name
  * @version V2.0
  * @date    2026-06-24
  * 
  * @details 本驱动提供以下核心功能（全部使用 HAL 库实现）：
  *          1. 双图层管理：背景层（BG）和前景层（FG），可独立设置窗口裁剪（软件裁剪）。
  *          2. 动态横竖屏切换：支持 0°、90°、180°、270° 旋转，所有绘图坐标自动映射。
  *          3. 动态像素格式：RGB565、RGB888、ARGB8888 运行时切换，颜色值统一以 RGB565 宏输入。
  *          4. 丰富绘图原语：点、线、圆（空心/实心）、矩形（空心/实心，实心使用 DMA2D 加速）。
  *          5. 字符/字符串显示：基于光标位置，支持换行、回车、自动换行，背景透明/不透明可选。
  *          6. 数值与格式化输出：十进制、十六进制、浮点数、printf 风格格式化。
  *          7. 硬件加速：DMA2D 块填充（使用 HAL 库 API），用于清屏和实心矩形，大幅提升性能。
  * 
  * @note   1. 所有坐标均为逻辑坐标（相对于当前旋转方向），内部自动映射到物理坐标。
  *         2. 颜色输入使用 RGB565 宏（如 COLOR_RED），驱动内部自动转换为当前格式。
  *         3. 需外部提供 DMA2D 句柄 hdma2d 和 LTDC 句柄 hltdc，并在调用本驱动前完成初始化。
  *         4. 物理尺寸 LCD_PHYS_WIDTH/HEIGHT 应与 LTDC 输出配置一致。
  *         5. 野火 STM32F429IGT6 配套 5 寸屏注意事项：ARGB8888 时 LTDC 时钟 ≤ 19MHz，RGB565 时 ≤ 25MHz。
  *         6. SDRAM 型号 W9825G6KH-6I 应配置为读突发（WB=1），突发长度 4。
  */

#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "fonts/fonts.h"          /* 字体结构，包含点阵数据 */
#include "stm32f4xx_hal.h"        /* HAL 库，提供基本类型和 DMA2D/LTDC 定义 */

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 *                               物理尺寸配置
 *============================================================================*/
/**
  * @defgroup LCD_Physical_Size 物理尺寸（不可变更，须匹配 LTDC 输出）
  * @{
  */
#define LCD_PHYS_WIDTH           800U   /*!< LCD 物理横向像素数（固定） */
#define LCD_PHYS_HEIGHT          480U   /*!< LCD 物理纵向像素数（固定） */
#define DMA2D_TIMEOUT_MS         100U   /*!< DMA2D 操作超时阈值（毫秒） */
/** @} */

/*=============================================================================
 *                               字符串绘制参数
 *============================================================================*/
/**
  * @defgroup LCD_Text_Params 文本绘制参数（具体可调）
  * @note  用于控制文本绘制时的行间距和字符间距，具体间距由字体点阵决定。不同字体最小间距不一致。
  *        例如，Font24 字体的最小行间距为 -5 像素，字符间距为 -2 像素。如果需要调整，可修改此结构体中的值。
  * @{
  */
#define LCD_LINE_OFFSET          -4     /*!< 行间距（像素），用于自动换行时增加的行间隔 */
#define LCD_CHAR_SPACE           -2     /*!< 字符间距（像素），字符之间的额外间隔 */
/** @} */

/*=============================================================================
 *                               图层显存地址
 *============================================================================*/
/**
  * @defgroup LCD_Layer_Addresses 图层 SDRAM 基地址（需与链接脚本一致）
  * @{
  */
#define LCD_BG_LAYER_ADDR        0xD0000000U   /*!< 背景层显存基地址（SDRAM） */
#define LCD_FG_LAYER_ADDR        0xD0200000U   /*!< 前景层显存基地址（SDRAM） */
/** @} */

/*=============================================================================
 *                               像素格式枚举
 *============================================================================*/
/**
  * @brief 支持的像素格式类型
  * @note  需与 LTDC 图层配置的像素格式保持一致，否则显示异常。
  */
typedef enum {
    LCD_PIXEL_FORMAT_RGB565   = 0,  /*!< 16位 RGB565：R(5) G(6) B(5) */
    LCD_PIXEL_FORMAT_RGB888   = 1,  /*!< 24位 RGB888：R(8) G(8) B(8)，内存连续排列 */
    LCD_PIXEL_FORMAT_ARGB8888 = 2   /*!< 32位 ARGB8888：A(8) R(8) G(8) B(8)，内存连续 */
} LCD_PixelFormat_t;

/*=============================================================================
 *                               颜色宏定义（RGB565）
 *============================================================================*/
/**
  * @defgroup LCD_Colors 常用颜色定义（均以 RGB565 格式提供）
  * @note  内部转换函数 LCD_ConvertColor 会将 RGB565 转为当前格式。
  * @{
  */
#define RGB565(r,g,b)       ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)) /*!< 8-8-8 -> RGB565 宏 */

#define COLOR_WHITE         RGB565(255,255,255)
#define COLOR_BLACK         RGB565(0,0,0)
#define COLOR_RED           RGB565(255,0,0)
#define COLOR_GREEN         RGB565(0,255,0)
#define COLOR_BLUE          RGB565(0,0,255)
#define COLOR_YELLOW        RGB565(255,255,0)
#define COLOR_CYAN          RGB565(0,255,255)
#define COLOR_MAGENTA       RGB565(255,0,255)
#define COLOR_ORANGE        RGB565(255,165,0)
#define COLOR_PURPLE        RGB565(128,0,128)
#define COLOR_PINK          RGB565(255,192,203)
#define COLOR_BROWN         RGB565(165,42,42)
#define COLOR_GRAY          RGB565(128,128,128)
#define COLOR_LIGHT_GRAY    RGB565(192,192,192)
#define COLOR_DARK_RED      RGB565(128,0,0)
#define COLOR_DARK_GREEN    RGB565(0,128,0)
#define COLOR_DARK_BLUE     RGB565(0,0,128)
/** @} */

/*=============================================================================
 *                               结构体定义
 *============================================================================*/
/**
  * @brief LCD 窗口结构，用于图层裁剪区域（逻辑坐标）
  * @note  窗口内的像素才被绘制，窗口外被裁剪（软件裁剪）。
  */
typedef struct {
    uint16_t x;      /*!< 窗口左上角 X 坐标（逻辑坐标） */
    uint16_t y;      /*!< 窗口左上角 Y 坐标（逻辑坐标） */
    uint16_t width;  /*!< 窗口宽度（像素） */
    uint16_t height; /*!< 窗口高度（像素） */
} LCD_Window_t;

/**
  * @brief LCD 图层枚举
  */
typedef enum {
    LCD_LAYER_BG = 0,  /*!< 背景层（通常用于静态背景） */
    LCD_LAYER_FG = 1   /*!< 前景层（通常用于动态绘制） */
} LCD_Layer_t;

/*=============================================================================
 *                               全局状态变量（外部可见，但建议使用访问函数）
 *============================================================================*/
/**
  * @defgroup LCD_Global_Vars 全局状态变量（可读写，但建议使用 setter/getter）
  * @{
  */
extern LCD_Layer_t currentLayer;          /*!< 当前激活图层，所有绘图操作针对此图层 */
extern uint16_t lcd_cursor_x;             /*!< 光标 X 坐标（逻辑坐标），用于文本/数值输出 */
extern uint16_t lcd_cursor_y;             /*!< 光标 Y 坐标（逻辑坐标） */
extern uint16_t lcd_fgColor;              /*!< 当前前景色（RGB565 格式） */
extern uint16_t lcd_bgColor;              /*!< 当前背景色（RGB565 格式），当背景透明时忽略 */
extern uint8_t  lcd_bg_transparent;       /*!< 背景透明标志：1-透明（不绘制背景像素），0-不透明 */
extern sFONT*   lcd_font;                 /*!< 当前字体指针（如 &Font24） */
extern LCD_Window_t lcd_bg_window;        /*!< 背景层逻辑窗口（裁剪区域） */
extern LCD_Window_t lcd_fg_window;        /*!< 前景层逻辑窗口（裁剪区域） */

extern uint16_t lcd_orientation;          /*!< 当前旋转角度（0,90,180,270）单位：度 */
extern LCD_PixelFormat_t lcd_pixel_format;/*!< 当前像素格式枚举值 */
extern uint8_t lcd_pixel_bytes;           /*!< 每像素字节数（由格式自动决定） */
extern uint32_t lcd_dma2d_output_fmt;     /*!< DMA2D 输出格式寄存器值（由格式自动决定） */
/** @} */

/*=============================================================================
 *                               公共函数声明
 *============================================================================*/

/**
  * @brief 初始化 LCD 硬件（使能 DMA2D 时钟，初始化句柄，设置默认格式/旋转，清屏）
  * @note  必须在 LTDC 初始化之后调用，且需确保 SDRAM 已就绪。
  *        默认格式为 RGB565，旋转 0°，两图层清为黑色。
  *        若初始化失败，会调用 Error_Handler()（需用户实现）。
  */
void BSP_LCD_Init(void);

/**
  * @brief 动态设置屏幕旋转角度
  * @param angle 旋转角度，必须为 0, 90, 180, 270 之一（单位：度）
  * @note  仅接受 90 的倍数，其他值被忽略。
  *        切换后，所有后续绘图坐标将自动映射，但已绘制内容不会旋转，需用户重绘。
  */
void BSP_LCD_SetOrientation(uint16_t angle);

/**
  * @brief 获取当前旋转角度
  * @return 角度值（0,90,180,270）
  */
uint16_t BSP_LCD_GetOrientation(void);

/**
  * @brief 动态设置像素格式
  * @param format 格式枚举值
  * @note  切换后，颜色输入仍以 RGB565 宏提供，内部自动转换。
  *        需保证 LTDC 图层像素格式同步更新，否则显示错乱。
  *        同时更新 lcd_pixel_bytes 和 DMA2D 输出格式。
  */
void BSP_LCD_SetPixelFormat(LCD_PixelFormat_t format);

/**
  * @brief 获取当前像素格式
  * @return 格式枚举值
  */
LCD_PixelFormat_t BSP_LCD_GetPixelFormat(void);

/* ----- 图层管理 ----- */
LCD_Layer_t BSP_LCD_GetActiveLayer(void);
void BSP_LCD_SetActiveLayer(LCD_Layer_t layer);
uint32_t BSP_LCD_GetLayerAddr(LCD_Layer_t layer);

/**
  * @brief 设置图层的逻辑窗口（裁剪区域，软件裁剪）
  * @param layer 目标图层
  * @param window 窗口结构（逻辑坐标）
  * @retval HAL_OK 成功，HAL_ERROR 窗口无效或超出物理范围
  * @note   窗口用于裁剪后续绘制，超出窗口的像素被忽略。
  */
HAL_StatusTypeDef BSP_LCD_SetLayerWindow(LCD_Layer_t layer, LCD_Window_t window);

LCD_Window_t BSP_LCD_GetLayerWindow(LCD_Layer_t layer);

/* ----- DMA2D 底层填充（硬件加速，基于 HAL） ----- */
/**
  * @brief 使用 DMA2D 以指定颜色填充矩形区域（物理坐标，硬件加速）
  * @param addr  目标显存起始地址（物理地址）
  * @param width 填充宽度（像素，物理坐标）
  * @param height 填充高度（像素，物理坐标）
  * @param color 填充颜色（已转换为当前格式的 uint32_t 值）
  * @retval HAL_OK 成功，HAL_ERROR 参数无效，HAL_TIMEOUT 超时
  * @note  行偏移自动设置为 (LCD_PHYS_WIDTH - width)，确保跨行连续。
  *        使用 HAL_DMA2D_Start() 和 HAL_DMA2D_PollForTransfer() 实现超时保护。
  */
HAL_StatusTypeDef BSP_LCD_DMA2D_Fill(uint32_t addr, uint16_t width, uint16_t height, uint32_t color);

/* ----- 清屏操作 ----- */
void BSP_LCD_ClearActiveLayer(uint16_t color);   /*!< 清空当前图层的逻辑窗口区域 */
void BSP_LCD_FillBackground(uint16_t color);     /*!< 填充背景层全屏（物理尺寸） */
void BSP_LCD_FillForeground(uint16_t color);     /*!< 填充前景层全屏（物理尺寸） */

/* ----- 基础图形绘制（逻辑坐标） ----- */
void BSP_LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void BSP_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void BSP_LCD_DrawCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void BSP_LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void BSP_LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void BSP_LCD_FillRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/* ----- 字符与字符串（基于光标） ----- */
void BSP_LCD_DrawChar(char c);
void BSP_LCD_DrawString(const char* str);

/* ----- 数值与格式化输出 ----- */
void BSP_LCD_DrawDec(int32_t num);
void BSP_LCD_DrawHex(uint32_t num);
void BSP_LCD_DrawFloat(float fnum, uint8_t decimals);
void BSP_LCD_Printf(const char* fmt, ...);

/* ----- 属性设置函数 ----- */
void BSP_LCD_SetCursor(uint16_t x, uint16_t y);
void BSP_LCD_SetColor(uint16_t color);
void BSP_LCD_SetBgColor(uint16_t color);
void BSP_LCD_SetBgTransparent(uint8_t enable);  /*!< enable=1 透明，0 不透明 */
void BSP_LCD_SetFont(sFONT* font);

/* ----- 自测试函数（演示所有功能） ----- */
void BSP_LCD_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LCD_H */