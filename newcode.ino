/*
 * ============================================================
 *  ESP-WROOM-32  —  Modbus RTU Sniffer → InfluxDB 3 Core Gateway
 * ============================================================
 *
 *  Passively sniffs the UART TTL bus shared between an
 *  ATMEGA32 (Modbus master) and a pH/Temperature sensor
 *  (Modbus slave, address 0x03).
 *
 *  Extracts pH and temperature float values from Modbus RTU
 *  "Read Holding Registers" (FC 0x03) responses and POSTs
 *  them to InfluxDB 3 Core via the native /api/v3/write_lp
 *  HTTP endpoint.
 *
 *  ── Master request (sent by ATMEGA32): ──
 *   TX: 03 03 00 00 00 06 C4 2A
 *        │  │  └─────┘ └─────┘ └── CRC-16
 *        │  └─ FC 0x03 (Read Holding Registers)
 *        └─── Slave address 0x03
 *
 *  ── Sensor response: ──
 *   RX: 03 03 0C [b0 b1 b2 b3] [b4 b5 b6 b7] [...] CRC_lo CRC_hi
 *        │  │  │   └─ pH float    └─ Temp float
 *        │  │  └── byte count = 12 (6 regs × 2 bytes)
 *        │  └─ FC 0x03
 *        └─── Slave address 0x03
 *
 *  Floats: big-endian IEEE 754 from sensor → byte-swapped
 *  to little-endian (matches ATMEGA32 Sensor.cpp logic).
 *
 *  Board: "ESP32 Dev Module" in Arduino IDE
 * ============================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>

/* ═══════════════════════════════════════════════════════════
 *                    USER CONFIGURATION
 * ═══════════════════════════════════════════════════════════ */

// WiFi
const char* WIFI_SSID     = "ESP";
const char* WIFI_PASSWORD = "12345678";

// InfluxDB 3 Core
// NOTE: update this IP any time your host's DHCP lease changes
// (very common on phone hotspots). Check with:
//   ifconfig en0 | grep "inet "        (macOS)
const char* INFLUXDB_URL      = "http://10.82.55.58:8181";  // Docker host LAN IP
const char* INFLUXDB_DATABASE = "sensor_monitoring";        // InfluxDB 3 "database" (was "bucket" in v2)
const char* INFLUXDB_TOKEN    = "";                          // empty = no auth

// Measurement (table) name written to InfluxDB
const char* MEASUREMENT = "water_quality";

// UART — RX only (passive sniffer)
#define UART_BAUD   9600
#define UART_RX_PIN 16   // GPIO16 (RX2)
#define UART_TX_PIN 17   // GPIO17 (TX2) — unused, required by API

// Modbus target
#define MODBUS_SLAVE_ADDR  0x03   // pH/Temp sensor address
#define MODBUS_FUNC_CODE   0x03   // Read Holding Registers

// Post interval — how often to send data to InfluxDB (ms)
unsigned long POST_INTERVAL_MS = 5000;

// ─── TEST MODE ──────────────────────────────────────────
//  true  → sends fake pH/temperature every POST_INTERVAL_MS
//  false → normal Modbus sniffer mode
#define TEST_MODE true

/* ═══════════════════════════════════════════════════════════
 *                   END CONFIGURATION
 * ═══════════════════════════════════════════════════════════ */

HardwareSerial UARTSerial(2);

// Modbus frame buffer
#define FRAME_BUF_SIZE 128
uint8_t frameBuf[FRAME_BUF_SIZE];
size_t  frameLen = 0;

// Inter-character timeout for frame boundary detection
// 9600 baud: 1 char ≈ 1.04 ms → 3.5 chars ≈ 3.6 ms → use 4 ms
#define MODBUS_FRAME_TIMEOUT_US 4000
unsigned long lastByteTime = 0;

// Latest readings
float lastPH          = 0.0;
float lastTemperature = 0.0;
bool  hasNewData      = false;
unsigned long lastPostTime = 0;

// Test mode
uint32_t testCounter = 0;

