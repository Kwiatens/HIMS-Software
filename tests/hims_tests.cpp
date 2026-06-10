#include "core/Inventory.h"
#include "import/DigiKeyCsvImport.h"

#include <cstdlib>
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace hims;
using namespace std;

int main() {
  {
    auto items = seedInventory();
    assert(!items.empty());

    const auto filtered = filterItems(items, "cat:resistors qty>100");
    assert(filtered.size() == 1);
    assert(items[filtered[0]].id == "res-0603-10k");

    const auto tagFiltered = filterItems(items, "tag:module param:Flash=16MB");
    assert(tagFiltered.size() == 1);
    assert(items[tagFiltered[0]].id == "esp32-s3-module");
  }

  {
    auto items = seedInventory();
    InventoryStore store;
    store.items() = items;

    const auto resolution = resolveScanCode(store, "311-10.0KHRCT-ND");
    assert(resolution.matched);
    assert(!resolution.created);
    assert(resolution.itemId == "res-0603-10k");

    const auto created = resolveScanCode(store, "new-digikey-code");
    assert(created.matched);
    assert(created.created);
    assert(store.findById(created.itemId) != nullptr);
  }

  {
    InventoryItem item;
    item.id = "abc";
    item.partName = "Test Part";
    item.manufacturer = "Acme";
    item.category = "Test";
    item.quantity = 2;
    item.reorderThreshold = 1;
    item.location = "Bin 1";
    item.tags = {"alpha", "beta"};
    item.parameters = {{"Voltage", "5V"}, {"Package", "0805"}};
    item.notes = "Roundtrip test";
    item.digikeyPartNumber = "123";
    item.datasheetUrl = "https://example.com/datasheet";
    item.productUrl = "https://example.com/product";
    item.syncStatus = "synced";
    item.sku = "SKU-1";
    item.lastUpdated = 1710000000;

    const auto line = serializeItem(item);
    InventoryItem restored;
    assert(deserializeItem(line, restored));
    assert(restored.partName == item.partName);
    assert(restored.parameters.size() == 2);
    assert(restored.tags.size() == 2);
  }

  {
    const string csv =
        "Indeks,Nr kat. DigiKey,Manufacturer Part Number,Producent,Opis,Numer referencyjny klienta,Ilość,"
        "Niezrealizowana pozycja zamówienia,Cena jednostkowa,Wartość\n"
        "1,308-1571-1-ND,CDMC6D28NP-4R7MC,Sumida America Components Inc.,FIXED IND 4.7UH 3.7A 46.4 MOHM,,10,0,"
        "\"2,62800 zł\",\"26,28 zł\"\n";

    const auto result = parseDigiKeyCsvText(csv, {});
    assert(result.ok);
    assert(result.candidates.size() == 1);
    const auto& candidate = result.candidates.front();
    assert(candidate.item.digikeyPartNumber == "308-1571-1-ND");
    assert(candidate.item.sku == "CDMC6D28NP-4R7MC");
    assert(candidate.item.manufacturer == "Sumida America Components Inc.");
    assert(candidate.item.quantity == 10);
    assert(candidate.item.category == "Inductors");
    assert(candidate.item.parameters.size() == 4);
  }

  {
    const string csv =
        "Index,Digi-Key Part Number,Manufacturer Part Number,Manufacturer,Description,Quantity,Unit Price,Extended Price\n"
        "1,399-C0603C105K4RACTUCT-ND,C0603C105K4RACTU,KEMET,CAP CER 1UF 16V X7R 0603,50,\"0,13420 zł\",\"6,71 zł\"\n";

    auto existing = seedInventory();
    InventoryItem duplicate;
    duplicate.id = "existing-cap";
    duplicate.partName = "Existing Cap";
    duplicate.manufacturer = "KEMET";
    duplicate.category = "Capacitors";
    duplicate.quantity = 7;
    duplicate.digikeyPartNumber = "399-C0603C105K4RACTUCT-ND";
    duplicate.sku = "C0603C105K4RACTU";
    existing.push_back(duplicate);

    const auto result = parseDigiKeyCsvText(csv, existing);
    assert(result.ok);
    assert(result.candidates.size() == 1);
    assert(result.candidates.front().hasConflict);
    assert(result.candidates.front().existingItemId == "existing-cap");
    assert(result.candidates.front().matchedField == "DigiKey part");

    mergeImportedMetadata(duplicate, result.candidates.front().item);
    duplicate.quantity += result.candidates.front().item.quantity;
    assert(duplicate.quantity == 57);
  }

  {
    const string csv = "Digi-Key Part Number,Manufacturer,Description,Quantity\n"
                       "123-ND,Acme,Missing manufacturer part,3\n";
    const auto result = parseDigiKeyCsvText(csv, {});
    assert(!result.ok);
  }

  {
    const char* profile = getenv("USERPROFILE");
    if (profile != nullptr && *profile != '\0') {
      const auto dbPath = filesystem::path(profile) / "Documents" / "HIMS" / "inventory.db";
      if (filesystem::exists(dbPath)) {
        InventoryStore store;
        const auto loaded = store.load(dbPath);
        if (!loaded) {
          return 1;
        }
        if (store.items().size() <= 5) {
          return 1;
        }
      }
    }
  }

  cout << "HIMS core tests passed\n";
  return 0;
}

