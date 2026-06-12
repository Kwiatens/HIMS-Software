// HIMS - Hardware Inventory Management System
// Zebra label generation and printer queue integration.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "label_printer/LabelPrinter.h"

#include "ui/shared/AppUiShared.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <winspool.h>

#pragma comment(lib, "Winspool.lib")
#endif

namespace hims {

using namespace std;

namespace {

string windowsErrorText(unsigned long code) {
#ifdef _WIN32
  if (code == 0) {
    return "Unknown error";
  }

  LPWSTR buffer = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD length = FormatMessageW(flags, nullptr, static_cast<DWORD>(code), 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  if (length == 0 || buffer == nullptr) {
    return "Windows error " + to_string(code);
  }

  wstring wide(buffer, length);
  LocalFree(buffer);
  string result;
  result.reserve(wide.size());
  for (wchar_t ch : wide) {
    if (ch == L'\r' || ch == L'\n') {
      continue;
    }
    if (ch <= 0x7f) {
      result.push_back(static_cast<char>(ch));
    } else {
      result.push_back('?');
    }
  }
  return trim(result);
#else
  (void)code;
  return "Windows error";
#endif
}

string narrowFromWide(const wstring& value) {
#ifdef _WIN32
  if (value.empty()) {
    return {};
  }
  const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (sizeNeeded <= 0) {
    return {};
  }
  string result(static_cast<size_t>(sizeNeeded), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), sizeNeeded, nullptr, nullptr);
  return result;
#else
  return {};
#endif
}

wstring widenFromUtf8(const string& value) {
#ifdef _WIN32
  if (value.empty()) {
    return {};
  }
  const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
  if (sizeNeeded <= 0) {
    return {};
  }
  wstring result(static_cast<size_t>(sizeNeeded), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), sizeNeeded);
  return result;
#else
  return {};
#endif
}

string uppercaseAscii(string value) {
  transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(toupper(ch));
  });
  return value;
}

string lowerAscii(string value) {
  transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(tolower(ch));
  });
  return value;
}

string shortCode(const string& value, size_t maxLength = 14) {
  return ellipsize(trim(value), maxLength);
}

string dateOnly(time_t value) {
  const auto ts = nowTimestampString(value);
  if (ts.size() >= 10) {
    return ts.substr(0, 10);
  }
  return ts;
}

string shortHimsHint(const string& himsId) {
  const auto trimmed = trim(himsId);
  const auto colon = trimmed.find(':');
  if (colon != string::npos && colon + 1 < trimmed.size()) {
    return trimmed.substr(colon + 1);
  }
  return trimmed;
}

string compactJoin(const vector<string>& parts, const string& separator) {
  vector<string> filtered;
  for (const auto& part : parts) {
    const auto cleaned = trim(part);
    if (!cleaned.empty()) {
      filtered.push_back(cleaned);
    }
  }
  return join(filtered, separator.empty() ? ' ' : separator.front());
}

string normalizeResistanceValue(string value) {
  value = trim(value);
  if (value.empty()) {
    return value;
  }

  auto lowered = toLower(value);
  auto stripSuffix = [&](const string& suffix) {
    if (lowered.size() < suffix.size()) {
      return false;
    }
    if (lowered.compare(lowered.size() - suffix.size(), suffix.size(), suffix) != 0) {
      return false;
    }
    value = trim(value.substr(0, value.size() - suffix.size()));
    lowered = toLower(value);
    return true;
  };

  stripSuffix(" ohms");
  stripSuffix(" ohm");
  stripSuffix("ohms");
  stripSuffix("ohm");

  value = trim(value);
  if (value.empty()) {
    return {};
  }

  return value + u8"\u03A9";
}

string fieldOrBlank(const string& value, size_t maxLength);

string fitSingleLineLabel(const string& value, size_t maxLength) {
  return fieldOrBlank(value, maxLength);
}

