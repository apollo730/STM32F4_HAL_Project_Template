#include "bsp_lcd.h"
#include "fonts/fonts.h"
#include "main.h"
#include "stm32f4xx_hal_dma2d.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 外部DMA2D句柄声明
extern DMA2D_HandleTypeDef hdma2d;

// ========================== 全局变量定义 ==========================
LCD_Layer_t currentLayer = LCD_LAYER_FG;                      // 当前激活图层
uint16_t lcd_cursor_x = 0U;                                   // 光标X坐标
uint16_t lcd_cursor_y = 0U;                                   // 光标Y坐标
uint16_t lcd_fgColor = COLOR_WHITE;                           // 前景色
uint16_t lcd_bgColor = COLOR_BLACK;                           // 背景色（默认不透明）
sFONT* lcd_font = &Font24;                                    // 默认字体
LCD_Window_t lcd_bg_window = {0U, 0U, LCD_WIDTH, LCD_HEIGHT}; // 背景层默认全屏
LCD_Window_t lcd_fg_window = {0U, 0U, LCD_WIDTH, LCD_HEIGHT}; // 前景层默认全屏

// ========================== 图层操作函数 ==========================
uint32_t BSP_LCD_GetLayerAddr(LCD_Layer_t layer)
{
  return (layer == LCD_LAYER_BG) ? LCD_BG_LAYER_ADDR : LCD_FG_LAYER_ADDR;
}

LCD_Layer_t BSP_LCD_GetActiveLayer(void)
{
  return currentLayer;
}

void BSP_LCD_SetActiveLayer(LCD_Layer_t layer)
{
  if (layer == LCD_LAYER_BG || layer == LCD_LAYER_FG)
  {
    currentLayer = layer;
  }
}

