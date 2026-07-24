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

key pressed_key()
{
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
                return {r, c};
            }else{
                DDRC &= ~(1 << (r+2)); 
                return {-1, -1}; // no key pressed
            }
        }
}
}




int get_key(key pressedKey)
{
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
 

#endif /* KEYPAD_H */