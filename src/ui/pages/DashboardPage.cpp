// HIMS - Hardware Inventory Management System
// Dashboard page rendering and keyboard handling.

#include "App.h"

#include "ui/shared/AppUiShared.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

namespace hims {

using namespace std;

namespace {

struct AlertRow {
  string partName;
  string category;
  string location;
  string supplierCode;
  string reason;
  int quantity = 0;
  int threshold = 0;
  bool outOfStock = false;
};

struct DeviceStatus {
  string name;
  bool connected = false;
  string endpoint;
  string lastActivity;
  string error;
};

struct CategoryBucket {
  string name;
  size_t count = 0;
};

struct DashboardSnapshot {
  size_t itemCount = 0;
  size_t totalQuantity = 0;
  size_t categoryCount = 0;
  size_t locationCount = 0;
  size_t lowStockCount = 0;
  size_t outOfStockCount = 0;
  size_t dataErrorCount = 0;
  size_t missingMetadataCount = 0;
  size_t unsyncedCount = 0;
  time_t lastUpdated = 0;
  string lastUpdatedPart;
  string lastScannedPart;
  float stockHealth = 1.0f;
  vector<AlertRow> stockWarnings;
  vector<CategoryBucket> categories;
  vector<InventoryHistoryPoint> history;
  vector<DeviceStatus> devices;
  vector<string> recentEvents;
};

struct ThresholdRule {
  const char* needle;
  int threshold;
};

constexpr ThresholdRule kThresholdRules[] = {
    {"resistor", 20},
    {"capacitor", 20},
    {"led", 20},
    {"indicator", 20},
    {"connector", 10},
    {"ic", 5},
    {"integrated circuit", 5},
    {"microcontroller", 3},
    {"mcu", 3},
    {"sensor", 3},
    {"module", 3},
    {"pcb", 2},
    {"mechanical", 10},
    {"switch", 10},
    {"relay", 5},
};

string supplierCodeFor(const InventoryItem& item) {
  if (!trim(item.digikeyPartNumber).empty()) {
    return item.digikeyPartNumber;
  }
  if (!trim(item.sku).empty()) {
    return item.sku;
  }
  return "-";
}

AlertRow makeAlertRow(const InventoryItem& item, int threshold, const string& reason = {}, bool outOfStock = false) {
  return {item.partName, displayCategory(item.category), item.location, supplierCodeFor(item), reason,
          item.quantity, threshold, outOfStock};
}

string statusLabel(bool connected) {
  return connected ? "CONNECTED" : "DISCONNECTED";
}

ftxui::Color statusColor(bool connected) {
  return connected ? uiSuccessColor() : uiDangerColor();
}

ftxui::Element statusChip(bool connected) {
  const auto bg = connected ? ftxui::Color::RGB(18, 44, 28) : ftxui::Color::RGB(56, 24, 24);
  return styledText(" " + statusLabel(connected) + " ", statusColor(connected), bg) | ftxui::bold;
}

ftxui::Element keyChip(const string& key, const string& action, ftxui::Color fg, ftxui::Color bg) {
  return styledText("[" + key + "] " + action, fg, bg) | ftxui::bold;
}

ftxui::Color stripedRowBg(size_t index) {
  return index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg();
}

long long currentTickMs() {
  return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}

size_t animatedOffset(size_t size, int periodMs) {
  if (size == 0 || periodMs <= 0) {
    return 0;
  }
  return static_cast<size_t>((currentTickMs() / periodMs) % static_cast<long long>(size));
}

bool pulseOn(int periodMs) {
  if (periodMs <= 0) {
    return true;
  }
  return ((currentTickMs() / periodMs) % 2) == 0;
}

ftxui::Element fixedCell(const string& text, int width, ftxui::Color color, bool rightAlign = false) {
  auto content = rightAlign ? ftxui::hbox({ftxui::filler(), styledText(ellipsize(text, static_cast<size_t>(max(0, width))), color)})
                            : ftxui::hbox({styledText(ellipsize(text, static_cast<size_t>(max(0, width))), color), ftxui::filler()});
  return content | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
}

ftxui::Element metricLine(const string& label, const string& value, ftxui::Color valueColor) {
  return ftxui::hbox({
      styledText(label, uiMutedColor()),
      ftxui::filler(),
      styledText(value, valueColor),
  });
}

string barString(float ratio, int width) {
  if (width <= 0) {
    return {};
  }
  ratio = max(0.0f, min(1.0f, ratio));
  const auto filled = static_cast<int>(ratio * static_cast<float>(width));
  string bar;
  bar.reserve(static_cast<size_t>(width));
  bar.append(static_cast<size_t>(filled), '#');
  bar.append(static_cast<size_t>(width - filled), '.');
  return bar;
}

ftxui::Element ratioLine(const string& label, float ratio, ftxui::Color color) {
  const int percent = static_cast<int>(ratio * 100.0f + 0.5f);
  return ftxui::hbox({
      styledText(label, uiMutedColor()),
      ftxui::filler(),
      styledText("[" + barString(ratio, 16) + "] " + to_string(percent) + "%", color),
  });
}

ftxui::Element stockWarningRow(const AlertRow& row, size_t visibleIndex, bool flashing, int statusWidth,
                              int partWidth, int quantityWidth, int categoryWidth, int locationWidth) {
  const auto bg = stripedRowBg(visibleIndex);
  const auto qtyBg = row.outOfStock ? (flashing ? ftxui::Color::RGB(72, 20, 28) : ftxui::Color::RGB(48, 18, 22))
                                    : ftxui::Color::RGB(58, 44, 14);
  const auto qtyFg = row.outOfStock ? (flashing ? uiTitleColor() : uiDangerColor()) : uiWarnColor();
  const auto partFg = row.outOfStock ? uiDangerColor() : uiTitleColor();
  const auto statusLabel = row.outOfStock ? "OUT" : "LOW";
  const auto statusFg = row.outOfStock ? uiDangerColor() : uiWarnColor();
  const auto statusBg = row.outOfStock ? ftxui::Color::RGB(56, 24, 24) : ftxui::Color::RGB(56, 42, 14);

  return ftxui::hbox({
             fixedCell(statusLabel, statusWidth, statusFg, true) | ftxui::bgcolor(statusBg),
             styledText(" | ", uiDimColor()),
             fixedCell(row.partName, partWidth, partFg),
             styledText(" | ", uiDimColor()),
             fixedCell("QTY " + to_string(row.quantity), quantityWidth, qtyFg, true) | ftxui::bgcolor(qtyBg),
             styledText(" | ", uiDimColor()),
             fixedCell(displayCategory(row.category), categoryWidth, uiMutedColor()),
             styledText(" | ", uiDimColor()),
             fixedCell(row.location.empty() ? "-" : row.location, locationWidth, uiInfoColor()),
             ftxui::filler(),
         }) |
         ftxui::bgcolor(bg);
}

ftxui::Element stockWarningSection(const string& title, const vector<AlertRow>& rows, ftxui::Color accent,
                                   bool flashing, size_t visibleCount, size_t offset, int statusWidth, int partWidth,
                                   int quantityWidth, int categoryWidth, int locationWidth) {
  ftxui::Elements body;
  if (rows.empty()) {
    body.push_back(styledText("No stock warnings.", uiMutedColor()));
  } else {
    body.push_back(ftxui::hbox({
        fixedCell("Status", statusWidth, uiMutedColor()),
        styledText(" | ", uiDimColor()),
        fixedCell("Part", partWidth, uiMutedColor()),
        styledText(" | ", uiDimColor()),
        fixedCell("Qty", quantityWidth, uiMutedColor(), true),
        styledText(" | ", uiDimColor()),
        fixedCell("Category", categoryWidth, uiMutedColor()),
        styledText(" | ", uiDimColor()),
        fixedCell("Location", locationWidth, uiMutedColor()),
        ftxui::filler(),
    }) | ftxui::bgcolor(uiRowDarkBg()));

    const auto rowCount = rows.size();
    const auto start = rowCount > visibleCount ? offset % rowCount : 0;
    const auto count = min(rowCount, visibleCount);
    for (size_t index = 0; index < count; ++index) {
      const auto rowIndex = (start + index) % rowCount;
      body.push_back(stockWarningRow(rows[rowIndex], index, flashing, statusWidth, partWidth, quantityWidth,
                                    categoryWidth, locationWidth));
    }
  }
  return panel(title + " (" + to_string(rows.size()) + ")", move(body), accent, accent) |
         ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
}

ftxui::Element categoryRow(const CategoryBucket& bucket, size_t maxCount, size_t visibleIndex, int nameWidth,
                           int countWidth, int barWidth) {
  const float ratio = maxCount == 0 ? 0.0f : static_cast<float>(bucket.count) / static_cast<float>(maxCount);
  const auto bar = barString(ratio, barWidth);
  return ftxui::hbox({
             fixedCell(bucket.name, nameWidth, uiTitleColor()),
             styledText(" | ", uiDimColor()),
             fixedCell(to_string(bucket.count), countWidth, uiAccentColor(), true),
             styledText(" | ", uiDimColor()),
             fixedCell(bar, barWidth, uiInfoColor()),
             ftxui::filler(),
         }) |
         ftxui::bgcolor(stripedRowBg(visibleIndex));
}

ftxui::Element historyRow(const InventoryHistoryPoint& point, size_t maxUnits) {
  const float ratio = maxUnits == 0 ? 0.0f : static_cast<float>(point.totalUnits) / static_cast<float>(maxUnits);
  const auto label = nowTimestampString(point.timestamp);
  const auto counts = "units " + to_string(point.totalUnits) + "  low " + to_string(point.lowStockCount) +
                      "  out " + to_string(point.outOfStockCount) + "  err " + to_string(point.dataErrorCount);
  return ftxui::vbox({
      ftxui::hbox({
          styledText(label, uiMutedColor()),
          ftxui::filler(),
          styledText("[" + barString(ratio, 18) + "]", uiAccentColor()),
      }),
      styledText(counts, uiInfoColor()),
  });
}

ftxui::Element deviceRow(const DeviceStatus& device) {
  ftxui::Elements body;
  body.push_back(ftxui::hbox({
      styledText(device.name, uiTitleColor()),
      ftxui::filler(),
      statusChip(device.connected),
  }));
  body.push_back(styledText(ellipsize(device.endpoint.empty() ? "-" : device.endpoint, 56), uiInfoColor()));
  body.push_back(styledText("last: " + ellipsize(device.lastActivity.empty() ? "n/a" : device.lastActivity, 56),
                            uiMutedColor()));
  if (!device.error.empty()) {
    body.push_back(styledText("error: " + ellipsize(device.error, 56), uiWarnColor()));
  }
  return ftxui::vbox(move(body));
}

vector<string> recentEventLines(const vector<ActivityEntry>& activities) {
  vector<string> lines;
  const auto count = min<size_t>(activities.size(), 3);
  for (size_t offset = 0; offset < count; ++offset) {
    const auto& entry = activities[activities.size() - 1 - offset];
    const auto line = nowTimestampString(entry.timestamp) + "  " + entry.kind + "  " + entry.message;
    lines.push_back(line);
  }
  return lines;
}

DashboardSnapshot buildDashboardSnapshot(const vector<InventoryItem>& items, const vector<ActivityEntry>& activities,
                                         const vector<InventoryHistoryPoint>& history, bool scannerRunning,
                                         const string& scannerUrl) {
  DashboardSnapshot snapshot;
  snapshot.itemCount = items.size();
  snapshot.history = history;
  snapshot.recentEvents = recentEventLines(activities);

  unordered_set<string> categories;
  unordered_set<string> locations;
  unordered_set<string> seenIds;
  size_t healthyCount = 0;

  for (const auto& item : items) {
    snapshot.totalQuantity += static_cast<size_t>(max(item.quantity, 0));
    snapshot.missingMetadataCount += item.hasMissingMetadata() ? 1 : 0;
    snapshot.unsyncedCount += toLower(item.syncStatus) == "synced" ? 0 : 1;

    const auto category = displayCategory(item.category);
    if (!trim(category).empty()) {
      categories.insert(toLower(category));
    }
    if (!trim(item.location).empty()) {
      locations.insert(toLower(item.location));
    }

    const bool duplicateId = !seenIds.insert(item.id).second;
    const bool dataError = duplicateId || item.quantity < 0 || item.reorderThreshold < 0;
    if (dataError) {
      ++snapshot.dataErrorCount;
    }

    const auto threshold = categoryLowStockThreshold(item.category);
    const bool lowStock = item.quantity <= threshold;
    const bool outOfStock = item.quantity <= 0;

    if (lowStock) {
      ++snapshot.lowStockCount;
      if (item.quantity > 0) {
        snapshot.stockWarnings.push_back(makeAlertRow(item, threshold));
      }
    } else {
      ++healthyCount;
    }

    if (outOfStock) {
      ++snapshot.outOfStockCount;
      snapshot.stockWarnings.push_back(makeAlertRow(item, threshold, {}, true));
    }

    if (item.lastUpdated >= snapshot.lastUpdated) {
      snapshot.lastUpdated = item.lastUpdated;
      snapshot.lastUpdatedPart = item.partName;
    }
  }

  snapshot.categoryCount = categories.size();
  snapshot.locationCount = locations.size();
  snapshot.stockHealth = snapshot.itemCount == 0 ? 1.0f : static_cast<float>(healthyCount) / static_cast<float>(snapshot.itemCount);

  for (auto it = activities.rbegin(); it != activities.rend(); ++it) {
    if (it->kind.find("scan") != string::npos) {
      snapshot.lastScannedPart = it->message;
      break;
    }
  }

  if (snapshot.history.empty()) {
    snapshot.history.push_back(makeInventoryHistoryPoint(items));
  }

  snapshot.categories.reserve(categories.size());
  unordered_map<string, size_t> bucketCounts;
  for (const auto& item : items) {
    const auto key = displayCategory(item.category);
    if (!trim(key).empty()) {
      ++bucketCounts[key];
    }
  }
  for (const auto& [name, count] : bucketCounts) {
    snapshot.categories.push_back({name, count});
  }
  sort(snapshot.categories.begin(), snapshot.categories.end(), [](const CategoryBucket& lhs, const CategoryBucket& rhs) {
    if (lhs.count != rhs.count) {
      return lhs.count > rhs.count;
    }
    return toLower(lhs.name) < toLower(rhs.name);
  });

  snapshot.devices = {
      {"HIMS Scan", scannerRunning, scannerUrl,
       snapshot.lastScannedPart.empty() ? "Waiting for scans" : snapshot.lastScannedPart,
       scannerRunning ? string() : "Scanner server is not running"},
      {"Label Printer", false, "not connected", "No print jobs yet", "Printer integration pending"},
  };

  sort(snapshot.stockWarnings.begin(), snapshot.stockWarnings.end(), [](const AlertRow& lhs, const AlertRow& rhs) {
    if (lhs.outOfStock != rhs.outOfStock) {
      return lhs.outOfStock > rhs.outOfStock;
    }
    if (lhs.quantity != rhs.quantity) {
      return lhs.quantity < rhs.quantity;
    }
    return toLower(lhs.partName) < toLower(rhs.partName);
  });

  return snapshot;
}

string timestampOrDash(time_t value) {
  return value == 0 ? string("n/a") : nowTimestampString(value);
}

}  // namespace

ftxui::Element App::renderDashboardUi() const {
  const auto snapshot = buildDashboardSnapshot(store_.items(), activities_, inventoryHistory_, server_.running(), scannerUrl());
  const auto* activeScreen = ftxui::ScreenInteractive::Active();
  const int screenWidth = activeScreen != nullptr ? activeScreen->dimx() : 120;
  const int screenHeight = activeScreen != nullptr ? activeScreen->dimy() : 40;
  const bool stackedLayout = screenWidth < 140;

  const int leftOuterWidth = stackedLayout ? max(48, screenWidth - 4)
                                           : clamp((screenWidth * 62) / 100, 78, screenWidth - 42);
  const int rightOuterWidth = stackedLayout ? max(48, screenWidth - 4) : max(34, screenWidth - leftOuterWidth - 1);
  const int leftInnerWidth = max(20, leftOuterWidth - 2);
  const int rightInnerWidth = max(20, rightOuterWidth - 2);

  const auto overviewPanel = panel(
      "Inventory overview",
      {
          ftxui::hbox({
              ftxui::vbox({
                  metricLine("Parts", to_string(snapshot.itemCount), uiTitleColor()),
                  metricLine("Units", to_string(snapshot.totalQuantity), uiSuccessColor()),
                  metricLine("Categories", to_string(snapshot.categoryCount), uiLabelColor()),
                  metricLine("Bins", to_string(snapshot.locationCount), uiInfoColor()),
              }) | ftxui::flex,
              ftxui::separator(),
              ftxui::vbox({
                  metricLine("Low stock", to_string(snapshot.lowStockCount), uiWarnColor()),
                  metricLine("Out of stock", to_string(snapshot.outOfStockCount), uiDangerColor()),
                  metricLine("Data errors", to_string(snapshot.dataErrorCount), uiDangerColor()),
                  metricLine("Metadata", to_string(snapshot.missingMetadataCount), uiWarnColor()),
              }) | ftxui::flex,
          }),
          ftxui::separator(),
          ratioLine("Stock health", snapshot.stockHealth, uiSuccessColor()),
          metricLine("Last update", timestampOrDash(snapshot.lastUpdated), uiAccentColor()),
          metricLine("Last modified", snapshot.lastUpdatedPart.empty() ? string("n/a") : snapshot.lastUpdatedPart,
                     uiInfoColor()),
          metricLine("Last scan", snapshot.lastScannedPart.empty() ? string("n/a") : snapshot.lastScannedPart,
                     uiLinkColor()),
      },
      uiAccentColor(), uiAccentColor()) |
      ftxui::bgcolor(uiPanelRightBg()) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, rightOuterWidth);