vector<string> wrapLabelLines(const string& value, size_t maxWidth, size_t maxLines) {
  vector<string> lines;
  if (maxWidth == 0 || maxLines == 0) {
    return lines;
  }

  istringstream input(trim(value));
  string word;
  string current;

  auto flushCurrent = [&]() {
    if (!current.empty()) {
      lines.push_back(current);
      current.clear();
    }
  };

  while (input >> word) {
    if (word.size() > maxWidth) {
      word = ellipsize(word, maxWidth);
    }

    if (current.empty()) {
      current = word;
      continue;
    }

    if (current.size() + 1 + word.size() <= maxWidth) {
      current.push_back(' ');
      current += word;
      continue;
    }

    flushCurrent();
    if (lines.size() >= maxLines) {
      break;
    }
    current = word;
  }

  flushCurrent();
  if (lines.size() > maxLines) {
    lines.resize(maxLines);
  }

  if (!input.eof() && !lines.empty()) {
    lines.back() = ellipsize(lines.back(), maxWidth);
  }

  return lines;
}

string sanitiseZplFragment(const string& value) {
  string output;
  output.reserve(value.size());
  bool previousSpace = false;
  for (char ch : value) {
    unsigned char uch = static_cast<unsigned char>(ch);
    if (ch == '^' || ch == '~') {
      if (!previousSpace && !output.empty()) {
        output.push_back(' ');
        previousSpace = true;
      }
      continue;
    }
    if (ch == '\r' || ch == '\n' || ch == '\t' || iscntrl(uch)) {
      if (!previousSpace && !output.empty()) {
        output.push_back(' ');
        previousSpace = true;
      }
      continue;
    }
    if (isspace(uch) != 0) {
      if (!previousSpace && !output.empty()) {
        output.push_back(' ');
        previousSpace = true;
      }
      continue;
    }
    output.push_back(ch);
    previousSpace = false;
  }
  return trim(output);
}

string fieldOrBlank(const string& value, size_t maxLength) {
  return sanitiseZplFragment(ellipsize(trim(value), maxLength));
}

string collectLineFromValues(initializer_list<string> values, const string& separator, size_t maxLength) {
  vector<string> parts;
  for (const auto& value : values) {
    const auto cleaned = trim(value);
    if (!cleaned.empty()) {
      parts.push_back(cleaned);
    }
  }
  if (parts.empty()) {
    return {};
  }
  return fieldOrBlank(join(parts, separator.empty() ? ' ' : separator.front()), maxLength);
}

bool containsTag(const InventoryItem& item, initializer_list<const char*> needles) {
  for (const auto& tag : item.tags) {
    for (const auto* needle : needles) {
      if (containsInsensitive(tag, needle)) {
        return true;
      }
    }
  }
  return false;
}

optional<string> firstParameter(const InventoryItem& item, initializer_list<const char*> names) {
  return parameterValue(item, names);
}

string mainLabelValue(const InventoryItem& item) {
  if (categoryContains(item, {"capacitor"})) {
    return collectLineFromValues({firstParameter(item, {"Capacitance", "Value"}).value_or({}),
                                  firstParameter(item, {"Tolerance"}).value_or({})},
                                 " ", 24);
  }
  if (categoryContains(item, {"resistor"})) {
    return collectLineFromValues({normalizeResistanceValue(firstParameter(item, {"Resistance", "Value"}).value_or({})),
                                  firstParameter(item, {"Tolerance"}).value_or({})},
                                 " ", 24);
  }
  if (categoryContains(item, {"inductor", "choke", "coil"})) {
    return collectLineFromValues({firstParameter(item, {"Inductance", "Value"}).value_or({}),
                                  firstParameter(item, {"Current Rating", "Current"}).value_or({})},
                                 " ", 24);
  }

  if (!trim(item.partName).empty()) {
    return item.partName;
  }
  if (!trim(item.sku).empty()) {
    return item.sku;
  }
  return item.category;
}

