/**
 * @file    bsp_buzzer.h
 * @author  apollo730
 * @brief
 *
 */
#ifndef __BSP_BUZZER_H
#define __BSP_BUZZER_H

#include "stm32f4xx_hal.h"

// 引脚定义
#define Buzzer_Pin       GPIO_PIN_11
#define Buzzer_GPIO_Port GPIOI

// 蜂鸣器控制宏定义
#ifdef Buzzer_Pin
#define Buzzer_ON()     HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET)
#define Buzzer_OFF()    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET)
#define Buzzer_TOGGLE() HAL_GPIO_TogglePin(Buzzer_GPIO_Port, Buzzer_Pin)
#endif

#endif /* LED_KEY_H */