#include <HardwareSerial.h>

#define UART_BAUD   9600
#define UART_RX_PIN 16
#define UART_TX_PIN 17   // Required by begin(), not used

HardwareSerial UARTSerial(2);

void setup() {
  Serial.begin(115200);

  UARTSerial.begin(
    UART_BAUD,
    SERIAL_8N1,
    UART_RX_PIN,
    UART_TX_PIN
  );

  Serial.println();
  Serial.println("=== Raw UART Sniffer ===");
  Serial.printf("Listening on GPIO%d @ %d baud\n",
                UART_RX_PIN, UART_BAUD);
}

void loop() {
  int available = UARTSerial.available();

  if (available > 0) {
    Serial.printf("Received %d byte(s): ", available);

    while (UARTSerial.available()) {
      uint8_t b = UARTSerial.read();
      Serial.printf("%02X ", b);
    }

    Serial.println();
  }
}
