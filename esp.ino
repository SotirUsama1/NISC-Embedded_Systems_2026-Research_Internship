/*
  ESP32 DevKit V1 -> InfluxDB 2.7
  Reads pH + temperature from an external sensor module over UART,
  then writes them to InfluxDB (measurement: "readings").

  ASSUMED UART DATA FORMAT (one line per reading):
    <ph>,<temperature>\n
    example: 6.8,25.3

  If your sensor sends a different format (labels, different order,
  different delimiter, extra fields), tell me the exact string and
  I'll adjust parseSensorLine().

  Wiring:
    Sensor TX  -> ESP32 GPIO 16 (RX2)
    Sensor RX  -> ESP32 GPIO 17 (TX2)   (only needed if sensor expects commands)
    Sensor GND -> ESP32 GND
    (Do NOT use GPIO 1/3 - those are USB serial, used for debug output)

  Library needed (Arduino Library Manager):
    - "InfluxDbClient" by Tobias Schürg
*/

#include <WiFi.h>
#include <InfluxDbClient.h>

// ---------- WiFi ----------
#define WIFI_SSID "ESP"
#define WIFI_PASSWORD "12345678"

// ---------- InfluxDB ----------
#define INFLUXDB_URL "http://192.168.1.X:8086"   // your server's LAN IP
#define INFLUXDB_TOKEN "opBJY9ugC1Pv0FYuCdSBOZYU_1BrLd4DcuYAY999Kvgc5Iq68rLJRG0HbA2WKHUfuVsgClOzMUk8TIXU8HzTsg=="
#define INFLUXDB_ORG "NU"
#define INFLUXDB_BUCKET "water_monitoring"

// ---------- UART to sensor ----------
#define SENSOR_RX_PIN 16   // ESP32 RX2 <- sensor TX
#define SENSOR_TX_PIN 17   // ESP32 TX2 -> sensor RX
#define SENSOR_BAUD 9600   // match your sensor module's baud rate

HardwareSerial SensorSerial(2); // use UART2

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point sensorPoint("readings");

String lineBuffer = "";

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected, IP: " + WiFi.localIP().toString());
}

// Parses "ph,temperature" -> fills ph/temperature by reference
// Returns true if parsing succeeded
bool parseSensorLine(String line, float &ph, float &temperature) {
  line.trim();
  int commaIndex = line.indexOf(',');
  if (commaIndex == -1) return false;

  String phStr = line.substring(0, commaIndex);
  String tempStr = line.substring(commaIndex + 1);

  ph = phStr.toFloat();
  temperature = tempStr.toFloat();

  // Basic sanity check to reject garbled reads (both toFloat() failures return 0.0)
  if (ph == 0.0 && temperature == 0.0) return false;

  return true;
}

void sendToInflux(float ph, float temperature) {
  sensorPoint.clearFields();
  sensorPoint.addField("ph", ph);
  sensorPoint.addField("temperature", temperature);

  Serial.print("Writing: ");
  Serial.println(sensorPoint.toLineProtocol());

  if (!client.writePoint(sensorPoint)) {
    Serial.print("Write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void setup() {
  Serial.begin(115200);                 // USB debug console
  SensorSerial.begin(SENSOR_BAUD, SERIAL_8N1, SENSOR_RX_PIN, SENSOR_TX_PIN); // sensor UART

  connectWiFi();

  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  while (SensorSerial.available()) {
    char c = SensorSerial.read();

    if (c == '\n') {
      float ph, temperature;
      if (parseSensorLine(lineBuffer, ph, temperature)) {
        sendToInflux(ph, temperature);
      } else {
        Serial.print("Could not parse line: ");
        Serial.println(lineBuffer);
      }
      lineBuffer = "";
    } else if (c != '\r') {
      lineBuffer += c;
    }
  }
}

