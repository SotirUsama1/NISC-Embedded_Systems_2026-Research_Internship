/*
 * ============================================================
 *  ESP32 — RX2 17-byte Modbus Sniffer -> InfluxDB 3 Gateway
 * ============================================================
 *
 *  Reads fixed 17-byte packets off UART2 (RX2), extracts pH and
 *  Temperature from a Modbus RTU "Read Holding Registers" (FC 0x03)
 *  response, and POSTs the values to InfluxDB 3 Core via HTTP.
 *
 *  Frame layout (17 bytes total):
 *   [addr][func][byteCount][ pH: b0 b1 b2 b3 ][ Temp: b4 b5 b6 b7 ][ 4 unused
 * bytes ][crc_lo][crc_hi] 1     1         1              4 4 4               1
 * 1
 *
 *  Floats are big-endian IEEE 754 on the wire -> byte-swapped to
 *  little-endian to match the sensor's transmission format.
 *
 *  Board: "ESP32 Dev Module" in Arduino IDE
 * ============================================================
 */

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

/* ═══════════════════════════════════════════════════════════
 *                    USER CONFIGURATION
 * ═══════════════════════════════════════════════════════════ */

// WiFi
const char *WIFI_SSID = "ESP";
const char *WIFI_PASSWORD = "12345678";

// InfluxDB 3 Core
const char *INFLUXDB_URL = "http://172.20.10.5:8181"; // WSL/server LAN IP
const char *INFLUXDB_DATABASE = "sensor_monitoring";
const char *INFLUXDB_TOKEN = ""; // empty = no auth

// Measurement name written to InfluxDB
const char *MEASUREMENT = "water_quality";

// UART — RX only (passive sniffer)
#define UART_BAUD 9600
#define UART_RX_PIN 16 // GPIO16 (RX2)
#define UART_TX_PIN 17 // GPIO17 (TX2) — unused, required by API

// Modbus target
#define MODBUS_SLAVE_ADDR 0x03 // pH/Temp sensor address
#define MODBUS_FUNC_CODE 0x03  // Read Holding Registers

// Fixed packet length we're sniffing
#define PACKET_LEN 17

// Post interval — minimum time between InfluxDB writes (ms)
unsigned long POST_INTERVAL_MS = 1000;

/* ═══════════════════════════════════════════════════════════
 *                    END CONFIGURATION
 * ═══════════════════════════════════════════════════════════ */

HardwareSerial RxSerial(2); // UART2

uint8_t packetBuf[PACKET_LEN];
uint8_t packetIdx = 0;

unsigned long lastPostTime = 0;

#define LED_PIN 2

void blinkLED(int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
    digitalWrite(LED_PIN, LOW);
    delay(ms);
  }
}

/* ═══════════════════════════════════════════════════════════
 *                         WiFi
 * ═══════════════════════════════════════════════════════════ */