  const int warningStatusWidth = 6;
  const int warningQuantityWidth = 8;
  const int warningCategoryWidth = clamp(leftInnerWidth / 8, 10, 18);
  const int warningLocationWidth = clamp(leftInnerWidth / 6, 12, 24);
  const int warningPartWidth = max(18, leftInnerWidth - warningStatusWidth - warningQuantityWidth - warningCategoryWidth -
                                           warningLocationWidth - 12);
  const size_t warningVisibleCount = snapshot.stockWarnings.empty()
                                         ? 0
                                         : static_cast<size_t>(max(16, min(static_cast<int>(snapshot.stockWarnings.size()),
                                                                          max(16, screenHeight - 10))));
  const size_t warningOffset = animatedOffset(snapshot.stockWarnings.empty() ? 1 : snapshot.stockWarnings.size(), 1000);

  const int categoryNameWidth = clamp(rightInnerWidth / 5, 18, 28);
  const int categoryCountWidth = 5;
  const int categoryBarWidth = clamp(rightInnerWidth / 6, 14, 20);

  ftxui::Elements recentActivityRows;
  if (snapshot.recentEvents.empty()) {
    recentActivityRows.push_back(styledText("No activity yet.", uiMutedColor()));
  } else {
    for (const auto& line : snapshot.recentEvents) {
      recentActivityRows.push_back(styledText(ellipsize(line, 72), uiMutedColor()));
    }
  }
  const auto recentEventsPanel = panel("Recent events", move(recentActivityRows), uiInfoColor(), uiPanelRightBg()) |
                                 ftxui::bgcolor(uiPanelRightBg());

