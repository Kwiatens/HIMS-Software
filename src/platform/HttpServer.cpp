// HIMS - Hardware Inventory Management System
// Local HTTP server for the mobile scanner companion page.

#define NOMINMAX

#include "platform/HttpServer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#include "core/Inventory.h"
#include "platform/Console.h"

namespace hims {

using namespace std;

namespace {

string jsonEscape(const string& value) {
  ostringstream out;
  for (char ch : value) {
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

string trimHttp(const string& value) {
  size_t begin = 0;
  while (begin < value.size() && isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }

  size_t end = value.size();
  while (end > begin && isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return value.substr(begin, end - begin);
}

string extractScanCode(const string& body) {
  // Accept either a raw code body or the small JSON payload from scanner.html.
  const auto raw = trimHttp(body);
  if (raw.empty()) {
    return {};
  }

  if (raw.front() != '{') {
    return raw;
  }

  const auto codePos = raw.find("\"code\"");
  if (codePos == string::npos) {
    return raw;
  }

  const auto colon = raw.find(':', codePos);
  if (colon == string::npos) {
    return raw;
  }

  const auto firstQuote = raw.find('"', colon);
  if (firstQuote == string::npos) {
    return raw;
  }

  const auto secondQuote = raw.find('"', firstQuote + 1);
  if (secondQuote == string::npos) {
    return raw;
  }

  return raw.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

}  // namespace

LocalHttpServer::~LocalHttpServer() {
  stop();
}

bool LocalHttpServer::start(uint16_t preferredPort, filesystem::path scannerPagePath, ScanCallback onScan) {
  stop();

  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    lastError_ = "Failed to initialize Winsock";
    return false;
  }
  winsockStarted_ = true;

  if (!loadScannerPage(scannerPagePath)) {
    if (winsockStarted_) {
      WSACleanup();
      winsockStarted_ = false;
    }
    return false;
  }

  onScan_ = move(onScan);

  for (uint16_t candidate = preferredPort; candidate < static_cast<uint16_t>(preferredPort + 20); ++candidate) {
    if (bindSocket(candidate)) {
      running_.store(true);
      worker_ = thread(&LocalHttpServer::workerLoop, this);
      return true;
    }
  }

  lastError_ = "Unable to bind any scanner port";
  if (winsockStarted_) {
    WSACleanup();
    winsockStarted_ = false;
  }
  return false;
}

void LocalHttpServer::stop() {
  running_.store(false);

  if (listenSocket_ != INVALID_SOCKET) {
    shutdown(listenSocket_, SD_BOTH);
    closesocket(listenSocket_);
    listenSocket_ = INVALID_SOCKET;
  }

  if (worker_.joinable()) {
    worker_.join();
  }

  if (winsockStarted_) {
    WSACleanup();
    winsockStarted_ = false;
  }
}

void LocalHttpServer::setRecentActivity(vector<ActivityEntry> activities) {
  lock_guard<mutex> lock(stateMutex_);
  recentActivities_ = move(activities);
}

bool LocalHttpServer::running() const {
  return running_.load();
}

uint16_t LocalHttpServer::port() const {
  return port_;
}

string LocalHttpServer::baseUrl() const {
  const auto addresses = this->addresses();
  const auto host = addresses.empty() ? string("127.0.0.1") : addresses.front();
  ostringstream out;
  out << "http://" << host << ':' << port_;
  return out.str();
}

vector<string> LocalHttpServer::addresses() const {
  lock_guard<mutex> lock(stateMutex_);
  if (!addresses_.empty()) {
    return addresses_;
  }
  return {"127.0.0.1"};
}

string LocalHttpServer::lastScan() const {
  lock_guard<mutex> lock(stateMutex_);
  return lastScan_;
}

bool LocalHttpServer::loadScannerPage(const filesystem::path& scannerPagePath) {
  scannerPagePath_ = scannerPagePath;

  ifstream input(scannerPagePath_, ios::binary);
  if (!input) {
    lastError_ = "Unable to load scanner page at " + scannerPagePath_.string();
    scannerPageHtml_.clear();
    return false;
  }

  ostringstream buffer;
  buffer << input.rdbuf();
  scannerPageHtml_ = buffer.str();
  return true;
}

bool LocalHttpServer::bindSocket(uint16_t port) {
  SOCKET socketHandle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketHandle == INVALID_SOCKET) {
    return false;
  }

  BOOL reuse = TRUE;
  setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  if (::bind(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
    closesocket(socketHandle);
    return false;
  }

  if (listen(socketHandle, SOMAXCONN) == SOCKET_ERROR) {
    closesocket(socketHandle);
    return false;
  }

  listenSocket_ = socketHandle;
  port_ = port;
  addresses_ = localAddresses();
  return true;
}

void LocalHttpServer::workerLoop() {
  while (running_.load()) {
    sockaddr_in clientAddress{};
    int clientSize = sizeof(clientAddress);
    SOCKET client = accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddress), &clientSize);
    if (client == INVALID_SOCKET) {
      if (running_.load()) {
        continue;
      }
      break;
    }

    string request;
    array<char, 4096> buffer{};
    int received = 0;
    do {
      received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
      if (received > 0) {
        request.append(buffer.data(), buffer.data() + received);
      }
    } while (received > 0 && request.find("\r\n\r\n") == string::npos);

    serveConnection(client, move(request));
    closesocket(client);
  }
}

string LocalHttpServer::responseText(const string& status, const string& contentType, const string& body) const {
  ostringstream out;
  out << "HTTP/1.1 " << status << "\r\n";
  out << "Content-Type: " << contentType << "\r\n";
  out << "Content-Length: " << body.size() << "\r\n";
  out << "Connection: close\r\n";
  out << "Cache-Control: no-store\r\n\r\n";
  out << body;
  return out.str();
}

string LocalHttpServer::scanCallbackMessage(const string& code) const {
  ostringstream out;
  out << "{"
      << "\"ok\":true,"
      << "\"code\":\"" << jsonEscape(code) << "\","
      << "\"message\":\"queued\""
      << "}";
  return out.str();
}

string LocalHttpServer::jsonStatus() const {
  lock_guard<mutex> lock(stateMutex_);
  ostringstream out;
  const auto addresses = addresses_;
  const auto activities = recentActivities_;
  out << "{"
      << "\"ok\":true,"
      << "\"port\":" << port_ << ','
      << "\"baseUrl\":\"http://" << (addresses.empty() ? string("127.0.0.1") : addresses.front()) << ':' << port_ << "\","
      << "\"lastScan\":\"" << jsonEscape(lastScan_) << "\","
      << "\"addresses\":[";
  for (size_t index = 0; index < addresses.size(); ++index) {
    if (index > 0) {
      out << ',';
    }
    out << "\"" << jsonEscape(addresses[index]) << "\"";
  }
  out << "],";
  out << "\"activity\":[";
  for (size_t index = 0; index < activities.size(); ++index) {
    if (index > 0) {
      out << ',';
    }
    out << "{"
        << "\"kind\":\"" << jsonEscape(activities[index].kind) << "\","
        << "\"message\":\"" << jsonEscape(activities[index].message) << "\","
        << "\"timestamp\":\"" << jsonEscape(nowTimestampString(activities[index].timestamp)) << "\""
        << "}";
  }
  out << "]";
  out << "}";
  return out.str();
}

bool LocalHttpServer::serveConnection(SOCKET clientSocket, string requestText) {
  // Parse the first request line and route only the tiny local API surface.
  const auto headerEnd = requestText.find("\r\n\r\n");
  if (headerEnd == string::npos) {
    return false;
  }

  const auto headers = requestText.substr(0, headerEnd);
  const auto body = requestText.substr(headerEnd + 4);
  istringstream input(headers);
  string method;
  string target;
  string version;
  input >> method >> target >> version;
  (void)version;

  if (method == "GET" && (target == "/" || target == "/index.html")) {
    const auto response = responseText("200 OK", "text/html; charset=utf-8", scannerPageHtml_);
    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    return true;
  }

  if (method == "GET" && target == "/api/state") {
    const auto response = responseText("200 OK", "application/json; charset=utf-8", jsonStatus());
    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    return true;
  }

  if (method == "POST" && target == "/api/scan") {
    const auto code = extractScanCode(body);
    {
      lock_guard<mutex> lock(stateMutex_);
      lastScan_ = code;
    }
    if (onScan_) {
      onScan_(code);
    }
    const auto response = responseText("200 OK", "application/json; charset=utf-8", scanCallbackMessage(code));
    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    return true;
  }

  const auto response = responseText("404 Not Found", "text/plain; charset=utf-8", "Not found");
  send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
  return false;
}

}  // namespace hims

