#ifndef KEYPAD_H
#define KEYPAD_H

#include "common.h"

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

extern unsigned char rowCol;                // register to hold current row and column the 4 High bits for 4 rows and the 4 low bits for the columns

unsigned char rowColDecoder_keypad (unsigned char rowCol);
void colRead_keyPad (unsigned char c);
void colScan_keypad();
void rowEn_KeyPad (unsigned char r);
void call_keypad();


#endif