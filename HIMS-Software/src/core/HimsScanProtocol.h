// HIMS - Hardware Inventory Management System
// HIMS Scan R1 protocol types, validation, persistence, and stock mutation rules.

#pragma once

#include "core/Inventory.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <unordered_map>

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

struct DeviceScanRequest {
  std::string deviceId;
  std::string requestId;
  std::string code;
  int quantity = 1;
};

struct DeviceDebugReport {
  std::string deviceId;
  std::string requestId;
  std::string level;
  std::string message;
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
  std::string debug;
};

bool loadHimsScanConfig(const std::filesystem::path& path, HimsScanConfig& config);
bool saveHimsScanConfig(const std::filesystem::path& path, const HimsScanConfig& config);
std::string generateHimsScanToken();

bool parseQuantityRequestJson(const std::string& body, DeviceQuantityRequest& request, std::string& error);
bool parseScanRequestJson(const std::string& body, DeviceScanRequest& request, std::string& error);
bool parseDebugReportJson(const std::string& body, DeviceDebugReport& report, std::string& error);
bool parseStatusReportJson(const std::string& body, DeviceStatusReport& report, std::string& error);
DeviceQuantityResult applyDeviceQuantity(InventoryStore& store, const DeviceQuantityRequest& request);
DeviceQuantityResult applyDeviceQuantityCached(
    InventoryStore& store, const DeviceQuantityRequest& request,
    std::unordered_map<std::string, DeviceQuantityResult>& cache, std::deque<std::string>& order,
    std::size_t maxEntries = 64);
std::string debugResultJson(bool ok, const std::string& error = {});
std::string scanResultJson(bool ok, const std::string& error = {});
std::string quantityResultJson(const DeviceQuantityResult& result);
std::string statusResultJson(bool ok, const std::string& error = {});

}  // namespace hims
