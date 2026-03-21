#define BLYNK_TEMPLATE_ID "TMPL3JP5JsDML"
#define BLYNK_TEMPLATE_NAME "HEALTH MONITOR"
#define BLYNK_AUTH_TOKEN "CqjbDb7a8CTPnf8WGUasTVm2pQWfRxIP"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "SUJI";
char pass[] = "sujith2021";

BlynkTimer timer;

// Function to send dummy data
void sendSensorData()
{
  float temp = random(250, 350) / 10.0;   // 25.0 - 35.0 °C
  float humidity = random(400, 800) / 10.0; // 40 - 80 %
  float bodyTemp = random(360, 390) / 10.0; // 36 - 39 °C
  int pulse = random(60, 100); // 60 - 100 BPM

  Blynk.virtualWrite(V0, temp);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, bodyTemp);
  Blynk.virtualWrite(V3, pulse);

  Serial.print("Temp: "); Serial.print(temp);
  Serial.print(" Humidity: "); Serial.print(humidity);
  Serial.print(" BodyTemp: "); Serial.print(bodyTemp);
  Serial.print(" Pulse: "); Serial.println(pulse);
}

void setup()
{
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Send data every 2 seconds
  timer.setInterval(2000L, sendSensorData);
}

void loop()
{
  Blynk.run();
  timer.run();
}
