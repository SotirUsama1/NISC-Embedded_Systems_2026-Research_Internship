/*
 * GccApplication3.cpp
 *
 * Created: 7/26/2026 1:33:22 PM
 * Author : sotir
 */ 
#include <avr/io.h>
#include <stdlib.h>

#define F_CPU 16000000UL
#include <util/delay.h>

#include "usart.h"

#define LCDMask (1 | 1<<1 | 1<<2 | 1<<4)
#define LCDPort PORTB
#define LCDDDR  DDRB
#define enable 2        //Enable = on
#define RS 3            //Send command = off, send data = on

void updateLCD(void);
void sendCommand(unsigned char command);
void sendData(unsigned char character);
void sendDataString(char* str);
void sendDataString(char* str, uint8 length);
void sendInitCommand(unsigned char command);
void LCDInit();

#define UART_PORT PORTD
#define UART_DDR DDRD
#define RX 0
#define TX 1

#define UART_DIR_PORT PORTB
#define UART_DIR_DDR DDRB
#define DIR 3

void float_to_string(float* value, char *buffer, int precision);

int main(void)
{
	LCDDDR |= LCDMask;
	DDRA |= 1 << enable | 1 << RS;
	LCDInit();
	
	UART_DIR_DDR |= 1<<DIR;
	UART_DDR |= 1<<TX;
	UART_DDR &= ~(1<<RX);
	USART_Init();
	
	uint8 buffer[17];
	uint8 ph[4];
	uint8 command[] = {0x03, 0x03, 0x00, 0x00, 0x00, 0x06, 0xc4, 0x2a}; // {0x2a, 0xc4, 0x06, 0x00, 0x00, 0x00, 0x03, 0x03,'\0'}; // 
	
	UART_DIR_PORT |= 1<<DIR; 
	USART_SendString(command, 8);

	while (!(UCSRA & (1<<TXC)));   // wait for the LAST byte to finish shifting out
	UCSRA |= (1<<TXC);             // clear the flag (write 1 to clear, per datasheet)
	
	UART_DIR_PORT &= ~(1<<DIR);
	USART_ReceiveString(buffer, sizeof(buffer));
	
	ph[0] = buffer[3];
	ph[1] = buffer[4];
	ph[2] = buffer[5];
	ph[3] = buffer[6];
	
	char str[2];
	float_to_string((float*) ph, str, 2);
	for (int i =0; i < 4; i++){
		itoa((int)buffer[3+i], str, 16);
		sendDataString(str, 2);
	}
	
	

}

void updateLCD() {

	PORTA |= 1 << enable;                     //Enable
	asm volatile ("nop");
	asm volatile ("nop");
	PORTA &= ~1 << enable;                    //Disable
}

void sendCommand(unsigned char command) {
	PORTA &= ~(1 << RS);     //Set RS low (write command)
	LCDPort |= ((((command >> 4) & ~0x07) << 1) | ((command >> 4) & 0x07)) & LCDMask;             //Send 4 ms bits
	updateLCD();
	LCDPort &= ~LCDMask;
	LCDPort |= (((command & ~0x07) << 1) | (command & 0x07)) & LCDMask;                    //Send 4 ls bits
	updateLCD();
	LCDPort &= ~LCDMask;                             //Clear data lines

}

void sendData(unsigned char character) {
	_delay_ms(5);
	PORTA |= 1 << RS;                 //Set R/W low and RS high (write data)
	LCDPort |= ((((character >> 4) & ~0x07) << 1) | ((character >> 4) & 0x07)) & LCDMask;           //Send 4 ms bits
	updateLCD();
	LCDPort &= ~LCDMask;
	PORTA |= 1 << RS;
	LCDPort |= (((character & ~0x07) << 1) | (character & 0x07)) & LCDMask;                //Send 4 ls bits
	updateLCD();
	LCDPort &= ~LCDMask;                             //Clear data lines
}

void sendInitCommand(unsigned char command) {

	PORTA &= ~(1 << RS);     // Set RS low (write command)
	LCDPort |= (((command & ~0x07) << 1) | (command & 0x07)) & LCDMask;
	updateLCD();
	LCDPort &= ~LCDMask;                             //Clear data lines
}

void LCDInit(){
	_delay_ms(100);                              //Wait for LCD to boot
	sendInitCommand(0x3);                        //Init function set 1
	_delay_ms(100);
	sendInitCommand(0x3);                        //Init function set 2
	_delay_us(100);
	sendInitCommand(0x3);                        //Init function set 3
	_delay_us(100);
	sendInitCommand(0x2);                        //Function set (set 4 bit mode)
	_delay_us(100);
	sendInitCommand(0x28);                       //Funcion set I=1, N=0, F=0
	//sendInitCommand(0x8);
	_delay_us(60);
	sendInitCommand(0x8);                        //On/off control D=0, C=0, B=0
	//sendInitCommand(0x8);
	_delay_us(60);
	sendCommand(0x01);                           //Clear display
	//sendInitCommand(0x1);
	_delay_ms(60);
	sendCommand(0x06);                           //Entry mode set I/D=1, S=0
	//sendInitCommand(0x6);
	_delay_us(60);
	sendCommand(0x0C);                           //On/off control D=1, C=0, B=0
	//sendInitCommand(0xC);
	_delay_us(60);
}

void sendDataString(char* str) {
	int i = 0;

	while (str[i] != '\0') {
		sendData(str[i]); // Send the current character to the LCD
		i++;              // Move to the next character
	}
}

void sendDataString(char* str, uint8 length) {
	for (uint8 i = 0; i < length; i++)
		sendData(str[i]); // Send the current character to the LCD
}

void float_to_string(float* value, char *buffer, int precision) {
	float val = *value;          // dereference the pointer once

	int is_negative = val < 0;
	if (is_negative) val = -val;

	int int_part = (int)val;
	float frac_part = val - int_part;

	char *p = buffer;
	if (is_negative) *p++ = '-';

	// integer part
	char temp[16]; int i = 0;
	if (int_part == 0) temp[i++] = '0';
	while (int_part > 0) { temp[i++] = '0' + (int_part % 10); int_part /= 10; }
	while (i > 0) *p++ = temp[--i];

	// decimal part
	*p++ = '.';
	for (int d = 0; d < precision; d++) {
		frac_part *= 10;
		int digit = (int)frac_part;
		*p++ = '0' + digit;
		frac_part -= digit;
	}
	*p = '\0';
}