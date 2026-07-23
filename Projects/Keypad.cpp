#include <avr/io.h>

#define F_CPU 16000000UL
#include <util/delay.h>

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

char rowCol = 0x00;                // register to hold current row and column the 4 High bits for 4 rows and the 4 low bits for the columns
bool preesed = 0;

unsigned char rowColDecoder_keypad (unsigned char rowCol);
void colScan_keypad();
void rowEn_KeyPad (unsigned char r);
void colRead_keyPad (unsigned char c);

void rowEn_KeyPad (unsigned char r){
	DDRC |= 1<<r;
	PORTC &= ~(1<<r);
	rowCol = 1<<(r+2);
	colScan_keypad();
	DDRC ^= 1<<r;
}

void colRead_keyPad (unsigned char c){
	unsigned char m = (c==c1)?0x08:(c==c2)?0x04:(c==c1)?0x02:(c==c2)?0x01:0x00;
	if(!(PIND & (1<<c)))
		if(!pressed){
			rowCol |= (1<<m);
			pressed=1;
			rowColDecoder_keypad(rowCol);
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

void rowColDecoder_keypad (unsigned char rowCol){
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

int main(void) {
	

	
	while (1) {

		
		
	}
	
	return 0;
}