// HIMS - Hardware Inventory Management System
// Keyboard-first USB provisioning page for one HIMS Scan R1 device.

#include "App.h"

#include "ui/shared/AppUiShared.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace hims {

using namespace std;

namespace {

vector<string> enumerateComPorts() {
  vector<string> ports;
  HKEY key = nullptr;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) != ERROR_SUCCESS) {
    return ports;
  }
  for (DWORD index = 0;; ++index) {
    char name[256] = {};
    BYTE data[256] = {};
    DWORD nameSize = sizeof(name);
    DWORD dataSize = sizeof(data);
    DWORD type = 0;
    const auto status = RegEnumValueA(key, index, name, &nameSize, nullptr, &type, data, &dataSize);
    if (status == ERROR_NO_MORE_ITEMS) break;
    if (status == ERROR_SUCCESS && type == REG_SZ && dataSize > 1) {
      ports.emplace_back(reinterpret_cast<const char*>(data));
    }
  }
  RegCloseKey(key);
  sort(ports.begin(), ports.end());
  ports.erase(unique(ports.begin(), ports.end()), ports.end());
  return ports;
}

string hexEncode(const string& value) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  string output;
  output.reserve(value.size() * 2);
  for (const auto ch : value) {
    const auto byte = static_cast<unsigned char>(ch);
    output.push_back(kHex[byte >> 4U]);
    output.push_back(kHex[byte & 0x0fU]);
  }
  return output;
}

bool exchangeLine(const string& port, const string& command, string& response, string& error) {
  const auto path = string("\\\\.\\") + port;
  HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    error = "Unable to open " + port;
    return false;
  }

  DCB dcb{};
  dcb.DCBlength = sizeof(dcb);
  GetCommState(handle, &dcb);
  dcb.BaudRate = CBR_115200;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  SetCommState(handle, &dcb);
  COMMTIMEOUTS timeouts{};
  timeouts.ReadIntervalTimeout = 50;
  timeouts.ReadTotalTimeoutConstant = 1500;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 1000;
  SetCommTimeouts(handle, &timeouts);
  PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

  DWORD written = 0;
  const bool sent = WriteFile(handle, command.data(), static_cast<DWORD>(command.size()), &written, nullptr) != 0 &&
                    written == command.size();
  if (!sent) {
    CloseHandle(handle);
    error = "Unable to write to " + port;
    return false;
  }

  response.clear();
  string currentLine;
  const auto deadline = GetTickCount64() + 2500;
  while (GetTickCount64() < deadline && currentLine.size() < 512) {
    char ch = 0;
    DWORD read = 0;
    if (ReadFile(handle, &ch, 1, &read, nullptr) != 0 && read == 1) {
      if (ch == '\n') {
        if (currentLine.rfind("HIMS/1 ", 0) == 0) {
          response = currentLine;
          break;
        }
        currentLine.clear();
      } else if (ch != '\r') {
        currentLine.push_back(ch);
      }
    }
  }
  CloseHandle(handle);
  if (response.empty()) {
    error = "No HIMS Scan response on " + port;
    return false;
  }
  return true;
}

}  // namespace

void App::refreshHimsScanPorts() {
  himsScanPorts_ = enumerateComPorts();
  if (himsScanPortSelection_ >= himsScanPorts_.size()) himsScanPortSelection_ = 0;
  himsScanAddresses_ = localAddresses();
  himsScanAddresses_.erase(remove(himsScanAddresses_.begin(), himsScanAddresses_.end(), "127.0.0.1"),
                           himsScanAddresses_.end());
  if (himsScanAddresses_.empty()) himsScanAddresses_.push_back("127.0.0.1");
  if (himsScanAddressSelection_ >= himsScanAddresses_.size()) himsScanAddressSelection_ = 0;
  dirty_ = true;
}

void App::openHimsScanSetup() {
  refreshHimsScanPorts();
  himsScanSetupField_ = 0;
  himsScanSetupInput_.clear();
  changePage(Page::HimsScanSetup);
  setMessage("Select the native USB COM port", 3);
}

bool App::provisionSelectedHimsScan() {
  if (himsScanPorts_.empty() || himsScanSsid_.empty() || himsScanPassword_.empty()) {
    setMessage("Port, Wi-Fi name, and password are required", 4);
    return false;
  }
  string response;
  string error;
  const auto port = himsScanPorts_[himsScanPortSelection_];
  if (!exchangeLine(port, "HIMS/1 HELLO\n", response, error) || response.rfind("HIMS/1 DEVICE ", 0) != 0) {
    setMessage(error.empty() ? "Selected port is not a HIMS Scan R1" : error, 5);
    return false;
  }
  const auto deviceId = trim(response.substr(14));
  const auto token = generateHimsScanToken();
  const auto host = himsScanAddresses_[himsScanAddressSelection_];
  ostringstream command;
  command << "HIMS/1 CONFIG " << hexEncode(himsScanSsid_) << ' ' << hexEncode(himsScanPassword_) << ' '
          << host << ' ' << server_.port() << ' ' << token << "\n";
  if (!exchangeLine(port, command.str(), response, error) || response != "HIMS/1 OK " + deviceId) {
    setMessage(error.empty() ? "Device rejected provisioning settings" : error, 5);
    return false;
  }

  himsScanConfig_ = {deviceId, token, host, server_.port()};
  if (!saveHimsScanConfig(himsScanConfigPath_, himsScanConfig_)) {
    setMessage("Device configured, but HIMS could not save its pairing", 6);
    return false;
  }
  server_.setDeviceCredentials(deviceId, token);
  himsScanPassword_.clear();
  changePage(Page::Dashboard);
  setMessage("HIMS Scan R1 paired; waiting for Wi-Fi heartbeat", 5);
  return true;
}

