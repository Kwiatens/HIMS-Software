#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hims_scan {

struct QuantityRequest {
  String deviceId;
  String requestId;
  uint32_t sequence = 0;
  String code;
  int delta = 0;
};

class OutboxStore {
 public:
  static constexpr std::size_t kCapacity = 16;

  bool begin(const char* namespaceName = "hims_scan", const char* deviceId = nullptr);
  void end();

  bool load();
  bool save();

  bool enqueue(const QuantityRequest& request);
  bool peek(QuantityRequest& request) const;
  bool pop(QuantityRequest& request);
  void clear();

  std::size_t size() const {
    return count_;
  }

  bool empty() const {
    return count_ == 0;
  }

  uint32_t nextSequence() const {
    return nextSequence_;
  }

  void setNextSequence(uint32_t value) {
    nextSequence_ = value;
  }

 private:
  static constexpr uint32_t kMagic = 0x48534f51UL;
  static constexpr uint16_t kVersion = 2;
  static constexpr std::size_t kCodeLen = 96;

  struct Record {
    uint32_t sequence = 0;
    char code[kCodeLen]{};
    int32_t delta = 0;
  };

  struct PersistedState {
    uint32_t magic = kMagic;
    uint16_t version = kVersion;
    uint16_t count = 0;
    uint16_t head = 0;
    uint16_t reserved = 0;
    uint32_t nextSequence = 1;
    Record records[kCapacity]{};
  };

  static_assert(std::is_trivially_copyable<Record>::value, "Record must be trivially copyable");
  static_assert(std::is_trivially_copyable<PersistedState>::value, "PersistedState must be trivially copyable");

  static void copyString(char* dest, std::size_t capacity, const String& value);
  static String readString(const char* value);
  static bool toRecord(const QuantityRequest& request, Record& record);
  QuantityRequest fromRecord(const Record& record) const;

  void syncFromState();
  void syncToState();

  Preferences prefs_;
  String deviceId_;
  PersistedState state_{};
  std::size_t count_ = 0;
  std::size_t head_ = 0;
  std::size_t tail_ = 0;
  uint32_t nextSequence_ = 1;
};

}  // namespace hims_scan