string shortPackageLine(const InventoryItem& item) {
  const auto package = firstParameter(item, {"Package / Case", "Package Case", "Case / Package", "Case Package",
                                             "Supplier Device Package", "Device Package", "Package"});
  const auto size = firstParameter(item, {"Size / Dimension", "Dimensions"});
  return collectLineFromValues({package.value_or({}), size.value_or({})}, " / ", 24);
}

string manufacturerLine(const InventoryItem& item) {
  const auto manufacturer = trim(item.manufacturer);
  if (!manufacturer.empty()) {
    return fitSingleLineLabel(manufacturer, 20);
  }
  if (!trim(item.partName).empty()) {
    return fitSingleLineLabel(item.partName, 20);
  }
  if (!trim(item.sku).empty()) {
    return fitSingleLineLabel(item.sku, 20);
  }
  return fitSingleLineLabel(displayCategory(item.category), 20);
}

string normalizedShieldingLine(const optional<string>& value) {
  if (!value) {
    return {};
  }
  const auto cleaned = trim(*value);
  if (cleaned.empty()) {
    return {};
  }

  const auto normalized = normalizeKey(cleaned);
  if (normalized == "yes" || normalized == "true" || normalized == "shielded") {
    return "Shielded";
  }
  if (normalized == "no" || normalized == "false" || normalized == "unshielded") {
    return "Unshielded";
  }
  return fitSingleLineLabel(cleaned, 16);
}

vector<string> fallbackDetailLines(const InventoryItem& item, size_t maxLines) {
  vector<string> lines;
  for (const auto& field : electricalFieldsForItem(item)) {
    const auto value = trim(field.value);
    if (value.empty()) {
      continue;
    }
    if (containsInsensitive(field.label, "package")) {
      continue;
    }
    lines.push_back(fitSingleLineLabel(value, 24));
    if (lines.size() >= maxLines) {
      break;
    }
  }
  return lines;
}