  const auto warningFlashing = pulseOn(500);
  const auto stockWarningsPanel = stockWarningSection("Stock warnings", snapshot.stockWarnings, uiWarnColor(),
                                                      warningFlashing, warningVisibleCount, warningOffset,
                                                      warningStatusWidth, warningPartWidth, warningQuantityWidth,
                                                      warningCategoryWidth, warningLocationWidth);

  const size_t maxHistoryUnits = [&]() {
    size_t maxUnits = 0;
    for (const auto& point : snapshot.history) {
      maxUnits = max(maxUnits, point.totalUnits);
    }
    return maxUnits;
  }();

  ftxui::Elements historyRows;
  if (snapshot.history.empty()) {
    historyRows.push_back(styledText("No history yet. A snapshot is saved when the inventory changes.", uiMutedColor()));
  } else {
    const auto visibleCount = min<size_t>(snapshot.history.size(), 5);
    for (size_t offset = 0; offset < visibleCount; ++offset) {
      const auto& point = snapshot.history[snapshot.history.size() - 1 - offset];
      historyRows.push_back(historyRow(point, maxHistoryUnits));
      if (offset + 1 < visibleCount) {
        historyRows.push_back(ftxui::separator());
      }
    }
  }
  const auto historyPanel = panel("Inventory history", move(historyRows), uiInfoColor(), uiPanelRightBg()) |
                            ftxui::bgcolor(uiPanelRightBg());

