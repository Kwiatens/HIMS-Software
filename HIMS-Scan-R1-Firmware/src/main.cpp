#include <Arduino.h>

#include "keypad/keypad.h"

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial to initialize
  Serial.println("HIMS Scan R1 by Kwiatens");
  keypadInit();
}

void loop() {
  keypadUpdate();
}