void printNetworkDetails() {
  Serial.println("========================================");
  Serial.println("       ESP32 Network Information        ");
  Serial.println("========================================");

  // Device & Connection Info
  Serial.print("SSID (Network Name): ");
  Serial.println(WiFi.SSID());

  Serial.print("Hostname:            ");
  Serial.println(WiFi.getHostname());

  Serial.print("ESP32 MAC Address:   ");
  Serial.println(WiFi.macAddress());

  Serial.print("Router MAC (BSSID):  ");
  Serial.println(WiFi.BSSIDstr());

  Serial.print("Signal Strength:     ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  Serial.println("----------------------------------------");

  // IP Configuration Info
  Serial.print("IPv4 Address:        ");
  Serial.println(WiFi.localIP());

  Serial.print("Subnet Mask:         ");
  Serial.println(WiFi.subnetMask());

  Serial.print("Gateway IP:          ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("Primary DNS:         ");
  Serial.println(WiFi.dnsIP(0)); // 0 gets the primary DNS

  Serial.print("Secondary DNS:       ");
  Serial.println(WiFi.dnsIP(1)); // 1 gets the secondary DNS

  Serial.println("========================================");
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 15000) {
      Serial.println("\n[WiFi] Timeout — retrying...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      start = millis();
    }
  }
  Serial.printf("\n[WiFi] Connected | IP: %s\n",
                WiFi.localIP().toString().c_str());
  printNetworkDetails();
  blinkLED(3, 150);
}

/* ═══════════════════════════════════════════════════════════
 *                      Modbus CRC-16
 * ═══════════════════════════════════════════════════════════ */

uint16_t modbusCRC16(const uint8_t *data, size_t len) {
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
 *           Float conversion (big-endian → LE)
 * ═══════════════════════════════════════════════════════════ */

float bytesToFloat(const uint8_t *b) {
  uint8_t swapped[4] = {b[3], b[2], b[1], b[0]};
  float val;
  memcpy(&val, swapped, sizeof(val));
  return val;
}

/* ═══════════════════════════════════════════════════════════
 *                      Debug helpers
 * ═══════════════════════════════════════════════════════════ */

void printHex(const uint8_t *data, uint16_t len) {
  Serial.print("[RAW] ");
  for (uint16_t i = 0; i < len; i++) {
    if (data[i] < 0x10)
      Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

/* ═══════════════════════════════════════════════════════════
 *                      InfluxDB Writer
 * ═══════════════════════════════════════════════════════════ */

bool writeToInflux(const String &lineProtocol) {
  String url = String(INFLUXDB_URL) +
               "/api/v2/write?bucket=" + String(INFLUXDB_DATABASE) +
               "&precision=ms";

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "text/plain");
  if (strlen(INFLUXDB_TOKEN) > 0) {
    http.addHeader("Authorization", "Bearer " + String(INFLUXDB_TOKEN));
  }

  int code = http.POST(lineProtocol);
  bool ok = (code == 200 || code == 204);

  if (ok) {
    Serial.printf("[InfluxDB] OK (HTTP %d)\n", code);
  } else {
    Serial.printf("[InfluxDB] FAIL HTTP %d: %s\n", code,
                  http.getString().c_str());
  }
  http.end();
  return ok;
}

void postSensorData(float ph, float temp) {
  connectWiFi(); // make sure we're still connected before posting

  String line = String(MEASUREMENT) + ",sensor=ph_temp" +
                " ph=" + String(ph, 2) + ",temperature=" + String(temp, 1);

  Serial.printf("[POST] pH=%.2f  Temp=%.1f C -> InfluxDB\n", ph, temp);
  writeToInflux(line);
  blinkLED(1, 50);
}

/* ═══════════════════════════════════════════════════════════
 *   Postprocessing: parse the 17-byte packet
 * ═══════════════════════════════════════════════════════════ */

void postprocess_response(uint8_t *data, uint16_t len) {
  Serial.println("---------------------------------------------");
  Serial.printf("[Packet] Received %d bytes on RX2\n", len);
  printHex(data, len);

  uint8_t addr = data[0];
  uint8_t func = data[1];
  uint8_t byteCount = data[2];

  // Step 1: check this is the response we expect
  if (addr != MODBUS_SLAVE_ADDR || func != MODBUS_FUNC_CODE) {
    Serial.println("[Modbus] Address/function mismatch — ignoring packet");
    return;
  }

  // Step 2: verify CRC-16 (last 2 bytes of the 17)
  uint16_t rxCRC = data[len - 2] | (data[len - 1] << 8);
  uint16_t calcCRC = modbusCRC16(data, len - 2);

  if (rxCRC != calcCRC) {
    Serial.println("[Modbus] CRC check FAILED — discarding packet");
    return;
  }

  // Step 3: sanity-check byte count (need >= 8 data bytes for 2 floats)
  if (byteCount < 8) {
    Serial.printf("[Modbus] Too few data bytes: %d (need >= 8)\n", byteCount);
    return;
  }

  // Step 4: extract pH (bytes 3-6) and temperature (bytes 11-14)
  float ph = bytesToFloat(&data[3]);
  float temp = bytesToFloat(&data[11]);

  Serial.printf("[Sensor] pH = %.2f    Temperature = %.1f C\n", ph, temp);

  // Step 5: post to InfluxDB
  postSensorData(ph, temp);
}

/* ═══════════════════════════════════════════════════════════
 *                          SETUP
 * ═══════════════════════════════════════════════════════════ */

void setup() {
  Serial.begin(115200);
  Serial.println("\n================================================");
  Serial.println("  ESP32 RX2 17-byte Modbus Sniffer -> InfluxDB");
  Serial.println("================================================");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  RxSerial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.printf("[UART] RX=GPIO%d  %d baud\n", UART_RX_PIN, UART_BAUD);
  Serial.printf("[Modbus] Slave 0x%02X  FC 0x%02X  packet len=%d\n",
                MODBUS_SLAVE_ADDR, MODBUS_FUNC_CODE, PACKET_LEN);

  connectWiFi();

  // ─── Block and Retry until InfluxDB Server is Online & Configured ───
  bool influxReady = false;
  while (!influxReady) {
    Serial.println(
        "[InfluxDB] Attempting connection and schema initialization...");
    connectWiFi(); // Keep Wi-Fi alive during retry attempts

    HTTPClient http;

    // 1. Check if Database Exists and Create if not
    String dbUrl = String(INFLUXDB_URL) + "/api/v3/configure/database";
    http.begin(dbUrl);
    if (strlen(INFLUXDB_TOKEN) > 0) {
      http.addHeader("Authorization", "Bearer " + String(INFLUXDB_TOKEN));
    }
    int checkCode = http.GET();
    bool dbExists = false;

    if (checkCode == 200) {
      String response = http.getString();
      if (response.indexOf("\"" + String(INFLUXDB_DATABASE) + "\"") != -1) {
        dbExists = true;
      }
    }
    http.end();

    int dbCode = 200; // Default to success if exists
    if (!dbExists) {
      Serial.println("[InfluxDB] Database not found. Creating it...");
      http.begin(dbUrl);
      http.addHeader("Content-Type", "application/json");
      if (strlen(INFLUXDB_TOKEN) > 0) {
        http.addHeader("Authorization", "Bearer " + String(INFLUXDB_TOKEN));
      }
      dbCode = http.POST("{\"db\": \"" + String(INFLUXDB_DATABASE) + "\"}");
      http.end();
    } else {
      Serial.println("[InfluxDB] Database already exists.");
    }

    if (dbCode == 200 || dbCode == 201 || dbCode == 204 || dbCode == 409) {
      // 2. Explicitly Configure Table Schema
      String tableUrl = String(INFLUXDB_URL) + "/api/v3/configure/table";
      http.begin(tableUrl);
      http.addHeader("Content-Type", "application/json");
      if (strlen(INFLUXDB_TOKEN) > 0) {
        http.addHeader("Authorization", "Bearer " + String(INFLUXDB_TOKEN));
      }

      String tablePayload = "{"
                            "\"db\": \"" +
                            String(INFLUXDB_DATABASE) +
                            "\","
                            "\"table\": \"" +
                            String(MEASUREMENT) +
                            "\","
                            "\"tags\": [\"sensor\"],"
                            "\"fields\": ["
                            "{\"name\": \"ph\", \"type\": \"float64\"},"
                            "{\"name\": \"temperature\", \"type\": \"float64\"}"
                            "]"
                            "}";

      int tableCode = http.POST(tablePayload);
      http.end();

      if (tableCode == 200 || tableCode == 201 || tableCode == 204 ||
          tableCode == 409) {
        Serial.println("[InfluxDB] Connected! Database and schema successfully "
                       "initialized.");
        influxReady = true;
      } else {
        Serial.printf("[InfluxDB] Table configuration returned HTTP %d. "
                      "Retrying in 3s...\n",
                      tableCode);
      }
    } else {
      Serial.printf("[InfluxDB] Server unreachable or returned HTTP %d. "
                    "Retrying in 3s...\n",
                    dbCode);
    }

    if (!influxReady) {
      blinkLED(1, 100);
      delay(3000); // Wait 3 seconds before trying again
    }
  }
  // ────────────────────────────────────────────────────────────────────

  Serial.printf("[Config] Post interval: %lu ms\n", POST_INTERVAL_MS);
  Serial.printf("[Config] InfluxDB: %s -> %s\n\n", INFLUXDB_URL,
                INFLUXDB_DATABASE);

  lastPostTime = millis();
}

/* ═══════════════════════════════════════════════════════════
 *                           LOOP
 * ═══════════════════════════════════════════════════════════ */

void loop() {
  connectWiFi(); // reconnect automatically if dropped

  while (RxSerial.available()) {
    packetBuf[packetIdx++] = RxSerial.read();

    if (packetIdx == PACKET_LEN) {
      postprocess_response(packetBuf, PACKET_LEN);
      packetIdx = 0; // reset for next packet
    }
  }
}