HAL_StatusTypeDef BSP_LCD_SetLayerWindow(LCD_Layer_t layer, LCD_Window_t window)
{
  // 边界校验：窗口不能超出屏幕范围
  if ((window.x + window.width > LCD_WIDTH) || (window.y + window.height > LCD_HEIGHT) || (window.width == 0U) ||
      (window.height == 0U))
  {
    return HAL_ERROR;
  }

  if (layer == LCD_LAYER_BG)
  {
    memcpy(&lcd_bg_window, &window, sizeof(LCD_Window_t));
  }
  else if (layer == LCD_LAYER_FG)
  {
    memcpy(&lcd_fg_window, &window, sizeof(LCD_Window_t));
  }
  else
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

LCD_Window_t BSP_LCD_GetLayerWindow(LCD_Layer_t layer)
{
  return (layer == LCD_LAYER_BG) ? lcd_bg_window : lcd_fg_window;
}

// ========================== DMA2D 操作函数 ==========================
HAL_StatusTypeDef BSP_LCD_DMA2D_Fill(uint32_t addr, uint16_t width, uint16_t height, uint16_t color)
{
  // 参数合法性校验
  if ((width == 0U) || (height == 0U) || (addr == 0U))
  {
    return HAL_ERROR;
  }

  uint32_t tick_start = HAL_GetTick();

  // 停止当前DMA2D传输（防止忙状态）
  if (DMA2D->CR & DMA2D_CR_START)
  {
    DMA2D->CR &= ~DMA2D_CR_START;
    while (DMA2D->CR & DMA2D_CR_START)
    {
      if ((HAL_GetTick() - tick_start) > DMA2D_TIMEOUT_MS)
      {
        return HAL_TIMEOUT;
      }
    }
  }

  // 重置DMA2D状态标志
  DMA2D->IFCR = DMA2D_FLAG_TE | DMA2D_FLAG_TC | DMA2D_FLAG_CE | DMA2D_FLAG_CTC;

  // 配置DMA2D参数（RGB565格式）
  DMA2D->CR = DMA2D_R2M;               // 寄存器到内存模式
  DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565; // 输出格式RGB565
  DMA2D->OOR = (LCD_WIDTH - width);    // 行偏移（全屏宽度 - 窗口宽度）
  DMA2D->NLR = (width << 16) | height; // 像素数量（宽度<<16 | 高度）
  DMA2D->OMAR = addr;                  // 输出内存地址
  DMA2D->OCOLR = color;                // 输出颜色

  // 启动DMA2D传输并等待完成
  DMA2D->CR |= DMA2D_CR_START;
  while ((DMA2D->ISR & DMA2D_FLAG_TC) == 0U)
  {
    // 超时检测
    if ((HAL_GetTick() - tick_start) > DMA2D_TIMEOUT_MS)
    {
      DMA2D->CR &= ~DMA2D_CR_START;
      return HAL_TIMEOUT;
    }
  }

  // 清除传输完成标志
  DMA2D->IFCR = DMA2D_FLAG_TC;

  return HAL_OK;
}

// ========================== 清屏函数 ==========================
void BSP_LCD_ClearActiveLayer(uint16_t color)
{
  LCD_Layer_t active_layer = BSP_LCD_GetActiveLayer();
  LCD_Window_t window = BSP_LCD_GetLayerWindow(active_layer);
  uint32_t layer_addr = BSP_LCD_GetLayerAddr(active_layer);

  // 计算窗口起始地址（基地址 + 偏移）
  uint32_t window_addr = layer_addr + (window.y * LCD_WIDTH + window.x) * LCD_PIXEL_SIZE;

  // 填充窗口区域
  (void)BSP_LCD_DMA2D_Fill(window_addr, window.width, window.height, color);
}

void BSP_LCD_FillBackground(uint16_t color)
{
  (void)BSP_LCD_DMA2D_Fill(LCD_BG_LAYER_ADDR, LCD_WIDTH, LCD_HEIGHT, color);
}

void BSP_LCD_FillForeground(uint16_t color)
{
  (void)BSP_LCD_DMA2D_Fill(LCD_FG_LAYER_ADDR, LCD_WIDTH, LCD_HEIGHT, color);
}

// ========================== 基础绘图函数 ==========================
void BSP_LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  // 全局屏幕边界校验
  if ((x >= LCD_WIDTH) || (y >= LCD_HEIGHT))
  {
    return;
  }

  // 当前图层窗口边界校验
  LCD_Layer_t active_layer = BSP_LCD_GetActiveLayer();
  LCD_Window_t window = BSP_LCD_GetLayerWindow(active_layer);
  if ((x < window.x) || (x >= (window.x + window.width)) || (y < window.y) || (y >= (window.y + window.height)))
  {
    return;
  }

  // 计算像素物理地址并写入颜色
  uint32_t layerAddr = BSP_LCD_GetLayerAddr(active_layer);
  uint32_t pixelAddr = layerAddr + (y * LCD_WIDTH + x) * LCD_PIXEL_SIZE;
  *((__IO uint16_t*)pixelAddr) = color;
}

void BSP_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
  int16_t dx = abs((int16_t)(x2 - x1));
  int16_t dy = abs((int16_t)(y2 - y1));
  int16_t sx = (x1 < x2) ? 1 : -1;
  int16_t sy = (y1 < y2) ? 1 : -1;
  int16_t err = dx - dy;
  int16_t e2;

  while (1)
  {
    BSP_LCD_DrawPixel(x1, y1, color);

    // 到达终点退出
    if ((x1 == x2) && (y1 == y2))
    {
      break;
    }

    e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y1 += sy;
    }
  }
}

void BSP_LCD_DrawCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;

  if (radius == 0U)
  {
    BSP_LCD_DrawPixel(x0, y0, color);
    return;
  }

  while (x >= y)
  {
    BSP_LCD_DrawPixel(x0 + x, y0 + y, color);
    BSP_LCD_DrawPixel(x0 + y, y0 + x, color);
    BSP_LCD_DrawPixel(x0 - y, y0 + x, color);
    BSP_LCD_DrawPixel(x0 - x, y0 + y, color);
    BSP_LCD_DrawPixel(x0 - x, y0 - y, color);
    BSP_LCD_DrawPixel(x0 - y, y0 - x, color);
    BSP_LCD_DrawPixel(x0 + y, y0 - x, color);
    BSP_LCD_DrawPixel(x0 + x, y0 - y, color);

    if (err <= 0)
    {
      y++;
      err += 2 * y + 1;
    }
    if (err > 0)
    {
      x--;
      err -= 2 * x + 1;
    }
  }
}