vector<string> parameterLinesForItem(const InventoryItem& item) {
  vector<string> lines;

  if (categoryContains(item, {"capacitor"})) {
    if (const auto voltage = firstParameter(item, {"Operating Voltage", "Voltage", "Voltage - Rated", "Rated Voltage"})) {
      lines.push_back(fitSingleLineLabel("V " + trim(*voltage), 24));
    }
    if (const auto dielectric = firstParameter(item, {"Type", "Dielectric", "Dielectric Type"})) {
      lines.push_back(fitSingleLineLabel(trim(*dielectric), 24));
    }
    if (const auto tolerance = firstParameter(item, {"Tolerance"})) {
      lines.push_back(fitSingleLineLabel("Tol " + trim(*tolerance), 24));
    } else if (const auto esr = firstParameter(item, {"ESR", "ESR (Equivalent Series Resistance)"})) {
      lines.push_back(fitSingleLineLabel("ESR " + trim(*esr), 24));
    }
    return lines;
  }

  if (categoryContains(item, {"resistor"})) {
    if (const auto power = firstParameter(item, {"Power Dissipation", "Power (Watts)", "Power Rating", "Power",
                                                 "Power - Max", "Watts"})) {
      lines.push_back(fitSingleLineLabel("Pwr " + trim(*power), 24));
    }
    if (const auto tolerance = firstParameter(item, {"Tolerance"})) {
      lines.push_back(fitSingleLineLabel("Tol " + trim(*tolerance), 24));
    }
    if (const auto tempco = firstParameter(item, {"Temperature Coefficient", "Tempco"})) {
      lines.push_back(fitSingleLineLabel("Tempco " + trim(*tempco), 24));
    } else if (const auto size = firstParameter(item, {"Size / Dimension"})) {
      lines.push_back(fitSingleLineLabel(trim(*size), 24));
    }
    return lines;
  }

  if (categoryContains(item, {"inductor", "choke", "coil"})) {
    if (const auto current = firstParameter(item, {"Current Rating", "Current Rating (Amps)", "Current"})) {
      lines.push_back(fitSingleLineLabel("I " + trim(*current), 24));
    }
    if (const auto saturation = firstParameter(item, {"Saturation Current", "Current - Saturation (Isat)"})) {
      lines.push_back(fitSingleLineLabel("Isat " + trim(*saturation), 24));
    }
    string frequencyLine;
    if (const auto frequency = firstParameter(item, {"Frequency - Self Resonant", "Frequency"})) {
      frequencyLine = trim(*frequency);
    }
    const auto shielded = normalizedShieldingLine(firstParameter(item, {"Shielding"}));
    if (!shielded.empty()) {
      if (!frequencyLine.empty()) {
        frequencyLine += ' ';
      }
      frequencyLine += shielded;
    }
    if (!frequencyLine.empty()) {
      lines.push_back(fitSingleLineLabel(frequencyLine, 24));
    }
    return lines;
  }

  if (categoryContains(item, {"mcu", "microcontroller"})) {
    if (const auto voltage = firstParameter(item, {"Operating Voltage", "Voltage - Supply", "Voltage - Supply (Min/Max)", "Voltage"})) {
      lines.push_back(fitSingleLineLabel("V " + trim(*voltage), 24));
    }

    string coreLine;
    if (const auto core = firstParameter(item, {"Core", "Core Processor"})) {
      coreLine = trim(*core);
    }
    if (const auto clock = firstParameter(item, {"Clock Speed", "Clock Frequency", "Speed"})) {
      if (!coreLine.empty()) {
        coreLine += " @ ";
      }
      coreLine += trim(*clock);
    }
    if (!coreLine.empty()) {
      lines.push_back(fitSingleLineLabel(coreLine, 24));
    }

    string memoryLine;
    if (const auto flash = firstParameter(item, {"Flash", "Program Memory Size"})) {
      memoryLine = trim(*flash);
    }
    if (const auto ram = firstParameter(item, {"RAM", "Memory"})) {
      if (!memoryLine.empty()) {
        memoryLine += " / ";
      }
      memoryLine += trim(*ram);
    }
    if (!memoryLine.empty()) {
      lines.push_back(fitSingleLineLabel(memoryLine, 24));
    }
    return lines;
  }

  if (categoryContains(item, {"transistor", "mosfet", "fet", "discrete semiconductor"})) {
    if (categoryContains(item, {"mosfet", "fet"})) {
      if (const auto drainSource = firstParameter(item, {"Drain-Source Voltage", "Drain to Source Voltage (Vdss)", "Vds", "Vdss"})) {
        lines.push_back(fitSingleLineLabel("Vds " + trim(*drainSource), 24));
      }
      if (const auto current = firstParameter(item, {"Continuous Drain Current", "Current - Continuous Drain (Id) @ 25°C", "Current", "Id"})) {
        lines.push_back(fitSingleLineLabel("Id " + trim(*current), 24));
      }
      vector<string> thirdLine;
      if (const auto rds = firstParameter(item, {"Rds On", "Rds On (Max) @ Id, Vgs", "RDS(ON)"})) {
        thirdLine.push_back("Rds " + trim(*rds));
      }
      if (const auto gateCharge = firstParameter(item, {"Gate Charge", "Gate Charge (Qg) (Max) @ Vgs"})) {
        thirdLine.push_back("Qg " + trim(*gateCharge));
      }
      if (!thirdLine.empty()) {
        lines.push_back(fitSingleLineLabel(join(thirdLine, '/'), 24));
      }
      return lines;
    }

    if (const auto collectorEmitter = firstParameter(item, {"Collector-Emitter Voltage", "Collector Emitter Voltage", "Vce", "Vceo"})) {
      lines.push_back(fitSingleLineLabel("Vce " + trim(*collectorEmitter), 24));
    } else if (const auto voltage = firstParameter(item, {"Voltage", "Voltage - Collector Emitter", "Voltage - CE", "Vceo"})) {
      lines.push_back(fitSingleLineLabel(trim(*voltage), 24));
    }
    if (const auto current = firstParameter(item, {"Collector Current", "Current", "Ic", "Continuous Collector Current"})) {
      lines.push_back(fitSingleLineLabel("Ic " + trim(*current), 24));
    }
    if (const auto gain = firstParameter(item, {"hFE", "DC Current Gain", "Gain"})) {
      lines.push_back(fitSingleLineLabel("hFE " + trim(*gain), 24));
    } else if (const auto power = firstParameter(item, {"Power - Max", "Power"})) {
      lines.push_back(fitSingleLineLabel("Pwr " + trim(*power), 24));
    }
    return lines;
  }

  return fallbackDetailLines(item, 3);
}

