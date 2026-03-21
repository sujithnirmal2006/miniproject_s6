//ECG->arduino
#define ECG_PIN A0
#define LO_PLUS 10
#define LO_MINUS 11

void setup() {
  Serial.begin(115200);

  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);

  Serial.println("ECG Monitoring Started...");
}

void loop() {

  // Check if leads are connected
  if ((digitalRead(LO_PLUS) == 1) || (digitalRead(LO_MINUS) == 1)) {
    Serial.println("Leads off! Please check electrodes.");
  } 
  else {
    int ecgValue = analogRead(ECG_PIN);
    Serial.println(ecgValue);
  }

  delay(10); // small delay for stable graph
}
