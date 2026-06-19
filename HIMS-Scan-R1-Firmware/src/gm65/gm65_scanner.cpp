#include "gm65/gm65_scanner.h"

#include "config/config.h"

#include <cctype>

namespace hims_scan {

bool Gm65Scanner::begin(uint32_t baudRate) {
  buffer_.clear();
  lastByteAt_ = 0;
  serial_.setRxBufferSize(1024);
  serial_.begin(baudRate, SERIAL_8N1, GM65_RX_PIN, GM65_TX_PIN);
  serial_.setTimeout(5);
  return true;
}

void Gm65Scanner::flushInput() {
  while (serial_.available() > 0) {
    serial_.read();
  }
  buffer_.clear();
  lastByteAt_ = 0;
}

bool Gm65Scanner::poll(String& code) {
  code.clear();
  while (serial_.available() > 0) {
    const int raw = serial_.read();
    if (raw < 0) {
      break;
    }
    const char ch = static_cast<char>(raw);
    if (ch == '\r' || ch == '\n') {
      if (buffer_.length() > 0) {
        code = buffer_;
        buffer_.clear();
        lastByteAt_ = 0;
        return true;
      }
      continue;
    }
    if (std::isprint(static_cast<unsigned char>(ch)) != 0) {
      buffer_ += ch;
      lastByteAt_ = millis();
    }
    if (buffer_.length() > 160U) {
      buffer_.clear();
      lastByteAt_ = 0;
    }
  }

  // GM65 normally terminates scans with CR/LF. Also accept a completed burst
  // after a short idle gap so a delayed network pass or lost terminator cannot
  // leave a valid barcode stuck in the receive buffer forever.
  if (buffer_.length() > 0 && lastByteAt_ != 0 && millis() - lastByteAt_ >= 30U) {
    code = buffer_;
    buffer_.clear();
    lastByteAt_ = 0;
    return true;
  }
  return false;
}

}  // namespace hims_scan
