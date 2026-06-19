#pragma once

#include <Arduino.h>

namespace hims_scan {

class Gm65Scanner {
 public:
  bool begin(uint32_t baudRate = 9600);
  bool poll(String& code);
  void flushInput();

 private:
  HardwareSerial serial_{1};
  String buffer_;
  unsigned long lastByteAt_ = 0;
};

}  // namespace hims_scan
