#include "lcd.h"
void updateLCD(void) {
	DDRA ^= (1 << enable);
	_delay_us(1);
	DDRA ^= (1 << enable);
}

void sendCommand(unsigned char command) {
	unsigned char highNibble = (command >> 4) & 0x0F;
	unsigned char lowNibble  = command & 0x0F;

	// ---- Higher 4 bits ----
	LCDPort &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	if (highNibble & 0x01) LCDPort |= (1 << PB0);   // D4
	if (highNibble & 0x02) LCDPort |= (1 << PB1);   // D5
	if (highNibble & 0x04) LCDPort |= (1 << PB2);   // D6
	if (highNibble & 0x08) LCDPort |= (1 << PB4);   // D7
	updateLCD();
	LCDPort &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));

	// ---- Lower 4 bits ----
	if (lowNibble & 0x01) LCDPort |= (1 << PB0);
	if (lowNibble & 0x02) LCDPort |= (1 << PB1);
	if (lowNibble & 0x04) LCDPort |= (1 << PB2);
	if (lowNibble & 0x08) LCDPort |= (1 << PB4);
	updateLCD();
	LCDPort &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
}

void sendData(unsigned char character) {
	unsigned char highNibble = (character >> 4) & 0x0F;
	unsigned char lowNibble  = character & 0x0F;

	PORTA |= (1 << RS);   // RS = 1 for data

	LCDPort &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	if (highNibble & 0x01) LCDPort |= (1 << PB0);   // D4
	if (highNibble & 0x02) LCDPort |= (1 << PB1);   // D5
	if (highNibble & 0x04) LCDPort |= (1 << PB2);   // D6
	if (highNibble & 0x08) LCDPort |= (1 << PB4);   // D7
	updateLCD();
	LCDPort &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));

	if (lowNibble & 0x01) LCDPort |= (1 << PB0);    // D4
	if (lowNibble & 0x02) LCDPort |= (1 << PB1);    // D5
	if (lowNibble & 0x04) LCDPort |= (1 << PB2);    // D6
	if (lowNibble & 0x08) LCDPort |= (1 << PB4);    // D7
	updateLCD();
	LCDPort &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
}

// Just send higher 4 bits (for initialization)
void sendInitCommand(unsigned char command) {
	unsigned char nibble = command & 0x0F;

	PORTA &= ~(1 << RS); // 0
	LCDPort &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	if (nibble & 0x01) LCDPort |= (1 << PB0);
	if (nibble & 0x02) LCDPort |= (1 << PB1);
	if (nibble & 0x04) LCDPort |= (1 << PB2);
	if (nibble & 0x08) LCDPort |= (1 << PB4);
	updateLCD();
	LCDPort &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
}

void Init_LCD(void) {
	LCDDDR |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4);
	DDRA   |= (1 << enable) | (1 << RS);

	_delay_ms(100);  // delay after power

	// SET 8-bit mode
	sendInitCommand(0x03);
	_delay_ms(100);
	sendInitCommand(0x3);
	_delay_us(200);
	sendInitCommand(0x3);
	_delay_us(200);

	sendInitCommand(0x02); // set 4 bit mode
	_delay_us(100);
	sendCommand(0x28);     // Function set I=1, N=0, F=0
	_delay_us(60);
	sendCommand(0x8);      // On/off control D=0, C=0, B=0
	_delay_us(60);
	sendCommand(0x01);     // Clear display
	_delay_ms(60);
	sendCommand(0x06);     // Entry mode set I/D=1, S=0
	_delay_us(60);
	sendCommand(0x0C);     // On/off control D=1, C=0, B=0
	_delay_us(60);
}

