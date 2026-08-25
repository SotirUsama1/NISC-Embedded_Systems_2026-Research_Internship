/*

EN = PA2
RS PA3  -> 0
EN PA2  -> 1
0x08 -> 1000
0x0E -> 1110
N=1
F=0
0x28 = 00101000
I/D = 1: Increment
*/
// Source - https://stackoverflow.com/q/14166730
// Posted by nettogrisen
// Retrieved 2026-07-21, License - CC BY-SA 3.0

//  Connection:
//
//  Atmega32    LCD
//  PB0     ->  DB4
//  PB1     ->  DB5
//  PB2     ->  DB6
//  PB3     ->  DB7
//  PB4     ->  RS
//  PB5     ->  R/W
//  PB6     ->  E
//  PB7     ->

#include <avr/io.h>

#define F_CPU 16000000UL
#include <util/delay.h>


#define LCDPort PORTB
#define LCDDDR  DDRB
#define enable 2        //Enable = on
#define RS 3            //Send command = off, send data = on
#define readWrite 5

void updateLCD(void);
void sendCommand(unsigned char command);
void sendData(unsigned char character);
void sendInitCommand(unsigned char command);

int main(void)
{
	LCDDDR |= 0b1111;
	DDRA |= 1 << enable;                      //Set control lines as output (high)
	DDRA  |= 1 << RS;

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


	sendData('A');                             //Display "A"
	 _delay_ms(5);
	
	sendData(0x42);                             //Display "B"
	 _delay_ms(5);

	sendData(0x43);                             //Display "C"
	 _delay_ms(5);

	sendData(0x44);                             //Display "D"
	 _delay_ms(5);

	sendData(0x45);                             //Display "E"



	while(1) {

	}

	return 0;
}


void updateLCD() {

	PORTA |= 1 << enable;                     //Enable
	asm volatile ("nop");
	asm volatile ("nop");
	PORTA &= ~1 << enable;                    //Disable
}

void sendCommand(unsigned char command) {

	PORTA &= ~(1 << readWrite | 1 << RS);     //Set R/W and RS low (write command)
	LCDPort |= (command >> 4) & 15;             //Send 4 ms bits
	updateLCD();
	LCDPort &= ~15;
	LCDPort |= command & 15;                    //Send 4 ls bits
	updateLCD();
	LCDPort &= ~15;                             //Clear data lines

}

void sendData(unsigned char character) {

	LCDPort &= ~1 << readWrite;                 //Set R/W low and RS high (write data)
	PORTA |= 1 << RS;
	LCDPort |= (character >> 4 & 15);           //Send 4 ms bits
	updateLCD();
	LCDPort &= ~1 << readWrite;
	LCDPort &= ~15;
	PORTA |= 1 << RS;
	LCDPort |= (character & 15);                //Send 4 ls bits
	updateLCD();
	LCDPort &= ~15;                             //Clear data lines
}

void sendInitCommand(unsigned char command) {

	PORTA &= ~(1 << readWrite | 1 << RS);     //Set R/W and RS low (write command)
	LCDPort |= command & 15;
	updateLCD();
	LCDPort &= ~15;                             //Clear data lines
}
