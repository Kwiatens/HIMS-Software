#include "App.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <thread>

namespace hims {

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

std::string padRight(std::string value, int width);

std::string styleText(const std::string& text, const char* fg = nullptr, const char* bg = nullptr) {
  std::ostringstream out;
  if (bg != nullptr) {
    out << bg;
  }
  if (fg != nullptr) {
    out << fg;
  }
  out << text << kColorReset;
  return out.str();
}

std::string styleCell(const std::string& text, int width, const char* fg = nullptr, const char* bg = nullptr) {
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

ftxui::Element styledText(const std::string& text, std::optional<ftxui::Color> fg = std::nullopt,
                          std::optional<ftxui::Color> bg = std::nullopt) {
  auto element = ftxui::text(text);
  if (fg) {
    element = element | ftxui::color(*fg);
  }
  if (bg) {
    element = element | ftxui::bgcolor(*bg);
  }
  return element;
}

ftxui::Element fullLine(const std::string& text, std::optional<ftxui::Color> fg = std::nullopt,
                        std::optional<ftxui::Color> bg = std::nullopt) {
  auto element = ftxui::hbox({ftxui::text(text), ftxui::filler()});
  if (fg) {
    element = element | ftxui::color(*fg);
  }
  if (bg) {
    element = element | ftxui::bgcolor(*bg);
  }
  return element;
}

ftxui::Element linesBlock(const std::vector<std::string>& lines, std::optional<ftxui::Color> fg = std::nullopt,
                          std::optional<ftxui::Color> bg = std::nullopt) {
  ftxui::Elements elements;
  elements.reserve(lines.size());
  for (const auto& line : lines) {
    elements.push_back(fullLine(line, fg, bg));
  }
  return ftxui::vbox(std::move(elements));
}

ftxui::Element bulletLine(const std::string& label, const std::string& value, ftxui::Color labelColor,
                          ftxui::Color valueColor) {
  return ftxui::hbox({
      styledText(label, labelColor),
      styledText(value, valueColor),
      ftxui::filler(),
  });
}

ftxui::Element panel(const std::string& title, ftxui::Elements body, std::optional<ftxui::Color> titleColor = std::nullopt,
                     std::optional<ftxui::Color> borderColor = std::nullopt) {
  auto titleElement = styledText(title, titleColor);
  auto element = ftxui::window(titleElement, ftxui::vbox(std::move(body)));
  if (borderColor) {
    element = element | ftxui::color(*borderColor);
  }
  return element;
}

KeyEvent translateEvent(const ftxui::Event& event) {
  if (event == ftxui::Event::Return) {
    return {KeyType::Enter, '\0'};
  }
  if (event == ftxui::Event::Escape) {
    return {KeyType::Escape, '\0'};
  }
  if (event == ftxui::Event::Backspace) {
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

std::string padRight(std::string value, int width) {
  if (width <= 0) {
    return {};
  }
  if (static_cast<int>(value.size()) > width) {
    if (width <= 1) {
      return value.substr(0, width);
    }
    return value.substr(0, width - 3) + "...";
  }
  value.append(static_cast<std::size_t>(width - static_cast<int>(value.size())), ' ');
  return value;
}

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream input(text);
  std::string line;

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(std::move(line));
  }

  if (lines.empty()) {
    lines.push_back({});
  }

  return lines;
}

std::vector<std::string> wrapText(const std::string& text, int width) {
  std::vector<std::string> lines;
  if (width <= 0) {
    return lines;
  }

  std::istringstream words(text);
  std::string word;
  std::string line;

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

void renderColumns(std::ostringstream& out, const std::vector<std::string>& leftLines,
                   const std::vector<std::string>& rightLines, int leftWidth, int rightWidth, int maxRows,
                   int gap = 4, const char* leftFg = nullptr, const char* leftBg = nullptr,
                   const char* rightFg = nullptr, const char* rightBg = nullptr) {
  const int rowCount = std::min(
      maxRows,
      std::max(static_cast<int>(leftLines.size()), static_cast<int>(rightLines.size())));

  for (int row = 0; row < rowCount; ++row) {
    const auto left = row < static_cast<int>(leftLines.size()) ? leftLines[static_cast<std::size_t>(row)] : "";
    const auto right = row < static_cast<int>(rightLines.size()) ? rightLines[static_cast<std::size_t>(row)] : "";
    out << styleCell(left, leftWidth, leftFg, leftBg) << std::string(static_cast<std::size_t>(gap), ' ')
        << styleCell(right, rightWidth, rightFg, rightBg) << '\n';
  }
}

void appendWrapped(std::vector<std::string>& lines, const std::string& text, int width) {
  const auto wrapped = wrapText(text, width);
  lines.insert(lines.end(), wrapped.begin(), wrapped.end());
}

void appendWrappedStyled(std::vector<std::string>& lines, const std::string& text, int width, const char* fg) {
  const auto wrapped = wrapText(text, width);
  for (const auto& line : wrapped) {
    lines.push_back(styleText(line, fg));
  }
}

std::string joinTags(const std::vector<std::string>& tags) {
  return join(tags, ',');
}

std::vector<std::string> splitFlexible(const std::string& text) {
  std::vector<std::string> values;
  std::string current;
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

std::vector<Parameter> parseParameters(const std::string& text) {
  std::vector<Parameter> values;
  for (const auto& entry : splitFlexible(text)) {
    const auto equalsPos = entry.find('=');
    if (equalsPos == std::string::npos) {
      continue;
    }
    values.push_back({trim(entry.substr(0, equalsPos)), trim(entry.substr(equalsPos + 1))});
  }
  return values;
}

std::string renderTags(const std::vector<std::string>& tags) {
  if (tags.empty()) {
    return "-";
  }
  return join(tags, ',');
}

std::string renderParameters(const std::vector<Parameter>& parameters) {
  if (parameters.empty()) {
    return "-";
  }

  std::ostringstream out;
  for (std::size_t index = 0; index < parameters.size(); ++index) {
    if (index > 0) {
      out << "; ";
    }
    out << parameters[index].name << '=' << parameters[index].value;
  }
  return out.str();
}

std::string renderUrl(const std::string& url) {
  return url.empty() ? std::string("-") : url;
}

std::filesystem::path documentsHimsPath() {
  if (const char* profile = std::getenv("USERPROFILE"); profile != nullptr && *profile != '\0') {
    return std::filesystem::path(profile) / "Documents" / "HIMS";
  }
  return std::filesystem::current_path() / "Documents" / "HIMS";
}

std::filesystem::path legacyDatabasePath() {
  const auto githubRoot = std::filesystem::current_path().parent_path().parent_path();
  return githubRoot / "Kwiatens Stock Management System" / "KwiatensStockManagementSystem" / "data" / "kwiatens-stock.db";
}

void copyDatabaseSidecar(const std::filesystem::path& sourceBase, const std::filesystem::path& destinationBase,
                         const std::string& suffix) {
  const auto source = std::filesystem::path(sourceBase.string() + suffix);
  const auto destination = std::filesystem::path(destinationBase.string() + suffix);
  std::error_code error;
  if (std::filesystem::exists(source, error)) {
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, error);
  }
}

void ensureInventoryDatabaseCopied(const std::filesystem::path& localBase) {
  std::error_code error;
  if (std::filesystem::exists(localBase, error)) {
    return;
  }

  const auto sourceBase = legacyDatabasePath();
  if (!std::filesystem::exists(sourceBase, error)) {
    return;
  }

  std::filesystem::create_directories(localBase.parent_path(), error);
  std::filesystem::copy_file(sourceBase, localBase, std::filesystem::copy_options::overwrite_existing, error);
  copyDatabaseSidecar(sourceBase, localBase, "-wal");
  copyDatabaseSidecar(sourceBase, localBase, "-shm");
}

}  // namespace

App::App()
    : root_(std::filesystem::current_path()),
      dataPath_(documentsHimsPath()),
      inventoryPath_(dataPath_ / "inventory.db"),
      activityPath_(dataPath_ / "activity.tsv") {
  ensureInventoryDatabaseCopied(inventoryPath_);
  loadState();

  if (!server_.start(8080, [this](const std::string& code) { pushScanCode(code); })) {
    setMessage("Scanner server failed to start; terminal still works", 5);
  } else {
    setMessage("Scanner ready at " + scannerUrl(), 5);
  }
}

void App::loadState() {
  store_.load(inventoryPath_);
  loadActivities(activityPath_, activities_);
  if (activities_.empty()) {
    activities_.push_back(makeActivity("system", "Inventory loaded"));
    activities_.push_back(makeActivity("system", "Terminal dashboard initialized"));
  }
  server_.setRecentActivity(activities_);
  saveState();
}

void App::saveState() {
  store_.save(inventoryPath_);
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
  return ftxui::vbox(std::move(body));
}

ftxui::Element App::renderPageUi() const {
  switch (page_) {
    case Page::Dashboard:
      return renderDashboardUi();
    case Page::Stock:
      return renderStockUi();
    case Page::Detail:
      return renderDetailUi();
  }

  return ftxui::text("");
}

ftxui::Element App::renderDashboardUi() const {
  const auto summary = summarize(store_.items());

  ftxui::Elements leftBody;
  leftBody.push_back(fullLine("Summary", uiAccentColor(), uiPanelLeftBg()));
  leftBody.push_back(bulletLine("Parts: ", std::to_string(summary.itemCount), uiInfoColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Units: ", std::to_string(summary.totalUnits), uiSuccessColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Low stock: ", std::to_string(summary.lowStockCount), uiWarnColor(), uiTitleColor()));
  leftBody.push_back(
      bulletLine("Missing metadata: ", std::to_string(summary.missingMetadataCount), uiDangerColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Unsynced: ", std::to_string(summary.unsyncedCount), uiLabelColor(), uiTitleColor()));
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
    rightBody.push_back(fullLine("  - " + item.partName + " qty " + std::to_string(item.quantity) + " / " +
                                     std::to_string(item.reorderThreshold) + "  [" + item.category + "]",
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
  const auto activityCount = std::min<std::size_t>(activities_.size(), 8);
  for (std::size_t offset = 0; offset < activityCount; ++offset) {
    const auto& entry = activities_[activities_.size() - 1 - offset];
    const auto line = "  - " + nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message;
    ftxui::Color color = uiMutedColor();
    if (entry.kind.find("scan") != std::string::npos) {
      color = uiLinkColor();
    } else if (entry.kind.find("add") != std::string::npos) {
      color = uiSuccessColor();
    } else if (entry.kind.find("edit") != std::string::npos) {
      color = uiInfoColor();
    } else if (entry.kind == "system") {
      color = uiLabelColor();
    }
    rightBody.push_back(ftxui::paragraphAlignLeft(line) | ftxui::color(color));
  }
  if (activityCount == 0) {
    rightBody.push_back(fullLine("  No recent activity.", uiMutedColor(), uiPanelRightBg()));
  }

  auto leftPanel = panel("Overview", std::move(leftBody), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto rightPanel = panel("Activity", std::move(rightBody), uiWarnColor()) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;

  return ftxui::hbox({
      leftPanel,
      ftxui::separator(),
      rightPanel,
  });
}

ftxui::Element App::renderStockUi() const {
  const auto filtered = filteredIndices();

  ftxui::Elements listRows;
  listRows.push_back(fullLine("Part                          Category       Qty", uiMutedColor(), uiPanelLeftBg()));

  if (filtered.empty()) {
    listRows.push_back(fullLine("No items match \"" + searchQuery_ + "\".", uiMutedColor(), uiPanelLeftBg()));
  } else {
    for (std::size_t index = 0; index < filtered.size(); ++index) {
      const auto& item = store_.items()[filtered[index]];
      const bool selected = index == selectedPosition_;
      const bool lowStock = item.lowStock();
      const auto bg = selected ? uiRowSelectedBg() : (index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg());
      const auto fg = selected ? uiTitleColor() : (lowStock ? uiWarnColor() : uiMutedColor());
      auto row = fullLine("  " + padRight(item.partName, 28) + "  " + padRight(item.category, 11) + " " +
                              padRight(std::to_string(item.quantity), 4),
                          fg, bg);
      if (selected) {
        row = row | ftxui::select;
      }
      listRows.push_back(row);
    }
  }

  ftxui::Elements detailRows;
  if (const auto* item = selectedItem()) {
    detailRows.push_back(fullLine("Core details", uiAccentColor(), uiPanelRightBg()));
    detailRows.push_back(bulletLine("Name: ", item->partName, uiInfoColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Manufacturer: ", item->manufacturer, uiInfoColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Category: ", item->category, uiLabelColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Quantity: ", std::to_string(item->quantity), uiSuccessColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Threshold: ", std::to_string(item->reorderThreshold), uiWarnColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Location: ", item->location, uiMutedColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Tags: ", renderTags(item->tags), uiLabelColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Parameters: ", renderParameters(item->parameters), uiInfoColor(), uiTitleColor()));
    detailRows.push_back(ftxui::separator());
    detailRows.push_back(fullLine("Metadata", uiAccentColor(), uiPanelRightBg()));
    detailRows.push_back(bulletLine("DigiKey: ", renderUrl(item->digikeyPartNumber), uiLinkColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Datasheet: ", renderUrl(item->datasheetUrl), uiLinkColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Product: ", renderUrl(item->productUrl), uiLinkColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("SKU: ", renderUrl(item->sku), uiLabelColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Status: ", item->syncStatus, uiSuccessColor(), uiTitleColor()));
    detailRows.push_back(bulletLine("Updated: ", nowTimestampString(item->lastUpdated), uiMutedColor(), uiTitleColor()));
    detailRows.push_back(ftxui::separator());
    detailRows.push_back(fullLine("Notes", uiWarnColor(), uiPanelRightBg()));
    detailRows.push_back(ftxui::paragraphAlignLeft(item->notes) | ftxui::color(uiMutedColor()));
  } else {
    detailRows.push_back(fullLine("No item selected.", uiMutedColor(), uiPanelRightBg()));
  }

  auto listContent = ftxui::vbox(std::move(listRows)) | ftxui::frame | ftxui::vscroll_indicator;
  auto listPanel = ftxui::window(styledText("Items", uiAccentColor()), std::move(listContent)) |
                   ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto detailPanel = panel("Detail", std::move(detailRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;

  return ftxui::hbox({
      listPanel,
      ftxui::separator(),
      detailPanel,
  });
}

ftxui::Element App::renderDetailUi() const {
  ftxui::Elements leftRows;
  ftxui::Elements rightRows;

  if (const auto* item = selectedItem()) {
    leftRows.push_back(fullLine("Core details", uiAccentColor(), uiPanelLeftBg()));
    leftRows.push_back(bulletLine("Name: ", item->partName, uiInfoColor(), uiTitleColor()));
    leftRows.push_back(bulletLine("Manufacturer: ", item->manufacturer, uiInfoColor(), uiTitleColor()));
    leftRows.push_back(bulletLine("Category: ", item->category, uiLabelColor(), uiTitleColor()));
    leftRows.push_back(bulletLine("Quantity: ", std::to_string(item->quantity), uiSuccessColor(), uiTitleColor()));
    leftRows.push_back(bulletLine("Threshold: ", std::to_string(item->reorderThreshold), uiWarnColor(), uiTitleColor()));
    leftRows.push_back(bulletLine("Location: ", item->location, uiMutedColor(), uiTitleColor()));
    leftRows.push_back(bulletLine("Tags: ", renderTags(item->tags), uiLabelColor(), uiTitleColor()));
    leftRows.push_back(bulletLine("Parameters: ", renderParameters(item->parameters), uiInfoColor(), uiTitleColor()));

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

  auto leftPanel = panel("Item detail", std::move(leftRows), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto rightPanel = panel("Links and notes", std::move(rightRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;

  return ftxui::hbox({
      leftPanel,
      ftxui::separator(),
      rightPanel,
  });
}

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
    for (std::size_t index = 0; index < menuOptions_.size(); ++index) {
      const auto bg = static_cast<int>(index) == fieldMenuIndex_ ? uiRowSelectedBg() : (index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg());
      auto option = fullLine("  [" + std::to_string(index < 9 ? index + 1 : 0) + "] " + menuOptions_[index].label,
                             static_cast<int>(index) == fieldMenuIndex_ ? uiTitleColor() : uiMutedColor(), bg);
      if (static_cast<int>(index) == fieldMenuIndex_) {
        option = option | ftxui::select;
      }
      options.push_back(option);
    }
    rows.push_back(panel("Edit fields", std::move(options), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()));
  } else if (inputMode_ == InputMode::EditValue) {
    rows.push_back(fullLine("Input  " + activePrompt() + inputBuffer_ + "_", uiLinkColor(), uiPanelLeftBg()));
  }

  return ftxui::vbox(std::move(rows));
}

ftxui::Element App::renderStatusBarUi() const {
  return fullLine(scannerUrl() + "  Dashboard: Tab/1  Stock: arrows/jk  Detail: Enter  Edit: e  New: n  Scanner: s  Quit: q",
                  uiLinkColor(), uiPanelRightBg());
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
  auto renderer = ftxui::Renderer([this] { return renderUi(); });
  auto component = ftxui::CatchEvent(renderer, [this, &screen](ftxui::Event event) {
    if (event == ftxui::Event::Custom) {
      processScans();
      clearMessageIfExpired();
      return true;
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

  std::thread ticker([this, &screen] {
    while (running_) {
      screen.PostEvent(ftxui::Event::Custom);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
  }
}

void App::handleDashboardKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    switch (std::tolower(static_cast<unsigned char>(key.ch))) {
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
    const auto ch = std::tolower(static_cast<unsigned char>(key.ch));
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
                                         ? std::string()
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
    const auto ch = std::tolower(static_cast<unsigned char>(key.ch));
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
                                         ? std::string()
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
    const auto ch = std::tolower(static_cast<unsigned char>(key.ch));
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
    fieldMenuIndex_ = std::max(0, fieldMenuIndex_ - 1);
    dirty_ = true;
  } else if (key.type == KeyType::Down) {
    fieldMenuIndex_ = std::min(fieldMenuIndex_ + 1, static_cast<int>(menuOptions_.size()) - 1);
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
  std::ostringstream out;
  out << "\x1b[?25l";
  out << kColorTitle << "HIMS Terminal" << kColorReset << "  ";
  out << kColorMuted << summaryLine() << kColorReset;
  out << "\n";
  out << std::string(std::max(0, size.columns), '=') << '\n';

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
  }

  renderSearchBar(out, size);
  renderMessage(out, size);
  renderStatusBar(out, size);

  std::ostringstream screen;
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

  std::cout << screen.str() << std::flush;
}

void App::renderDashboard(std::ostringstream& out, const ConsoleSize& size) {
  const auto summary = summarize(store_.items());
  const int contentRows = std::max(6, size.rows - 6);

  out << kColorAccent << "Dashboard" << kColorReset << "  ";
  out << kColorMuted << "Overview, alerts, and activity" << kColorReset << '\n';
  out << std::string(std::max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    std::vector<std::string> lines;
    lines.push_back(styleText("Summary", kColorAccent));
    lines.push_back(styleText("  Parts: " + std::to_string(summary.itemCount), kColorInfo));
    lines.push_back(styleText("  Units: " + std::to_string(summary.totalUnits), kColorSuccess));
    lines.push_back(styleText("  Low stock: " + std::to_string(summary.lowStockCount), kColorWarn));
    lines.push_back(styleText("  Missing metadata: " + std::to_string(summary.missingMetadataCount), kColorDanger));
    lines.push_back(styleText("  Unsynced: " + std::to_string(summary.unsyncedCount), kColorLabel));
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
                          "  - " + item.partName + " qty " + std::to_string(item.quantity) + " / " +
                              std::to_string(item.reorderThreshold) + "  [" + item.category + "]",
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
    const auto activityCount = std::min<std::size_t>(activities_.size(), 5);
    for (std::size_t offset = 0; offset < activityCount; ++offset) {
      const auto& entry = activities_[activities_.size() - 1 - offset];
      const char* activityColor = kColorMuted;
      if (entry.kind.find("scan") != std::string::npos) {
        activityColor = kColorLink;
      } else if (entry.kind.find("add") != std::string::npos) {
        activityColor = kColorSuccess;
      } else if (entry.kind.find("edit") != std::string::npos) {
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
      lines.resize(static_cast<std::size_t>(contentRows));
    }
    for (const auto& line : lines) {
      out << line << '\n';
    }
    return;
  }

  const int leftWidth = std::max(36, (size.columns - 4) / 2);
  const int rightWidth = std::max(36, size.columns - leftWidth - 4);

  std::vector<std::string> leftLines;
  std::vector<std::string> rightLines;

  leftLines.push_back("Summary");
  leftLines.push_back("  Parts: " + std::to_string(summary.itemCount));
  leftLines.push_back("  Units: " + std::to_string(summary.totalUnits));
  leftLines.push_back("  Low stock: " + std::to_string(summary.lowStockCount));
  leftLines.push_back("  Missing metadata: " + std::to_string(summary.missingMetadataCount));
  leftLines.push_back("  Unsynced: " + std::to_string(summary.unsyncedCount));
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
                  "  - " + item.partName + " qty " + std::to_string(item.quantity) + " / " +
                      std::to_string(item.reorderThreshold) + "  [" + item.category + "]",
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
  const auto activityCount = std::min<std::size_t>(activities_.size(), 8);
  for (std::size_t offset = 0; offset < activityCount; ++offset) {
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

void App::renderStock(std::ostringstream& out, const ConsoleSize& size) {
  const auto filtered = filteredIndices();
  const int contentRows = std::max(6, size.rows - 6);

  out << kColorAccent << "Stock browser" << kColorReset << "  ";
  out << kColorMuted << "(j/k or arrows to move, Enter detail, e edit, n new, +/- adjust)" << kColorReset << '\n';
  out << std::string(std::max(0, size.columns), '-') << '\n';

  struct StockLine {
    std::string text;
    bool isHeader = false;
    bool isMessage = false;
    bool isSelected = false;
    bool isLowStock = false;
    bool isEven = false;
  };

  const bool compact = size.columns < 96;
  const int listHeight = compact ? std::max(4, contentRows / 2) : std::max(4, contentRows - 1);
  const int listWidth = compact ? std::max(0, size.columns) : std::max(36, (size.columns - 4) / 3);
  const int detailWidth = compact ? std::max(40, size.columns - 4) : std::max(40, size.columns - listWidth - 4);
  const int partWidth = std::max(12, listWidth - 21);

  std::vector<StockLine> listLines;
  listLines.push_back({"Items", true});
  listLines.push_back({"Part                          Category       Qty", true});

  if (filtered.empty()) {
    listLines.push_back({"No items match \"" + searchQuery_ + "\".", false, true});
  } else {
    std::size_t visibleStart = 0;
    if (selectedPosition_ >= static_cast<std::size_t>(listHeight)) {
      visibleStart = selectedPosition_ - static_cast<std::size_t>(listHeight) + 1;
    }
    if (selectedPosition_ < visibleStart) {
      visibleStart = selectedPosition_;
    }

    stockScroll_ = visibleStart;

    for (int row = 0; row < listHeight; ++row) {
      const auto pos = visibleStart + static_cast<std::size_t>(row);
      if (pos >= filtered.size()) {
        break;
      }

      const auto& item = store_.items()[filtered[pos]];
      std::ostringstream rowText;
      rowText << (pos == selectedPosition_ ? '>' : ' ') << ' ' << padRight(item.partName, partWidth) << "  "
              << padRight(item.category, 11) << ' ' << std::setw(4) << item.quantity;
      listLines.push_back({rowText.str(), false, false, pos == selectedPosition_, item.lowStock(), row % 2 == 0});
    }
  }

  std::vector<StockLine> detailLines;
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
    if (line.text.rfind("Name: ", 0) == 0 || line.text.rfind("Manufacturer: ", 0) == 0 ||
        line.text.rfind("Category: ", 0) == 0) {
      return styleCell(line.text, detailWidth, kColorInfo, kBgPanelRight);
    }
    if (line.text.rfind("Quantity: ", 0) == 0 || line.text.rfind("Threshold: ", 0) == 0 ||
        line.text.rfind("Status: ", 0) == 0) {
      return styleCell(line.text, detailWidth, kColorSuccess, kBgPanelRight);
    }
    if (line.text.rfind("DigiKey: ", 0) == 0 || line.text.rfind("Datasheet: ", 0) == 0 ||
        line.text.rfind("Product: ", 0) == 0) {
      return styleCell(line.text, detailWidth, kColorLink, kBgPanelRight);
    }
    if (line.text.rfind("SKU: ", 0) == 0 || line.text.rfind("Updated: ", 0) == 0) {
      return styleCell(line.text, detailWidth, kColorLabel, kBgPanelRight);
    }
    if (line.text == "Notes") {
      return styleCell(line.text, detailWidth, kColorWarn, kBgPanelRight);
    }
    if (line.text == "Shortcuts") {
      return styleCell(line.text, detailWidth, kColorDim, kBgPanelRight);
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

  const std::size_t maxRows = std::max(listLines.size(), detailLines.size());
  for (std::size_t row = 0; row < maxRows; ++row) {
    const auto left = row < listLines.size() ? styleListLine(listLines[row]) : styleCell("", listWidth, nullptr, kBgPanelLeft);
    const auto right =
        row < detailLines.size() ? styleDetailLine(detailLines[row]) : styleCell("", detailWidth, nullptr, kBgPanelRight);
    out << left << "    " << right << '\n';
  }
}

void App::renderDetail(std::ostringstream& out, const ConsoleSize& size) {
  const int contentRows = std::max(6, size.rows - 6);

  out << kColorAccent << "Item detail" << kColorReset << "  " << kColorMuted << "(Esc back to stock)" << kColorReset
      << '\n';
  out << std::string(std::max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    if (const auto* item = selectedItem()) {
      std::vector<std::string> lines;
      lines.push_back(styleText("Core details", kColorAccent));
      lines.push_back(styleText("  Name: " + item->partName, kColorInfo));
      lines.push_back(styleText("  Manufacturer: " + item->manufacturer, kColorInfo));
      lines.push_back(styleText("  Category: " + item->category, kColorLabel));
      lines.push_back(styleText("  Quantity: " + std::to_string(item->quantity), kColorSuccess));
      lines.push_back(styleText("  Threshold: " + std::to_string(item->reorderThreshold), kColorWarn));
      lines.push_back(styleText("  Location: " + item->location, kColorMuted));
      lines.push_back(styleText("  Tags: " + renderTags(item->tags), kColorLabel));
      lines.push_back(styleText("  Parameters: " + renderParameters(item->parameters), kColorInfo));
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
      appendWrappedStyled(lines, item->notes, std::max(50, size.columns - 4), kColorMuted);
      lines.push_back("");
      lines.push_back(styleText("Shortcuts", kColorAccent));
      appendWrappedStyled(lines, "e Edit   n New   + / - Quantity   Enter Open detail   / Search", std::max(50, size.columns - 4),
                          kColorDim);
      for (const auto& line : lines) {
        out << line << '\n';
      }
    } else {
      out << "No item selected.\n";
    }
    return;
  }

  const int leftWidth = std::max(40, (size.columns - 4) / 2);
  const int rightWidth = std::max(40, size.columns - leftWidth - 4);

  std::vector<std::string> leftLines;
  std::vector<std::string> rightLines;

  if (const auto* item = selectedItem()) {
    leftLines.push_back("Core details");
    leftLines.push_back("Name: " + item->partName);
    leftLines.push_back("Manufacturer: " + item->manufacturer);
    leftLines.push_back("Category: " + item->category);
    leftLines.push_back("Quantity: " + std::to_string(item->quantity) + "  Threshold: " +
                       std::to_string(item->reorderThreshold));
    leftLines.push_back("Location: " + item->location);
    leftLines.push_back("Tags: " + renderTags(item->tags));
    leftLines.push_back("Parameters: " + renderParameters(item->parameters));

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

void App::renderSearchBar(std::ostringstream& out, const ConsoleSize&) {
  out << '\n' << kColorAccent << "Search" << kColorReset << "  ";
  if (inputMode_ == InputMode::Search) {
    out << kColorLink << '/' << inputBuffer_ << '_' << kColorReset;
  } else {
    out << kColorMuted << '/' << searchQuery_ << kColorReset;
  }
  out << '\n';

  if (inputMode_ == InputMode::EditFieldMenu) {
    out << kColorAccent << "Edit fields" << kColorReset << '\n';
    for (std::size_t index = 0; index < menuOptions_.size(); ++index) {
      if (static_cast<int>(index) == fieldMenuIndex_) {
        out << kColorSelect;
      }
      out << "  [" << (index < 9 ? std::to_string(index + 1) : "0") << "] " << menuOptions_[index].label
          << kColorReset << '\n';
    }
  } else if (inputMode_ == InputMode::EditValue) {
    out << kColorAccent << "Input" << kColorReset << "  " << kColorLabel << activePrompt() << kColorReset
        << kColorLink << inputBuffer_ << '_' << kColorReset << '\n';
  }
}

void App::renderStatusBar(std::ostringstream& out, const ConsoleSize& size) {
  out << std::string(size.columns, '-') << '\n';
  out << kColorLink << scannerUrl() << kColorReset << "  ";
  out << kColorDim << "Dashboard: Tab/1  Stock: arrows/jk  Detail: Enter  Edit: e  New: n  Scanner: s  Quit: q" << kColorReset << '\n';
}

void App::renderMessage(std::ostringstream& out, const ConsoleSize&) {
  if (!message_.empty()) {
    out << kColorAccent << message_ << kColorReset << '\n';
  }
}

void App::setMessage(std::string text, int seconds) {
  message_ = std::move(text);
  messageUntil_ = std::time(nullptr) + seconds;
  dirty_ = true;
}

bool App::messageVisible() const {
  return !message_.empty() && std::time(nullptr) <= messageUntil_;
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

std::vector<std::size_t> App::filteredIndices() const {
  return filterItems(store_.items(), searchQuery_);
}

std::size_t App::selectedIndex() const {
  const auto filtered = filteredIndices();
  if (filtered.empty()) {
    return std::numeric_limits<std::size_t>::max();
  }
  const auto position = std::min(selectedPosition_, filtered.size() - 1);
  return filtered[position];
}

InventoryItem* App::selectedItem() {
  const auto index = selectedIndex();
  if (index == std::numeric_limits<std::size_t>::max()) {
    return nullptr;
  }
  return &store_.items()[index];
}

const InventoryItem* App::selectedItem() const {
  const auto index = selectedIndex();
  if (index == std::numeric_limits<std::size_t>::max()) {
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

  const auto current = static_cast<int>(std::min(selectedPosition_, filtered.size() - 1));
  const auto next = std::clamp(current + delta, 0, static_cast<int>(filtered.size() - 1));
  selectedPosition_ = static_cast<std::size_t>(next);
  dirty_ = true;
}

void App::changePage(Page page) {
  page_ = page;
  inputMode_ = InputMode::None;
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
    workingCopy_.item.lastUpdated = std::time(nullptr);
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

void App::openFieldMenu() {
  menuOptions_ = fieldOptions();
  fieldMenuIndex_ = 0;
  inputMode_ = InputMode::EditFieldMenu;
}

void App::commitEditField(EditField field, const std::string& value) {
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
        workingCopy_.item.quantity = std::max(0, std::stoi(trimmed));
      } catch (...) {
        valid = false;
      }
      break;
    case EditField::ReorderThreshold:
      try {
        workingCopy_.item.reorderThreshold = std::max(0, std::stoi(trimmed));
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

  workingCopy_.item.lastUpdated = std::time(nullptr);
  setMessage(fieldLabel(field) + " updated", 2);
  inputBuffer_.clear();
  inputMode_ = InputMode::EditFieldMenu;
  dirty_ = true;
}

void App::saveWorkingCopy() {
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

  item->quantity = std::max(0, item->quantity + delta);
  item->lastUpdated = std::time(nullptr);
  logActivity(delta > 0 ? "stock" : "usage", item->partName + " quantity changed to " + std::to_string(item->quantity));
  saveState();
  setMessage(item->partName + " quantity is now " + std::to_string(item->quantity), 2);
  dirty_ = true;
}

void App::logActivity(const std::string& kind, const std::string& message) {
  appendActivity(activities_, makeActivity(kind, message));
  server_.setRecentActivity(activities_);
  saveActivities(activityPath_, activities_);
  dirty_ = true;
}

void App::pushScanCode(const std::string& code) {
  std::lock_guard<std::mutex> lock(scanMutex_);
  scanQueue_.push_back(code);
}

void App::processScans() {
  std::vector<std::string> pending;
  {
    std::lock_guard<std::mutex> lock(scanMutex_);
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
        const auto it = std::find_if(store_.items().begin(), store_.items().end(), [&](const InventoryItem& entry) {
          return entry.id == item->id;
        });
        if (it != store_.items().end()) {
          selectedPosition_ = static_cast<std::size_t>(std::distance(store_.items().begin(), it));
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

void App::openCurrentUrl(const std::string& url, const std::string& label) {
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

std::string App::fieldLabel(EditField field) const {
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

std::string App::currentFieldValue(EditField field) const {
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
      return std::to_string(item->quantity);
    case EditField::ReorderThreshold:
      return std::to_string(item->reorderThreshold);
    case EditField::Location:
      return item->location;
    case EditField::Tags:
      return join(item->tags, ',');
    case EditField::Parameters: {
      std::ostringstream out;
      for (std::size_t index = 0; index < item->parameters.size(); ++index) {
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

std::vector<App::FieldOption> App::fieldOptions() const {
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

std::string App::itemDetailText(const InventoryItem& item, int width) const {
  std::ostringstream out;
  const auto lines = {
      std::string("Name: ") + item.partName,
      std::string("Manufacturer: ") + item.manufacturer,
      std::string("Category: ") + item.category,
      std::string("Quantity: ") + std::to_string(item.quantity) + "  Threshold: " + std::to_string(item.reorderThreshold),
      std::string("Location: ") + item.location,
      std::string("Tags: ") + renderTags(item.tags),
      std::string("Parameters: ") + renderParameters(item.parameters),
      std::string("DigiKey: ") + renderUrl(item.digikeyPartNumber),
      std::string("Datasheet: ") + renderUrl(item.datasheetUrl),
      std::string("Product: ") + renderUrl(item.productUrl),
      std::string("SKU: ") + renderUrl(item.sku),
      std::string("Status: ") + item.syncStatus,
      std::string("Updated: ") + nowTimestampString(item.lastUpdated),
      std::string("Notes: ") + item.notes,
  };

  for (const auto& line : lines) {
    for (const auto& wrapped : wrapText(line, width)) {
      out << wrapped << '\n';
    }
  }

  return out.str();
}

std::string App::summaryLine() const {
  const auto summary = summarize(store_.items());
  std::ostringstream out;
  out << summary.itemCount << " items"
      << " | " << summary.totalUnits << " units"
      << " | " << summary.lowStockCount << " low"
      << " | " << summary.missingMetadataCount << " missing metadata"
      << " | " << summary.unsyncedCount << " unsynced";
  return out.str();
}

std::string App::scannerUrl() const {
  if (!server_.running() || server_.port() == 0) {
    return "scanner unavailable";
  }
  return server_.baseUrl() + "/";
}

std::string App::activePrompt() const {
  if (inputMode_ == InputMode::EditValue && fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
    return fieldLabel(menuOptions_[fieldMenuIndex_].field) + ": ";
  }
  return "";
}

}  // namespace hims