ftxui::Element App::renderHimsScanSetupUi() const {
  ftxui::Elements ports;
  ports.push_back(fullLine("Native USB serial ports", uiAccentColor(), uiPanelLeftBg()));
  if (himsScanPorts_.empty()) {
    ports.push_back(fullLine("No COM ports detected. Press r to refresh.", uiWarnColor(), uiPanelLeftBg()));
  } else {
    for (size_t index = 0; index < himsScanPorts_.size(); ++index) {
      const auto selected = index == himsScanPortSelection_;
      ports.push_back(fullLine(string(selected ? "> " : "  ") + himsScanPorts_[index],
                               selected ? uiTitleColor() : uiMutedColor(),
                               selected ? uiRowSelectedBg() : (index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg())));
    }
  }

  const auto passwordText = himsScanSetupField_ == 2 ? string(himsScanSetupInput_.size(), '*')
                                                     : string(himsScanPassword_.size(), '*');
  const auto ssidText = himsScanSetupField_ == 1 ? himsScanSetupInput_ : himsScanSsid_;
  const auto host = himsScanAddresses_.empty() ? string("n/a") : himsScanAddresses_[himsScanAddressSelection_];
  ftxui::Elements details;
  details.push_back(fullLine("HIMS Scan R1 setup", uiAccentColor(), uiPanelRightBg()));
  details.push_back(ftxui::paragraph("Enter advances: port -> Wi-Fi name -> password -> fallback address -> provision.") |
                    ftxui::color(uiMutedColor()));
  details.push_back(ftxui::separator());
  details.push_back(fullLine(string(himsScanSetupField_ == 0 ? "> " : "  ") + "Port: " +
                                (himsScanPorts_.empty() ? "none" : himsScanPorts_[himsScanPortSelection_]),
                            uiTitleColor(), uiPanelRightBg()));
  details.push_back(fullLine(string(himsScanSetupField_ == 1 ? "> " : "  ") + "Wi-Fi: " + ssidText,
                            himsScanSetupField_ == 1 ? uiLinkColor() : uiTitleColor(), uiPanelRightBg()));
  details.push_back(fullLine(string(himsScanSetupField_ == 2 ? "> " : "  ") + "Password: " + passwordText,
                            himsScanSetupField_ == 2 ? uiLinkColor() : uiTitleColor(), uiPanelRightBg()));
  details.push_back(fullLine(string(himsScanSetupField_ == 3 ? "> " : "  ") + "Fallback: " + host + ':' +
                                to_string(server_.port()),
                            himsScanSetupField_ == 3 ? uiLinkColor() : uiTitleColor(), uiPanelRightBg()));
  details.push_back(ftxui::separator());
  details.push_back(ftxui::paragraph("The password is sent directly to the device and is not retained by HIMS.") |
                    ftxui::color(uiInfoColor()));

  return ftxui::hbox({ftxui::vbox(move(ports)) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex,
                      ftxui::separator(),
                      ftxui::vbox(move(details)) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex});
}

void App::handleHimsScanSetupKey(const KeyEvent& key) {
  if (key.type == KeyType::Escape) {
    himsScanPassword_.clear();
    changePage(Page::Dashboard);
    return;
  }
  if (key.type == KeyType::Character) {
    const auto ch = key.ch;
    if (himsScanSetupField_ == 0 && tolower(static_cast<unsigned char>(ch)) == 'r') {
      refreshHimsScanPorts();
      setMessage("Serial ports refreshed", 2);
      return;
    }
    if ((himsScanSetupField_ == 1 || himsScanSetupField_ == 2) && isprint(static_cast<unsigned char>(ch)) &&
        himsScanSetupInput_.size() < 63) {
      himsScanSetupInput_.push_back(ch);
      dirty_ = true;
    }
    return;
  }
  if (key.type == KeyType::Backspace && (himsScanSetupField_ == 1 || himsScanSetupField_ == 2)) {
    if (!himsScanSetupInput_.empty()) himsScanSetupInput_.pop_back();
    dirty_ = true;
    return;
  }
  if (key.type == KeyType::Up || key.type == KeyType::Down) {
    const int delta = key.type == KeyType::Up ? -1 : 1;
    if (himsScanSetupField_ == 0 && !himsScanPorts_.empty()) {
      himsScanPortSelection_ = static_cast<size_t>(clamp<int>(static_cast<int>(himsScanPortSelection_) + delta, 0,
                                                            static_cast<int>(himsScanPorts_.size()) - 1));
    } else if (himsScanSetupField_ == 3 && !himsScanAddresses_.empty()) {
      himsScanAddressSelection_ = static_cast<size_t>(clamp<int>(static_cast<int>(himsScanAddressSelection_) + delta, 0,
                                                               static_cast<int>(himsScanAddresses_.size()) - 1));
    }
    dirty_ = true;
    return;
  }
  if (key.type == KeyType::Enter || key.type == KeyType::Tab) {
    if (himsScanSetupField_ == 0) {
      if (himsScanPorts_.empty()) return;
      himsScanSetupField_ = 1;
      himsScanSetupInput_ = himsScanSsid_;
    } else if (himsScanSetupField_ == 1) {
      if (himsScanSetupInput_.empty()) return;
      himsScanSsid_ = himsScanSetupInput_;
      himsScanSetupInput_ = himsScanPassword_;
      himsScanSetupField_ = 2;
    } else if (himsScanSetupField_ == 2) {
      if (himsScanSetupInput_.empty()) return;
      himsScanPassword_ = himsScanSetupInput_;
      himsScanSetupInput_.clear();
      himsScanSetupField_ = 3;
    } else {
      provisionSelectedHimsScan();
    }
    dirty_ = true;
  }
}

}  // namespace hims