  ftxui::Elements categoryRows;
  if (snapshot.categories.empty()) {
    categoryRows.push_back(styledText("No categories yet.", uiMutedColor()));
  } else {
    const auto visibleCount = min<size_t>(snapshot.categories.size(), 3);
    const auto maxCount = snapshot.categories.front().count;
    categoryRows.push_back(ftxui::hbox({
        fixedCell("Category", categoryNameWidth, uiMutedColor()),
        styledText(" | ", uiDimColor()),
        fixedCell("Count", categoryCountWidth, uiMutedColor(), true),
        styledText(" | ", uiDimColor()),
        fixedCell("Share", categoryBarWidth, uiMutedColor()),
        ftxui::filler(),
    }) | ftxui::bgcolor(uiRowDarkBg()));
    for (size_t index = 0; index < visibleCount; ++index) {
      categoryRows.push_back(categoryRow(snapshot.categories[index], maxCount, index, categoryNameWidth,
                                         categoryCountWidth, categoryBarWidth));
      if (index + 1 < visibleCount) {
        categoryRows.push_back(ftxui::separator());
      }
    }
  }
  const auto categoryPanel = panel("Parts by category", move(categoryRows), uiLabelColor(), uiPanelRightBg()) |
                             ftxui::bgcolor(uiPanelRightBg());

