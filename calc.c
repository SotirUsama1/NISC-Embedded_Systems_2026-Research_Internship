#include <avr/io.h>
#include <stdint.h>
#define F_CPU 16000000UL
#include <util/delay.h>
#ifndef PORTB
#define PORTB (*(volatile uint8_t*)0x25)
#define DDRB (*(volatile uint8_t*)0x24)
#endif
#define LCDPort PORTB
#define LCDDDR  DDRB
#define enable 6        //Enable = on
#define readWrite 5     //Read = on, Write = off
#define RS 4            //Send command = 0, send data = 1


void updateLCD(void);
void sendCommand(unsigned char command);
void sendData(unsigned char character);
void sendInitCommand(unsigned char command);

int main(void)
{
    LCDDDR |= 15;
    LCDDDR |= 1 << enable;                      //Set control lines as output (high)
    LCDDDR  |= 1 << readWrite;
    LCDDDR  |= 1 << RS;                         

    _delay_ms(100);                              //Wait for LCD to boot
    sendInitCommand(0x3);                        //Init function set 1
    _delay_ms(100);
    sendInitCommand(0x3);                        //Init function set 2
    _delay_us(100);
    sendInitCommand(0x3);                        //Init function set 3
    _delay_us(100);
    sendInitCommand(0x2);                        //Function set (set 4 bit mode)
    _delay_us(100);
    sendInitCommand(0x28);                       //Funcion set I=1, N=0, F=0
    //sendInitCommand(0x8);                      
    _delay_us(60);
    sendInitCommand(0x8);                        //On/off control D=0, C=0, B=0
    //sendInitCommand(0x8);
    _delay_us(60);
    sendCommand(0x01);                           //Clear display
    //sendInitCommand(0x1);
    _delay_ms(60);
    sendCommand(0x06);                           //Entry mode set I/D=1, S=0
    //sendInitCommand(0x6);
    _delay_us(60);
    sendCommand(0x0C);                           //On/off control D=1, C=0, B=0
    //sendInitCommand(0xC);
    _delay_us(60);


    sendData(0x41);                             //Display "A"
    sendData(0x42);                             //Display "B"
    sendData(0x43);                             //Display "C"
    sendData(0x44);                             //Display "D"
    sendData(0x45);                             //Display "E"



    while(1) {

    }

    return 0;
}


void updateLCD() {

    LCDPort |= 1 << enable;                     //Enable
    asm volatile ("nop");
    asm volatile ("nop");
    LCDPort &= ~1 << enable;                    //Disable
}

void sendCommand(unsigned char command) {

    LCDPort &= ~(1 << readWrite | 1 << RS);     //Set R/W and RS low (write command)
    LCDPort |= (command >> 4) & 15;             //Send 4 ms bits
    updateLCD();
    LCDPort &= ~15; 
    LCDPort |= command & 15;                    //Send 4 ls bits
    updateLCD();
    LCDPort &= ~15;                             //Clear data lines

}

void sendData(unsigned char character) {

    LCDPort &= ~1 << readWrite;                 //Set R/W low and RS high (write data)
    LCDPort |= 1 << RS;
    LCDPort |= (character >> 4 & 15);           //Send 4 ms bits
    updateLCD();
    LCDPort &= ~1 << readWrite;
    LCDPort &= ~15;
    LCDPort |= 1 << RS;
    LCDPort |= (character & 15);                //Send 4 ls bits
    updateLCD();
    LCDPort &= ~15;                             //Clear data lines
}

void sendInitCommand(unsigned char command) {

    LCDPort &= ~(1 << readWrite | 1 << RS);     //Set R/W and RS low (write command)
    LCDPort |= command & 15;                    
    updateLCD();
    LCDPort &= ~15;                             //Clear data lines
}
