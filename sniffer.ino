#include <HardwareSerial.h>

#define UART_BAUD   9600
#define UART_RX_PIN 16
#define UART_TX_PIN 17   // Required by begin()

HardwareSerial UARTSerial(2);

#define BUFFER_SIZE 256
uint8_t buffer[BUFFER_SIZE];

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
}

void loop() {

  int count = UARTSerial.available();

  if (count > 0) {

    // Prevent overflow
    if (count > BUFFER_SIZE)
      count = BUFFER_SIZE;

    // Read everything currently available into the buffer
    int bytesRead = UARTSerial.readBytes(buffer, count);

    // Print all bytes at once
    Serial.printf("Received %d byte(s): ", bytesRead);

    for (int i = 0; i < bytesRead; i++) {
      Serial.printf("%02X ", buffer[i]);
    }

    Serial.println();
  }
}
