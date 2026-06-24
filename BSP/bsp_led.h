#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "stm32f4xx_hal.h"

//LED的GPIO 宏定义

#define LED_R_Pin        GPIO_PIN_10
#define LED_R_GPIO_Port  GPIOH
#define LED_G_Pin        GPIO_PIN_11
#define LED_G_GPIO_Port  GPIOH
#define LED_B_Pin        GPIO_PIN_12
#define LED_B_GPIO_Port  GPIOH


/** @brief   LED_R控制宏定义
 * @version 1.0.0
 * @author  apollo730
 * @date    2026-05-31
 */
#ifdef LED_R_Pin
#define LED_R_ON()     HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET)
#define LED_R_OFF()    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET)
#define LED_R_TOGGLE() HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin)
#endif

/**
 * @brief   LED_G控制宏定义
 * @version 1.0.0
 * @author  apollo730
 * @date    2026-05-31
 */
#ifdef LED_G_Pin
#define LED_G_ON()     HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET)
#define LED_G_OFF()    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET)
#define LED_G_TOGGLE() HAL_GPIO_TogglePin(LED_G_GPIO_Port, LED_G_Pin)
#endif

/**
 * @brief   LED_B控制宏定义
 * @version 1.0.0
 * @author  apollo730
 * @date    2026-05-31
 */
#ifdef LED_B_Pin
#define LED_B_ON()     HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET)
#define LED_B_OFF()    HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET)
#define LED_B_TOGGLE() HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin)
#endif


#endif
