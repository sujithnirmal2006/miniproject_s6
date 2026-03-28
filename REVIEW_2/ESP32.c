//STM32->ESP32->BLYNK
#define BLYNK_TEMPLATE_ID "TMPL3JP5JsDML"
#define BLYNK_TEMPLATE_NAME "HEALTH MONIITOR"
#define BLYNK_AUTH_TOKEN "CqjbDb7a8CTPnf8WGUasTVm2pQWfRxIP"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// --- WiFi Credentials ---
char ssid[] = "SUJI";
char password[] = "sujith2021";

// --- UART Pins ---
#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200); // For USB debugging
  // Note: 115200 must match the STM32 USART1 baud rate
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); 
  
  Serial.println("Connecting to Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  Serial.println("--- System Online: Awaiting STM32 Data ---");
}

void loop() {
  Blynk.run();

  if (Serial2.available()) {
    // 1. Read the full string from STM32 (e.g., "1024,72,36.5,28,55")
    String data = Serial2.readStringUntil('\n');
    data.trim(); 

    if (data.length() > 0) {
      Serial.print("Raw Data: ");
      Serial.println(data);

      // 2. Parse Comma Separated Values
      // Variables to store the split strings
      String values[5]; 
      int commaIndex = 0;
      int startIndex = 0;

      for (int i = 0; i < 5; i++) {
        commaIndex = data.indexOf(',', startIndex);
        if (commaIndex == -1) {
          values[i] = data.substring(startIndex); // Last value
        } else {
          values[i] = data.substring(startIndex, commaIndex);
          startIndex = commaIndex + 1;
        }
      }

      // 3. Map values to Blynk Virtual Pins
      // values[0] = ECG (V0)
      // values[1] = BPM (V3)
      // values[2] = Body Temp (V4)
      // values[3] = Room Temp (V1)
      // values[4] = Humidity (V2)

      Blynk.virtualWrite(V0, values[0].toInt());     // ECG
      Blynk.virtualWrite(V3, values[1].toInt());     // Heart Rate
      Blynk.virtualWrite(V4, values[2].toFloat());   // Body Temp (Float for decimal)
      Blynk.virtualWrite(V1, values[3].toInt());     // Room Temp
      Blynk.virtualWrite(V2, values[4].toInt());     // Humidity
      
      Serial.printf("Blynk Sync -> ECG:%s BPM:%s Body:%s Room:%s Hum:%s\n", 
                    values[0].c_str(), values[1].c_str(), values[2].c_str(), 
                    values[3].c_str(), values[4].c_str());
    }
  }
}
