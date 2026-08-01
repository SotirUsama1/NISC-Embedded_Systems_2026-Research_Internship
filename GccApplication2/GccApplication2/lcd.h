#ifndef LCD_H
#define LCD_H
#include "common.h"
#define LCDPort PORTB
#define LCDDDR  DDRB

#define enable 6
#define RS 4

void updateLCD(void);
void sendCommand(unsigned char command);
void sendData(unsigned char character);
void sendInitCommand(unsigned char command);
void Init_LCD(void);

#endif