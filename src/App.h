// HIMS - Hardware Inventory Management System
// Terminal application controller and shared app state.

#pragma once

#include "core/Inventory.h"
#include "platform/Console.h"
#include "platform/HttpServer.h"

#include <ftxui/dom/elements.hpp>

#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace hims {

using namespace std;

class App {
 public:
  App();
  int run();

 private:
  enum class Page {
    Dashboard,
    Stock,
    Detail,
  };

  enum class InputMode {
    None,
    Search,
    EditFieldMenu,
    EditValue,
  };

  enum class EditField {
    PartName,
    Manufacturer,
    Category,
    Quantity,
    ReorderThreshold,
    Location,
    Tags,
    Parameters,
    Notes,
    DigiKeyPart,
    DatasheetUrl,
    ProductUrl,
    Sku,
    SyncStatus,
  };

  struct FieldOption {
    string label;
    EditField field;
  };

  struct WorkingCopy {
    InventoryItem item;
    bool isNew = false;
    size_t originalIndex = 0;
  };

  void loadState();
  void saveState();
  void processInput();
  void handleKey(const KeyEvent& key);
  void handleDashboardKey(const KeyEvent& key);
  void handleStockKey(const KeyEvent& key);
  void handleDetailKey(const KeyEvent& key);
  void handleSearchKey(const KeyEvent& key);
  void handleEditMenuKey(const KeyEvent& key);
  void handleEditValueKey(const KeyEvent& key);

  void render();
  void renderDashboard(ostringstream& out, const ConsoleSize& size);
  void renderStock(ostringstream& out, const ConsoleSize& size);
  void renderDetail(ostringstream& out, const ConsoleSize& size);
  void renderSearchBar(ostringstream& out, const ConsoleSize& size);
  void renderStatusBar(ostringstream& out, const ConsoleSize& size);
  void renderMessage(ostringstream& out, const ConsoleSize& size);

  ftxui::Element renderUi() const;
  ftxui::Element renderDashboardUi() const;
  ftxui::Element renderStockUi() const;
  ftxui::Element renderDetailUi() const;
  ftxui::Element renderSearchBarUi() const;
  ftxui::Element renderStatusBarUi() const;
  ftxui::Element renderMessageUi() const;
  ftxui::Element renderPageUi() const;

  void setMessage(string text, int seconds = 3);
  bool messageVisible() const;
  void clearMessageIfExpired();
  void markDirty();

  vector<size_t> filteredIndices() const;
  size_t selectedIndex() const;
  InventoryItem* selectedItem();
  const InventoryItem* selectedItem() const;
  void syncSelectionToFilter();
  void moveSelection(int delta);
  void changePage(Page page);
  void openSelectedDetail();
  void startSearch();
  void cancelInput();
  void beginEditCurrentItem(bool createNew);
  void openFieldMenu();
  void commitEditField(EditField field, const string& value);
  void saveWorkingCopy();
  void adjustQuantity(int delta);
  void logActivity(const string& kind, const string& message);
  void pushScanCode(const string& code);
  void processScans();
  void openCurrentUrl(const string& url, const string& label);
  string fieldLabel(EditField field) const;
  string currentFieldValue(EditField field) const;
  vector<FieldOption> fieldOptions() const;
  string itemDetailText(const InventoryItem& item, int width) const;
  string summaryLine() const;
  string scannerUrl() const;
  string activePrompt() const;

  InventoryStore store_;
  vector<ActivityEntry> activities_;
  LocalHttpServer server_;
  filesystem::path root_;
  filesystem::path dataPath_;
  filesystem::path inventoryPath_;
  filesystem::path activityPath_;
  Page page_ = Page::Dashboard;
  InputMode inputMode_ = InputMode::None;
  string searchQuery_;
  string inputBuffer_;
  string message_;
  time_t messageUntil_ = 0;
  size_t selectedPosition_ = 0;
  size_t stockScroll_ = 0;
  size_t detailScroll_ = 0;
  vector<string> scanQueue_;
  vector<InventoryHistoryPoint> inventoryHistory_;
  mutex scanMutex_;
  bool running_ = true;
  bool dirty_ = true;
  ConsoleSize lastDrawSize_{};
  WorkingCopy workingCopy_;
  int fieldMenuIndex_ = 0;
  vector<FieldOption> menuOptions_;
};

}  // namespace hims

