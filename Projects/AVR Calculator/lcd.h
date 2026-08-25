#ifndef LCD_H
#define LCD_H

#include "common.h"

#define LCDMask (1 | 1<<1 | 1<<2 | 1<<4)
#define LCDPort PORTB
#define LCDDDR  DDRB
#define enable 2        //Enable = on
#define RS 3            //Send command = off, send data = on

void updateLCD(void);
void sendCommand(unsigned char command);
void sendData(unsigned char character);
void sendDataString(char* str);
void sendInitCommand(unsigned char command);
void LCDInit();

#endif