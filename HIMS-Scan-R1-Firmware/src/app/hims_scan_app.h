#pragma once

#include "HimsScanCore.h"
#include "gm65/gm65_scanner.h"
#include "keypad/keypad.h"
#include "net/hims_client.h"
#include "HimsScanOutbox.h"
#include "storage/outbox_store.h"

namespace hims_scan {

class HimsScanApp {
 public:
  bool begin();
  void loop();

 private:
  enum class State {
    Idle,
    AwaitQuantity,
  };

  enum class PowerMode {
    Normal,
    Standby,
  };

  enum class PendingScanType {
    None,
    HimsQuantity,
    DigiKeyDataMatrix,
  };

  void handleScan(const String& code);
  void pollScanner();
  void queueScan(const String& code, int quantity = 1);
  void handleKey(const HimsKeyEvent& event);
  void requestOtaUpdate();
  void sendDebugMessage(const String& message, const String& level = "info");
  bool serviceOta();
  bool startOtaService();
  bool ensureWiFiForOta();
  String buildWifiDebugText() const;
  void enterStandby();
  void exitStandby();
  void submitCurrent(char action);
  bool flushScanQueue();
  bool flushQueue();
  void reconnectWiFi();
  void primeHimsSoftwareConnection();
  void maybeSendStatus();
  void logState(const char* reason) const;
  void setAwaitingQuantity();
  void resetPending();

  Gm65Scanner scanner_;
  OutboxStore outbox_;
  FixedRingBuffer<ScanRequest, 16> scanQueue_;
  HimsClient client_;
  HimsClientConfig clientConfig_;
  QuantityComposer quantity_;
  String scannedCode_;
  State state_ = State::Idle;
  PendingScanType pendingScanType_ = PendingScanType::None;
  PowerMode powerMode_ = PowerMode::Normal;
  bool wifiStarted_ = false;
  bool wifiConnectedReported_ = false;
  bool himsSoftwarePrimed_ = false;
  bool otaRequested_ = false;
  bool otaActive_ = false;
  unsigned long lastReconnectAttempt_ = 0;
  unsigned long lastScanFlushAttempt_ = 0;
  unsigned long lastFlushAttempt_ = 0;
  unsigned long lastStatusAttempt_ = 0;
  uint32_t scanSequence_ = 1;
};

}  // namespace hims_scan