void BSP_LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;

  if (radius == 0U)
  {
    BSP_LCD_DrawPixel(x0, y0, color);
    return;
  }

  while (x >= y)
  {
    // 绘制水平扫描线填充
    BSP_LCD_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
    BSP_LCD_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
    BSP_LCD_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
    BSP_LCD_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);

    if (err <= 0)
    {
      y++;
      err += 2 * y + 1;
    }
    if (err > 0)
    {
      x--;
      err -= 2 * x + 1;
    }
  }
}

void BSP_LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
  // 坐标归一化（确保x1<=x2，y1<=y2）
  uint16_t x_start = (x1 < x2) ? x1 : x2;
  uint16_t x_end = (x1 < x2) ? x2 : x1;
  uint16_t y_start = (y1 < y2) ? y1 : y2;
  uint16_t y_end = (y1 < y2) ? y2 : y1;

  // 绘制四条边
  BSP_LCD_DrawLine(x_start, y_start, x_end, y_start, color); // 上
  BSP_LCD_DrawLine(x_end, y_start, x_end, y_end, color);     // 右
  BSP_LCD_DrawLine(x_end, y_end, x_start, y_end, color);     // 下
  BSP_LCD_DrawLine(x_start, y_end, x_start, y_start, color); // 左
}

void BSP_LCD_FillRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
  // 坐标归一化
  uint16_t x_start = (x1 < x2) ? x1 : x2;
  uint16_t x_end = (x1 < x2) ? x2 : x1;
  uint16_t y_start = (y1 < y2) ? y1 : y2;
  uint16_t y_end = (y1 < y2) ? y2 : y1;

  // 逐行填充
  for (uint16_t y = y_start; y <= y_end; y++)
  {
    BSP_LCD_DrawLine(x_start, y, x_end, y, color);
  }
}

// ========================== 字符/字符串绘制函数 ==========================
void BSP_LCD_DrawChar(char c)
{
  // 参数校验
  if ((c < ' ') || (c > '~')) // 仅支持可打印ASCII字符
  {
    return;
  }

  uint16_t x = lcd_cursor_x;
  uint16_t y = lcd_cursor_y;
  uint16_t color = lcd_fgColor;
  uint16_t bgColor = lcd_bgColor;
  sFONT* font = lcd_font;

  // 屏幕边界校验
  if ((x >= LCD_WIDTH) || (y >= LCD_HEIGHT) || (font == NULL))
  {
    return;
  }

  uint16_t charWidth = font->Width;
  uint16_t charHeight = font->Height;
  uint32_t charOffset = (c - ' ') * charHeight * ((charWidth + 7U) / 8U);
  const uint8_t* charData = &font->table[charOffset];

  // 逐行绘制字符点阵
  for (uint16_t i = 0U; i < charHeight; i++)
  {
    for (uint16_t byte = 0U; byte < ((charWidth + 7U) / 8U); byte++)
    {
      uint8_t data = charData[i * ((charWidth + 7U) / 8U) + byte];

      for (uint16_t j = 0U; j < 8U && (byte * 8U + j) < charWidth; j++)
      {
        uint16_t pixel_x = x + byte * 8U + j;
        uint16_t pixel_y = y + i;

        // 仅在屏幕范围内绘制
        if ((pixel_x < LCD_WIDTH) && (pixel_y < LCD_HEIGHT))
        {
          if (data & (1U << (7U - j)))
          {
            BSP_LCD_DrawPixel(pixel_x, pixel_y, color);
          }
          else if (bgColor != COLOR_TRANSPARENT)
          {
            BSP_LCD_DrawPixel(pixel_x, pixel_y, bgColor);
          }
        }
      }
    }
  }
}

