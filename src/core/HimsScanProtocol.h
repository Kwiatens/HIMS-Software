// HIMS - Hardware Inventory Management System
// HIMS Scan R1 protocol types, validation, persistence, and stock mutation rules.

#pragma once

#include "core/Inventory.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace hims {

struct HimsScanConfig {
  std::string deviceId;
  std::string token;
  std::string fallbackHost;
  std::uint16_t fallbackPort = 0;

  bool paired() const;
};

struct DeviceQuantityRequest {
  std::string deviceId;
  std::string requestId;
  std::string code;
  int delta = 0;
};

struct DeviceQuantityResult {
  int httpStatus = 400;
  bool ok = false;
  std::string error;
  std::string item;
  int requestedDelta = 0;
  int appliedDelta = 0;
  int quantity = 0;
};

struct DeviceStatusReport {
  std::string deviceId;
  std::string firmwareVersion;
  int rssi = 0;
};

bool loadHimsScanConfig(const std::filesystem::path& path, HimsScanConfig& config);
bool saveHimsScanConfig(const std::filesystem::path& path, const HimsScanConfig& config);
std::string generateHimsScanToken();

bool parseQuantityRequestJson(const std::string& body, DeviceQuantityRequest& request, std::string& error);
bool parseStatusReportJson(const std::string& body, DeviceStatusReport& report, std::string& error);
DeviceQuantityResult applyDeviceQuantity(InventoryStore& store, const DeviceQuantityRequest& request);
std::string quantityResultJson(const DeviceQuantityResult& result);
std::string statusResultJson(bool ok, const std::string& error = {});

}  // namespace hims
