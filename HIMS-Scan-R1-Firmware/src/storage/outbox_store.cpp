#include "storage/outbox_store.h"

#include "HimsScanCore.h"

#include <algorithm>
#include <cstring>

namespace hims_scan {

bool OutboxStore::begin(const char* namespaceName, const char* deviceId) {
  deviceId_ = deviceId == nullptr ? String() : String(deviceId);
  return prefs_.begin(namespaceName, false);
}

void OutboxStore::end() {
  prefs_.end();
}

void OutboxStore::copyString(char* dest, std::size_t capacity, const String& value) {
  if (capacity == 0) {
    return;
  }
  const auto trimmed = trimCopy(value);
  const auto limit = std::min(capacity - 1U, static_cast<std::size_t>(trimmed.length()));
  memcpy(dest, trimmed.c_str(), limit);
  dest[limit] = '\0';
}

String OutboxStore::readString(const char* value) {
  return value == nullptr ? String() : String(value);
}

bool OutboxStore::toRecord(const QuantityRequest& request, Record& record) {
  record.sequence = request.sequence;
  copyString(record.code, kCodeLen, request.code);
  record.delta = request.delta;
  return record.sequence > 0 && readString(record.code).length() > 0;
}

QuantityRequest OutboxStore::fromRecord(const Record& record) const {
  QuantityRequest request;
  request.deviceId = deviceId_;
  request.sequence = record.sequence;
  request.requestId = makeRequestId(request.deviceId, request.sequence);
  request.code = readString(record.code);
  request.delta = record.delta;
  return request;
}

void OutboxStore::syncFromState() {
  count_ = std::min<std::size_t>(state_.count, kCapacity);
  head_ = std::min<std::size_t>(state_.head, kCapacity == 0 ? 0 : kCapacity - 1U);
  tail_ = (head_ + count_) % kCapacity;
  nextSequence_ = state_.nextSequence == 0 ? 1U : state_.nextSequence;
}

void OutboxStore::syncToState() {
  state_.magic = kMagic;
  state_.version = kVersion;
  state_.count = static_cast<uint16_t>(count_);
  state_.head = static_cast<uint16_t>(head_);
  state_.nextSequence = nextSequence_;
}

bool OutboxStore::load() {
  if (!prefs_.isKey("state")) {
    clear();
    return true;
  }
  const auto bytes = prefs_.getBytes("state", &state_, sizeof(state_));
  if (bytes != sizeof(state_) || state_.magic != kMagic || state_.version != kVersion) {
    clear();
    return save();
  }
  syncFromState();
  return true;
}

bool OutboxStore::save() {
  syncToState();
  return prefs_.putBytes("state", &state_, sizeof(state_)) == sizeof(state_);
}

bool OutboxStore::enqueue(const QuantityRequest& request) {
  Record record{};
  if (!toRecord(request, record)) {
    return false;
  }

  if (kCapacity == 0) {
    return false;
  }

  if (count_ == kCapacity) {
    state_.records[head_] = record;
    head_ = (head_ + 1U) % kCapacity;
    tail_ = head_;
  } else {
    state_.records[tail_] = record;
    tail_ = (tail_ + 1U) % kCapacity;
    ++count_;
  }

  return save();
}

bool OutboxStore::peek(QuantityRequest& request) const {
  if (empty()) {
    return false;
  }
  request = fromRecord(state_.records[head_]);
  return true;
}

bool OutboxStore::pop(QuantityRequest& request) {
  if (!peek(request)) {
    return false;
  }
  head_ = (head_ + 1U) % kCapacity;
  --count_;
  if (count_ == 0) {
    head_ = 0;
    tail_ = 0;
  }
  return save();
}

void OutboxStore::clear() {
  memset(&state_, 0, sizeof(state_));
  state_.magic = kMagic;
  state_.version = kVersion;
  state_.nextSequence = 1;
  count_ = 0;
  head_ = 0;
  tail_ = 0;
  nextSequence_ = 1;
  save();
}

}  // namespace hims_scan