string makeJobName(const InventoryItem& item) {
  const auto id = trim(item.himsId);
  if (!id.empty()) {
    return "HIMS Label " + id;
  }
  if (!trim(item.partName).empty()) {
    return "HIMS Label " + item.partName;
  }
  return "HIMS Label";
}

#ifdef _WIN32
PrinterQueueInfo makePrinterInfo(const wstring& name, const wstring& driver, const wstring& port, DWORD status,
                                 bool isDefault) {
  PrinterQueueInfo info;
  info.name = narrowFromWide(name);
  info.driverName = narrowFromWide(driver);
  info.portName = narrowFromWide(port);
  info.isDefault = isDefault;

  vector<string> statusParts;
  if (status == 0) {
    info.isReady = true;
    statusParts.push_back("Ready");
  } else {
    if (status & PRINTER_STATUS_PAUSED) {
      statusParts.push_back("Paused");
    }
    if (status & PRINTER_STATUS_OFFLINE) {
      statusParts.push_back("Offline");
    }
    if (status & PRINTER_STATUS_ERROR) {
      statusParts.push_back("Error");
    }
    if (status & PRINTER_STATUS_PAPER_OUT) {
      statusParts.push_back("Label stock empty");
    }
    if (status & PRINTER_STATUS_NOT_AVAILABLE) {
      statusParts.push_back("Not available");
    }
    if (status & PRINTER_STATUS_NO_TONER) {
      statusParts.push_back("Media error");
    }
    if (status & PRINTER_STATUS_DOOR_OPEN) {
      statusParts.push_back("Door open");
    }
    info.isReady = statusParts.empty();
  }
  if (statusParts.empty()) {
    statusParts.push_back("Ready");
  }
  info.statusText = join(statusParts, ';');
  return info;
}

class WindowsPrinterBackend final : public PrinterBackend {
 public:
  vector<PrinterQueueInfo> enumeratePrinters() const override {
    vector<PrinterQueueInfo> printers;
    DWORD bytesNeeded = 0;
    DWORD count = 0;
    const DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bytesNeeded, &count);
    if (bytesNeeded == 0) {
      return printers;
    }

    vector<BYTE> buffer(bytesNeeded);
    if (!EnumPrintersW(flags, nullptr, 2, buffer.data(), bytesNeeded, &bytesNeeded, &count)) {
      return printers;
    }

    const auto* entries = reinterpret_cast<PRINTER_INFO_2W*>(buffer.data());
    printers.reserve(count);
    for (DWORD index = 0; index < count; ++index) {
      const auto& entry = entries[index];
      printers.push_back(makePrinterInfo(entry.pPrinterName ? entry.pPrinterName : L"",
                                        entry.pDriverName ? entry.pDriverName : L"",
                                        entry.pPortName ? entry.pPortName : L"", entry.Status,
                                        (entry.Attributes & PRINTER_ATTRIBUTE_DEFAULT) != 0));
    }

