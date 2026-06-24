#include "bsp_uart.h"

/**
 * @brief   发送一个字节
 * @param   byte
 * @return  BSP_StatusTypeDef
 */
HAL_StatusTypeDef BSP_UART_TransmitByte(uint8_t* byte, uint32_t Timeout)
{
  if (HAL_UART_Transmit(&huart1, byte, 1, Timeout) == HAL_OK)
  {
    return HAL_OK;
  }
  else
  {
    return HAL_ERROR;
  }
  return HAL_ERROR;
}
/**
 * @brief   发送固定数量字节数
 * @param   pData
 * @param   Size 最大65535
 * @return  BSP_StatusTypeDef
 */
HAL_StatusTypeDef BSP_UART_Transmit(uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
  if (HAL_UART_Transmit(&huart1, pData, Size, 200) == HAL_OK)
  {
    return HAL_OK;
  }
  else
  {
    return HAL_ERROR;
  }
  return HAL_ERROR;
}

/**
 * @brief   接收一个字节数据
 * @param   pByte
 * @param   Timeout
 * @return  HAL_StatusTypeDef
 */
HAL_StatusTypeDef BSP_UART_ReceiveByte(uint8_t* pByte, uint32_t Timeout)
{
  if (HAL_UART_Receive(&huart1, pByte, 1, 200) == HAL_OK)
  {
    return HAL_OK;
  }
  else
  {
    return HAL_ERROR;
  }
  return HAL_ERROR;
}

/**
 * @brief   接受固定长度数据
 * @param   pData
 * @param   Size
 * @param   Timeout
 * @return  HAL_StatusTypeDef
 */
HAL_StatusTypeDef BSP_UART_Receive(uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
  if (HAL_UART_Receive(&huart1, pData, Size, 200) == HAL_OK)
  {
    return HAL_OK;
  }
  else
  {
    return HAL_ERROR;
  }
  return HAL_ERROR;
}