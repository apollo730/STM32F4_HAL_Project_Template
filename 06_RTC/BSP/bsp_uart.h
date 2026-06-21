#ifndef __UART_H_
#define __UART_H_

#include "stm32f4xx_hal.h"
#include "usart.h"





HAL_StatusTypeDef BSP_UART_TransmitByte(uint8_t* byte, uint32_t Timeout);
HAL_StatusTypeDef BSP_UART_Transmit(uint8_t* pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef BSP_UART_ReceiveByte(uint8_t* pByte, uint32_t Timeout);
HAL_StatusTypeDef BSP_UART_Receive(uint8_t* pData, uint16_t Size, uint32_t Timeout);


#if(BSP_UART_DMA==1)


#endif


#endif