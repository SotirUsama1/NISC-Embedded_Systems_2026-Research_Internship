#include "lcd.h"
#include "keypad.h"
struct key{
    int row;
    int col;
};
int main(void)
{
    lcd_init();
    keypad_init();
    while (1) {
        key pressedKey = pressed_key();
        int keyValue = get_key(pressedKey);
        if (keyValue != -1) {
            lcd_write_char('0' + keyValue);
        }
    }
}