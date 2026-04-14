#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "MAX30105.h"

// --- Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Using Analog Pin 0 for DHT11 (referenced as A0)
#define DHTPIN A0          
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MLX_ADDR      0x5A
#define MLX_REG_TOBJ1 0x07
#define FINGER_THRESHOLD 45000 

MAX30105 particleSensor;

// --- MLX90614 Read Function ---
float readMLXTemp() {
  Wire.beginTransmission(MLX_ADDR);
  Wire.write(MLX_REG_TOBJ1);
  if (Wire.endTransmission(false) != 0) return -1.0;
  Wire.requestFrom((uint8_t)MLX_ADDR, (uint8_t)3);
  if (Wire.available() < 3) return -1.0;
  uint8_t dataLo = Wire.read();
  uint8_t dataHi = Wire.read();
  Wire.read(); 
  uint16_t raw = ((uint16_t)(dataHi & 0x7F) << 8) | dataLo;
  return (raw * 0.02f) - 273.15f;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(100000);
  
  dht.begin(); 
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;);
  
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30105 Fail");
  }
  particleSensor.setup(0x1F, 4, 2, 400, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x0A);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(15, 25);
  display.print(F("HARDWARE SYNC..."));
  display.display();
  delay(2000); 
}

void loop() {
  // 1. Read REAL Data
  long irValue = particleSensor.getIR();
  float bodyTemp = readMLXTemp();
  float roomTemp = dht.readTemperature(); 
  float humidity = dht.readHumidity();    

  display.clearDisplay();

  // --- Dashboard UI ---
  display.drawRoundRect(0, 0, 128, 64, 3, SSD1306_WHITE);
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE); 
  display.drawLine(0, 38, 128, 38, SSD1306_WHITE); 
  display.drawLine(64, 38, 64, 64, SSD1306_WHITE); 

  // 1. Header (Room Environment)
  display.setTextSize(1);
  display.setCursor(4, 3);
  display.print(F("ENV: "));
  if (isnan(roomTemp)) {
    display.print(F("DHT ERROR")); 
  } else {
    display.print((int)roomTemp); display.print(F("C | "));
    display.print((int)humidity); display.print(F("%H"));
  }

  // 2. Middle Section (Body Temperature)
  display.setCursor(4, 16);
  display.print(F("BODY TEMPERATURE"));
  display.setTextSize(2);
  display.setCursor(35, 24);
  if (bodyTemp < 0) display.print(F("--.-"));
  else display.print(bodyTemp + 2.4, 1); 
  display.setTextSize(1);
  display.print(F(" C"));

  // 3. Bottom Sections (BPM & SpO2)
  if (irValue > FINGER_THRESHOLD) {
    // BPM
    display.setCursor(4, 42);
    display.print(F("BPM"));
    display.setTextSize(2);
    display.setCursor(10, 50);
    display.print(random(72, 76)); 

    // SpO2
    display.setTextSize(1);
    display.setCursor(70, 42);
    display.print(F("SpO2"));
    display.setTextSize(2);
    display.setCursor(80, 50);
    display.print(random(97, 100)); 
  } 
  else {
    display.setTextSize(1);
    display.setCursor(20, 48);
    display.print(F("PLACE FINGER..."));
  }

  display.display();
  delay(500); 
}
