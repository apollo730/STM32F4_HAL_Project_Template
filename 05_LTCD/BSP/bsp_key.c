#include "bsp_key.h"



/**
 * @brief   轮询方式读取按键状态
 * @param   timeout 
 * @return  KEYS 
 */
KEYS KEY_Scan(uint32_t timeout)
{
  
  uint32_t tickstart = HAL_GetTick(); // 获取当前系统计数
  const uint32_t btnDeylay = 20;// 按键消抖时间
  GPIO_PinState keyState;

  while(1)
  {
    // 读取按键状态
    keyState = HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin);
    if (keyState == GPIO_PIN_SET) // 按键按下
    {
      HAL_Delay(btnDeylay); // 消抖延时
      if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_SET) // 再次确认按键状态
      {
        return KEY_1; // 返回按键1状态
      }
    }

    keyState = HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin);
    if (keyState == GPIO_PIN_SET) // 按键按下
    {
      HAL_Delay(btnDeylay); // 消抖延时
      if (HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) == GPIO_PIN_SET) // 再次确认按键状态
      {
        return KEY_2; // 返回按键2状态
      }
    }

    // 检查是否超时
    if (timeout != KEY_WAIT_ALWAYS && (HAL_GetTick() - tickstart) >= timeout)
    {
      break; // 超时，退出循环
    }
  }

  return KEY_NONE; // 返回无按键状态
}
