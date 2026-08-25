#ifndef CALCULATOR_H
#define CALCULATOR_H

#include "common.h"

int dtoi (unsigned char s);					// digit to int
void operate (unsigned char digit);
void calculate();
void printRes();

extern int res;
extern int operands[2];
extern unsigned char op;
extern bool opSelect, zeroDiv;
extern char buffer[12];
extern char *zeroDivMsg;
extern bool idle;

#endif