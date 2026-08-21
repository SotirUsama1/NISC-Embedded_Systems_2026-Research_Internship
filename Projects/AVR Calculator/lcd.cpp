#include "lcd.h"

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