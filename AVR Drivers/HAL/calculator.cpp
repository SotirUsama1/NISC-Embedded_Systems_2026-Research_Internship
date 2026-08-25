#include "calculator.h"

int res = 0;
int operands[] = {0,0};
unsigned char op = 0x00;
bool opSelect = 0, zeroDiv = 0;
char buffer[12];
char *zeroDivMsg = "Zero Division Error";
bool idle = 0;

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