  ftxui::Elements deviceRows;
  for (size_t index = 0; index < snapshot.devices.size(); ++index) {
    deviceRows.push_back(deviceRow(snapshot.devices[index]));
    if (index + 1 < snapshot.devices.size()) {
      deviceRows.push_back(ftxui::separator());
    }
  }
  const auto devicesPanel = panel("Device status", move(deviceRows), uiAccentColor(), uiPanelRightBg()) |
                            ftxui::bgcolor(uiPanelRightBg());

  const auto rightColumn = ftxui::vbox({
      overviewPanel,
      ftxui::separator(),
      recentEventsPanel,
      ftxui::separator(),
      historyPanel,
      ftxui::separator(),
      devicesPanel,
      ftxui::separator(),
      categoryPanel,
  }) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, rightOuterWidth) | ftxui::flex;

  const auto leftColumn = ftxui::vbox({
      stockWarningsPanel | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, leftOuterWidth),
  }) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, leftOuterWidth) | ftxui::flex;

  ftxui::Elements shortcutRows;
  if (stackedLayout) {
    shortcutRows.push_back(ftxui::hbox({
        keyChip("S", "Scan/Add stock", uiLinkColor(), ftxui::Color::RGB(18, 38, 52)),
        ftxui::filler(),
        keyChip("U", "Use/remove stock", uiWarnColor(), ftxui::Color::RGB(52, 38, 18)),
        ftxui::filler(),
        keyChip("F", "Find part", uiInfoColor(), ftxui::Color::RGB(18, 30, 48)),
    }));
    shortcutRows.push_back(ftxui::separator());
    shortcutRows.push_back(ftxui::hbox({
        keyChip("L", "Print label", uiLabelColor(), ftxui::Color::RGB(32, 26, 52)),
        ftxui::filler(),
        keyChip("R", "Refresh", uiSuccessColor(), ftxui::Color::RGB(18, 42, 28)),
        ftxui::filler(),
        keyChip("D", "Database", uiDangerColor(), ftxui::Color::RGB(50, 24, 28)),
        ftxui::filler(),
        keyChip("Q", "Quit", uiMutedColor(), ftxui::Color::RGB(32, 32, 32)),
    }));
  } else {
    shortcutRows.push_back(ftxui::hbox({
        keyChip("S", "Scan/Add stock", uiLinkColor(), ftxui::Color::RGB(18, 38, 52)),
        ftxui::filler(),
        keyChip("U", "Use/remove stock", uiWarnColor(), ftxui::Color::RGB(52, 38, 18)),
        ftxui::filler(),
        keyChip("F", "Find part", uiInfoColor(), ftxui::Color::RGB(18, 30, 48)),
        ftxui::filler(),
        keyChip("L", "Print label", uiLabelColor(), ftxui::Color::RGB(32, 26, 52)),
        ftxui::filler(),
        keyChip("R", "Refresh", uiSuccessColor(), ftxui::Color::RGB(18, 42, 28)),
        ftxui::filler(),
        keyChip("D", "Database", uiDangerColor(), ftxui::Color::RGB(50, 24, 28)),
        ftxui::filler(),
        keyChip("Q", "Quit", uiMutedColor(), ftxui::Color::RGB(32, 32, 32)),
    }));
  }

  const auto shortcutsPanel = panel("Keyboard", move(shortcutRows), uiAccentColor(), uiPanelLeftBg()) |
                              ftxui::bgcolor(uiPanelLeftBg());

  ftxui::Element content;
  if (stackedLayout) {
    content = ftxui::vbox({
        overviewPanel,
        ftxui::separator(),
        stockWarningsPanel,
        ftxui::separator(),
        recentEventsPanel,
        ftxui::separator(),
        historyPanel,
        ftxui::separator(),
        devicesPanel,
        ftxui::separator(),
        categoryPanel,
        ftxui::separator(),
        shortcutsPanel,
    });
  } else {
    content = ftxui::vbox({
        ftxui::hbox({
            leftColumn,
            ftxui::separator(),
            rightColumn,
        }),
        ftxui::separator(),
        shortcutsPanel,
    });
  }

  return content;
}

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
      case 'r':
      case 'd':
        store_.load(inventoryPath_);
        loadInventoryHistory(inventoryPath_, inventoryHistory_);
        if (inventoryHistory_.empty()) {
          appendInventoryHistory(inventoryHistory_, makeInventoryHistoryPoint(store_.items()));
        }
        saveInventoryHistory(inventoryPath_, inventoryHistory_);
        setMessage("Inventory reloaded from the database", 2);
        break;
      case 'u':
        changePage(Page::Stock);
        setMessage("Use/remove stock from the stock browser with +/-", 3);
        break;
      case 'f':
        changePage(Page::Stock);
        startSearch();
        break;
      case 'l':
        setMessage("Label printing is not connected yet", 3);
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

