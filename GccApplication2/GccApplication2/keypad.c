#include "keypad.h"
 void keypad_init(void)
{
	DDRC  &= ~(rMask);
	DDRD  &= ~(cMask);
	PORTD |=  cMask; // start high. when press -> go low
}
struct key pressed_key()
{
	struct key pressedKey={-1,-1};
		bool pressed = false;
	for (unsigned char r = 0; r < 4; r++) {
		//implement it
		DDRC |=(1<< (r+2)); // make row output
		PORTC &= ~(1 << (r+2)); // drive it low
		// iterate over columns
		for(unsigned char c = 0; c < 4; c++) {
			unsigned char colPin = (c == 0) ? c1 :
			(c == 1) ? c2 :
			(c == 2) ? c3 :
			(c == 3) ? c4 : 0;
			if (!(PIND & (1 << colPin))) {
				// key is pressed
				DDRC &= ~(1 << (r+2)); // back to input
				pressedKey.row=r; pressedKey.col=c;
				pressed = true;
				break;
			}
		}
		if(pressed) break;
	}
	return pressedKey;
}




int get_key()
{
	struct key pressedKey = pressed_key();
	if (pressedKey.row == -1 || pressedKey.col == -1) {
		return -1; // no key pressed
	}
	const char keys[4][4] = {
		{'1', '2', '3', '+'},
		{'4', '5', '6', '-'},
		{'7', '8', '9', '*'},
		{'c', '0', '=', '/'}
	};
	return keys[pressedKey.row][pressedKey.col];
}