    sort(printers.begin(), printers.end(), [](const PrinterQueueInfo& lhs, const PrinterQueueInfo& rhs) {
      if (lhs.isDefault != rhs.isDefault) {
        return lhs.isDefault > rhs.isDefault;
      }
      return lowerAscii(lhs.name) < lowerAscii(rhs.name);
    });
    return printers;
  }

  PrinterCheckResult probePrinter(const string& printerName) const override {
    PrinterCheckResult result;
    if (trim(printerName).empty()) {
      result.message = "No printer configured";
      return result;
    }

    const auto wideName = widenFromUtf8(printerName);
    if (wideName.empty()) {
      result.message = "Printer name could not be converted";
      return result;
    }

    PRINTER_DEFAULTSW defaults{};
    defaults.DesiredAccess = PRINTER_ACCESS_USE;
    HANDLE handle = nullptr;
    if (!OpenPrinterW(const_cast<LPWSTR>(wideName.c_str()), &handle, &defaults)) {
      result.message = "Unable to open printer queue: " + windowsErrorText(GetLastError());
      return result;
    }

    unique_ptr<void, decltype(&ClosePrinter)> printerHandle(handle, ClosePrinter);
    DWORD bytesNeeded = 0;
    GetPrinterW(handle, 2, nullptr, 0, &bytesNeeded);
    if (bytesNeeded == 0) {
      result.ok = true;
      result.message = "Printer queue opened";
      return result;
    }

    vector<BYTE> buffer(bytesNeeded);
    if (!GetPrinterW(handle, 2, buffer.data(), bytesNeeded, &bytesNeeded)) {
      result.message = "Unable to query printer status: " + windowsErrorText(GetLastError());
      return result;
    }

    const auto* info = reinterpret_cast<PRINTER_INFO_2W*>(buffer.data());
    const auto queue = makePrinterInfo(info->pPrinterName ? info->pPrinterName : L"",
                                       info->pDriverName ? info->pDriverName : L"",
                                       info->pPortName ? info->pPortName : L"", info->Status,
                                       (info->Attributes & PRINTER_ATTRIBUTE_DEFAULT) != 0);
    result.ok = queue.isReady;
    result.message = queue.statusText;
    return result;
  }

  bool sendRawJob(const string& printerName, const string& jobName, const string& zpl, string* error) const override {
    const auto wideName = widenFromUtf8(printerName);
    if (wideName.empty()) {
      if (error != nullptr) {
        *error = "Printer name could not be converted";
      }
      return false;
    }

    PRINTER_DEFAULTSW defaults{};
    defaults.DesiredAccess = PRINTER_ACCESS_USE;
    HANDLE handle = nullptr;
    if (!OpenPrinterW(const_cast<LPWSTR>(wideName.c_str()), &handle, &defaults)) {
      if (error != nullptr) {
        *error = "Unable to open printer queue: " + windowsErrorText(GetLastError());
      }
      return false;
    }

    unique_ptr<void, decltype(&ClosePrinter)> printerHandle(handle, ClosePrinter);

    DOC_INFO_1W doc{};
    const auto jobWide = widenFromUtf8(jobName);
    const auto rawWide = widenFromUtf8("RAW");
    doc.pDocName = const_cast<LPWSTR>(jobWide.empty() ? L"HIMS Label" : jobWide.c_str());
    doc.pOutputFile = nullptr;
    doc.pDatatype = const_cast<LPWSTR>(rawWide.empty() ? L"RAW" : rawWide.c_str());

    if (StartDocPrinterW(handle, 1, reinterpret_cast<LPBYTE>(&doc)) == 0) {
      if (error != nullptr) {
        *error = "Unable to start printer job: " + windowsErrorText(GetLastError());
      }
      return false;
    }

    bool success = false;
    const auto endDoc = [&]() {
      EndDocPrinter(handle);
    };

    do {
      if (!StartPagePrinter(handle)) {
        if (error != nullptr) {
          *error = "Unable to start printer page: " + windowsErrorText(GetLastError());
        }
        break;
      }

      const auto body = zpl;
      DWORD bytesWritten = 0;
      if (!WritePrinter(handle, const_cast<char*>(body.data()), static_cast<DWORD>(body.size()), &bytesWritten) ||
          bytesWritten != body.size()) {
        if (error != nullptr) {
          *error = "Unable to write print data: " + windowsErrorText(GetLastError());
        }
        break;
      }

      if (!EndPagePrinter(handle)) {
        if (error != nullptr) {
          *error = "Unable to finish printer page: " + windowsErrorText(GetLastError());
        }
        break;
      }

      success = true;
    } while (false);

    endDoc();
    return success;
  }
};
#endif

