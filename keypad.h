#ifndef KEYPAD_H
#define KEYPAD_H
#include <avr/io.h>
#include <stdbool.h>

/* ---- Row pins ---- */
#define rMask 0b00111100
#define r1 PORTC5
#define r2 PORTC4
#define r3 PORTC3
#define r4 PORTC2

/* ---- Column pins ---- */
#define cMask 0b11101000
#define c1 PORTD7
#define c2 PORTD6
#define c3 PORTD5
#define c4 PORTD3

static void keypad_init(void)
{
    DDRC  &= ~(rMask); 
    DDRD  &= ~(cMask);
    PORTD |=  cMask; // start high. when press -> go low
}
struct key{
    int row;
    int col;
};

key pressed_key();

int get_key(); 

#endif /* KEYPAD_H */