void BSP_LCD_DrawString(const char* str)
{
  if ((str == NULL) || (lcd_font == NULL))
  {
    return;
  }

  uint16_t charWidth = lcd_font->Width;
  uint16_t charHeight = lcd_font->Height;
  uint16_t lineStartX = lcd_cursor_x;

  while (*str != '\0')
  {
    // 处理换行符
    if (*str == '\n')
    {
      lcd_cursor_x = lineStartX;
      lcd_cursor_y += charHeight + LCD_LINE_OFFSET;

      // 屏幕底部溢出处理
      if (lcd_cursor_y >= LCD_HEIGHT)
      {
        lcd_cursor_y = 0U;
        BSP_LCD_ClearActiveLayer(lcd_bgColor);
      }
      str++;
      continue;
    }

    // 处理回车符
    if (*str == '\r')
    {
      lcd_cursor_x = lineStartX;
      str++;
      continue;
    }

    // 行宽度溢出自动换行
    if ((lcd_cursor_x + charWidth + LCD_CHAR_SPACE) >= LCD_WIDTH)
    {
      lcd_cursor_x = lineStartX;
      lcd_cursor_y += charHeight + LCD_LINE_OFFSET;

      if (lcd_cursor_y >= LCD_HEIGHT)
      {
        lcd_cursor_y = 0U;
        BSP_LCD_ClearActiveLayer(lcd_bgColor);
      }
    }

    // 绘制当前字符
    BSP_LCD_DrawChar(*str);

    // 更新光标位置
    lcd_cursor_x += charWidth + LCD_CHAR_SPACE;
    str++;
  }
}

// ========================== 数值绘制函数 ==========================
void BSP_LCD_DrawDec(int32_t num)
{
  if (lcd_font == NULL)
  {
    return;
  }

  char buf[16U] = {0};
  char* p = buf + 15U;
  uint8_t isNegative = 0U;
  *p = '\0';

  // 处理负数
  if (num < 0)
  {
    isNegative = 1U;
    num = -num; // 转为正数处理（注意：INT32_MIN取反会溢出，此处简化处理）
  }

  // 处理0值
  if (num == 0)
  {
    *(--p) = '0';
  }
  else
  {
    // 逐位拆分数字
    while (num > 0)
    {
      *(--p) = '0' + (num % 10);
      num /= 10;
    }
  }

  // 添加负号
  if (isNegative)
  {
    *(--p) = '-';
  }

  // 绘制数字字符串
  while (*p != '\0')
  {
    // 自动换行处理
    if ((lcd_cursor_x + lcd_font->Width + LCD_CHAR_SPACE) >= LCD_WIDTH)
    {
      lcd_cursor_x = 0U;
      lcd_cursor_y += lcd_font->Height + LCD_LINE_OFFSET;

      if (lcd_cursor_y >= LCD_HEIGHT)
      {
        lcd_cursor_y = 0U;
        BSP_LCD_ClearActiveLayer(lcd_bgColor);
      }
    }

    BSP_LCD_DrawChar(*p);
    lcd_cursor_x += lcd_font->Width + LCD_CHAR_SPACE;
    p++;
  }
}

void BSP_LCD_DrawHex(uint32_t num)
{
  if (lcd_font == NULL)
  {
    return;
  }

  char buf[12U] = {0};
  char* p = buf + 11U;
  const char hexDigits[] = "0123456789ABCDEF";
  *p = '\0';

  // 处理0值
  if (num == 0U)
  {
    *(--p) = '0';
  }
  else
  {
    // 逐4位拆分十六进制
    while (num > 0U)
    {
      *(--p) = hexDigits[num & 0x0FU];
      num >>= 4U;
    }
  }

  // 添加0x前缀
  *(--p) = 'x';
  *(--p) = '0';

  // 绘制十六进制字符串
  while (*p != '\0')
  {
    if ((lcd_cursor_x + lcd_font->Width + LCD_CHAR_SPACE) >= LCD_WIDTH)
    {
      lcd_cursor_x = 0U;
      lcd_cursor_y += lcd_font->Height + LCD_LINE_OFFSET;

      if (lcd_cursor_y >= LCD_HEIGHT)
      {
        lcd_cursor_y = 0U;
        BSP_LCD_ClearActiveLayer(lcd_bgColor);
      }
    }

    BSP_LCD_DrawChar(*p);
    lcd_cursor_x += lcd_font->Width + LCD_CHAR_SPACE;
    p++;
  }
}

