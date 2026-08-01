#include "lcd.h"
#include "keypad.h"
int do_op(char op, int num1, int num2)
{
	switch (op) {
		case '+':
		return num1 + num2;
		case '-':
		return num1 - num2;
		case '*':
		return num1 * num2;
		case '/':
		if (num2 != 0) {
			return num1 / num2;
			} else {
			// Handle division by zero error
			return 0; // or some error code
		}
		default:
		return 0; // or some error code
	}

}
bool not_operator(int keyValue)
{
	return keyValue != '+' && keyValue != '-' && keyValue != '*' && keyValue != '/';
}
int main(void)
{
	Init_LCD();
	keypad_init();
	while (1) {
		int num1=0;
		int keyValue = get_key();
		if (keyValue != -1 && not_operator(keyValue)) {
			num1*= 10;
			num1+= keyValue;
			sendData('0' + keyValue); // to write as character not int
			}else if (keyValue != -1 && !not_operator(keyValue)) {
			char op = keyValue;
			sendData(op);
			int num2=0;
			while (1) {
				keyValue = get_key();
				if (keyValue != -1 && not_operator(keyValue)) {
					num2*= 10;
					num2+= keyValue;
					sendData('0' + keyValue); // to write as character not int
					}else if (keyValue != -1 && keyValue == '=') {
					sendData('=');
					int result = do_op(op, num1, num2);
					sendData(result);
					break; // exit inner loop after displaying result
				}
			}
		}
		
	}
}