void App::renderDashboard(ostringstream& out, const ConsoleSize& size) {
  const auto summary = summarize(store_.items());
  const int contentRows = max(6, size.rows - 6);

  out << kColorAccent << "Dashboard" << kColorReset << "  ";
  out << kColorMuted << "Overview, alerts, and activity" << kColorReset << '\n';
  out << string(max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    vector<string> lines;
    lines.push_back(styleText("Dashboard panels are optimized for the FTXUI view.", kColorMuted));
    lines.push_back(styleText("Use the terminal UI for the structured dashboard.", kColorWarn));
    renderColumns(out, lines, {}, max(40, size.columns - 4), 0, contentRows);
    return;
  }

  vector<string> leftLines;
  leftLines.push_back("Inventory overview");
  leftLines.push_back("  Parts: " + to_string(summary.itemCount));
  leftLines.push_back("  Units: " + to_string(summary.totalUnits));
  leftLines.push_back("  Low stock: " + to_string(summary.lowStockCount));
  leftLines.push_back("  Missing metadata: " + to_string(summary.missingMetadataCount));
  leftLines.push_back("  Unsynced: " + to_string(summary.unsyncedCount));
  leftLines.push_back("  Database history stored in inventory.db");

  vector<string> rightLines;
  rightLines.push_back("Recent activity");
  const auto activityCount = min<size_t>(activities_.size(), 5);
  for (size_t offset = 0; offset < activityCount; ++offset) {
    const auto& entry = activities_[activities_.size() - 1 - offset];
    rightLines.push_back(nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message);
  }
  if (activityCount == 0) {
    rightLines.push_back("No recent activity.");
  }

  renderColumns(out, leftLines, rightLines, max(34, size.columns / 2 - 3), max(34, size.columns / 2 - 3), contentRows);
}

}  // namespace hims
