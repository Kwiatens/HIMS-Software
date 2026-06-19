#ifndef HIMS_SCAN_CONFIG_PINS_H
#define HIMS_SCAN_CONFIG_PINS_H

#include <Arduino.h>

// HIMS Scan Firmware
// File: Pins.h
// Purpose: Defines board-level GPIO assignments used by firmware modules.

// Keypad pins
const int KEYPAD_ROW_1 = 0;
const int KEYPAD_ROW_2 = 1;
const int KEYPAD_ROW_3 = 2;
const int KEYPAD_ROW_4 = 3;
const int KEYPAD_COL_1 = 4;
const int KEYPAD_COL_2 = 5;
const int KEYPAD_COL_3 = 6;
const int KEYPAD_COL_4 = 7;

// GM65 1D/2D barcode scanner pins
const int GM65_TX_PIN = 43;
const int GM65_RX_PIN = 44;

#endif // HIMS_SCAN_CONFIG_PINS_H