string upperCategoryHeader(const InventoryItem& item) {
  auto category = trim(displayCategory(item.category));
  if (category.empty()) {
    category = trim(item.partName);
  }
  return uppercaseAscii(category);
}

}  // namespace

LabelPrinterService::LabelPrinterService(unique_ptr<PrinterBackend> backend)
    : backend_(move(backend)) {
#ifdef _WIN32
  if (backend_ == nullptr) {
    backend_ = make_unique<WindowsPrinterBackend>();
  }
#endif
}

LabelPrinterService::~LabelPrinterService() = default;

vector<PrinterQueueInfo> LabelPrinterService::enumeratePrinters() const {
  if (backend_ == nullptr) {
    return {};
  }
  return backend_->enumeratePrinters();
}

bool LabelPrinterService::loadConfig(const filesystem::path& path) {
  configPath_ = path;
  ifstream input(configPath_);
  if (!input) {
    configuredPrinter_.clear();
    return false;
  }

  string value;
  if (!(input >> quoted(value))) {
    configuredPrinter_.clear();
    return false;
  }

  configuredPrinter_ = trim(value);
  return !configuredPrinter_.empty();
}

bool LabelPrinterService::saveConfig(const filesystem::path& path) const {
  filesystem::create_directories(path.parent_path());
  ofstream output(path, ios::trunc);
  if (!output) {
    return false;
  }

  output << quoted(configuredPrinter_) << '\n';
  return true;
}

void LabelPrinterService::setConfiguredPrinter(string printerName) {
  configuredPrinter_ = trim(printerName);
}

const string& LabelPrinterService::configuredPrinter() const {
  return configuredPrinter_;
}

bool LabelPrinterService::hasConfiguredPrinter() const {
  return !trim(configuredPrinter_).empty();
}

optional<PrinterQueueInfo> LabelPrinterService::configuredPrinterInfo() const {
  if (!hasConfiguredPrinter()) {
    return nullopt;
  }

  const auto printers = enumeratePrinters();
  const auto needle = lowerAscii(trim(configuredPrinter_));
  for (const auto& printer : printers) {
    if (lowerAscii(trim(printer.name)) == needle) {
      return printer;
    }
  }
  return nullopt;
}

PrinterCheckResult LabelPrinterService::probeConfiguredPrinter() const {
  if (backend_ == nullptr) {
    return {false, "Printer backend unavailable"};
  }
  return backend_->probePrinter(configuredPrinter_);
}

HimsLabelPlan LabelPrinterService::buildLabelPlan(const InventoryItem& item) const {
  HimsLabelPlan plan;
  const auto parameterLines = parameterLinesForItem(item);
  plan.categoryHeader = upperCategoryHeader(item);
  plan.mainValue = mainLabelValue(item);
  plan.packageLine = shortPackageLine(item);
  plan.manufacturerLine = manufacturerLine(item);
  if (!parameterLines.empty()) {
    plan.parameterLine1 = parameterLines[0];
  }
  if (parameterLines.size() > 1) {
    plan.parameterLine2 = parameterLines[1];
  }
  if (parameterLines.size() > 2) {
    plan.parameterLine3 = parameterLines[2];
  }
  plan.himsId = trim(item.himsId);
  plan.scannerHint = shortHimsHint(plan.himsId);
  if (trim(plan.scannerHint).empty()) {
    plan.scannerHint = shortCode(plan.himsId, 16);
  }

  return plan;
}