void BSP_LCD_DrawFloat(float fnum, uint8_t decimals)
{
  if (lcd_font == NULL || decimals > 15U)
  {
    return;
  }

  char buf[32U] = {0};
  char* p = buf;
  uint8_t isNegative = 0U;
  int32_t intPart;
  uint32_t fracPart = 0U;
  uint32_t multiplier = 1U;

  // 限制小数位数并计算放大倍数
  decimals = (decimals > 15U) ? 15U : decimals;
  for (uint8_t i = 0U; i < decimals; i++)
  {
    multiplier *= 10U;
  }

  // 处理负数
  if (fnum < 0.0f)
  {
    isNegative = 1U;
    fnum = -fnum;
  }

  // 拆分整数和小数部分（四舍五入）
  intPart = (int32_t)fnum;
  fracPart = (uint32_t)((fnum - (float)intPart) * (float)multiplier + 0.5f);

  // 处理小数进位溢出
  if (fracPart >= multiplier)
  {
    fracPart = 0U;
    intPart++;
  }

  // 写入负号
  if (isNegative)
  {
    *p++ = '-';
  }

  // 处理整数部分
  if (intPart == 0)
  {
    *p++ = '0';
  }
  else
  {
    char intBuf[16U] = {0};
    char* pi = intBuf + 15U;
    *pi = '\0';
    int32_t temp = intPart;

    while (temp > 0)
    {
      *(--pi) = '0' + (temp % 10);
      temp /= 10;
    }

    while (*pi != '\0')
    {
      *p++ = *pi++;
    }
  }

  // 处理小数部分
  if (decimals > 0U)
  {
    *p++ = '.';
    char fracBuf[16U] = {0};
    char* pf = fracBuf + 15U;
    *pf = '\0';
    uint32_t temp = fracPart;
    uint8_t digits = 0U;

    // 拆分小数位
    while (temp > 0U && digits < decimals)
    {
      *(--pf) = '0' + (temp % 10U);
      temp /= 10U;
      digits++;
    }

    // 补零到指定小数位数
    while (digits < decimals)
    {
      *(--pf) = '0';
      digits++;
    }

    // 写入小数部分
    while (*pf != '\0')
    {
      *p++ = *pf++;
    }
  }

  // 绘制浮点数字符串
  p = buf;
  while (*p != '\0')
  {
    if ((lcd_cursor_x + lcd_font->Width + LCD_CHAR_SPACE) >= LCD_WIDTH)
    {
      lcd_cursor_x = 0U;
      lcd_cursor_y += lcd_font->Height + LCD_LINE_OFFSET;

      if (lcd_cursor_y >= LCD_HEIGHT)
      {
        lcd_cursor_y = 0U;
        BSP_LCD_ClearActiveLayer(lcd_bgColor);
      }
    }

    BSP_LCD_DrawChar(*p);
    lcd_cursor_x += lcd_font->Width + LCD_CHAR_SPACE;
    p++;
  }
}

// ========================== 格式化输出函数 ==========================
void BSP_LCD_Printf(const char* fmt, ...)
{
  if (fmt == NULL)
  {
    return;
  }

  char buf[256U] = {0};
  va_list args;

  // 格式化字符串到缓冲区
  va_start(args, fmt);
  (void)vsnprintf(buf, sizeof(buf) - 1U, fmt, args);
  va_end(args);

  // 绘制格式化后的字符串
  BSP_LCD_DrawString(buf);
}

