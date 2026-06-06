// HIMS - Hardware Inventory Management System
// Dashboard page rendering and keyboard handling.

#include "App.h"

#include "ui/shared/AppUiShared.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace hims {

using namespace std;

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

    renderColumns(out, lines, {}, max(40, size.columns - 4), 0, contentRows);
    return;
  }

  const int leftWidth = max(34, size.columns / 2 - 3);
  const int rightWidth = max(34, size.columns - leftWidth - 4);

  vector<string> leftLines;
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
  leftLines.push_back("1 Stock browser   2/s Scanner   3 Add item   4 Reload   / Search   q Quit");

  vector<string> rightLines;
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
    appendWrapped(rightLines, "  - " + nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message,
                  rightWidth);
  }
  if (activityCount == 0) {
    rightLines.push_back("  No recent activity.");
  }

  renderColumns(out, leftLines, rightLines, leftWidth, rightWidth, contentRows);
}

}  // namespace hims

