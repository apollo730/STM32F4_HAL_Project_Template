#ifndef __BSP_CONFIG_H
#define __BSP_CONFIG_H
/**
 * @file    bsp_config.h
 * @author  apollo730
 * @brief
 *
 */
#include "stm32f4xx_hal.h"

// 使用哪个芯片
#define STM32F_4 1

// 设置包含哪些外设
#define BSP_LED    1
#define BSP_KEY    1
#define BSP_BUZZER 1
#define BSP_UART   1
//是否使用DMA
#define BSP_UART_DMA 1

//是否使用SDRAM
#define BSP_SDRAM 1
//是否使用LCD
#define BSP_LCD   1


/* ****************************************************************************** */


// 根据外设包含头文件
#if (BSP_LED == 1)
#include "bsp_led.h"
#endif

#if (BSP_KEY == 1)
#include "bsp_key.h"
#endif

#if (BSP_BUZZER == 1)
#include "bsp_buzzer.h"
#endif

#if (BSP_UART == 1)
#include "bsp_uart.h"
#endif

#if (BSP_SDRAM == 1)
#include "bsp_sdram.h"
#endif

#if (BSP_LCD == 1)
#include "bsp_lcd.h"
#endif


#endif /* __BSP_CONFIG_H */