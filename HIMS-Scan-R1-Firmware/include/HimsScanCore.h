// HIMS Scan R1 firmware core helpers.

#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace hims_scan {

using std::size_t;

inline std::string trimCopy(const std::string& value) {
  size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }

  size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return value.substr(begin, end - begin);
}

inline std::string escapeJson(const std::string& value) {
  std::ostringstream out;
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << ch;
        break;
    }
  }
  return out.str();
}

inline std::string hexByte(unsigned char value) {
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.push_back(kHex[(value >> 4U) & 0x0fU]);
  out.push_back(kHex[value & 0x0fU]);
  return out;
}

inline std::string makeRequestId(const std::string& deviceId, unsigned long sequence, unsigned long tickCount) {
  std::ostringstream out;
  out << trimCopy(deviceId) << '-' << tickCount << '-' << sequence;
  return out.str();
}

class QuantityComposer {
 public:
  void clear() {
    digits_.clear();
  }

  bool empty() const {
    return digits_.empty();
  }

  void appendDigit(char digit) {
    if (!std::isdigit(static_cast<unsigned char>(digit))) {
      return;
    }
    if (digits_.size() >= 4) {
      return;
    }
    digits_.push_back(digit);
  }

  int valueOrOne() const {
    if (digits_.empty()) {
      return 1;
    }
    try {
      const auto parsed = std::stoi(digits_);
      return parsed <= 0 ? 1 : parsed;
    } catch (...) {
      return 1;
    }
  }

  std::string displayText() const {
    return digits_.empty() ? std::string("1") : digits_;
  }

  int consume(bool add) {
    const auto value = valueOrOne();
    clear();
    return add ? value : -value;
  }

 private:
  std::string digits_;
};

inline std::string buildQuantityRequestJson(const std::string& deviceId, const std::string& requestId,
                                            const std::string& code, int delta) {
  std::ostringstream out;
  out << "{\"deviceId\":\"" << escapeJson(trimCopy(deviceId)) << "\","
      << "\"requestId\":\"" << escapeJson(trimCopy(requestId)) << "\","
      << "\"code\":\"" << escapeJson(trimCopy(code)) << "\","
      << "\"delta\":" << delta << '}';
  return out.str();
}

inline std::string buildStatusRequestJson(const std::string& deviceId, const std::string& firmwareVersion, int rssi) {
  std::ostringstream out;
  out << "{\"deviceId\":\"" << escapeJson(trimCopy(deviceId)) << "\","
      << "\"firmwareVersion\":\"" << escapeJson(trimCopy(firmwareVersion)) << "\","
      << "\"rssi\":" << rssi << '}';
  return out.str();
}

}  // namespace hims_scan