string LabelPrinterService::buildZpl(const InventoryItem& item) const {
  const auto plan = buildLabelPlan(item);
  const auto categoryHeader = fitSingleLineLabel(plan.categoryHeader, 16);
  const auto mainValue = fitSingleLineLabel(plan.mainValue, 14);
  const auto packageLine = fitSingleLineLabel(plan.packageLine, 24);
  const auto manufacturerLine = fitSingleLineLabel(plan.manufacturerLine, 20);
  const auto parameterLine1 = fitSingleLineLabel(plan.parameterLine1, 24);
  const auto parameterLine2 = fitSingleLineLabel(plan.parameterLine2, 24);
  const auto parameterLine3 = fitSingleLineLabel(plan.parameterLine3, 24);
  const auto scannerHint = fitSingleLineLabel(plan.scannerHint, 14);
  ostringstream out;
  out << "^XA\r\n";
  out << "^CI28\r\n";
  out << "^PW256\r\n";
  out << "^LL200\r\n";
  out << "^LH0,0\r\n";
  out << "^PR3\r\n";
  out << "^MD12\r\n";
  out << "\r\n";
  out << "^FX --- Header ---\r\n";
  out << "^FO5,0^GB180,24,24,B,6^FS\r\n";
  out << "^FO12,6^A0N,17,17^FR^FD" << sanitizeLabelText(categoryHeader) << "^FS\r\n";
  out << "^FO200,6^A0N,17,17^FR^FDHIMS^FS\r\n";
  out << "\r\n";
  out << "^FX --- Main value ---\r\n";
  out << "^FO10,33^A0N,34,31^FD" << sanitizeLabelText(mainValue) << "^FS\r\n";
  out << "\r\n";
  out << "^FX --- Package ---\r\n";
  if (!packageLine.empty()) {
    out << "^FO10,70^A0N,17,17^FD" << sanitizeLabelText(packageLine) << "^FS\r\n";
  }
  out << "\r\n";
  out << "^FX --- Thin divider ---\r\n";
  out << "^FO10,93^GB146,1,1^FS\r\n";
  out << "\r\n";
  out << "^FX --- Manufacturer / parameters ---\r\n";
  if (!manufacturerLine.empty()) {
    out << "^FO10,100^A0N,16,16^FD" << sanitizeLabelText(manufacturerLine) << "^FS\r\n";
  }
  if (!parameterLine1.empty()) {
    out << "^FO10,118^A0N,15,15^FD" << sanitizeLabelText(parameterLine1) << "^FS\r\n";
  }
  if (!parameterLine2.empty()) {
    out << "^FO10,136^A0N,15,15^FD" << sanitizeLabelText(parameterLine2) << "^FS\r\n";
  }
  if (!parameterLine3.empty()) {
    out << "^FO10,154^A0N,15,15^FD" << sanitizeLabelText(parameterLine3) << "^FS\r\n";
  }
  out << "\r\n";
  out << "^FX --- DataMatrix ---\r\n";
  out << "^FO160,60^BXN,5,200,0,0^FD" << sanitizeLabelText(plan.himsId) << "^FS\r\n";
  out << "\r\n";
  out << "^FX --- Scanner hint ---\r\n";
  out << "^FO173,145^A0N,15,14^FD" << sanitizeLabelText(scannerHint) << "^FS\r\n";
  out << "^XZ\r\n";
  return out.str();
}

bool LabelPrinterService::printItemLabel(const InventoryItem& item, string* error) const {
  if (backend_ == nullptr) {
    if (error != nullptr) {
      *error = "Printer backend unavailable";
    }
    return false;
  }

  if (!hasConfiguredPrinter()) {
    if (error != nullptr) {
      *error = "No printer configured";
    }
    return false;
  }

  const auto zpl = buildZpl(item);
  return backend_->sendRawJob(configuredPrinter_, makeJobName(item), zpl, error);
}

string LabelPrinterService::summaryText() const {
  if (!hasConfiguredPrinter()) {
    return "Printer: not configured";
  }

  const auto info = configuredPrinterInfo();
  if (!info) {
    return "Printer: " + configuredPrinter_ + " [missing]";
  }

  ostringstream out;
  out << "Printer: " << info->name << " [" << info->statusText << "]";
  return out.str();
}

string sanitizeLabelText(const string& value) {
  return sanitiseZplFragment(value);
}

}  // namespace hims
