/*
 * usart.h
 *
 *  Created on: Jan 6, 2018
 *      Author: Mohamed
 */

#ifndef USART_H_
#define USART_H_

#include "Micro_config.h"
#include "Common_Macros.h"
#include "Std_Types.h"

void USART_Init();
void USART_SendByte(uint8 data);
uint8 USART_ReceiveByte();
void USART_SendString(uint8 *str, uint8 length);
void USART_ReceiveString(uint8 *str, uint8 length);
uint8 UART_ReceiveWithTimeout(unsigned char *data, uint16_t timeout_ms);
uint8 USART_ReceiveStringWithTimeout(uint8 *str, uint8 length, uint16_t single_char_timeout_ms);


#endif /* USART_H_ */