// ========================== 底层IO重定向函数 ==========================
int __io_putchar(int ch)
{
  // 过滤无效字符
  if (ch < 0)
  {
    return ch;
  }

  // 处理回车符
  if (ch == '\r')
  {
    lcd_cursor_x = 0U;
    return ch;
  }

  // 处理换行符
  if (ch == '\n')
  {
    lcd_cursor_x = 0U;
    lcd_cursor_y += lcd_font->Height + LCD_LINE_OFFSET;

    // 屏幕底部溢出处理
    if (lcd_cursor_y >= LCD_HEIGHT)
    {
      lcd_cursor_y = 0U;
      BSP_LCD_ClearActiveLayer(lcd_bgColor);
    }
    return ch;
  }

  // 行宽度溢出自动换行
  if ((lcd_cursor_x + lcd_font->Width + LCD_CHAR_SPACE) >= LCD_WIDTH)
  {
    lcd_cursor_x = 0U;
    lcd_cursor_y += lcd_font->Height + LCD_LINE_OFFSET;

    if (lcd_cursor_y >= LCD_HEIGHT)
    {
      lcd_cursor_y = 0U;
      BSP_LCD_ClearActiveLayer(lcd_bgColor);
    }
  }

  // 绘制字符并更新光标
  BSP_LCD_DrawChar((char)ch);
  lcd_cursor_x += lcd_font->Width + LCD_CHAR_SPACE;

  return ch;
}

// ========================== 初始化/测试函数 ==========================
void BSP_LCD_Init(void)
{
  // 使能DMA2D时钟
  __HAL_RCC_DMA2D_CLK_ENABLE();

  // 配置DMA2D基础参数
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_R2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = 0U;

  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler(); // 错误处理（需在main.c中实现）
  }

  // 初始化默认图层窗口
  (void)BSP_LCD_SetLayerWindow(LCD_LAYER_BG, lcd_bg_window);
  (void)BSP_LCD_SetLayerWindow(LCD_LAYER_FG, lcd_fg_window);

  // 清空默认图层
  BSP_LCD_FillBackground(COLOR_BLACK);
  BSP_LCD_FillForeground(COLOR_BLACK);
}

