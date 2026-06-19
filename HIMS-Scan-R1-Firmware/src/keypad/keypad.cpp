#include <Arduino.h>
#include <Keypad.h>
#include "config/pins.h"
#include "keypad.h"

static char KeyTable[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Arduino pins connected to keypad rows
static byte rowPins[4] = {KEYPAD_ROW_1, KEYPAD_ROW_2, KEYPAD_ROW_3, KEYPAD_ROW_4};

// Arduino pins connected to keypad columns
static byte colPins[4] = {KEYPAD_COL_1, KEYPAD_COL_2, KEYPAD_COL_3, KEYPAD_COL_4};

static Keypad keypad = Keypad(makeKeymap(KeyTable), rowPins, colPins, 4, 4);

static char lastKey = 0;

void keypadInit() {
  keypad.addEventListener([](char key) {
    lastKey = key;
  });
}

void keypadUpdate() {
  char key = keypad.getKey();

  if (key) {
    lastKey = key;

    Serial.print("Key pressed: ");
    Serial.println(key);
  } 
}

char keypadGetLastKey() {
    return lastKey;
}