// Status LED
#define LED_PIN 2

void blinkLED(int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW);  delay(ms);
  }
}

/* ═══════════════════════════════════════════════════════════
 *                          WiFi
 * ═══════════════════════════════════════════════════════════ */

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 15000) {
      Serial.println("\n[WiFi] Timeout — retrying...");
      WiFi.disconnect(); delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      start = millis();
    }
  }
  Serial.printf("\n[WiFi] Connected | IP: %s\n",
                WiFi.localIP().toString().c_str());
  blinkLED(3, 150);
}

/* ═══════════════════════════════════════════════════════════
 *                      Modbus CRC-16
 * ═══════════════════════════════════════════════════════════ */

uint16_t modbusCRC16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

/* ═══════════════════════════════════════════════════════════
 *            Float conversion (big-endian → LE)
 * ═══════════════════════════════════════════════════════════ */

// Sensor transmits IEEE 754 floats in big-endian order.
// Byte-swap to little-endian — same as ATMEGA32 Sensor.cpp:
//   swapped = { b[3], b[2], b[1], b[0] }

float bytesToFloat(const uint8_t* b) {
  uint8_t swapped[4] = { b[3], b[2], b[1], b[0] };
  float val;
  memcpy(&val, swapped, sizeof(val));
  return val;
}

/* ═══════════════════════════════════════════════════════════
 *                  Modbus Frame Parser
 * ═══════════════════════════════════════════════════════════ */

void processModbusFrame() {
  // Minimum: addr(1) + func(1) + count(1) + data(2) + crc(2) = 7
  if (frameLen < 5) return;

  uint8_t addr = frameBuf[0];
  uint8_t func = frameBuf[1];

  // Only process responses from the target sensor
  if (addr != MODBUS_SLAVE_ADDR || func != MODBUS_FUNC_CODE) return;

  // Verify CRC-16
  uint16_t rxCRC = frameBuf[frameLen - 2] | (frameBuf[frameLen - 1] << 8);
  uint16_t calcCRC = modbusCRC16(frameBuf, frameLen - 2);
  if (rxCRC != calcCRC) {
    Serial.printf("[Modbus] CRC fail: got 0x%04X, want 0x%04X\n", rxCRC, calcCRC);
    return;
  }

  // FC 0x03 response: [addr][func][byte_count][data...][crc_lo][crc_hi]
  uint8_t byteCount = frameBuf[2];

  // Need at least 8 data bytes for 2 floats (pH + temperature)
  if (byteCount < 8) {
    Serial.printf("[Modbus] Too few data bytes: %d (need >= 8)\n", byteCount);
    return;
  }

  // Validate frame length: 3 (header) + byteCount + 2 (CRC)
  if (frameLen != (size_t)(3 + byteCount + 2)) {
    Serial.printf("[Modbus] Length mismatch: %d vs %d\n",
                  frameLen, 3 + byteCount + 2);
    return;
  }

  // Extract pH (bytes 3-6) and temperature (bytes 7-10)
  lastPH          = bytesToFloat(&frameBuf[3]);
  lastTemperature = bytesToFloat(&frameBuf[7]);
  hasNewData = true;

  Serial.printf("[Modbus] OK | pH=%.2f  Temp=%.1f C  (%d data bytes)\n",
                lastPH, lastTemperature, byteCount);
}

/* ═══════════════════════════════════════════════════════════
 *              InfluxDB 3 Core Writer (native v3 API)
 * ═══════════════════════════════════════════════════════════ */

bool writeToInflux(const String& lineProtocol) {
  // Native InfluxDB 3 write endpoint: /api/v3/write_lp
  // Query params: db=<database name>, precision=<auto|second|millisecond|...>
  String url = String(INFLUXDB_URL)
    + "/api/v3/write_lp?db=" + String(INFLUXDB_DATABASE)
    + "&precision=millisecond";

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "text/plain; charset=utf-8");
  if (strlen(INFLUXDB_TOKEN) > 0) {
    http.addHeader("Authorization", "Bearer " + String(INFLUXDB_TOKEN));
  }

  int code = http.POST(lineProtocol);
  // v3 write_lp returns 204 No Content on success
  bool ok = (code == 204 || code == 200);

  if (ok) {
    Serial.printf("[InfluxDB] OK (HTTP %d)\n", code);
  } else {
    Serial.printf("[InfluxDB] FAIL HTTP %d: %s\n",
                  code, http.getString().c_str());
  }
  http.end();
  return ok;
}

