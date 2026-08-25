#include "common.h"
#include "lcd.h"
#include "keypad.h"
#include "calculator.h"

int main(void)
{	
	LCDDDR |= LCDMask;
	DDRA |= 1 << enable | 1 << RS;
	DDRC &= ~(rMask);
	DDRD &= ~(cMask);
	
	LCDInit();
	sendData('A');

	while(1) {
		
		call_keypad();

		unsigned char letter = rowColDecoder_keypad(rowCol);
		
		if (letter) {
			if (letter == 'c') {
				res = 0;
				operands[0] = 0;
				operands[1] = 0;
				op = 0x00;
				opSelect = 0;
				zeroDiv = 0;
				idle = 0;
				sendCommand(0x01);
				_delay_ms(60);
			} else if (!idle && letter == '=') {
				sendCommand(0x94);
				sendData('=');
				calculate();
				if (zeroDiv) {
					sendDataString(zeroDivMsg);
				}
				else {
					itoa(res, buffer, 10);
					sendDataString(buffer);
				}
				idle = 1;

			} else if (!idle){
				operate(letter);
				sendData(letter);
			}
			_delay_ms(300);
		}
	}

	return 0;
}
