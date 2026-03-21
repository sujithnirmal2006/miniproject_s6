//ESP 32 (DHT11->STM32->ESP32->THINGSPEAK)
#include <WiFi.h>
#include "ThingSpeak.h"   // 🔴 REQUIRED (missing before)

/* ================== WiFi Credentials ================== */
const char* ssid = "SUJI";
const char* password = "sujith2021";

/* ================== ThingSpeak ================== */
WiFiClient client;
unsigned long channelID = 3128186;        // ✅ your Channel ID
const char* writeAPIKey = "KQR6KP8QRIKA8GXN";

/* ================== UART ================== */
#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200);                         // USB debug
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);  // STM32 UART

  /* WiFi connect */
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");

  ThingSpeak.begin(client);   // ✅ now compiler knows ThingSpeak
}

void loop() {
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    Serial.println("Received: " + data);

    int tIndex = data.indexOf("T:");
    int hIndex = data.indexOf("H:");

    if (tIndex != -1 && hIndex != -1) {
      int temperature = data.substring(tIndex + 2, data.indexOf(",", tIndex)).toInt();
      int humidity    = data.substring(hIndex + 2).toInt();

      Serial.print("Temp: ");
      Serial.print(temperature);
      Serial.print("  Hum: ");
      Serial.println(humidity);

      ThingSpeak.setField(1, temperature);
      ThingSpeak.setField(2, humidity);

      int status = ThingSpeak.writeFields(channelID, writeAPIKey);

      if (status == 200) {
        Serial.println("Data uploaded to ThingSpeak ✔");
      } else {
        Serial.print("Upload failed ❌ HTTP error: ");
        Serial.println(status);
      }
    }
  }

  delay(15000);   // ⏱ ThingSpeak minimum update interval
}