void BSP_LCD_Test(void)
{
  BSP_LCD_Init();

  // 1. 测试清屏
  BSP_LCD_FillBackground(COLOR_RED);
  HAL_Delay(1000U);
  BSP_LCD_FillForeground(COLOR_BLUE);
  HAL_Delay(1000U);

  // 2. 测试像素绘制
  BSP_LCD_SetActiveLayer(LCD_LAYER_FG);
  BSP_LCD_DrawPixel(0U, LCD_HEIGHT - 1U, COLOR_GREEN);
  HAL_Delay(1000U);

  // 3. 测试十字线
  for (uint16_t i = 0U; i < LCD_WIDTH; i++)
  {
    BSP_LCD_DrawPixel(i, LCD_HEIGHT / 2U, COLOR_YELLOW);
  }
  for (uint16_t i = 0U; i < LCD_HEIGHT; i++)
  {
    BSP_LCD_DrawPixel(LCD_WIDTH / 2U, i, COLOR_YELLOW);
  }
  HAL_Delay(1000U);

  // 4. 测试直线
  BSP_LCD_DrawLine(100U, 100U, 700U, 400U, COLOR_RED);
  BSP_LCD_DrawLine(100U, 150U, 100U, 400U, COLOR_GREEN);
  BSP_LCD_DrawLine(700U, 150U, 100U, 400U, COLOR_BLUE);
  HAL_Delay(1000U);

  // 5. 测试圆形
  BSP_LCD_DrawCircle(200U, 200U, 50U, COLOR_YELLOW);
  BSP_LCD_DrawCircle(600U, 300U, 80U, COLOR_RED);
  HAL_Delay(1000U);

  // 6. 测试矩形
  BSP_LCD_DrawRectangle(300U, 100U, 500U, 200U, COLOR_GREEN);
  BSP_LCD_DrawRectangle(50U, 300U, 300U, 400U, COLOR_BLUE);
  HAL_Delay(1000U);

  // 7. 测试实心矩形
  BSP_LCD_FillRectangle(350U, 250U, 450U, 350U, COLOR_YELLOW);
  BSP_LCD_FillRectangle(500U, 350U, 700U, 450U, COLOR_RED);
  HAL_Delay(1000U);

  // 8. 测试实心圆
  BSP_LCD_FillCircle(400U, 400U, 50U, COLOR_GREEN);
  BSP_LCD_FillCircle(150U, 150U, 30U, COLOR_BLUE);
  HAL_Delay(1000U);

  // 9. 清空屏幕准备字符测试
  BSP_LCD_FillForeground(COLOR_BLACK);
  BSP_LCD_SetBgColor(COLOR_BLACK);

  // 10. 测试单个字符
  BSP_LCD_SetCursor(10U, 50U);
  BSP_LCD_SetColor(COLOR_RED);
  BSP_LCD_SetFont(&Font24);
  BSP_LCD_DrawChar('A');
  HAL_Delay(1000U);

  // 11. 测试字符串
  BSP_LCD_SetCursor(50U, 100U);
  BSP_LCD_SetColor(COLOR_GREEN);
  BSP_LCD_SetFont(&Font24);
  BSP_LCD_DrawString("Hello World!");
  HAL_Delay(1000U);

  // 12. 测试多行字符串
  BSP_LCD_SetCursor(50U, 200U);
  BSP_LCD_SetColor(COLOR_YELLOW);
  BSP_LCD_SetFont(&Font8);
  BSP_LCD_DrawString("STM32F429\nLTDC+DMA2D\nRGB565 800x480");
  HAL_Delay(1000U);

  // 13. 清空屏幕准备数值测试
  BSP_LCD_FillForeground(COLOR_BLACK);
  BSP_LCD_SetBgColor(COLOR_BLACK);
  BSP_LCD_SetFont(&Font24);

  // 14. 测试十进制数
  BSP_LCD_SetCursor(50U, 50U);
  BSP_LCD_SetColor(COLOR_RED);
  BSP_LCD_DrawDec(12345);

  BSP_LCD_SetCursor(50U, 100U);
  BSP_LCD_SetColor(COLOR_GREEN);
  BSP_LCD_DrawDec(-6789);

  BSP_LCD_SetCursor(50U, 150U);
  BSP_LCD_SetColor(COLOR_BLUE);
  BSP_LCD_DrawDec(123456);
  HAL_Delay(1000U);

  // 15. 测试十六进制数
  BSP_LCD_SetCursor(50U, 200U);
  BSP_LCD_SetColor(COLOR_YELLOW);
  BSP_LCD_DrawHex(255U);

  BSP_LCD_SetCursor(50U, 250U);
  BSP_LCD_SetColor(COLOR_CYAN);
  BSP_LCD_DrawHex(0xABCDU);

  BSP_LCD_SetCursor(50U, 300U);
  BSP_LCD_SetColor(COLOR_MAGENTA);
  BSP_LCD_DrawHex(0U);
  HAL_Delay(1000U);

  // 16. 测试浮点数
  BSP_LCD_SetCursor(200U, 50U);
  BSP_LCD_SetColor(COLOR_ORANGE);
  BSP_LCD_DrawFloat(3.14159f, 5U);

  BSP_LCD_SetCursor(200U, 100U);
  BSP_LCD_SetColor(COLOR_PINK);
  BSP_LCD_DrawFloat(-1.59f, 1U);

  BSP_LCD_SetCursor(200U, 150U);
  BSP_LCD_SetColor(COLOR_PURPLE);
  BSP_LCD_DrawFloat(0.0f, 3U);
  HAL_Delay(1000U);

  // 17. 测试格式化输出
  BSP_LCD_SetCursor(100U, 200U);
  BSP_LCD_SetColor(COLOR_WHITE);
  BSP_LCD_Printf("LCD Test: %d, 0x%X, %.2f", 1234, 0x1234, 3.14f);
  HAL_Delay(2000U);

  // 18. 最终清屏
  BSP_LCD_FillForeground(COLOR_BLACK);
}


// ========================== Setter函数实现 ==========================
void BSP_LCD_SetCursor(uint16_t x, uint16_t y)
{
  lcd_cursor_x = (x < LCD_WIDTH) ? x : LCD_WIDTH - 1U;
  lcd_cursor_y = (y < LCD_HEIGHT) ? y : LCD_HEIGHT - 1U;
}

void BSP_LCD_SetColor(uint16_t color)
{
  lcd_fgColor = color;
}

void BSP_LCD_SetBgColor(uint16_t color)
{
  lcd_bgColor = color;
}

void BSP_LCD_SetFont(sFONT* font)
{
  if (font != NULL)
  {
    lcd_font = font;
  }
}