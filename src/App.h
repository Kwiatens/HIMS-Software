// HIMS - Hardware Inventory Management System
// Terminal application controller and shared app state.

#pragma once

#include "core/Inventory.h"
#include "import/DigiKeyCsvImport.h"
#include "label_printer/LabelPrinter.h"
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
    PrinterSetup,
    ImportCsv,
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
  void handlePrinterSetupKey(const KeyEvent& key);
  void handleImportCsvKey(const KeyEvent& key);
  void handleSearchKey(const KeyEvent& key);
  void handleEditMenuKey(const KeyEvent& key);
  void handleEditValueKey(const KeyEvent& key);

  void render();
  void renderDashboard(ostringstream& out, const ConsoleSize& size);
  void renderStock(ostringstream& out, const ConsoleSize& size);
  void renderDetail(ostringstream& out, const ConsoleSize& size);
  void renderImportCsv(ostringstream& out, const ConsoleSize& size);
  void renderSearchBar(ostringstream& out, const ConsoleSize& size);
  void renderStatusBar(ostringstream& out, const ConsoleSize& size);
  void renderMessage(ostringstream& out, const ConsoleSize& size);

  ftxui::Element renderUi() const;
  ftxui::Element renderDashboardUi() const;
  ftxui::Element renderStockUi() const;
  ftxui::Element renderDetailUi() const;
  ftxui::Element renderPrinterSetupUi() const;
  ftxui::Element renderImportCsvUi() const;
  ftxui::Element renderSearchBarUi() const;
  ftxui::Element renderStatusBarUi() const;
  ftxui::Element renderMessageUi() const;
  ftxui::Element renderPageUi() const;

  void setMessage(string text, int seconds = 3);
  bool messageVisible() const;
  void clearMessageIfExpired();
  void markDirty();
  void refreshPrinterState();
  void openPrinterSetup();
  bool printSelectedLabel();
  string printerSummary() const;

  vector<size_t> filteredIndices() const;
  size_t selectedIndex() const;
  InventoryItem* selectedItem();
  const InventoryItem* selectedItem() const;
  PrinterQueueInfo* selectedPrinterQueue();
  const PrinterQueueInfo* selectedPrinterQueue() const;
  void syncSelectionToFilter();
  void moveSelection(int delta);
  void changePage(Page page);
  void armDeleteConfirmation();
  void cancelDeleteConfirmation();
  void clearDeleteConfirmationIfExpired();
  void confirmDeleteSelectedItem();
  bool deleteConfirmationActive() const;
  bool deleteConfirmationReady() const;
  int deleteConfirmationSecondsLeft() const;
  void openSelectedDetail();
  void startSearch();
  void cancelInput();
  void beginEditCurrentItem(bool createNew);
  void beginEditImportCandidate();
  void openFieldMenu();
  void commitEditField(EditField field, const string& value);
  void saveWorkingCopy();
  void adjustQuantity(int delta);
  void logActivity(const string& kind, const string& message);
  void pushScanCode(const string& code);
  void processScans();
  void beginCsvImport();
  void moveImportSelection(int delta);
  void acceptImportCandidate();
  void skipImportCandidate();
  void finishImportReview();
  void finishCsvImport(bool syncWithDigiKey);
  void syncAcceptedImports();
  CsvImportCandidate* currentImportCandidate();
  const CsvImportCandidate* currentImportCandidate() const;
  string importCompletionMessage() const;
  void openCurrentUrl(const string& url, const string& label);
  string fieldLabel(EditField field) const;
  string currentFieldValue(EditField field) const;
  vector<FieldOption> fieldOptions() const;
  string itemDetailText(const InventoryItem& item, int width) const;
  string summaryLine() const;
  string scannerUrl() const;
  string activePrompt() const;
  string shortcutSummary() const;

  InventoryStore store_;
  vector<ActivityEntry> activities_;
  LabelPrinterService printerService_;
  vector<PrinterQueueInfo> printerQueues_;
  PrinterCheckResult printerCheck_;
  LocalHttpServer server_;
  filesystem::path root_;
  filesystem::path dataPath_;
  filesystem::path inventoryPath_;
  filesystem::path printerPath_;
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
  vector<CsvImportCandidate> importCandidates_;
  vector<string> importAcceptedItemIds_;
  filesystem::path importSourcePath_;
  vector<InventoryHistoryPoint> inventoryHistory_;
  mutex scanMutex_;
  bool running_ = true;
  bool dirty_ = true;
  ConsoleSize lastDrawSize_{};
  WorkingCopy workingCopy_;
  bool editingImportCandidate_ = false;
  size_t importEditIndex_ = 0;
  size_t importSelection_ = 0;
  bool importSyncPrompt_ = false;
  int importCreatedCount_ = 0;
  int importMergedCount_ = 0;
  int importSkippedCount_ = 0;
  int importSyncedCount_ = 0;
  int importSyncFailedCount_ = 0;
  int fieldMenuIndex_ = 0;
  vector<FieldOption> menuOptions_;
  string deleteConfirmationItemId_;
  time_t deleteConfirmationUntil_ = 0;
  size_t printerSelection_ = 0;
};

}  // namespace hims

