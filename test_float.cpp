#include "stdio.h"
//using namespace std;
void float_to_string(float* value, char *buffer, int precision) {
	float val = *value;          // dereference the pointer once

	int is_negative = val < 0;
	if (is_negative) val = -val;

	int int_part = (int)val;
	float frac_part = val - int_part;

	char *p = buffer;
	if (is_negative) *p++ = '-';

	// integer part
	char temp[16]; int i = 0;
	if (int_part == 0) temp[i++] = '0';
	while (int_part > 0) { temp[i++] = '0' + (int_part % 10); int_part /= 10; }
	while (i > 0) *p++ = temp[--i];

	// decimal part
	*p++ = '.';
	for (int d = 0; d < precision; d++) {
		frac_part *= 10;
		int digit = (int)frac_part;
		*p++ = '0' + digit;
		frac_part -= digit;
	}
	*p = '\0';
}
int main(){
char fl[20];
unsigned int myfloat = 0x40234322;
float_to_string((float*)&myfloat, fl, 2);
for (int i = 0; i < 5; i++){
	printf("I: %d\t",i);
	printf("%02x \n", (unsigned char)fl[i]);
}
}
