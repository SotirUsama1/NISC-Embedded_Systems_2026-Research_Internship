/*
 * GccApplication1.cpp
 *
 * Created: 7/20/2026 11:33:15 AM
 * Author : sotir
 */ 

#include <avr/io.h>	
#define F_CPU 16000000UL
#include <util/delay.h>

int main(void)
{
	DDRA |= (1<<PORTA5);
	DDRC = 0xFF;
	DDRD = 0x00;
	
	PORTA &= ~(1<<PORTA5);
	PORTC = 0x00;
	PORTD = 0xFF;
	

	int pressed =0;
    /* Replace with your application code */
    while (1) 
    {
		if(!(PIND & (1<<PORTD3))){
			if(!pressed){
							
							PORTA ^= (1<<PORTA5);
							pressed=1;
			}

		}else{
			pressed = 0;
		}
		
		
    }
}

