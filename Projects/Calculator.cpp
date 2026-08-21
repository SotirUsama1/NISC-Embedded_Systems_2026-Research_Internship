#include <avr/io.h>
#include <stdlib.h>

#define F_CPU 16000000UL
#include <util/delay.h>

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

unsigned char rowCol = 0x00;                // register to hold current row and column the 4 High bits for 4 rows and the 4 low bits for the columns
bool pressed = 0;

unsigned char rowColDecoder_keypad (unsigned char rowCol);
void colRead_keyPad (unsigned char c);
void colScan_keypad();
void rowEn_KeyPad (unsigned char r);
void call_keypad();

int dtoi (unsigned char s);					// digit to int
void operate (unsigned char digit);
void calculate();
void printRes();

int res = 0;
int operands[] = {0,0};
unsigned char op = 0x00;
bool opSelect = 0, zeroDiv = 0;
char buffer[12];
char *zeroDivMsg = "Zero Division Error";
bool idle = 0;

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


void updateLCD() {

	PORTA |= 1 << enable;                     //Enable
	asm volatile ("nop");
	asm volatile ("nop");
	PORTA &= ~1 << enable;                    //Disable
}

void sendCommand(unsigned char command) {
	PORTA &= ~(1 << RS);     //Set RS low (write command)
	LCDPort |= ((((command >> 4) & ~0x07) << 1) | ((command >> 4) & 0x07)) & LCDMask;             //Send 4 ms bits
	updateLCD();
	LCDPort &= ~LCDMask;
	LCDPort |= (((command & ~0x07) << 1) | (command & 0x07)) & LCDMask;                    //Send 4 ls bits
	updateLCD();
	LCDPort &= ~LCDMask;                             //Clear data lines

}

void sendData(unsigned char character) {
	_delay_ms(5);
	PORTA |= 1 << RS;                 //Set R/W low and RS high (write data)
	LCDPort |= ((((character >> 4) & ~0x07) << 1) | ((character >> 4) & 0x07)) & LCDMask;           //Send 4 ms bits
	updateLCD();
	LCDPort &= ~LCDMask;
	PORTA |= 1 << RS;
	LCDPort |= (((character & ~0x07) << 1) | (character & 0x07)) & LCDMask;                //Send 4 ls bits
	updateLCD();
	LCDPort &= ~LCDMask;                             //Clear data lines
}

void sendInitCommand(unsigned char command) {

	PORTA &= ~(1 << RS);     // Set RS low (write command)
	LCDPort |= (((command & ~0x07) << 1) | (command & 0x07)) & LCDMask;
	updateLCD();
	LCDPort &= ~LCDMask;                             //Clear data lines
}

void LCDInit(){
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
}

void rowEn_KeyPad (unsigned char r){
	DDRC |= 1<<r;
	PORTC &= ~(1<<r);
	rowCol = 1<<(r+2);
	colScan_keypad();
	DDRC ^= 1<<r;
}

void colRead_keyPad (unsigned char c){
	// 3. Fixed the c3 and c4 typo
	unsigned char m = (c==c1)?0x08:(c==c2)?0x04:(c==c3)?0x02:(c==c4)?0x01:0x00;
	
	if(!(PIND & (1<<c))){
		// 4. Fixed the bitshift overflow (m is already the mask)
		rowCol |= m;
	}
}

void colScan_keypad(){
	colRead_keyPad(c1);
	colRead_keyPad(c2);
	colRead_keyPad(c3);
	colRead_keyPad(c4);
}

void call_keypad(){
	rowCol = 0; // Clear any previous state
	
	// 2. Abort scanning if a column press (lower 4 bits) is detected
	rowEn_KeyPad(r1); if(rowCol & 0x0F) return;
	rowEn_KeyPad(r2); if(rowCol & 0x0F) return;
	rowEn_KeyPad(r3); if(rowCol & 0x0F) return;
	rowEn_KeyPad(r4); if(rowCol & 0x0F) return;
}

unsigned char rowColDecoder_keypad (unsigned char rowCol){
	unsigned char letter = 0x00;
	switch (rowCol)
	{
		case 0x88:
		letter = '1';
		break;
		case 0x84:
		letter = '2';
		break;
		case 0x82:
		letter = '3';
		break;
		case 0x81:
		letter = '+';
		break;
		case 0x48:
		letter = '4';
		break;
		case 0x44:
		letter = '5';
		break;
		case 0x42:
		letter = '6';
		break;
		case 0x41:
		letter = '-';
		break;
		case 0x28:
		letter = '7';
		break;
		case 0x24:
		letter = '8';
		break;
		case 0x22:
		letter = '9';
		break;
		case 0x21:
		letter = '*';
		break;
		case 0x18:
		letter = 'c';
		break;
		case 0x14:
		letter = '0';
		break;
		case 0x12:
		letter = '=';
		break;
		case 0x11:
		letter = '/';
		break;
		default:
		letter = 0x00;
		break;
	}
	
	return letter;
}

int dtoi (unsigned char s){
	return (int) s - 48;
}

void operate (unsigned char digit){
	if (digit == '+' || digit == '-' || digit == '*' || digit == '/') {
		opSelect = !opSelect;
		op = digit;
		return;
	}
	
	int dig = dtoi(digit), operand = (!opSelect)?operands[0]:operands[1];
	operand = operand * 10 + dig;
	if (!opSelect) operands[0] = operand;
	else operands[1] = operand;
}

void calculate(){
	if (op == '+') res = operands[0] + operands[1];
	else if (op == '-') res = operands[0] - operands[1];
	else if (op == '*') res = operands[0] * operands[1];
	else {
		if (operands[1] == 0) {
			zeroDiv = 1;
			return;
		}
		res = operands[0] / operands[1];
	}
}

void sendDataString(char* str) {
	int i = 0;

	while (str[i] != '\0') {
		sendData(str[i]); // Send the current character to the LCD
		i++;              // Move to the next character
	}
}