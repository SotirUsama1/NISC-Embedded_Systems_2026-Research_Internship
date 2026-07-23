#include <avr/io.h>
#define F_CPU 16000000UL
#include <util/delay.h>
#include "lcd.h"

int main(void) {
	Init_LCD();
	sendData(0x41);
	sendData(0x42);
	while (1) {}
	return 0;
}
