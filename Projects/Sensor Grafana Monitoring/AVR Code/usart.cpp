/*
 * usart.c
 *
 *  Created on: Jan 6, 2018
 *      Author: Mohamed
 */

#include "usart.h"

void USART_Init(void)
{
	// 1. Enable Receiver and Transmitter
	// Direct assignment wipes out any previous configurations
	UCSRB = (1<<RXEN) | (1<<TXEN);

	// 2. Set frame format: 8 data, 1 stop, no parity
	// MUST use direct assignment '=' due to the shared memory address
	UCSRC = (1<<URSEL) | (1<<UCSZ0) | (1<<UCSZ1);

	// 3. Set baud rate (High byte must be written before Low byte)
	// Writing 0 automatically has URSEL=0, directing the data to UBRRH
	UBRRH = 0;
	UBRRL = 103;
}

void USART_SendByte(uint8 data)
{
	while(Bit_Is_Clear(UCSRA,UDRE));
	UDR = data;
}

uint8 USART_ReceiveByte()
{
	while(Bit_Is_Clear(UCSRA,RXC));
	return UDR;
}

uint8 UART_ReceiveWithTimeout(unsigned char *data, uint16_t timeout_ms) {
	uint16 elapsed_ms = 0;

	// Loop until data arrives OR timeout is reached
	while (!(UCSRA & (1 << RXC))) {
		_delay_ms(1);
		elapsed_ms++;

		if (elapsed_ms >= timeout_ms) {
			return 0; // Timeout reached, return failure
		}
	}

	// Data arrived before timeout
	*data = UDR;
	return 1;
}

void USART_SendString(uint8 *str, uint8 length)
{
	for (uint8 i = 0; i < length; i++)
		USART_SendByte(str[i]);
}

void USART_ReceiveString(uint8 *str, uint8 length)
{
	for (uint8 i = 0; i < length; i++)
		str[i] = USART_ReceiveByte();
}

uint8 USART_ReceiveStringWithTimeout(uint8 *str, uint8 length, uint16_t single_char_timeout_ms)
{
	for (uint8 i = 0; i < length; i++)
	{
		uint16_t elapsed_ms = 0;

		// Wait for the RXC flag OR timeout for the current byte
		while (!(UCSRA & (1 << RXC)))
		{
			_delay_ms(1);
			elapsed_ms++;

			if (elapsed_ms >= single_char_timeout_ms)
			{
				return 0; // Returns how many bytes were actually read
			}
		}

		// Character arrived successfully, read it from UDR
		str[i] = UDR;
	}

	return length; // Success: all characters received
}