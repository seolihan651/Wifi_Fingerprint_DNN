#include <Arduino.h>
#include <SoftwareSerial.h> 

#define LED_BUILTIN_PIN 13

// 블루투스 모듈용 SoftwareSerial (RX: 2, TX: 3)
SoftwareSerial BTSerial(2, 3);

char wifiBuffer[1644]; 

unsigned long previousMillis = 0;
const long interval = 3000; 
void wifiscanEvent();
void setup() {
  pinMode(LED_BUILTIN_PIN, OUTPUT); 
  digitalWrite(LED_BUILTIN_PIN, HIGH);

  BTSerial.begin(9600);
  
  digitalWrite(LED_BUILTIN_PIN, LOW);

  Serial.begin(115200);
  
  digitalWrite(LED_BUILTIN_PIN, HIGH);
  
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    wifiscanEvent();
  }
}

void wifiscanEvent() {
  digitalWrite(LED_BUILTIN_PIN, !digitalRead(LED_BUILTIN_PIN));

  BTSerial.println("---SCAN_START---");

  Serial.println("AT+CWLAP"); 

  memset(wifiBuffer, 0, sizeof(wifiBuffer));
  int bufferIndex = 0;
  
  long startTime = millis();
  while (millis() - startTime < 3000) {
    if (Serial.available()) {
		startTime=millis();
      char c = Serial.read();
      if (bufferIndex < sizeof(wifiBuffer) - 1) {
        wifiBuffer[bufferIndex++] = c;
      }
    }
  }

  if (bufferIndex > 0) {
    for (int i = 0; i < bufferIndex; i++) {
      BTSerial.write(wifiBuffer[i]); 
      delay(1); 
    }
  }
  
  BTSerial.println("\n---SCAN_END---"); 
}