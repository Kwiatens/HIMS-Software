// HIMS - Hardware Inventory Management System
// Local HTTP server for the mobile scanner companion page.

#pragma once

#include <atomic>
#include <filesystem>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>

#include "core/Inventory.h"

namespace hims {

namespace filesystem = std::filesystem;
using std::atomic;
using std::filesystem::path;
using std::function;
using std::mutex;
using std::string;
using std::thread;
using std::uint16_t;
using std::vector;

class LocalHttpServer {
 public:
  using ScanCallback = function<void(const string&)>;

  LocalHttpServer() = default;
  ~LocalHttpServer();

  LocalHttpServer(const LocalHttpServer&) = delete;
  LocalHttpServer& operator=(const LocalHttpServer&) = delete;

  bool start(uint16_t preferredPort, filesystem::path scannerPagePath, ScanCallback onScan);
  void stop();
  void setRecentActivity(vector<ActivityEntry> activities);

  bool running() const;
  uint16_t port() const;
  string baseUrl() const;
  vector<string> addresses() const;
  string lastScan() const;

 private:
  void workerLoop();
  bool serveConnection(SOCKET clientSocket, string requestText);
  bool loadScannerPage(const filesystem::path& scannerPagePath);
  string jsonStatus() const;
  string responseText(const string& status, const string& contentType, const string& body) const;
  bool bindSocket(uint16_t port);
  string scanCallbackMessage(const string& code) const;

  atomic<bool> running_{false};
  bool winsockStarted_ = false;
  thread worker_;
  ScanCallback onScan_;
  mutable mutex stateMutex_;
  uint16_t port_ = 0;
  filesystem::path scannerPagePath_;
  string scannerPageHtml_;
  string lastScan_;
  string lastError_;
  vector<string> addresses_;
  vector<ActivityEntry> recentActivities_;
  SOCKET listenSocket_ = INVALID_SOCKET;
};

}  // namespace hims

