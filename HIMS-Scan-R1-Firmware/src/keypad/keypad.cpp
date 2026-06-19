#include <Arduino.h>
#include <Keypad.h>

#include "config/config.h"
#include "keypad.h"

static char KeyTable[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

static byte KEYPAD_ROW_PINS[4] = {
  KEYPAD_ROW_1_PIN,
  KEYPAD_ROW_2_PIN,
  KEYPAD_ROW_3_PIN,
  KEYPAD_ROW_4_PIN
};

static byte KEYPAD_COLUMN_PINS[4] = {
  KEYPAD_COLUMN_1_PIN,
  KEYPAD_COLUMN_2_PIN,
  KEYPAD_COLUMN_3_PIN,
  KEYPAD_COLUMN_4_PIN
};

static Keypad keypad = Keypad(makeKeymap(KeyTable), KEYPAD_ROW_PINS, KEYPAD_COLUMN_PINS, 4, 4);

void keypadInit() {
  keypad.setDebounceTime(20);

  Serial.print("Keypad rows: ");
  for (byte i = 0; i < 4; i++) {
    if (i > 0) {
      Serial.print(", ");
    }
    Serial.print(KEYPAD_ROW_PINS[i]);
  }
  Serial.print(" | cols: ");
  for (byte i = 0; i < 4; i++) {
    if (i > 0) {
      Serial.print(", ");
    }
    Serial.print(KEYPAD_COLUMN_PINS[i]);
  }
  Serial.println();
}

bool keypadPoll(HimsKeyEvent& event) {
  event = {};
  const char key = keypad.getKey();
  if (!key) {
    return false;
  }

  Serial.print("Key pressed: ");
  Serial.println(key);

  event.value = key;
  if (key >= '0' && key <= '9') {
    event.type = HimsKeyEventType::Digit;
    return true;
  }
  if (key == 'A') {
    event.type = HimsKeyEventType::Add;
    return true;
  }
  if (key == 'B') {
    event.type = HimsKeyEventType::Subtract;
    return true;
  }
  if (key == 'C') {
    event.type = HimsKeyEventType::Cancel;
    return true;
  }
  event.type = HimsKeyEventType::Other;
  return true;
}

