//MAX30105->ARDUINO
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 particleSensor;

#define REPORTING_PERIOD_MS 1000

long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

uint32_t tsLastReport = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing sensor...");

  Wire.begin();
  Wire.setClock(100000);

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
  {
    Serial.println("FAILED");
    while (1);
  }
  else
  {
    Serial.println("SUCCESS");
  }

  particleSensor.setup(); 
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x0A);
}

void loop()
{
  long irValue = particleSensor.getIR();

  // Check for beat
  if (checkForBeat(irValue) == true)
  {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      beatAvg = beatsPerMinute;
      Serial.println("Beat!");
    }
  }

  // Print every 1 second (same logic as yours)
  if (millis() - tsLastReport > REPORTING_PERIOD_MS)
  {
    Serial.print("Heart rate: ");
    Serial.print(beatAvg);
    Serial.print(" bpm");

    // Simple SpO2 estimation (basic)
    Serial.print(" / SpO2: ");
    Serial.print(map(irValue, 50000, 100000, 90, 100)); // approx
    Serial.println(" %");

    tsLastReport = millis();
  }
}