void postSensorData(float ph, float temp) {
  // Line protocol: measurement,tag=value field=value,field=value timestamp
  String line = String(MEASUREMENT)
    + ",sensor=ph_temp"
    + " ph=" + String(ph, 2)
    + ",temperature=" + String(temp, 1);

  Serial.printf("[POST] pH=%.2f  Temp=%.1f C -> InfluxDB ... ", ph, temp);
  writeToInflux(line);
}

/* ═══════════════════════════════════════════════════════════
 *                      Test Mode
 * ═══════════════════════════════════════════════════════════ */

void sendTestData() {
  testCounter++;
  float ph   = 7.0  + (float)(random(-150, 150)) / 100.0;  // 5.5–8.5
  float temp = 25.0 + (float)(random(-50, 50))   / 10.0;   // 20–30 C

  Serial.printf("[TEST #%u] pH=%.2f  Temp=%.1f C\n", testCounter, ph, temp);
  postSensorData(ph, temp);
}

/* ═══════════════════════════════════════════════════════════
 *                         SETUP
 * ═══════════════════════════════════════════════════════════ */

void setup() {
  Serial.begin(115200);
  Serial.println("\n================================================");
  Serial.println(TEST_MODE
    ? "  ESP32 Modbus Gateway  [TEST MODE]"
    : "  ESP32 Modbus RTU Sniffer -> InfluxDB 3");
  Serial.println("================================================");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  randomSeed(analogRead(0));

  if (!TEST_MODE) {
    UARTSerial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial.printf("[UART]   RX=GPIO%d  %d baud\n", UART_RX_PIN, UART_BAUD);
    Serial.printf("[Modbus] Slave 0x%02X  FC 0x%02X\n",
                  MODBUS_SLAVE_ADDR, MODBUS_FUNC_CODE);
  }

  connectWiFi();

  Serial.printf("[Config] Interval: %lu ms\n", POST_INTERVAL_MS);
  Serial.printf("[Config] InfluxDB: %s -> db=%s\n\n", INFLUXDB_URL, INFLUXDB_DATABASE);

  lastPostTime = millis();
  lastByteTime = micros();
}

/* ═══════════════════════════════════════════════════════════
 *                          LOOP
 * ═══════════════════════════════════════════════════════════ */

void loop() {
  connectWiFi();

  if (TEST_MODE) {
    // ── TEST: send fake data at interval ──
    if (millis() - lastPostTime >= POST_INTERVAL_MS) {
      sendTestData();
      lastPostTime = millis();
      blinkLED(1, 50);
    }

  } else {
    // ── SNIFFER: read UART, parse Modbus frames ──

    while (UARTSerial.available()) {
      // Frame boundary: silence > 3.5 char times → previous frame is done
      if (frameLen > 0 && (micros() - lastByteTime > MODBUS_FRAME_TIMEOUT_US)) {
        processModbusFrame();
        frameLen = 0;
      }

      if (frameLen < FRAME_BUF_SIZE) {
        frameBuf[frameLen++] = UARTSerial.read();
      }
      lastByteTime = micros();
    }

    // Check timeout when no more bytes
    if (frameLen > 0 && (micros() - lastByteTime > MODBUS_FRAME_TIMEOUT_US)) {
      processModbusFrame();
      frameLen = 0;
    }

    // Post at configured interval
    if (hasNewData && (millis() - lastPostTime >= POST_INTERVAL_MS)) {
      postSensorData(lastPH, lastTemperature);
      hasNewData   = false;
      lastPostTime = millis();
      blinkLED(1, 50);
    }
  }
}

