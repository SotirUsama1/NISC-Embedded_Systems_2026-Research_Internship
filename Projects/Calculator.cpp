#include <avr/io.h>

#define F_CPU 16000000UL
#include <util/delay.h>

#define LCDMask (1 | 1<<1 | 1<<2 | 1<<4)
#define LCDPort PORTB
#define LCDDDR  DDRB
#define enable 2        //Enable = on
#define RS 3            //Send command = off, send data = on

void updateLCD(void);
void sendCommand(unsigned char command);
void sendData(unsigned char character);
void sendInitCommand(unsigned char command);
void LCDInit();

#define NULL 0x00

#define rMask 0b00111100
#define r1 PORTC5
#define r2 PORTC4
#define r3 PORTC3
#define r4 PORTC2

#define cMask 0b11101000
#define c1 PORTD7
#define c2 PORTD6
#define c3 PORTD5
#define c4 PORTD3

unsigned char rowCol = NULL;                // register to hold current row and column the 4 High bits for 4 rows and the 4 low bits for the columns
bool pressed = 0;

unsigned char rowColDecoder_keypad (unsigned char rowCol);
void colRead_keyPad (unsigned char c);
void colScan_keypad();
void rowEn_KeyPad (unsigned char r);
void call_keypad();


int main(void)
{
	LCDDDR |= LCDMask;
	DDRA |= 1 << enable | 1 << RS;                     //Set control lines as output (high)
	DDRC &= ~(rMask);
	DDRD &= ~(cMask);

	while(1) {
		
		LCDInit();
		
		unsigned char letter = rowColDecoder_keypad(rowCol);
		if (!letter) continue;
		else sendData(letter);
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

void rowEn_KeyPad (unsigned char r){
	DDRC |= 1<<r;
	PORTC &= ~(1<<r);
	rowCol = 1<<(r+2);
	colScan_keypad();
	DDRC ^= 1<<r;
}

void colRead_keyPad (unsigned char c){
	unsigned char m = (c==c1)?0x08:(c==c2)?0x04:(c==c1)?0x02:(c==c2)?0x01:0x00;
	if(!(PIND & (1<<c))){
		if(!pressed){
			rowCol |= (1<<m);
			pressed=1;
			rowColDecoder_keypad(rowCol);
		}
	}
	else
	pressed = 0;
}

void colScan_keypad(){
	colRead_keyPad(c1);
	colRead_keyPad(c2);
	colRead_keyPad(c3);
	colRead_keyPad(c4);
}

void call_keypad(){
	rowEn_KeyPad(r1);
	rowEn_KeyPad(r2);
	rowEn_KeyPad(r3);
	rowEn_KeyPad(r4);
}

unsigned char rowColDecoder_keypad (unsigned char rowCol){
	unsigned char letter = NULL;
	switch (rowCol)
	{
		case 0x88:
		letter = '1';
		break;
		case 0x84:
		letter = '2';
		break;
		case 0x82:
		letter = '3';
		break;
		case 0x81:
		letter = '+';
		break;
		case 0x48:
		letter = '4';
		break;
		case 0x44:
		letter = '5';
		break;
		case 0x42:
		letter = '6';
		break;
		case 0x41:
		letter = '-';
		break;
		case 0x28:
		letter = '7';
		break;
		case 0x24:
		letter = '8';
		break;
		case 0x22:
		letter = '9';
		break;
		case 0x21:
		letter = '*';
		break;
		case 0x18:
		letter = 'c';
		break;
		case 0x14:
		letter = '0';
		break;
		case 0x12:
		letter = '=';
		break;
		case 0x11:
		letter = '/';
		break;
		default:
		letter = NULL;
		break;
	}
	
	return letter;
}