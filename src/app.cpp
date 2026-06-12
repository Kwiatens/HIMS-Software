// HIMS - Hardware Inventory Management System
// Terminal application controller and app-level state management.

#include "App.h"

#include "platform/DigiKeyApi.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <thread>

namespace hims {

using namespace std;

namespace {

constexpr const char* kColorReset = "\x1b[0m";
constexpr const char* kColorTitle = "\x1b[38;5;81m";
constexpr const char* kColorAccent = "\x1b[38;5;49m";
constexpr const char* kColorInfo = "\x1b[38;5;117m";
constexpr const char* kColorSuccess = "\x1b[38;5;114m";
constexpr const char* kColorLink = "\x1b[38;5;87m";
constexpr const char* kColorLabel = "\x1b[38;5;111m";
constexpr const char* kColorMuted = "\x1b[38;5;243m";
constexpr const char* kColorWarn = "\x1b[38;5;214m";
constexpr const char* kColorDanger = "\x1b[38;5;203m";
constexpr const char* kColorDim = "\x1b[38;5;245m";
constexpr const char* kColorSelect = "\x1b[48;5;236m";
constexpr const char* kBgPanelLeft = "\x1b[48;5;233m";
constexpr const char* kBgPanelRight = "\x1b[48;5;235m";
constexpr const char* kBgRowDark = "\x1b[48;5;232m";
constexpr const char* kBgRowLight = "\x1b[48;5;236m";
constexpr const char* kBgRowSelected = "\x1b[48;5;24m";

string padRight(string value, int width);

string styleText(const string& text, const char* fg = nullptr, const char* bg = nullptr) {
  ostringstream out;
  if (bg != nullptr) {
    out << bg;
  }
  if (fg != nullptr) {
    out << fg;
  }
  out << text << kColorReset;
  return out.str();
}

string styleCell(const string& text, int width, const char* fg = nullptr, const char* bg = nullptr) {
  return styleText(padRight(text, width), fg, bg);
}

ftxui::Color uiTitleColor() {
  return ftxui::Color::RGB(102, 204, 255);
}

ftxui::Color uiAccentColor() {
  return ftxui::Color::RGB(80, 220, 170);
}

ftxui::Color uiInfoColor() {
  return ftxui::Color::RGB(120, 180, 255);
}

ftxui::Color uiSuccessColor() {
  return ftxui::Color::RGB(115, 220, 140);
}

ftxui::Color uiLinkColor() {
  return ftxui::Color::RGB(98, 200, 255);
}

ftxui::Color uiLabelColor() {
  return ftxui::Color::RGB(170, 160, 255);
}

ftxui::Color uiWarnColor() {
  return ftxui::Color::RGB(255, 190, 80);
}

ftxui::Color uiDangerColor() {
  return ftxui::Color::RGB(255, 110, 120);
}

ftxui::Color uiMutedColor() {
  return ftxui::Color::RGB(180, 180, 180);
}

ftxui::Color uiDimColor() {
  return ftxui::Color::RGB(120, 120, 120);
}

ftxui::Color uiPanelLeftBg() {
  return ftxui::Color::RGB(24, 24, 24);
}

ftxui::Color uiPanelRightBg() {
  return ftxui::Color::RGB(30, 30, 30);
}

ftxui::Color uiRowDarkBg() {
  return ftxui::Color::RGB(20, 20, 20);
}

ftxui::Color uiRowLightBg() {
  return ftxui::Color::RGB(42, 42, 42);
}

ftxui::Color uiRowSelectedBg() {
  return ftxui::Color::RGB(25, 70, 110);
}

ftxui::Element styledText(const string& text, optional<ftxui::Color> fg = nullopt,
                          optional<ftxui::Color> bg = nullopt) {
  auto element = ftxui::text(text);
  if (fg) {
    element = element | ftxui::color(*fg);
  }
  if (bg) {
    element = element | ftxui::bgcolor(*bg);
  }
  return element;
}

ftxui::Element fullLine(const string& text, optional<ftxui::Color> fg = nullopt,
                        optional<ftxui::Color> bg = nullopt) {
  auto element = ftxui::hbox({ftxui::text(text), ftxui::filler()});
  if (fg) {
    element = element | ftxui::color(*fg);
  }
  if (bg) {
    element = element | ftxui::bgcolor(*bg);
  }
  return element;
}

ftxui::Element linesBlock(const vector<string>& lines, optional<ftxui::Color> fg = nullopt,
                          optional<ftxui::Color> bg = nullopt) {
  ftxui::Elements elements;
  elements.reserve(lines.size());
  for (const auto& line : lines) {
    elements.push_back(fullLine(line, fg, bg));
  }
  return ftxui::vbox(move(elements));
}

ftxui::Element bulletLine(const string& label, const string& value, ftxui::Color labelColor,
                          ftxui::Color valueColor) {
  return ftxui::hbox({
      styledText(label, labelColor),
      styledText(value, valueColor),
      ftxui::filler(),
  });
}

ftxui::Element panel(const string& title, ftxui::Elements body, optional<ftxui::Color> titleColor = nullopt,
                     optional<ftxui::Color> borderColor = nullopt) {
  auto titleElement = styledText(title, titleColor);
  auto element = ftxui::window(titleElement, ftxui::vbox(move(body)));
  if (borderColor) {
    element = element | ftxui::color(*borderColor);
  }
  return element;
}

ftxui::Element quantityBadge(int quantity, bool selected = false) {
  const auto fg = quantity <= 0 ? uiDangerColor() : (quantity <= 5 ? uiWarnColor() : uiSuccessColor());
  const auto bg = selected ? ftxui::Color::RGB(18, 18, 18)
                           : (quantity <= 0 ? ftxui::Color::RGB(52, 22, 22)
                                            : (quantity <= 5 ? ftxui::Color::RGB(58, 42, 14)
                                                             : ftxui::Color::RGB(18, 44, 28)));
  return ftxui::text(" QTY " + to_string(quantity) + " ") | ftxui::bold | ftxui::color(fg) | ftxui::bgcolor(bg);
}

string displayCategory(const string& category) {
  auto value = trim(category);
  const auto slash = value.find(" / ");
  if (slash != string::npos) {
    value = trim(value.substr(0, slash));
  }

  const auto openParen = value.find(" (");
  if (openParen != string::npos) {
    value = trim(value.substr(0, openParen));
  }

  return value;
}

string ellipsize(const string& value, size_t maxLength) {
  if (maxLength == 0 || value.size() <= maxLength) {
    return value;
  }
  if (maxLength <= 3) {
    return value.substr(0, maxLength);
  }
  return value.substr(0, maxLength - 3) + "...";
}

KeyEvent translateEvent(const ftxui::Event& event) {
  if (event == ftxui::Event::Return) {
    return {KeyType::Enter, '\0'};
  }
  if (event == ftxui::Event::Escape) {
    return {KeyType::Escape, '\0'};
  }
  if (event == ftxui::Event::CtrlH) {
    return {KeyType::CtrlBackspace, '\0'};
  }
  if (event == ftxui::Event::Backspace) {
    if (controlModifierPressed()) {
      return {KeyType::CtrlBackspace, '\0'};
    }
    return {KeyType::Backspace, '\0'};
  }
  if (event == ftxui::Event::Tab) {
    return {KeyType::Tab, '\0'};
  }
  if (event == ftxui::Event::ArrowUp) {
    return {KeyType::Up, '\0'};
  }
  if (event == ftxui::Event::ArrowDown) {
    return {KeyType::Down, '\0'};
  }
  if (event == ftxui::Event::ArrowLeft) {
    return {KeyType::Left, '\0'};
  }
  if (event == ftxui::Event::ArrowRight) {
    return {KeyType::Right, '\0'};
  }
  if (event == ftxui::Event::Home) {
    return {KeyType::Home, '\0'};
  }
  if (event == ftxui::Event::End) {
    return {KeyType::End, '\0'};
  }
  if (event == ftxui::Event::PageUp) {
    return {KeyType::PageUp, '\0'};
  }
  if (event == ftxui::Event::PageDown) {
    return {KeyType::PageDown, '\0'};
  }
  if (event == ftxui::Event::Delete) {
    return {KeyType::Delete, '\0'};
  }
  if (event.is_character() && !event.character().empty()) {
    return {KeyType::Character, event.character()[0]};
  }
  return {KeyType::Unknown, '\0'};
}

string padRight(string value, int width) {
  if (width <= 0) {
    return {};
  }
  if (static_cast<int>(value.size()) > width) {
    if (width <= 1) {
      return value.substr(0, width);
    }
    return value.substr(0, width - 3) + "...";
  }
  value.append(static_cast<size_t>(width - static_cast<int>(value.size())), ' ');
  return value;
}

vector<string> splitLines(const string& text) {
  vector<string> lines;
  istringstream input(text);
  string line;

  while (getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(move(line));
  }

  if (lines.empty()) {
    lines.push_back({});
  }

  return lines;
}

vector<string> wrapText(const string& text, int width) {
  vector<string> lines;
  if (width <= 0) {
    return lines;
  }

  istringstream words(text);
  string word;
  string line;

  while (words >> word) {
    if (static_cast<int>(line.size() + word.size() + 1) > width && !line.empty()) {
      lines.push_back(line);
      line.clear();
    }

    if (!line.empty()) {
      line.push_back(' ');
    }
    line += word;
  }

  if (!line.empty()) {
    lines.push_back(line);
  }

  if (lines.empty()) {
    lines.push_back({});
  }

  return lines;
}

void renderColumns(ostringstream& out, const vector<string>& leftLines,
                   const vector<string>& rightLines, int leftWidth, int rightWidth, int maxRows,
                   int gap = 4, const char* leftFg = nullptr, const char* leftBg = nullptr,
                   const char* rightFg = nullptr, const char* rightBg = nullptr) {
  const int rowCount = min(
      maxRows,
      max(static_cast<int>(leftLines.size()), static_cast<int>(rightLines.size())));

  for (int row = 0; row < rowCount; ++row) {
    const auto left = row < static_cast<int>(leftLines.size()) ? leftLines[static_cast<size_t>(row)] : "";
    const auto right = row < static_cast<int>(rightLines.size()) ? rightLines[static_cast<size_t>(row)] : "";
    out << styleCell(left, leftWidth, leftFg, leftBg) << string(static_cast<size_t>(gap), ' ')
        << styleCell(right, rightWidth, rightFg, rightBg) << '\n';
  }
}

void appendWrapped(vector<string>& lines, const string& text, int width) {
  const auto wrapped = wrapText(text, width);
  lines.insert(lines.end(), wrapped.begin(), wrapped.end());
}

void appendWrappedStyled(vector<string>& lines, const string& text, int width, const char* fg) {
  const auto wrapped = wrapText(text, width);
  for (const auto& line : wrapped) {
    lines.push_back(styleText(line, fg));
  }
}

string joinTags(const vector<string>& tags) {
  return join(tags, ',');
}

vector<string> splitFlexible(const string& text) {
  vector<string> values;
  string current;
  for (char ch : text) {
    if (ch == ',' || ch == ';' || ch == '\n') {
      current = trim(current);
      if (!current.empty()) {
        values.push_back(current);
      }
      current.clear();
    } else {
      current.push_back(ch);
    }
  }

  current = trim(current);
  if (!current.empty()) {
    values.push_back(current);
  }

  return values;
}

vector<Parameter> parseParameters(const string& text) {
  vector<Parameter> values;
  for (const auto& entry : splitFlexible(text)) {
    const auto equalsPos = entry.find('=');
    if (equalsPos == string::npos) {
      continue;
    }
    values.push_back({trim(entry.substr(0, equalsPos)), trim(entry.substr(equalsPos + 1))});
  }
  return values;
}

string renderTags(const vector<string>& tags) {
  if (tags.empty()) {
    return "-";
  }
  return ellipsize(join(tags, ','), 32);
}

string renderParameters(const vector<Parameter>& parameters) {
  if (parameters.empty()) {
    return "-";
  }

  ostringstream out;
  for (size_t index = 0; index < parameters.size(); ++index) {
    if (index > 0) {
      out << "; ";
    }
    out << parameters[index].name << '=' << parameters[index].value;
  }
  return out.str();
}

string renderUrl(const string& url) {
  if (url.empty()) {
    return "-";
  }
  return ellipsize(url, 32);
}

struct DetailField {
  string label;
  string value;
  ftxui::Color labelColor;
  ftxui::Color valueColor;
};

string normalizeKey(string value) {
  string normalized;
  normalized.reserve(value.size());
  for (unsigned char ch : value) {
    if (isalnum(ch)) {
      normalized.push_back(static_cast<char>(tolower(ch)));
    }
  }
  return normalized;
}

bool categoryContains(const InventoryItem& item, initializer_list<const char*> needles) {
  const auto category = normalizeKey(displayCategory(item.category));
  for (const auto* needle : needles) {
    const auto normalizedNeedle = normalizeKey(needle);
    if (category.find(normalizedNeedle) != string::npos || normalizedNeedle.find(category) != string::npos) {
      return true;
    }
  }
  return false;
}

bool parameterLabelMatches(const string& lhs, const string& rhs) {
  const auto normalizedLhs = normalizeKey(lhs);
  const auto normalizedRhs = normalizeKey(rhs);
  return normalizedLhs == normalizedRhs || normalizedLhs.find(normalizedRhs) != string::npos ||
         normalizedRhs.find(normalizedLhs) != string::npos;
}

bool looksLikePackagingValue(const string& value) {
  const auto normalized = normalizeKey(value);
  if (normalized.empty()) {
    return false;
  }

  static const initializer_list<const char*> kPackagingTokens = {
      "tapeandreel", "cuttape", "digireel", "tube", "tray", "bulk", "bag", "strip", "ammo", "box", "loose",
      "pack", "reel"};
  return any_of(kPackagingTokens.begin(), kPackagingTokens.end(), [&](const char* token) {
    return normalized == token || normalized.find(token) != string::npos;
  });
}

const Parameter* findParameter(const vector<Parameter>& parameters, initializer_list<const char*> names) {
  for (const auto* name : names) {
    for (const auto& parameter : parameters) {
      if (parameterLabelMatches(parameter.name, name)) {
        return &parameter;
      }
    }
  }
  return nullptr;
}

optional<string> parameterValue(const InventoryItem& item, initializer_list<const char*> names) {
  if (const auto* parameter = findParameter(item.parameters, names); parameter != nullptr) {
    const auto value = trim(parameter->value);
    if (!value.empty() && !looksLikePackagingValue(value)) {
      return value;
    }
  }
  return nullopt;
}

string prettyLabel(const string& label) {
  const auto trimmed = trim(label);
  if (!trimmed.empty()) {
    return trimmed;
  }
  return "Value";
}

vector<DetailField> electricalFieldsForItem(const InventoryItem& item) {
  vector<DetailField> fields;
  const auto addField = [&](const string& label, optional<string> value) {
    if (value && !trim(*value).empty()) {
      fields.push_back({label + ": ", *value, uiLabelColor(), uiTitleColor()});
    }
  };
  const auto addValueAndPackage = [&](const string& label, optional<string> value,
                                      optional<string> package) {
    addField(label, move(value));
    addField("Package", move(package));
  };
  const auto hasParameter = [&](initializer_list<const char*> names) {
    return findParameter(item.parameters, names) != nullptr;
  };

  const auto package = parameterValue(item, {"Package / Case", "Package Case", "Case / Package", "Case Package",
                                             "Supplier Device Package", "Device Package", "Package"});

  if (categoryContains(item, {"capacitor"})) {
    addValueAndPackage("Capacitance", parameterValue(item, {"Capacitance", "Value"}), package);
    addField("Operating Voltage", parameterValue(item, {"Operating Voltage", "Voltage", "Voltage - Rated", "Rated Voltage"}));
    addField("Tolerance", parameterValue(item, {"Tolerance"}));
    addField("Type", parameterValue(item, {"Type", "Dielectric", "Dielectric Type"}));
    addField("ESR", parameterValue(item, {"ESR", "ESR (Equivalent Series Resistance)"}));
    addField("Lifetime @ Temp.", parameterValue(item, {"Lifetime @ Temp.", "Lifetime"}));
    addField("Polarization", parameterValue(item, {"Polarization"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Applications", parameterValue(item, {"Applications"}));
    addField("Size / Dimension", parameterValue(item, {"Size / Dimension"}));
    return fields;
  }

  if (categoryContains(item, {"resistor"})) {
    addValueAndPackage("Resistance", parameterValue(item, {"Resistance", "Value"}), package);
    addField("Power Dissipation",
             parameterValue(item, {"Power Dissipation", "Power (Watts)", "Power Rating", "Power", "Power - Max", "Watts"}));
    addField("Tolerance", parameterValue(item, {"Tolerance"}));
    addField("Composition", parameterValue(item, {"Composition"}));
    addField("Temperature Coefficient", parameterValue(item, {"Temperature Coefficient", "Tempco"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Size / Dimension", parameterValue(item, {"Size / Dimension"}));
    addField("Height", parameterValue(item, {"Height - Seated (Max)", "Height"}));
    addField("Number of Terminations", parameterValue(item, {"Number of Terminations"}));
    return fields;
  }

  if (categoryContains(item, {"indicator", "led"})) {
    addValueAndPackage("Color", parameterValue(item, {"Color"}), package);
    addField("Forward Voltage", parameterValue(item, {"Forward Voltage", "Vf"}));
    addField("Wavelength", parameterValue(item, {"Wavelength"}));
    addField("Current", parameterValue(item, {"Current", "If", "Forward Current"}));
    addField("Type", parameterValue(item, {"Type"}));
    return fields;
  }

  if (categoryContains(item, {"integrated circuit", "integrated circuits"})) {
    if (hasParameter({"Memory Type", "Memory Format", "Memory Size"}) ||
        hasParameter({"Program Memory Size", "Program Memory Type"})) {
      addValueAndPackage("Memory", parameterValue(item, {"Memory Size", "Program Memory Size", "Memory"}), package);
      addField("Memory Type", parameterValue(item, {"Memory Type", "Memory Format"}));
      addField("Memory Interface", parameterValue(item, {"Memory Interface", "Interface"}));
      addField("Technology", parameterValue(item, {"Technology"}));
      addField("Clock", parameterValue(item, {"Clock Frequency", "Speed"}));
      addField("Operating Voltage", parameterValue(item, {"Voltage - Supply", "Voltage - Supply (Min/Max)", "Voltage - Supply (Min)", "Voltage - Supply (Max)"}));
      addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
      addField("Mounting Type", parameterValue(item, {"Mounting Type"}));
      addField("DigiKey Programmable", parameterValue(item, {"DigiKey Programmable"}));
      return fields;
    }

    if (hasParameter({"Voltage - Input", "Voltage - Output", "Current - Output", "Output Type", "Voltage - Output (Min/Fixed)"})) {
      addValueAndPackage("Output Voltage",
                         parameterValue(item, {"Voltage - Output", "Voltage - Output (Min/Fixed)", "Voltage - Output (Max)", "Output Voltage"}),
                         package);
      addField("Input Voltage", parameterValue(item, {"Voltage - Input", "Voltage - Input (Min/Max)", "Vin"}));
      addField("Current", parameterValue(item, {"Current - Output", "Output Current", "Current"}));
      addField("Type", parameterValue(item, {"Type", "Output Type"}));
      addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
      addField("Mounting Type", parameterValue(item, {"Mounting Type"}));
      return fields;
    }

    addValueAndPackage("Function", parameterValue(item, {"Function", "Type", "Memory Type"}), package);
    addField("Technology", parameterValue(item, {"Technology"}));
    addField("Interface", parameterValue(item, {"Interface", "Memory Interface"}));
    addField("Operating Voltage", parameterValue(item, {"Voltage - Supply", "Voltage - Supply (Min/Max)"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Mounting Type", parameterValue(item, {"Mounting Type"}));
    addField("DigiKey Programmable", parameterValue(item, {"DigiKey Programmable"}));
    return fields;
  }

  if (categoryContains(item, {"inductor", "choke", "coil"})) {
    addValueAndPackage("Inductance", parameterValue(item, {"Inductance", "Value"}), package);
    addField("Current Rating", parameterValue(item, {"Current Rating", "Current Rating (Amps)", "Current"}));
    addField("DC Resistance", parameterValue(item, {"DC Resistance", "DC Resistance (DCR)", "DCR"}));
    addField("Saturation Current", parameterValue(item, {"Saturation Current", "Current - Saturation (Isat)"}));
    addField("Q @ Freq", parameterValue(item, {"Q @ Freq"}));
    addField("Frequency", parameterValue(item, {"Frequency - Self Resonant", "Frequency"}));
    addField("Shielding", parameterValue(item, {"Shielding"}));
    addField("Material", parameterValue(item, {"Material - Core"}));
    return fields;
  }

  if (categoryContains(item, {"diode", "rectifier", "schottky"})) {
    addValueAndPackage("Forward Voltage", parameterValue(item, {"Forward Voltage", "Voltage - Forward (Vf) (Max) @ If", "Vf"}), package);
    addField("Reverse Voltage", parameterValue(item, {"Reverse Voltage", "Voltage - DC Reverse (Vr) (Max)", "Peak Reverse Voltage", "Vr"}));
    addField("Current", parameterValue(item, {"Current", "Current - Average Rectified (Io)", "If", "Forward Current"}));
    addField("Technology", parameterValue(item, {"Technology"}));
    addField("Speed", parameterValue(item, {"Speed"}));
    addField("Reverse Leakage", parameterValue(item, {"Current - Reverse Leakage @ Vr"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature - Junction", "Operating Temperature"}));
    return fields;
  }

  if (categoryContains(item, {"transistor", "mosfet", "fet", "discrete semiconductor"})) {
    addValueAndPackage("Drain-Source Voltage",
                       parameterValue(item, {"Drain-Source Voltage", "Drain to Source Voltage (Vdss)", "Vds", "Vdss"}), package);
    addField("Current", parameterValue(item, {"Continuous Drain Current", "Current - Continuous Drain (Id) @ 25°C", "Current", "Id"}));
    addField("RDS On", parameterValue(item, {"Rds On", "Rds On (Max) @ Id, Vgs", "RDS(ON)"}));
    addField("Gate Charge", parameterValue(item, {"Gate Charge", "Gate Charge (Qg) (Max) @ Vgs"}));
    addField("Drive Voltage", parameterValue(item, {"Drive Voltage", "Drive Voltage (Max Rds On, Min Rds On)"}));
    addField("VGS", parameterValue(item, {"Vgs", "Vgs (Max)", "Vgs(th) (Max) @ Id"}));
    addField("Input Capacitance", parameterValue(item, {"Input Capacitance (Ciss) (Max) @ Vds", "Input Capacitance"}));
    addField("Power - Max", parameterValue(item, {"Power - Max"}));
    addField("Technology", parameterValue(item, {"Technology"}));
    return fields;
  }

  if (categoryContains(item, {"connector"})) {
    addValueAndPackage("Pins", parameterValue(item, {"Pins", "Number of Positions", "Pin Count"}), package);
    addField("Connector Type", parameterValue(item, {"Connector Type"}));
    addField("Contact Type", parameterValue(item, {"Contact Type"}));
    addField("Rows", parameterValue(item, {"Rows", "Number of Rows"}));
    addField("Pitch", parameterValue(item, {"Pitch", "Pitch - Mating"}));
    addField("Mounting Type", parameterValue(item, {"Mounting Type"}));
    addField("Termination", parameterValue(item, {"Termination"}));
    addField("Fastening Type", parameterValue(item, {"Fastening Type"}));
    addField("Shrouding", parameterValue(item, {"Shrouding"}));
    return fields;
  }

  if (categoryContains(item, {"mcu", "microcontroller"})) {
    addValueAndPackage("Flash", parameterValue(item, {"Flash"}), package);
    addField("Core", parameterValue(item, {"Core", "Core Processor"}));
    addField("Operating Voltage", parameterValue(item, {"Operating Voltage", "Voltage - Supply", "Voltage"}));
    addField("RAM", parameterValue(item, {"RAM", "Memory"}));
    addField("Clock", parameterValue(item, {"Clock Speed", "Speed"}));
    addField("I/O", parameterValue(item, {"Number of I/O"}));
    addField("Program Memory", parameterValue(item, {"Program Memory Size", "Program Memory Type"}));
    addField("EEPROM", parameterValue(item, {"EEPROM Size"}));
    addField("Connectivity", parameterValue(item, {"Connectivity"}));
    addField("Peripherals", parameterValue(item, {"Peripherals"}));
    return fields;
  }

  if (categoryContains(item, {"regulator", "voltage regulator", "power management"})) {
    addValueAndPackage("Output Voltage", parameterValue(item, {"Output Voltage", "Voltage - Output", "Vout"}), package);
    addField("Input Voltage", parameterValue(item, {"Voltage - Input", "Vin"}));
    addField("Current", parameterValue(item, {"Output Current", "Current - Output", "Iout"}));
    addField("Type", parameterValue(item, {"Type", "Output Type"}));
    return fields;
  }

  if (categoryContains(item, {"crystal", "oscillator", "resonator"})) {
    addValueAndPackage("Frequency", parameterValue(item, {"Frequency"}), package);
    addField("Load Capacitance", parameterValue(item, {"Load Capacitance"}));
    addField("ESR", parameterValue(item, {"ESR", "Equivalent Series Resistance"}));
    addField("Operating Mode", parameterValue(item, {"Operating Mode"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Size / Dimension", parameterValue(item, {"Size / Dimension"}));
    return fields;
  }

  if (categoryContains(item, {"sensor", "temperature sensor", "pressure sensor"})) {
    addValueAndPackage("Type", parameterValue(item, {"Type"}), package);
    addField("Sensor Type", parameterValue(item, {"Sensor Type"}));
    addField("Output", parameterValue(item, {"Output", "Output Type"}));
    addField("Voltage - Supply", parameterValue(item, {"Voltage - Supply"}));
    addField("Resolution", parameterValue(item, {"Resolution"}));
    addField("Accuracy", parameterValue(item, {"Accuracy - Highest (Lowest)"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Features", parameterValue(item, {"Features"}));
    return fields;
  }

  if (categoryContains(item, {"circuit protection", "fuse", "tvs", "transient voltage suppressor"})) {
    addValueAndPackage("Current Rating", parameterValue(item, {"Current Rating (Amps)", "Current Rating", "Current"}), package);
    addField("Voltage AC", parameterValue(item, {"Voltage Rating - AC"}));
    addField("Voltage DC", parameterValue(item, {"Voltage Rating - DC"}));
    addField("Reverse Standoff", parameterValue(item, {"Voltage - Reverse Standoff (Typ)"}));
    addField("Breakdown", parameterValue(item, {"Voltage - Breakdown (Min)"}));
    addField("Clamping", parameterValue(item, {"Voltage - Clamping (Max) @ Ipp"}));
    addField("Peak Pulse Current", parameterValue(item, {"Current - Peak Pulse (10/1000µs)"}));
    addField("Peak Pulse Power", parameterValue(item, {"Power - Peak Pulse"}));
    addField("Response Time", parameterValue(item, {"Response Time"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    return fields;
  }

  optional<string> primaryValue;
  optional<string> primaryLabel;
  for (const auto& parameter : item.parameters) {
    const auto label = prettyLabel(parameter.name);
    if (normalizeKey(label) == "package") {
      continue;
    }
    if (!primaryValue) {
      primaryLabel = label;
      primaryValue = trim(parameter.value);
      continue;
    }
    addField(label, trim(parameter.value));
  }

  if (primaryValue && !primaryValue->empty()) {
    fields.insert(fields.begin(),
                  DetailField{primaryLabel.value_or("Value") + ": ", *primaryValue, uiLabelColor(), uiTitleColor()});
  } else if (package && !package->empty()) {
    fields.insert(fields.begin(), DetailField{"Package: ", *package, uiLabelColor(), uiTitleColor()});
  }

  return fields;
}

vector<DetailField> stockPreviewFields(const InventoryItem& item) {
  vector<DetailField> fields;
  fields.push_back({"Name: ", item.partName, uiInfoColor(), uiTitleColor()});
  fields.push_back({"Manufacturer: ", item.manufacturer, uiInfoColor(), uiTitleColor()});
  fields.push_back({"Quantity: ", to_string(item.quantity), uiSuccessColor(), uiTitleColor()});

  const auto electricalFields = electricalFieldsForItem(item);
  fields.insert(fields.end(), electricalFields.begin(), electricalFields.end());
  return fields;
}

vector<DetailField> detailCoreFields(const InventoryItem& item) {
  return {
      {"Name: ", item.partName, uiInfoColor(), uiTitleColor()},
      {"Manufacturer: ", item.manufacturer, uiInfoColor(), uiTitleColor()},
      {"Category: ", displayCategory(item.category), uiLabelColor(), uiTitleColor()},
      {"Quantity: ", to_string(item.quantity), uiSuccessColor(), uiTitleColor()},
      {"Threshold: ", to_string(item.reorderThreshold), uiWarnColor(), uiTitleColor()},
      {"Location: ", item.location, uiMutedColor(), uiTitleColor()},
  };
}

ftxui::Element detailFieldLine(const DetailField& field, int width) {
  const int valueWidth = max(0, width - static_cast<int>(field.label.size()) - 1);
  return ftxui::hbox({
             styledText(field.label, field.labelColor),
             styledText(ellipsize(field.value, static_cast<size_t>(valueWidth)), field.valueColor),
             ftxui::filler(),
         }) |
         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
}

bool upsertParameter(vector<Parameter>& parameters, const string& name, const string& value) {
  const auto trimmedValue = trim(value);
  if (trimmedValue.empty()) {
    return false;
  }

  for (auto& parameter : parameters) {
    if (parameterLabelMatches(parameter.name, name)) {
      if (parameter.name.empty()) {
        parameter.name = name;
      }
      const bool changed = parameter.value != trimmedValue;
      parameter.value = trimmedValue;
      return changed;
    }
  }

  parameters.push_back({name, trimmedValue});
  return true;
}

bool mergeDigiKeyMetadata(InventoryItem& item, const DigiKeyProductDetails& details) {
  bool changed = false;

  const auto normalizePackageLabels = [&]() {
    for (auto& parameter : item.parameters) {
      if (parameterLabelMatches(parameter.name, "Package") && looksLikePackagingValue(parameter.value)) {
        parameter.name = "Packaging";
        changed = true;
      }
    }
  };
  normalizePackageLabels();

  const auto assignIfUseful = [&](string& target, const string& value, bool replaceUnknown = false) {
    const auto trimmed = trim(value);
    if (trimmed.empty()) {
      return;
    }
    if (target.empty() || (replaceUnknown && (target == "Unknown" || target == "Scanned DigiKey Item"))) {
      target = trimmed;
      changed = true;
    }
  };

  if ((item.partName.empty() || item.partName == "Scanned DigiKey Item") && !trim(details.productDescription).empty()) {
    item.partName = trim(details.productDescription);
    changed = true;
  }

  assignIfUseful(item.manufacturer, details.manufacturerName, true);
  assignIfUseful(item.sku, details.manufacturerPartNumber);
  assignIfUseful(item.productUrl, details.productUrl);
  assignIfUseful(item.datasheetUrl, details.datasheetUrl);

  for (const auto& parameter : details.parameters) {
    if (upsertParameter(item.parameters, parameter.name, parameter.value)) {
      changed = true;
    }
  }

  if (!trim(details.packagingType).empty()) {
    if (upsertParameter(item.parameters, "Packaging", details.packagingType)) {
      changed = true;
    }
  }

  if (!details.packageName.empty()) {
    if (upsertParameter(item.parameters, "Package", details.packageName)) {
      changed = true;
    }
  }

  if (item.syncStatus != "synced") {
    item.syncStatus = "synced";
    changed = true;
  }

  if (changed) {
    item.lastUpdated = time(nullptr);
  }

  return changed;
}

filesystem::path documentsHimsPath() {
  if (const char* profile = getenv("USERPROFILE"); profile != nullptr && *profile != '\0') {
    return filesystem::path(profile) / "Documents" / "HIMS";
  }
  return filesystem::current_path() / "Documents" / "HIMS";
}

filesystem::path legacyDatabasePath() {
  const auto githubRoot = filesystem::current_path().parent_path().parent_path();
  return githubRoot / "Kwiatens Stock Management System" / "KwiatensStockManagementSystem" / "data" / "kwiatens-stock.db";
}

void copyDatabaseSidecar(const filesystem::path& sourceBase, const filesystem::path& destinationBase,
                         const string& suffix) {
  const auto source = filesystem::path(sourceBase.string() + suffix);
  const auto destination = filesystem::path(destinationBase.string() + suffix);
  error_code error;
  if (filesystem::exists(source, error)) {
    filesystem::copy_file(source, destination, filesystem::copy_options::overwrite_existing, error);
  }
}

void ensureInventoryDatabaseCopied(const filesystem::path& localBase) {
  error_code error;
  if (filesystem::exists(localBase, error)) {
    return;
  }

  const auto sourceBase = legacyDatabasePath();
  if (!filesystem::exists(sourceBase, error)) {
    return;
  }

  filesystem::create_directories(localBase.parent_path(), error);
  filesystem::copy_file(sourceBase, localBase, filesystem::copy_options::overwrite_existing, error);
  copyDatabaseSidecar(sourceBase, localBase, "-wal");
  copyDatabaseSidecar(sourceBase, localBase, "-shm");
}

filesystem::path locateDotEnvFile() {
  error_code error;
  auto current = filesystem::current_path();
  for (int depth = 0; depth < 8 && !current.empty(); ++depth) {
    const auto candidate = current / ".env";
    if (filesystem::exists(candidate, error)) {
      return candidate;
    }
    const auto parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return {};
}

}  // namespace

App::App()
    : root_(filesystem::current_path()),
      dataPath_(documentsHimsPath()),
      inventoryPath_(dataPath_ / "inventory.db"),
      printerPath_(dataPath_ / "printer.conf"),
      activityPath_(dataPath_ / "activity.tsv") {
  loadEnvironmentFile(locateDotEnvFile());
  ensureInventoryDatabaseCopied(inventoryPath_);
  loadState();

  if (!server_.start(8080, filesystem::current_path() / "scanner.html",
                     [this](const string& code) { pushScanCode(code); })) {
    setMessage("Scanner server failed to start; terminal still works", 5);
  } else {
    setMessage("Scanner ready at " + scannerUrl(), 5);
  }
}

void App::loadState() {
  store_.load(inventoryPath_);
  loadActivities(activityPath_, activities_);
  printerService_.loadConfig(printerPath_);
  refreshPrinterState();
  if (activities_.empty()) {
    activities_.push_back(makeActivity("system", "Inventory loaded"));
    activities_.push_back(makeActivity("system", "Terminal dashboard initialized"));
  }
  // DigiKey metadata is fetched on demand during scan-driven workflows, not at startup.

  server_.setRecentActivity(activities_);
  saveState();
}

void App::saveState() {
  ensureInventoryIdentifiers(store_.items());
  store_.save(inventoryPath_);
  printerService_.saveConfig(printerPath_);
  saveActivities(activityPath_, activities_);
}

ftxui::Element App::renderUi() const {
  ftxui::Elements body;
  body.push_back(fullLine("HIMS Terminal  " + summaryLine(), uiTitleColor(), uiPanelLeftBg()));
  body.push_back(ftxui::separator());
  body.push_back(renderPageUi() | ftxui::flex);
  body.push_back(ftxui::separator());
  body.push_back(renderSearchBarUi());
  body.push_back(renderMessageUi());
  body.push_back(renderStatusBarUi());
  return ftxui::vbox(move(body));
}

ftxui::Element App::renderPageUi() const {
  switch (page_) {
    case Page::Dashboard:
      return renderDashboardUi();
    case Page::Stock:
      return renderStockUi();
    case Page::Detail:
      return renderDetailUi();
    case Page::PrinterSetup:
      return renderPrinterSetupUi();
    case Page::ImportCsv:
      return renderImportCsvUi();
  }

  return ftxui::text("");
}

#if 0
// Page implementations now live in src/ui/pages/.
ftxui::Element App::renderDashboardUi() const {
  const auto summary = summarize(store_.items());

  ftxui::Elements leftBody;
  leftBody.push_back(fullLine("Summary", uiAccentColor(), uiPanelLeftBg()));
  leftBody.push_back(bulletLine("Parts: ", to_string(summary.itemCount), uiInfoColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Units: ", to_string(summary.totalUnits), uiSuccessColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Low stock: ", to_string(summary.lowStockCount), uiWarnColor(), uiTitleColor()));
  leftBody.push_back(
      bulletLine("Missing metadata: ", to_string(summary.missingMetadataCount), uiDangerColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Unsynced: ", to_string(summary.unsyncedCount), uiLabelColor(), uiTitleColor()));
  leftBody.push_back(ftxui::separator());
  leftBody.push_back(fullLine("Scanner", uiTitleColor(), uiPanelLeftBg()));
  leftBody.push_back(ftxui::paragraphAlignLeft(scannerUrl()) | ftxui::color(uiLinkColor()));
  leftBody.push_back(ftxui::separator());
  leftBody.push_back(fullLine("Inventory file", uiTitleColor(), uiPanelLeftBg()));
  leftBody.push_back(ftxui::paragraphAlignLeft(inventoryPath_.string()) | ftxui::color(uiInfoColor()));
  leftBody.push_back(ftxui::separator());
  leftBody.push_back(fullLine("Quick actions", uiAccentColor(), uiPanelLeftBg()));
  leftBody.push_back(
      ftxui::paragraphAlignLeft("1 Stock browser   2/s Scanner   3 Add item   4 Reload   / Search   q Quit") |
      ftxui::color(uiDimColor()));

  ftxui::Elements rightBody;
  rightBody.push_back(fullLine("Low-stock alerts", uiWarnColor(), uiPanelRightBg()));
  int alertCount = 0;
  for (const auto& item : store_.items()) {
    if (!item.lowStock()) {
      continue;
    }
    rightBody.push_back(fullLine("  - " + item.partName + " qty " + to_string(item.quantity) + " / " +
                                     to_string(item.reorderThreshold) + "  [" + item.category + "]",
                                 uiWarnColor(), uiRowLightBg()));
    if (++alertCount >= 6) {
      break;
    }
  }
  if (alertCount == 0) {
    rightBody.push_back(fullLine("  No low-stock items.", uiMutedColor(), uiPanelRightBg()));
  }

  rightBody.push_back(ftxui::separator());
  rightBody.push_back(fullLine("Recent activity", uiAccentColor(), uiPanelRightBg()));
  const auto activityCount = min<size_t>(activities_.size(), 8);
  for (size_t offset = 0; offset < activityCount; ++offset) {
    const auto& entry = activities_[activities_.size() - 1 - offset];
    const auto line = "  - " + nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message;
    ftxui::Color color = uiMutedColor();
    if (entry.kind.find("scan") != string::npos) {
      color = uiLinkColor();
    } else if (entry.kind.find("add") != string::npos) {
      color = uiSuccessColor();
    } else if (entry.kind.find("edit") != string::npos) {
      color = uiInfoColor();
    } else if (entry.kind == "system") {
      color = uiLabelColor();
    }
    rightBody.push_back(ftxui::paragraphAlignLeft(line) | ftxui::color(color));
  }
  if (activityCount == 0) {
    rightBody.push_back(fullLine("  No recent activity.", uiMutedColor(), uiPanelRightBg()));
  }

  auto leftPanel = panel("Overview", move(leftBody), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto rightPanel = panel("Activity", move(rightBody), uiWarnColor()) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;

  return ftxui::hbox({
      leftPanel,
      ftxui::separator(),
      rightPanel,
  });
}

ftxui::Element App::renderStockUi() const {
  const auto filtered = filteredIndices();
  const auto* activeScreen = ftxui::ScreenInteractive::Active();
  const int screenWidth = activeScreen != nullptr ? activeScreen->dimx() : 120;
  const int detailOuterWidth = clamp(screenWidth / 4, 38, 52);
  const int listOuterWidth = max(40, screenWidth - detailOuterWidth - 1);
  const int listInnerWidth = max(20, listOuterWidth - 2);
  const int detailInnerWidth = max(20, detailOuterWidth - 2);
  const int qtyWidth = 10;
  int partWidth = clamp(listInnerWidth / 3, 22, 34);
  int categoryWidth = listInnerWidth - partWidth - qtyWidth - 2;
  if (categoryWidth < 12) {
    categoryWidth = 12;
    partWidth = max(18, listInnerWidth - categoryWidth - qtyWidth - 2);
  }
  if (partWidth < 18) {
    partWidth = 18;
  }

  auto fixedCell = [](const string& text, int width, ftxui::Color color) {
    return ftxui::hbox({
               styledText(ellipsize(text, static_cast<size_t>(max(width, 0))), color),
               ftxui::filler(),
           }) |
           ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
  };

  ftxui::Elements listRows;
  listRows.push_back(ftxui::hbox({
                         fixedCell("Part", partWidth, uiMutedColor()),
                         ftxui::separator() | ftxui::color(uiDimColor()),
                         fixedCell("Category", categoryWidth, uiMutedColor()),
                         ftxui::filler(),
                         ftxui::hbox({
                             ftxui::filler(),
                             styledText("Qty", uiMutedColor()),
                         }) |
                             ftxui::size(ftxui::WIDTH, ftxui::EQUAL, qtyWidth),
                     }) |
                     ftxui::bgcolor(uiPanelLeftBg()));

  if (filtered.empty()) {
    listRows.push_back(fullLine("No items match \"" + searchQuery_ + "\".", uiMutedColor(), uiPanelLeftBg()));
  } else {
    for (size_t index = 0; index < filtered.size(); ++index) {
      const auto& item = store_.items()[filtered[index]];
      const bool selected = index == selectedPosition_;
      const bool lowStock = item.lowStock();
      const auto bg = selected ? uiRowSelectedBg() : (index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg());
      const auto fg = selected ? uiTitleColor() : (lowStock ? uiWarnColor() : uiMutedColor());
      const auto category = displayCategory(item.category);
      auto row = ftxui::hbox({
                     fixedCell("  " + item.partName, partWidth, fg),
                     ftxui::separator() | ftxui::color(uiDimColor()),
                     fixedCell(category, categoryWidth, selected ? uiTitleColor() : uiLabelColor()),
                     ftxui::filler(),
                     ftxui::hbox({
                         ftxui::filler(),
                         quantityBadge(item.quantity, selected),
                     }) |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, qtyWidth),
                 }) |
                 ftxui::bgcolor(bg);
      if (selected) {
        row = row | ftxui::select;
      }
      listRows.push_back(row);
    }
  }

  ftxui::Elements detailRows;
  if (const auto* item = selectedItem()) {
    detailRows.push_back(fullLine("Part summary", uiAccentColor(), uiPanelRightBg()));
    const auto electricalFields = electricalFieldsForItem(*item);
    const auto previewFields = stockPreviewFields(*item);
    for (size_t index = 0; index < previewFields.size(); ++index) {
      const auto& field = previewFields[index];
      if (index == 3 && !electricalFields.empty()) {
        detailRows.push_back(ftxui::separator());
        detailRows.push_back(fullLine("Electrical parameters", uiAccentColor(), uiPanelRightBg()));
      }
      detailRows.push_back(detailFieldLine(field, detailInnerWidth));
    }
  } else {
    detailRows.push_back(fullLine("No item selected.", uiMutedColor(), uiPanelRightBg()));
  }

  auto listContent = ftxui::vbox(move(listRows)) | ftxui::yframe | ftxui::vscroll_indicator;
  auto listPanel = ftxui::window(styledText("Items", uiAccentColor()), move(listContent)) |
                   ftxui::bgcolor(uiPanelLeftBg()) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, listOuterWidth);
  auto detailPanel = panel("Detail", move(detailRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) |
                     ftxui::size(ftxui::WIDTH, ftxui::EQUAL, detailOuterWidth);

  return ftxui::hbox({
      listPanel,
      ftxui::separator() | ftxui::color(uiDimColor()),
      detailPanel,
  });
}

ftxui::Element App::renderDetailUi() const {
  ftxui::Elements leftRows;
  ftxui::Elements rightRows;

  if (const auto* item = selectedItem()) {
    const auto electricalFields = electricalFieldsForItem(*item);
    leftRows.push_back(fullLine("Core details", uiAccentColor(), uiPanelLeftBg()));
    for (const auto& field : detailCoreFields(*item)) {
      leftRows.push_back(detailFieldLine(field, 40));
    }
    if (!electricalFields.empty()) {
      leftRows.push_back(ftxui::separator());
      leftRows.push_back(fullLine("Electrical parameters", uiAccentColor(), uiPanelLeftBg()));
      for (const auto& field : electricalFields) {
        leftRows.push_back(detailFieldLine(field, 40));
      }
    }
    if (!item->tags.empty()) {
      leftRows.push_back(ftxui::separator());
      leftRows.push_back(detailFieldLine({"Tags: ", renderTags(item->tags), uiLabelColor(), uiTitleColor()}, 40));
    }

    rightRows.push_back(fullLine("Metadata", uiAccentColor(), uiPanelRightBg()));
    rightRows.push_back(bulletLine("DigiKey: ", renderUrl(item->digikeyPartNumber), uiLinkColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("Datasheet: ", renderUrl(item->datasheetUrl), uiLinkColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("Product: ", renderUrl(item->productUrl), uiLinkColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("SKU: ", renderUrl(item->sku), uiLabelColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("Status: ", item->syncStatus, uiSuccessColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("Updated: ", nowTimestampString(item->lastUpdated), uiMutedColor(), uiTitleColor()));
    rightRows.push_back(ftxui::separator());
    rightRows.push_back(fullLine("Notes", uiWarnColor(), uiPanelRightBg()));
    rightRows.push_back(ftxui::paragraphAlignLeft(item->notes) | ftxui::color(uiMutedColor()));
  } else {
    leftRows.push_back(fullLine("No item selected.", uiMutedColor(), uiPanelLeftBg()));
    rightRows.push_back(fullLine("Press Esc to return to stock.", uiMutedColor(), uiPanelRightBg()));
  }

  auto leftPanel = panel("Item detail", move(leftRows), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto rightPanel = panel("Links and notes", move(rightRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;

  return ftxui::hbox({
      leftPanel,
      ftxui::separator(),
      rightPanel,
  });
}

#endif

ftxui::Element App::renderSearchBarUi() const {
  ftxui::Elements rows;
  if (inputMode_ == InputMode::Search) {
    rows.push_back(fullLine("Search  /" + inputBuffer_ + "_", uiLinkColor(), uiPanelLeftBg()));
  } else {
    rows.push_back(fullLine("Search  /" + searchQuery_, uiMutedColor(), uiPanelLeftBg()));
  }

  if (inputMode_ == InputMode::EditFieldMenu) {
    ftxui::Elements options;
    options.push_back(fullLine("Edit fields", uiAccentColor(), uiPanelLeftBg()));
    for (size_t index = 0; index < menuOptions_.size(); ++index) {
      const auto bg = static_cast<int>(index) == fieldMenuIndex_ ? uiRowSelectedBg() : (index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg());
      auto option = fullLine("  [" + to_string(index < 9 ? index + 1 : 0) + "] " + menuOptions_[index].label,
                             static_cast<int>(index) == fieldMenuIndex_ ? uiTitleColor() : uiMutedColor(), bg);
      if (static_cast<int>(index) == fieldMenuIndex_) {
        option = option | ftxui::select;
      }
      options.push_back(option);
    }
    rows.push_back(panel("Edit fields", move(options), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()));
  } else if (inputMode_ == InputMode::EditValue) {
    rows.push_back(fullLine("Input  " + activePrompt() + inputBuffer_ + "_", uiLinkColor(), uiPanelLeftBg()));
  }

  return ftxui::vbox(move(rows));
}

ftxui::Element App::renderStatusBarUi() const {
  return ftxui::hbox({
             styledText(scannerUrl(), uiLinkColor()),
             ftxui::filler(),
             styledText(ellipsize(printerSummary(), 44), uiAccentColor()),
             ftxui::filler(),
             styledText(shortcutSummary(), uiDimColor()),
         }) |
         ftxui::bgcolor(uiPanelRightBg());
}

ftxui::Element App::renderMessageUi() const {
  if (message_.empty()) {
    return ftxui::text("");
  }
  return fullLine(message_, uiAccentColor(), uiPanelLeftBg());
}

int App::run() {
  running_ = true;
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  screen.TrackMouse();
  auto renderer = ftxui::Renderer([this] { return renderUi(); });
  auto component = ftxui::CatchEvent(renderer, [this, &screen](ftxui::Event event) {
    if (event == ftxui::Event::Custom) {
      processScans();
      clearMessageIfExpired();
      clearDeleteConfirmationIfExpired();
      return true;
    }

    if (event.is_mouse()) {
      if (page_ == Page::Stock || page_ == Page::ImportCsv) {
        const auto& mouse = event.mouse();
        if (mouse.button == ftxui::Mouse::WheelUp) {
          page_ == Page::ImportCsv ? moveImportSelection(-1) : moveSelection(-1);
          return true;
        }
        if (mouse.button == ftxui::Mouse::WheelDown) {
          page_ == Page::ImportCsv ? moveImportSelection(1) : moveSelection(1);
          return true;
        }
      }
      return false;
    }

    const auto key = translateEvent(event);
    if (key.type == KeyType::Unknown && event != ftxui::Event::Escape) {
      return false;
    }

    handleKey(key);
    if (!running_) {
      screen.ExitLoopClosure()();
    }
    return true;
  });

  thread ticker([this, &screen] {
    while (running_) {
      screen.PostEvent(ftxui::Event::Custom);
      this_thread::sleep_for(chrono::milliseconds(100));
    }
  });

  screen.Loop(component);
  running_ = false;
  if (ticker.joinable()) {
    ticker.join();
  }
  saveState();
  server_.stop();
  return 0;
}

void App::processInput() {
  for (const auto& key : pollKeys()) {
    handleKey(key);
  }
}

void App::handleKey(const KeyEvent& key) {
  switch (inputMode_) {
    case InputMode::Search:
      handleSearchKey(key);
      return;
    case InputMode::EditFieldMenu:
      handleEditMenuKey(key);
      return;
    case InputMode::EditValue:
      handleEditValueKey(key);
      return;
    case InputMode::None:
      break;
  }

  switch (page_) {
    case Page::Dashboard:
      handleDashboardKey(key);
      break;
    case Page::Stock:
      handleStockKey(key);
      break;
    case Page::Detail:
      handleDetailKey(key);
      break;
    case Page::PrinterSetup:
      handlePrinterSetupKey(key);
      break;
    case Page::ImportCsv:
      handleImportCsvKey(key);
      break;
  }
}

#if 0
// Page-specific key handlers now live in src/ui/pages/.
void App::handleDashboardKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    switch (tolower(static_cast<unsigned char>(key.ch))) {
      case '1':
      case '\t':
        changePage(Page::Stock);
        break;
      case '2':
      case 's':
        openUrl(scannerUrl());
        setMessage("Opened scanner page in the default browser", 3);
        break;
      case '3':
        beginEditCurrentItem(true);
        break;
      case '4':
        store_.load(inventoryPath_);
        setMessage("Inventory reloaded", 2);
        break;
      case '/':
        startSearch();
        break;
      case 'q':
        running_ = false;
        break;
      default:
        break;
    }
    return;
  }

  if (key.type == KeyType::Enter || key.type == KeyType::Tab) {
    changePage(Page::Stock);
  }
}

void App::handleStockKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    const auto ch = tolower(static_cast<unsigned char>(key.ch));
    switch (ch) {
      case 'j':
        moveSelection(1);
        break;
      case 'k':
        moveSelection(-1);
        break;
      case '/':
        startSearch();
        break;
      case 'e':
        beginEditCurrentItem(false);
        break;
      case 'n':
        beginEditCurrentItem(true);
        break;
      case '+':
        adjustQuantity(1);
        break;
      case '-':
        adjustQuantity(-1);
        break;
      case 'd':
        if (const auto* item = selectedItem()) {
          openCurrentUrl(item->datasheetUrl, "datasheet");
        }
        break;
      case 'o':
        if (const auto* item = selectedItem()) {
          openCurrentUrl(item->productUrl, "product");
        }
        break;
      case 'g':
        if (const auto* item = selectedItem()) {
          const auto digiKeySearch = item->digikeyPartNumber.empty()
                                         ? string()
                                         : "https://www.digikey.com/en/products/result?keywords=" + item->digikeyPartNumber;
          openCurrentUrl(digiKeySearch, "DigiKey");
        }
        break;
      case 'r':
        store_.load(inventoryPath_);
        syncSelectionToFilter();
        setMessage("Inventory refreshed", 2);
        break;
      case 's':
        openUrl(scannerUrl());
        setMessage("Opened scanner page in the default browser", 3);
        break;
      case '\t':
        changePage(Page::Dashboard);
        break;
      default:
        break;
    }
    return;
  }

  if (key.type == KeyType::Up) {
    moveSelection(-1);
  } else if (key.type == KeyType::Down) {
    moveSelection(1);
  } else if (key.type == KeyType::PageUp) {
    moveSelection(-10);
  } else if (key.type == KeyType::PageDown) {
    moveSelection(10);
  } else if (key.type == KeyType::Home) {
    selectedPosition_ = 0;
    syncSelectionToFilter();
  } else if (key.type == KeyType::End) {
    const auto filtered = filteredIndices();
    if (!filtered.empty()) {
      selectedPosition_ = filtered.size() - 1;
      syncSelectionToFilter();
    }
  } else if (key.type == KeyType::Enter) {
    openSelectedDetail();
  } else if (key.type == KeyType::Escape) {
    changePage(Page::Dashboard);
  }
}

void App::handleDetailKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    const auto ch = tolower(static_cast<unsigned char>(key.ch));
    switch (ch) {
      case 'e':
        beginEditCurrentItem(false);
        break;
      case '+':
        adjustQuantity(1);
        break;
      case '-':
        adjustQuantity(-1);
        break;
      case 'd':
        if (const auto* item = selectedItem()) {
          openCurrentUrl(item->datasheetUrl, "datasheet");
        }
        break;
      case 'o':
        if (const auto* item = selectedItem()) {
          openCurrentUrl(item->productUrl, "product");
        }
        break;
      case 'g':
        if (const auto* item = selectedItem()) {
          const auto digiKeySearch = item->digikeyPartNumber.empty()
                                         ? string()
                                         : "https://www.digikey.com/en/products/result?keywords=" + item->digikeyPartNumber;
          openCurrentUrl(digiKeySearch, "DigiKey");
        }
        break;
      case 's':
        openUrl(scannerUrl());
        setMessage("Opened scanner page in the default browser", 3);
        break;
      case 'j':
        moveSelection(1);
        break;
      case 'k':
        moveSelection(-1);
        break;
      case '/':
        startSearch();
        break;
      default:
        break;
    }
    return;
  }

  if (key.type == KeyType::Escape) {
    changePage(Page::Stock);
  }
}

#endif

void App::handleSearchKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    inputBuffer_.push_back(key.ch);
    dirty_ = true;
    return;
  }

  if (key.type == KeyType::Backspace) {
    if (!inputBuffer_.empty()) {
      inputBuffer_.pop_back();
      dirty_ = true;
    }
    return;
  }

  if (key.type == KeyType::Enter) {
    searchQuery_ = inputBuffer_;
    inputMode_ = InputMode::None;
    syncSelectionToFilter();
    setMessage(searchQuery_.empty() ? "Filter cleared" : "Filter applied", 2);
    return;
  }

  if (key.type == KeyType::Escape) {
    inputBuffer_ = searchQuery_;
    inputMode_ = InputMode::None;
    setMessage("Search cancelled", 2);
  }
}

void App::handleEditMenuKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    const auto ch = tolower(static_cast<unsigned char>(key.ch));
    if (ch >= '1' && ch <= '9') {
      fieldMenuIndex_ = ch - '1';
      if (fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
        inputBuffer_ = currentFieldValue(menuOptions_[fieldMenuIndex_].field);
        inputMode_ = InputMode::EditValue;
        setMessage("Editing " + fieldLabel(menuOptions_[fieldMenuIndex_].field), 2);
      }
      return;
    }
    if (ch == '0' && menuOptions_.size() >= 10) {
      fieldMenuIndex_ = 9;
      inputBuffer_ = currentFieldValue(menuOptions_[fieldMenuIndex_].field);
      inputMode_ = InputMode::EditValue;
      setMessage("Editing " + fieldLabel(menuOptions_[fieldMenuIndex_].field), 2);
      return;
    }
    if (ch == 'q' || ch == 'c') {
      cancelInput();
    }
    return;
  }

  if (key.type == KeyType::Up) {
    fieldMenuIndex_ = max(0, fieldMenuIndex_ - 1);
    dirty_ = true;
  } else if (key.type == KeyType::Down) {
    fieldMenuIndex_ = min(fieldMenuIndex_ + 1, static_cast<int>(menuOptions_.size()) - 1);
    dirty_ = true;
  } else if (key.type == KeyType::Enter) {
    if (fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
      if (fieldMenuIndex_ == static_cast<int>(menuOptions_.size()) - 2) {
        saveWorkingCopy();
        return;
      }
      if (fieldMenuIndex_ == static_cast<int>(menuOptions_.size()) - 1) {
        cancelInput();
        return;
      }
      inputBuffer_ = currentFieldValue(menuOptions_[fieldMenuIndex_].field);
      inputMode_ = InputMode::EditValue;
      setMessage("Editing " + fieldLabel(menuOptions_[fieldMenuIndex_].field), 2);
    }
  } else if (key.type == KeyType::Escape) {
    cancelInput();
  }
}

void App::handleEditValueKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    inputBuffer_.push_back(key.ch);
    dirty_ = true;
    return;
  }

  if (key.type == KeyType::Backspace) {
    if (!inputBuffer_.empty()) {
      inputBuffer_.pop_back();
      dirty_ = true;
    }
    return;
  }

  if (key.type == KeyType::Enter) {
    if (fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
      commitEditField(menuOptions_[fieldMenuIndex_].field, inputBuffer_);
    }
    inputBuffer_.clear();
    inputMode_ = InputMode::EditFieldMenu;
    return;
  }

  if (key.type == KeyType::Escape) {
    inputBuffer_.clear();
    inputMode_ = InputMode::EditFieldMenu;
    setMessage("Edit cancelled", 2);
  }
}

void App::render() {
  const auto size = consoleSize();
  ostringstream out;
  out << "\x1b[?25l";
  out << kColorTitle << "HIMS Terminal" << kColorReset << "  ";
  out << kColorMuted << summaryLine() << kColorReset;
  out << "\n";
  out << string(max(0, size.columns), '=') << '\n';

  switch (page_) {
    case Page::Dashboard:
      renderDashboard(out, size);
      break;
    case Page::Stock:
      renderStock(out, size);
      break;
    case Page::Detail:
      renderDetail(out, size);
      break;
    case Page::PrinterSetup:
      out << kColorAccent << "Printer Setup" << kColorReset << "  ";
      out << kColorMuted << "Zebra queue selection" << kColorReset << '\n';
      out << kColorDim << printerSummary() << kColorReset << '\n';
      break;
    case Page::ImportCsv:
      renderImportCsv(out, size);
      break;
  }

  renderSearchBar(out, size);
  renderMessage(out, size);
  renderStatusBar(out, size);

  ostringstream screen;
  screen << "\x1b[H";

  int renderedLines = 0;
  const auto frame = out.str();
  for (char ch : frame) {
    screen << ch;
    if (ch == '\n') {
      screen << "\x1b[K";
      ++renderedLines;
    }
  }

  while (renderedLines < size.rows) {
    screen << "\x1b[K\n";
    ++renderedLines;
  }

  cout << screen.str() << flush;
}

#if 0
// Page implementations now live in src/ui/pages/.
void App::renderDashboard(ostringstream& out, const ConsoleSize& size) {
  const auto summary = summarize(store_.items());
  const int contentRows = max(6, size.rows - 6);

  out << kColorAccent << "Dashboard" << kColorReset << "  ";
  out << kColorMuted << "Overview, alerts, and activity" << kColorReset << '\n';
  out << string(max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    vector<string> lines;
    lines.push_back(styleText("Summary", kColorAccent));
    lines.push_back(styleText("  Parts: " + to_string(summary.itemCount), kColorInfo));
    lines.push_back(styleText("  Units: " + to_string(summary.totalUnits), kColorSuccess));
    lines.push_back(styleText("  Low stock: " + to_string(summary.lowStockCount), kColorWarn));
    lines.push_back(styleText("  Missing metadata: " + to_string(summary.missingMetadataCount), kColorDanger));
    lines.push_back(styleText("  Unsynced: " + to_string(summary.unsyncedCount), kColorLabel));
    lines.push_back("");
    lines.push_back(styleText("Scanner", kColorTitle));
    appendWrappedStyled(lines, scannerUrl(), size.columns, kColorLink);
    lines.push_back("");
    lines.push_back(styleText("Inventory file", kColorTitle));
    appendWrappedStyled(lines, inventoryPath_.string(), size.columns, kColorInfo);
    lines.push_back("");
    lines.push_back(styleText("Low-stock alerts", kColorWarn));
    int alertCount = 0;
    for (const auto& item : store_.items()) {
      if (!item.lowStock()) {
        continue;
      }
      appendWrappedStyled(lines,
                          "  - " + item.partName + " qty " + to_string(item.quantity) + " / " +
                              to_string(item.reorderThreshold) + "  [" + item.category + "]",
                          size.columns, kColorWarn);
      if (++alertCount >= 4) {
        break;
      }
    }
    if (alertCount == 0) {
      lines.push_back(styleText("  No low-stock items.", kColorMuted));
    }

    lines.push_back("");
    lines.push_back(styleText("Recent activity", kColorAccent));
    const auto activityCount = min<size_t>(activities_.size(), 5);
    for (size_t offset = 0; offset < activityCount; ++offset) {
      const auto& entry = activities_[activities_.size() - 1 - offset];
      const char* activityColor = kColorMuted;
      if (entry.kind.find("scan") != string::npos) {
        activityColor = kColorLink;
      } else if (entry.kind.find("add") != string::npos) {
        activityColor = kColorSuccess;
      } else if (entry.kind.find("edit") != string::npos) {
        activityColor = kColorInfo;
      } else if (entry.kind == "system") {
        activityColor = kColorLabel;
      }
      appendWrappedStyled(lines,
                          "  - " + nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message,
                          size.columns, activityColor);
    }
    if (activityCount == 0) {
      lines.push_back(styleText("  No recent activity.", kColorMuted));
    }

    lines.push_back("");
    lines.push_back(styleText("Quick actions", kColorAccent));
    appendWrappedStyled(lines, "  [1] Stock browser   [2/s] Scanner   [3] Add item   [4] Reload   [/] Search   [q] Quit", size.columns,
                        kColorDim);

    if (static_cast<int>(lines.size()) > contentRows) {
      lines.resize(static_cast<size_t>(contentRows));
    }
    for (const auto& line : lines) {
      out << line << '\n';
    }
    return;
  }

  const int leftWidth = max(36, (size.columns - 4) / 2);
  const int rightWidth = max(36, size.columns - leftWidth - 4);

  vector<string> leftLines;
  vector<string> rightLines;

  leftLines.push_back("Summary");
  leftLines.push_back("  Parts: " + to_string(summary.itemCount));
  leftLines.push_back("  Units: " + to_string(summary.totalUnits));
  leftLines.push_back("  Low stock: " + to_string(summary.lowStockCount));
  leftLines.push_back("  Missing metadata: " + to_string(summary.missingMetadataCount));
  leftLines.push_back("  Unsynced: " + to_string(summary.unsyncedCount));
  leftLines.push_back("");
  leftLines.push_back("Scanner");
  appendWrapped(leftLines, scannerUrl(), leftWidth);
  leftLines.push_back("");
  leftLines.push_back("Inventory file");
  appendWrapped(leftLines, inventoryPath_.string(), leftWidth);
  leftLines.push_back("");
  leftLines.push_back("Quick actions");
  appendWrapped(leftLines, "1 Stock browser   2/s Scanner   3 Add item   4 Reload   / Search   q Quit", leftWidth);

  rightLines.push_back("Low-stock alerts");
  int alertCount = 0;
  for (const auto& item : store_.items()) {
    if (!item.lowStock()) {
      continue;
    }
    appendWrapped(rightLines,
                  "  - " + item.partName + " qty " + to_string(item.quantity) + " / " +
                      to_string(item.reorderThreshold) + "  [" + item.category + "]",
                  rightWidth);
    if (++alertCount >= 6) {
      break;
    }
  }
  if (alertCount == 0) {
    rightLines.push_back("  No low-stock items.");
  }

  rightLines.push_back("");
  rightLines.push_back("Recent activity");
  const auto activityCount = min<size_t>(activities_.size(), 8);
  for (size_t offset = 0; offset < activityCount; ++offset) {
    const auto& entry = activities_[activities_.size() - 1 - offset];
    appendWrapped(rightLines,
                  "  - " + nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message,
                  rightWidth);
  }
  if (activityCount == 0) {
    rightLines.push_back("  No recent activity.");
  }

  renderColumns(out, leftLines, rightLines, leftWidth, rightWidth, contentRows, 4, kColorInfo, kBgPanelLeft,
                kColorMuted, kBgPanelRight);
}

void App::renderStock(ostringstream& out, const ConsoleSize& size) {
  const auto filtered = filteredIndices();
  const int contentRows = max(6, size.rows - 6);

  out << kColorAccent << "Stock browser" << kColorReset << "  ";
  out << kColorMuted << "(j/k or arrows to move, Enter detail, e edit, n new, +/- adjust)" << kColorReset << '\n';
  out << string(max(0, size.columns), '-') << '\n';

  struct StockLine {
    string text;
    bool isHeader = false;
    bool isMessage = false;
    bool isSelected = false;
    bool isLowStock = false;
    bool isEven = false;
  };

  const bool compact = size.columns < 96;
  const int listHeight = compact ? max(4, contentRows / 2) : max(4, contentRows - 1);
  const int listWidth = compact ? max(0, size.columns) : max(36, (size.columns - 4) / 3);
  const int detailWidth = compact ? max(40, size.columns - 4) : max(40, size.columns - listWidth - 4);
  const int partWidth = max(12, listWidth - 21);

  vector<StockLine> listLines;
  listLines.push_back({"Items", true});
  listLines.push_back({"Part                          Category       Qty", true});

  if (filtered.empty()) {
    listLines.push_back({"No items match \"" + searchQuery_ + "\".", false, true});
  } else {
    size_t visibleStart = 0;
    if (selectedPosition_ >= static_cast<size_t>(listHeight)) {
      visibleStart = selectedPosition_ - static_cast<size_t>(listHeight) + 1;
    }
    if (selectedPosition_ < visibleStart) {
      visibleStart = selectedPosition_;
    }

    stockScroll_ = visibleStart;

    for (int row = 0; row < listHeight; ++row) {
      const auto pos = visibleStart + static_cast<size_t>(row);
      if (pos >= filtered.size()) {
        break;
      }

      const auto& item = store_.items()[filtered[pos]];
      ostringstream rowText;
      rowText << (pos == selectedPosition_ ? '>' : ' ') << ' ' << padRight(item.partName, partWidth) << "  "
              << padRight(item.category, 11) << ' ' << setw(4) << item.quantity;
      listLines.push_back({rowText.str(), false, false, pos == selectedPosition_, item.lowStock(), row % 2 == 0});
    }
  }

  vector<StockLine> detailLines;
  detailLines.push_back({"Detail", true});
  if (const auto* item = selectedItem()) {
    const auto lines = splitLines(itemDetailText(*item, detailWidth));
    for (const auto& line : lines) {
      detailLines.push_back({line});
    }
  } else {
    detailLines.push_back({"No selection.", false, true});
  }

  auto styleListLine = [&](const StockLine& line) {
    if (line.isHeader) {
      return styleCell(line.text, listWidth, kColorAccent, kBgPanelLeft);
    }
    if (line.isMessage) {
      return styleCell(line.text, listWidth, kColorMuted, kBgPanelLeft);
    }
    if (line.isSelected) {
      return styleCell(line.text, listWidth, kColorTitle, kBgRowSelected);
    }
    const char* fg = line.isLowStock ? kColorWarn : nullptr;
    const char* bg = line.isEven ? kBgRowDark : kBgRowLight;
    return styleCell(line.text, listWidth, fg, bg);
  };

  auto styleDetailLine = [&](const StockLine& line) {
    if (line.isHeader) {
      return styleCell(line.text, detailWidth, kColorAccent, kBgPanelRight);
    }
    if (line.isMessage) {
      return styleCell(line.text, detailWidth, kColorMuted, kBgPanelRight);
    }
    if (line.text.rfind("Quantity: ", 0) == 0) {
      return styleCell(line.text, detailWidth, kColorSuccess, kBgPanelRight);
    }
    if (line.text.find(": ") != string::npos) {
      return styleCell(line.text, detailWidth, kColorInfo, kBgPanelRight);
    }
    return styleCell(line.text, detailWidth, kColorMuted, kBgPanelRight);
  };

  if (compact) {
    for (const auto& line : listLines) {
      out << styleListLine(line) << '\n';
    }
    out << '\n';
    for (const auto& line : detailLines) {
      out << styleDetailLine(line) << '\n';
    }
    return;
  }

  const size_t maxRows = max(listLines.size(), detailLines.size());
  for (size_t row = 0; row < maxRows; ++row) {
    const auto left = row < listLines.size() ? styleListLine(listLines[row]) : styleCell("", listWidth, nullptr, kBgPanelLeft);
    const auto right =
        row < detailLines.size() ? styleDetailLine(detailLines[row]) : styleCell("", detailWidth, nullptr, kBgPanelRight);
    out << left << "    " << right << '\n';
  }
}

void App::renderDetail(ostringstream& out, const ConsoleSize& size) {
  const int contentRows = max(6, size.rows - 6);

  out << kColorAccent << "Item detail" << kColorReset << "  " << kColorMuted << "(Esc back to stock)" << kColorReset
      << '\n';
  out << string(max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    if (const auto* item = selectedItem()) {
      vector<string> lines;
      const auto electricalFields = electricalFieldsForItem(*item);
      lines.push_back(styleText("Core details", kColorAccent));
      for (const auto& field : detailCoreFields(*item)) {
        lines.push_back(styleText("  " + field.label + field.value, kColorInfo));
      }
      if (!electricalFields.empty()) {
        lines.push_back("");
        lines.push_back(styleText("Electrical parameters", kColorAccent));
        for (const auto& field : electricalFields) {
          lines.push_back(styleText("  " + field.label + field.value, kColorInfo));
        }
      }
      if (!item->tags.empty()) {
        lines.push_back("");
        lines.push_back(styleText("Tags: " + renderTags(item->tags), kColorLabel));
      }
      lines.push_back("");
      lines.push_back(styleText("Metadata", kColorAccent));
      lines.push_back(styleText("  DigiKey: " + renderUrl(item->digikeyPartNumber), kColorLink));
      lines.push_back(styleText("  Datasheet: " + renderUrl(item->datasheetUrl), kColorLink));
      lines.push_back(styleText("  Product: " + renderUrl(item->productUrl), kColorLink));
      lines.push_back(styleText("  SKU: " + renderUrl(item->sku), kColorLabel));
      lines.push_back(styleText("  Status: " + item->syncStatus, kColorSuccess));
      lines.push_back(styleText("  Updated: " + nowTimestampString(item->lastUpdated), kColorMuted));
      lines.push_back("");
      lines.push_back(styleText("Notes", kColorWarn));
      appendWrappedStyled(lines, item->notes, max(50, size.columns - 4), kColorMuted);
      lines.push_back("");
      lines.push_back(styleText("Shortcuts", kColorAccent));
      appendWrappedStyled(lines, "e Edit   n New   + / - Quantity   Enter Open detail   / Search", max(50, size.columns - 4),
                          kColorDim);
      for (const auto& line : lines) {
        out << line << '\n';
      }
    } else {
      out << "No item selected.\n";
    }
    return;
  }

  const int rightWidth = max(46, min(size.columns - 4, static_cast<int>(size.columns * 0.42)));
  const int leftWidth = max(40, size.columns - rightWidth - 4);

  vector<string> leftLines;
  vector<string> rightLines;

  if (const auto* item = selectedItem()) {
    const auto electricalFields = electricalFieldsForItem(*item);
    leftLines.push_back("Core details");
    for (const auto& field : detailCoreFields(*item)) {
      leftLines.push_back(field.label + field.value);
    }
    if (!electricalFields.empty()) {
      leftLines.push_back("");
      leftLines.push_back("Electrical parameters");
      for (const auto& field : electricalFields) {
        leftLines.push_back(field.label + field.value);
      }
    }
    if (!item->tags.empty()) {
      leftLines.push_back("");
      leftLines.push_back("Tags: " + renderTags(item->tags));
    }

    rightLines.push_back("Metadata");
    rightLines.push_back("DigiKey: " + renderUrl(item->digikeyPartNumber));
    rightLines.push_back("Datasheet: " + renderUrl(item->datasheetUrl));
    rightLines.push_back("Product: " + renderUrl(item->productUrl));
    rightLines.push_back("SKU: " + renderUrl(item->sku));
    rightLines.push_back("Status: " + item->syncStatus);
    rightLines.push_back("Updated: " + nowTimestampString(item->lastUpdated));
    rightLines.push_back("");
    rightLines.push_back("Notes");
    appendWrapped(rightLines, item->notes, rightWidth);
    rightLines.push_back("");
    rightLines.push_back("Shortcuts");
    appendWrapped(rightLines, "e Edit   n New   + / - Quantity   Enter Open detail   / Search", rightWidth);
  } else {
    leftLines.push_back("No item selected.");
    rightLines.push_back("Press Esc to return to stock.");
  }

  renderColumns(out, leftLines, rightLines, leftWidth, rightWidth, contentRows, 4, kColorInfo, kBgPanelLeft,
                kColorMuted, kBgPanelRight);
}

#endif

void App::renderSearchBar(ostringstream& out, const ConsoleSize&) {
  out << '\n' << kColorAccent << "Search" << kColorReset << "  ";
  if (inputMode_ == InputMode::Search) {
    out << kColorLink << '/' << inputBuffer_ << '_' << kColorReset;
  } else {
    out << kColorMuted << '/' << searchQuery_ << kColorReset;
  }
  out << '\n';

  if (inputMode_ == InputMode::EditFieldMenu) {
    out << kColorAccent << "Edit fields" << kColorReset << '\n';
    for (size_t index = 0; index < menuOptions_.size(); ++index) {
      if (static_cast<int>(index) == fieldMenuIndex_) {
        out << kColorSelect;
      }
      out << "  [" << (index < 9 ? to_string(index + 1) : "0") << "] " << menuOptions_[index].label
          << kColorReset << '\n';
    }
  } else if (inputMode_ == InputMode::EditValue) {
    out << kColorAccent << "Input" << kColorReset << "  " << kColorLabel << activePrompt() << kColorReset
        << kColorLink << inputBuffer_ << '_' << kColorReset << '\n';
  }
}

void App::renderStatusBar(ostringstream& out, const ConsoleSize& size) {
  out << string(size.columns, '-') << '\n';
  out << kColorLink << scannerUrl() << kColorReset << "  ";
  out << kColorAccent << ellipsize(printerSummary(), max(24, size.columns / 3)) << kColorReset << "  ";
  out << kColorDim << shortcutSummary() << kColorReset << '\n';
}

void App::renderMessage(ostringstream& out, const ConsoleSize&) {
  if (!message_.empty()) {
    out << kColorAccent << message_ << kColorReset << '\n';
  }
}

void App::setMessage(string text, int seconds) {
  message_ = move(text);
  messageUntil_ = time(nullptr) + seconds;
  dirty_ = true;
}

bool App::messageVisible() const {
  return !message_.empty() && time(nullptr) <= messageUntil_;
}

void App::clearMessageIfExpired() {
  if (!messageVisible() && !message_.empty()) {
    message_.clear();
    dirty_ = true;
  }
}

void App::markDirty() {
  dirty_ = true;
}

vector<size_t> App::filteredIndices() const {
  return filterItems(store_.items(), searchQuery_);
}

size_t App::selectedIndex() const {
  const auto filtered = filteredIndices();
  if (filtered.empty()) {
    return numeric_limits<size_t>::max();
  }
  const auto position = min(selectedPosition_, filtered.size() - 1);
  return filtered[position];
}

InventoryItem* App::selectedItem() {
  const auto index = selectedIndex();
  if (index == numeric_limits<size_t>::max()) {
    return nullptr;
  }
  return &store_.items()[index];
}

const InventoryItem* App::selectedItem() const {
  const auto index = selectedIndex();
  if (index == numeric_limits<size_t>::max()) {
    return nullptr;
  }
  return &store_.items()[index];
}

void App::syncSelectionToFilter() {
  const auto filtered = filteredIndices();
  if (filtered.empty()) {
    selectedPosition_ = 0;
    return;
  }
  if (selectedPosition_ >= filtered.size()) {
    selectedPosition_ = filtered.size() - 1;
  }
  dirty_ = true;
}

void App::moveSelection(int delta) {
  const auto filtered = filteredIndices();
  if (filtered.empty()) {
    selectedPosition_ = 0;
    return;
  }

  const auto current = static_cast<int>(min(selectedPosition_, filtered.size() - 1));
  const auto next = clamp(current + delta, 0, static_cast<int>(filtered.size() - 1));
  selectedPosition_ = static_cast<size_t>(next);
  dirty_ = true;
}

bool App::deleteConfirmationActive() const {
  return !deleteConfirmationItemId_.empty();
}

bool App::deleteConfirmationReady() const {
  return deleteConfirmationActive() && time(nullptr) >= deleteConfirmationUntil_;
}

int App::deleteConfirmationSecondsLeft() const {
  if (!deleteConfirmationActive()) {
    return 0;
  }
  return max(0, static_cast<int>(deleteConfirmationUntil_ - time(nullptr)));
}

void App::armDeleteConfirmation() {
  const auto* item = selectedItem();
  if (item == nullptr) {
    setMessage("No item selected", 2);
    return;
  }

  deleteConfirmationItemId_ = item->id;
  deleteConfirmationUntil_ = time(nullptr) + 3;
  dirty_ = true;
}

void App::cancelDeleteConfirmation() {
  if (deleteConfirmationItemId_.empty()) {
    return;
  }

  deleteConfirmationItemId_.clear();
  deleteConfirmationUntil_ = 0;
  dirty_ = true;
}

void App::clearDeleteConfirmationIfExpired() {
  // Keep the confirmation popup visible after the countdown reaches zero.
}

void App::confirmDeleteSelectedItem() {
  if (!deleteConfirmationActive()) {
    setMessage("Press Ctrl+Backspace first to arm delete", 2);
    return;
  }

  if (!deleteConfirmationReady()) {
    setMessage("Wait " + to_string(deleteConfirmationSecondsLeft()) + " more second" +
                   (deleteConfirmationSecondsLeft() == 1 ? string() : string("s")) + " to confirm delete",
               2);
    return;
  }

  const auto it = find_if(store_.items().begin(), store_.items().end(), [&](const InventoryItem& item) {
    return item.id == deleteConfirmationItemId_;
  });
  if (it == store_.items().end()) {
    cancelDeleteConfirmation();
    setMessage("Item no longer available", 2);
    return;
  }

  const auto itemName = it->partName;
  store_.items().erase(it);
  cancelDeleteConfirmation();
  logActivity("delete", itemName + " deleted");
  saveState();
  syncSelectionToFilter();
  page_ = Page::Stock;
  setMessage(itemName + " deleted", 2);
}

void App::changePage(Page page) {
  page_ = page;
  inputMode_ = InputMode::None;
  cancelDeleteConfirmation();
  dirty_ = true;
}

void App::openSelectedDetail() {
  if (selectedItem() != nullptr) {
    page_ = Page::Detail;
    dirty_ = true;
  }
}

void App::startSearch() {
  page_ = Page::Stock;
  inputMode_ = InputMode::Search;
  inputBuffer_ = searchQuery_;
  setMessage("Type a keyword, category, tag, parameter, or qty filter", 3);
}

void App::cancelInput() {
  inputMode_ = InputMode::None;
  inputBuffer_.clear();
  dirty_ = true;
}

void App::beginEditCurrentItem(bool createNew) {
  editingImportCandidate_ = false;
  page_ = Page::Stock;
  workingCopy_ = {};
  if (createNew) {
    workingCopy_.isNew = true;
    workingCopy_.item.id = makeId();
    workingCopy_.item.partName = "New Part";
    workingCopy_.item.manufacturer = "Unknown";
    workingCopy_.item.category = "Unsorted";
    workingCopy_.item.location = "Unassigned";
    workingCopy_.item.syncStatus = "needs_metadata";
    workingCopy_.item.lastUpdated = time(nullptr);
    workingCopy_.item.createdAt = workingCopy_.item.lastUpdated;
    workingCopy_.originalIndex = store_.items().size();
  } else {
    const auto* current = selectedItem();
    if (current == nullptr) {
      setMessage("No item selected", 2);
      return;
    }
    workingCopy_.isNew = false;
    workingCopy_.item = *current;
    workingCopy_.originalIndex = selectedIndex();
  }

  menuOptions_ = fieldOptions();
  fieldMenuIndex_ = 0;
  inputMode_ = InputMode::EditFieldMenu;
  setMessage("Choose a field to edit", 3);
}

void App::beginEditImportCandidate() {
  auto* candidate = currentImportCandidate();
  if (candidate == nullptr) {
    setMessage("No import row selected", 2);
    return;
  }

  editingImportCandidate_ = true;
  importEditIndex_ = importSelection_;
  workingCopy_ = {};
  workingCopy_.item = candidate->item;
  workingCopy_.originalIndex = importSelection_;
  menuOptions_ = fieldOptions();
  fieldMenuIndex_ = 0;
  inputMode_ = InputMode::EditFieldMenu;
  page_ = Page::ImportCsv;
  setMessage("Choose a field to edit for this import row", 3);
}

void App::openFieldMenu() {
  menuOptions_ = fieldOptions();
  fieldMenuIndex_ = 0;
  inputMode_ = InputMode::EditFieldMenu;
}

void App::commitEditField(EditField field, const string& value) {
  const auto trimmed = trim(value);
  bool valid = true;

  switch (field) {
    case EditField::PartName:
      workingCopy_.item.partName = trimmed;
      break;
    case EditField::Manufacturer:
      workingCopy_.item.manufacturer = trimmed;
      break;
    case EditField::Category:
      workingCopy_.item.category = trimmed;
      break;
    case EditField::Quantity:
      try {
        workingCopy_.item.quantity = max(0, stoi(trimmed));
      } catch (...) {
        valid = false;
      }
      break;
    case EditField::ReorderThreshold:
      try {
        workingCopy_.item.reorderThreshold = max(0, stoi(trimmed));
      } catch (...) {
        valid = false;
      }
      break;
    case EditField::Location:
      workingCopy_.item.location = trimmed;
      break;
    case EditField::Tags:
      workingCopy_.item.tags = splitFlexible(trimmed);
      break;
    case EditField::Parameters:
      workingCopy_.item.parameters = parseParameters(trimmed);
      break;
    case EditField::Notes:
      workingCopy_.item.notes = trimmed;
      break;
    case EditField::DigiKeyPart:
      workingCopy_.item.digikeyPartNumber = trimmed;
      break;
    case EditField::DatasheetUrl:
      workingCopy_.item.datasheetUrl = trimmed;
      break;
    case EditField::ProductUrl:
      workingCopy_.item.productUrl = trimmed;
      break;
    case EditField::Sku:
      workingCopy_.item.sku = trimmed;
      break;
    case EditField::SyncStatus:
      workingCopy_.item.syncStatus = toLower(trimmed);
      break;
  }

  if (!valid) {
    setMessage("Invalid numeric value", 3);
    return;
  }

  workingCopy_.item.lastUpdated = time(nullptr);
  setMessage(fieldLabel(field) + " updated", 2);
  inputBuffer_.clear();
  inputMode_ = InputMode::EditFieldMenu;
  dirty_ = true;
}

void App::saveWorkingCopy() {
  if (editingImportCandidate_) {
    if (importEditIndex_ < importCandidates_.size()) {
      importCandidates_[importEditIndex_].item = workingCopy_.item;
    }

    editingImportCandidate_ = false;
    inputMode_ = InputMode::None;
    page_ = Page::ImportCsv;
    setMessage("Import row updated", 2);
    dirty_ = true;
    return;
  }

  if (workingCopy_.isNew) {
    store_.items().push_back(workingCopy_.item);
    selectedPosition_ = store_.items().empty() ? 0 : store_.items().size() - 1;
  } else if (workingCopy_.originalIndex < store_.items().size()) {
    store_.items()[workingCopy_.originalIndex] = workingCopy_.item;
  }

  logActivity("edit", workingCopy_.item.partName + " updated");
  saveState();
  inputMode_ = InputMode::None;
  page_ = Page::Stock;
  syncSelectionToFilter();
  setMessage("Changes saved", 2);
}

void App::adjustQuantity(int delta) {
  auto* item = selectedItem();
  if (item == nullptr) {
    setMessage("No item selected", 2);
    return;
  }

  item->quantity = max(0, item->quantity + delta);
  item->lastUpdated = time(nullptr);
  logActivity(delta > 0 ? "stock" : "usage", item->partName + " quantity changed to " + to_string(item->quantity));
  saveState();
  setMessage(item->partName + " quantity is now " + to_string(item->quantity), 2);
  dirty_ = true;
}

void App::logActivity(const string& kind, const string& message) {
  appendActivity(activities_, makeActivity(kind, message));
  server_.setRecentActivity(activities_);
  saveActivities(activityPath_, activities_);
  dirty_ = true;
}

void App::pushScanCode(const string& code) {
  lock_guard<mutex> lock(scanMutex_);
  scanQueue_.push_back(code);
}

void App::processScans() {
  vector<string> pending;
  {
    lock_guard<mutex> lock(scanMutex_);
    pending.swap(scanQueue_);
  }

  for (const auto& code : pending) {
    const auto resolution = resolveScanCode(store_, code);
    if (resolution.matched) {
      if (resolution.created) {
        logActivity("scan", "Created item from code " + code);
      } else {
        logActivity("scan", "Matched existing item with code " + code);
      }

      if (const auto* item = store_.findById(resolution.itemId)) {
        const auto it = find_if(store_.items().begin(), store_.items().end(), [&](const InventoryItem& entry) {
          return entry.id == item->id;
        });
        if (it != store_.items().end()) {
          selectedPosition_ = static_cast<size_t>(distance(store_.items().begin(), it));
        }
      }

      saveState();
      changePage(Page::Detail);
      setMessage(resolution.message, 3);
    } else {
      setMessage("Scan ignored: " + resolution.message, 3);
    }
    syncSelectionToFilter();
  }
}

void App::beginCsvImport() {
  filesystem::path selectedPath;
  if (!openCsvFileDialog(selectedPath)) {
    setMessage("CSV import cancelled", 2);
    return;
  }

  const auto result = loadDigiKeyCsvFile(selectedPath, store_.items());
  if (!result.ok) {
    setMessage("CSV import failed: " + result.error, 6);
    return;
  }

  importCandidates_ = result.candidates;
  importAcceptedItemIds_.clear();
  importSourcePath_ = selectedPath;
  importSelection_ = 0;
  importSyncPrompt_ = false;
  importCreatedCount_ = 0;
  importMergedCount_ = 0;
  importSkippedCount_ = 0;
  importSyncedCount_ = 0;
  importSyncFailedCount_ = 0;
  editingImportCandidate_ = false;
  inputMode_ = InputMode::None;
  page_ = Page::ImportCsv;

  setMessage("Loaded " + to_string(importCandidates_.size()) + " CSV rows for review", 4);
}

CsvImportCandidate* App::currentImportCandidate() {
  if (importCandidates_.empty()) {
    return nullptr;
  }
  importSelection_ = min(importSelection_, importCandidates_.size() - 1);
  return &importCandidates_[importSelection_];
}

const CsvImportCandidate* App::currentImportCandidate() const {
  if (importCandidates_.empty()) {
    return nullptr;
  }
  const auto index = min(importSelection_, importCandidates_.size() - 1);
  return &importCandidates_[index];
}

void App::moveImportSelection(int delta) {
  if (importCandidates_.empty()) {
    importSelection_ = 0;
    dirty_ = true;
    return;
  }

  const auto current = static_cast<int>(min(importSelection_, importCandidates_.size() - 1));
  importSelection_ = static_cast<size_t>(clamp(current + delta, 0, static_cast<int>(importCandidates_.size() - 1)));
  dirty_ = true;
}

void App::acceptImportCandidate() {
  auto* candidate = currentImportCandidate();
  if (candidate == nullptr) {
    finishImportReview();
    return;
  }

  string acceptedId;
  if (candidate->hasConflict) {
    auto* existing = store_.findById(candidate->existingItemId);
    if (existing != nullptr) {
      existing->quantity = max(0, existing->quantity + candidate->item.quantity);
      existing->lastUpdated = time(nullptr);
      mergeImportedMetadata(*existing, candidate->item);
      acceptedId = existing->id;
      ++importMergedCount_;
    }
  }

  if (acceptedId.empty()) {
    store_.items().push_back(candidate->item);
    acceptedId = store_.items().back().id;
    ++importCreatedCount_;
  }

  importAcceptedItemIds_.push_back(acceptedId);
  importCandidates_.erase(importCandidates_.begin() + static_cast<ptrdiff_t>(importSelection_));
  if (importSelection_ >= importCandidates_.size() && !importCandidates_.empty()) {
    importSelection_ = importCandidates_.size() - 1;
  }

  saveState();
  if (importCandidates_.empty()) {
    finishImportReview();
  } else {
    setMessage("Accepted import row", 2);
  }
  dirty_ = true;
}

void App::skipImportCandidate() {
  if (importCandidates_.empty()) {
    finishImportReview();
    return;
  }

  importCandidates_.erase(importCandidates_.begin() + static_cast<ptrdiff_t>(importSelection_));
  ++importSkippedCount_;
  if (importSelection_ >= importCandidates_.size() && !importCandidates_.empty()) {
    importSelection_ = importCandidates_.size() - 1;
  }

  if (importCandidates_.empty()) {
    finishImportReview();
  } else {
    setMessage("Skipped import row", 2);
  }
  dirty_ = true;
}

void App::finishImportReview() {
  importSyncPrompt_ = true;
  inputMode_ = InputMode::None;
  page_ = Page::ImportCsv;
  setMessage("Sync accepted parts with DigiKey API? Highly recommended.", 8);
}

void App::syncAcceptedImports() {
  const auto config = loadDigiKeyConfig();
  unordered_set<string> uniqueIds(importAcceptedItemIds_.begin(), importAcceptedItemIds_.end());
  if (!config.valid()) {
    importSyncFailedCount_ += static_cast<int>(uniqueIds.size());
    return;
  }

  DigiKeyApiClient client(config);
  for (const auto& id : uniqueIds) {
    auto* item = store_.findById(id);
    if (item == nullptr) {
      continue;
    }

    auto lookup = trim(item->digikeyPartNumber);
    if (lookup.empty()) {
      lookup = trim(item->sku);
    }
    if (lookup.empty()) {
      ++importSyncFailedCount_;
      continue;
    }

    string error;
    auto details = client.fetchProductDetails(lookup, &error);
    if (!details) {
      ++importSyncFailedCount_;
      continue;
    }

    mergeDigiKeyMetadata(*item, *details);
    ++importSyncedCount_;
  }

  saveState();
}

void App::finishCsvImport(bool syncWithDigiKey) {
  if (syncWithDigiKey) {
    setMessage("Syncing accepted CSV rows with DigiKey API...", 3);
    syncAcceptedImports();
  }

  const auto summary = importCompletionMessage();
  logActivity("import", summary);
  importCandidates_.clear();
  importAcceptedItemIds_.clear();
  importSourcePath_.clear();
  importSelection_ = 0;
  importSyncPrompt_ = false;
  editingImportCandidate_ = false;
  changePage(Page::Dashboard);
  setMessage(summary, 8);
}

string App::importCompletionMessage() const {
  return "CSV import complete: " + to_string(importCreatedCount_) + " new, " +
         to_string(importMergedCount_) + " merged, " + to_string(importSkippedCount_) + " skipped, " +
         to_string(importSyncedCount_) + " synced, " + to_string(importSyncFailedCount_) + " sync failed";
}

void App::openCurrentUrl(const string& url, const string& label) {
  if (trim(url).empty()) {
    setMessage("No " + label + " link stored for this item", 3);
    return;
  }
  if (openUrl(url)) {
    setMessage("Opened " + label + " link", 2);
  } else {
    setMessage("Unable to open " + label + " link", 3);
  }
}

string App::fieldLabel(EditField field) const {
  switch (field) {
    case EditField::PartName:
      return "Part name";
    case EditField::Manufacturer:
      return "Manufacturer";
    case EditField::Category:
      return "Category";
    case EditField::Quantity:
      return "Quantity";
    case EditField::ReorderThreshold:
      return "Reorder threshold";
    case EditField::Location:
      return "Location";
    case EditField::Tags:
      return "Tags";
    case EditField::Parameters:
      return "Parameters";
    case EditField::Notes:
      return "Notes";
    case EditField::DigiKeyPart:
      return "DigiKey part";
    case EditField::DatasheetUrl:
      return "Datasheet URL";
    case EditField::ProductUrl:
      return "Product URL";
    case EditField::Sku:
      return "SKU";
    case EditField::SyncStatus:
      return "Sync status";
  }
  return "Field";
}

string App::currentFieldValue(EditField field) const {
  const auto* item = workingCopy_.item.id.empty() && !workingCopy_.isNew ? selectedItem() : &workingCopy_.item;
  if (item == nullptr) {
    return {};
  }

  switch (field) {
    case EditField::PartName:
      return item->partName;
    case EditField::Manufacturer:
      return item->manufacturer;
    case EditField::Category:
      return item->category;
    case EditField::Quantity:
      return to_string(item->quantity);
    case EditField::ReorderThreshold:
      return to_string(item->reorderThreshold);
    case EditField::Location:
      return item->location;
    case EditField::Tags:
      return join(item->tags, ',');
    case EditField::Parameters: {
      ostringstream out;
      for (size_t index = 0; index < item->parameters.size(); ++index) {
        if (index > 0) {
          out << "; ";
        }
        out << item->parameters[index].name << '=' << item->parameters[index].value;
      }
      return out.str();
    }
    case EditField::Notes:
      return item->notes;
    case EditField::DigiKeyPart:
      return item->digikeyPartNumber;
    case EditField::DatasheetUrl:
      return item->datasheetUrl;
    case EditField::ProductUrl:
      return item->productUrl;
    case EditField::Sku:
      return item->sku;
    case EditField::SyncStatus:
      return item->syncStatus;
  }

  return {};
}

vector<App::FieldOption> App::fieldOptions() const {
  return {
      {"Part name", EditField::PartName},
      {"Manufacturer", EditField::Manufacturer},
      {"Category", EditField::Category},
      {"Quantity", EditField::Quantity},
      {"Reorder threshold", EditField::ReorderThreshold},
      {"Location", EditField::Location},
      {"Tags", EditField::Tags},
      {"Parameters", EditField::Parameters},
      {"Notes", EditField::Notes},
      {"DigiKey part", EditField::DigiKeyPart},
      {"Datasheet URL", EditField::DatasheetUrl},
      {"Product URL", EditField::ProductUrl},
      {"SKU", EditField::Sku},
      {"Sync status", EditField::SyncStatus},
      {"Save changes", EditField::PartName},
      {"Cancel", EditField::PartName},
  };
}

string App::itemDetailText(const InventoryItem& item, int width) const {
  ostringstream out;
  const auto fields = stockPreviewFields(item);
  for (const auto& field : fields) {
    const auto line = field.label + field.value;
    for (const auto& wrapped : wrapText(line, width)) {
      out << wrapped << '\n';
    }
  }

  return out.str();
}

string App::summaryLine() const {
  const auto summary = summarize(store_.items());
  ostringstream out;
  out << summary.itemCount << " items"
      << " | " << summary.totalUnits << " units"
      << " | " << summary.lowStockCount << " low"
      << " | " << summary.missingMetadataCount << " missing metadata"
      << " | " << summary.unsyncedCount << " unsynced";
  return out.str();
}

string App::scannerUrl() const {
  if (!server_.running() || server_.port() == 0) {
    return "scanner unavailable";
  }
  return server_.baseUrl() + "/";
}

string App::activePrompt() const {
  if (inputMode_ == InputMode::EditValue && fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
    return fieldLabel(menuOptions_[fieldMenuIndex_].field) + ": ";
  }
  return "";
}

string App::shortcutSummary() const {
  if (inputMode_ == InputMode::Search) {
    return "Search: 'Enter' apply | 'Esc' cancel";
  }
  if (inputMode_ == InputMode::EditFieldMenu) {
    return "Edit fields: '1'-'0' choose | 'Esc' cancel";
  }
  if (inputMode_ == InputMode::EditValue) {
    return "Edit value: 'Enter' save | 'Esc' cancel";
  }

  switch (page_) {
    case Page::Dashboard:
      return "Dashboard: 'Tab'/'1' stock | '2' scanner | '3' add | '4' reload | '5/i' import CSV | 'l' printer | '/' search | 'q' quit";
    case Page::Stock:
      return "Stock: 'Tab'/'1' dashboard | 'Enter' detail | 'e' edit | 'n' new | 'Ctrl+Backspace' delete | 'p' print | '+/-' qty | '/' search | 's' scanner | 'q' quit";
    case Page::Detail:
      return "Detail: 'Esc' stock | 'e' edit | 'p' print | '+/-' qty | '/' search | 's' scanner | 'q' quit";
    case Page::ImportCsv:
      if (importSyncPrompt_) {
        return "Import sync: 'Enter'/'y' sync with DigiKey API | 'n'/'Esc' finish";
      }
      return "Import CSV: arrows/j/k move | 'Enter' accept | 'e' edit | 'Backspace' skip | 'Esc' cancel";
  }

  return {};
}

}  // namespace hims

