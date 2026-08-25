#include "keypad.h"

unsigned char rowCol = 0x00;

void rowEn_KeyPad (unsigned char r){
	DDRC |= 1<<r;
	PORTC &= ~(1<<r);
	rowCol = 1<<(r+2);
	colScan_keypad();
	DDRC ^= 1<<r;
}

void colRead_keyPad (unsigned char c){
	// 3. Fixed the c3 and c4 typo
	unsigned char m = (c==c1)?0x08:(c==c2)?0x04:(c==c3)?0x02:(c==c4)?0x01:0x00;
	
	if(!(PIND & (1<<c))){
		// 4. Fixed the bitshift overflow (m is already the mask)
		rowCol |= m;
	}
}

void colScan_keypad(){
	colRead_keyPad(c1);
	colRead_keyPad(c2);
	colRead_keyPad(c3);
	colRead_keyPad(c4);
}

void call_keypad(){
	rowCol = 0; // Clear any previous state
	
	// 2. Abort scanning if a column press (lower 4 bits) is detected
	rowEn_KeyPad(r1); if(rowCol & 0x0F) return;
	rowEn_KeyPad(r2); if(rowCol & 0x0F) return;
	rowEn_KeyPad(r3); if(rowCol & 0x0F) return;
	rowEn_KeyPad(r4); if(rowCol & 0x0F) return;
}

unsigned char rowColDecoder_keypad (unsigned char rowCol){
	unsigned char letter = 0x00;
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
		letter = 0x00;
		break;
	}
	
	return letter;
}