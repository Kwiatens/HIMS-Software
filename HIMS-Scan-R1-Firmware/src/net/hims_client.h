#pragma once

#include "storage/outbox_store.h"

#include <Arduino.h>
#include <WiFi.h>

namespace hims_scan {

struct HimsClientConfig {
  String deviceId;
  String token;
  String fallbackHost;
  uint16_t fallbackPort = 0;
};

class HimsClient {
 public:
  bool begin(const HimsClientConfig& config);
  void end();

  bool connected();
  bool resolveEndpoint(bool force = false);
  bool postQuantity(const QuantityRequest& request, int& httpStatus, String& responseBody);
  bool postStatus(const String& firmwareVersion, int rssi);

  String endpointSummary() const;

 private:
  bool connectToResolved();
  bool sendJson(const char* path, const String& body, int& httpStatus, String& responseBody);
  bool readResponse(int& httpStatus, String& responseBody);

  HimsClientConfig config_;
  WiFiClient client_;
  IPAddress endpointIp_{0, 0, 0, 0};
  uint16_t endpointPort_ = 0;
  String endpointName_;
  bool endpointValid_ = false;
  bool mdnsStarted_ = false;
  unsigned long lastResolveAttempt_ = 0;
};

}  // namespace hims_scan
