#ifndef __BSP_KEY_H
#define __BSP_KEY_H
/**
 * @file    bsp_key.h
 * @author  apollo730
 * @brief   
 * 
 */

#include "stm32f4xx_hal.h"

/** @brief   按键类型枚举
 */
typedef enum
{
  KEY_NONE = 0, ///< 无按键事件
  KEY_1,        ///< 按键1事件
  KEY_2,        ///< 按键2事件
  KEY_3,        ///< 按键3事件
  KEY_4,        ///< 按键4事件
} KEYS;

//KEY的GPIO宏定义
#define KEY_1_Pin        GPIO_PIN_0
#define KEY_1_GPIO_Port  GPIOA
#define KEY_2_Pin        GPIO_PIN_13
#define KEY_2_GPIO_Port  GPIOC


// 一直等待按键事件
#define KEY_WAIT_ALWAYS 0 

KEYS KEY_Scan(uint32_t timeout);

#endif