// HIMS - Hardware Inventory Management System
// Core inventory string, identifier, and item helpers.

#include "core/InventoryInternals.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace hims {

using namespace std;

namespace {

vector<string> splitTokensRespectingQuotes(const string& query) {
  vector<string> tokens;
  string current;
  bool inQuotes = false;

  for (char ch : query) {
    if (ch == '"') {
      inQuotes = !inQuotes;
      continue;
    }

    if (!inQuotes && isspace(static_cast<unsigned char>(ch))) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty()) {
    tokens.push_back(current);
  }

  return tokens;
}

string normalizeHimsPrefix(const string& category) {
  const auto lowered = toLower(trim(category));
  if (lowered.find("capacitor") != string::npos) {
    return "C";
  }
  if (lowered.find("resistor") != string::npos) {
    return "R";
  }
  if (lowered.find("inductor") != string::npos || lowered.find("choke") != string::npos ||
      lowered.find("coil") != string::npos) {
    return "L";
  }
  if (lowered.find("indicator") != string::npos || lowered.find("led") != string::npos) {
    return "I";
  }
  if (lowered.find("connector") != string::npos) {
    return "J";
  }
  if (lowered.find("microcontroller") != string::npos || lowered.find("mcu") != string::npos) {
    return "U";
  }
  if (lowered.find("integrated circuit") != string::npos || lowered == "ic" || lowered.find("ic ") != string::npos) {
    return "U";
  }
  if (lowered.find("sensor") != string::npos) {
    return "S";
  }
  if (lowered.find("switch") != string::npos) {
    return "K";
  }
  if (lowered.find("diode") != string::npos || lowered.find("rectifier") != string::npos) {
    return "D";
  }
  if (lowered.find("fuse") != string::npos) {
    return "F";
  }
  if (lowered.find("relay") != string::npos) {
    return "Y";
  }
  if (lowered.find("transistor") != string::npos || lowered.find("mosfet") != string::npos ||
      lowered.find("fet") != string::npos) {
    return "T";
  }

  for (char ch : lowered) {
    if (isalpha(static_cast<unsigned char>(ch))) {
      return string(1, static_cast<char>(toupper(static_cast<unsigned char>(ch))));
    }
  }

  return "X";
}

bool parseHimsIdValue(const string& value, string& prefix, size_t& sequence) {
  const auto trimmed = trim(value);
  if (trimmed.rfind("HIMS:", 0) != 0) {
    return false;
  }

  const auto dash = trimmed.find('-', 5);
  if (dash == string::npos || dash <= 5 || dash + 1 >= trimmed.size()) {
    return false;
  }

  const auto rawPrefix = trimmed.substr(5, dash - 5);
  if (rawPrefix.empty()) {
    return false;
  }

  size_t parsedSequence = 0;
  for (size_t index = dash + 1; index < trimmed.size(); ++index) {
    const char ch = trimmed[index];
    if (!isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }
    parsedSequence = parsedSequence * 10 + static_cast<size_t>(ch - '0');
  }

  prefix = rawPrefix;
  sequence = parsedSequence;
  return true;
}

}  // namespace

time_t nowEpoch() {
  return time(nullptr);
}

string sanitizeIdPart(const string& value) {
  string output;
  output.reserve(value.size());
  for (char ch : value) {
    if (isalnum(static_cast<unsigned char>(ch))) {
      output.push_back(static_cast<char>(tolower(static_cast<unsigned char>(ch))));
    } else if (!output.empty() && output.back() != '-') {
      output.push_back('-');
    }
  }

  while (!output.empty() && output.back() == '-') {
    output.pop_back();
  }

  if (output.empty()) {
    output = "item";
  }
  return output;
}

bool InventoryItem::lowStock() const {
  return quantity <= reorderThreshold;
}

bool InventoryItem::hasMissingMetadata() const {
  return partName.empty() || manufacturer.empty() || category.empty() || digikeyPartNumber.empty() ||
         datasheetUrl.empty() || productUrl.empty();
}

string InventoryItem::searchableText() const {
  ostringstream out;
  out << partName << ' ' << manufacturer << ' ' << category << ' ' << location << ' ' << notes << ' '
      << digikeyPartNumber << ' ' << datasheetUrl << ' ' << productUrl << ' ' << sku << ' ' << himsId << ' '
      << syncStatus;

  for (const auto& tag : tags) {
    out << ' ' << tag;
  }

  for (const auto& parameter : parameters) {
    out << ' ' << parameter.name << ':' << parameter.value;
  }

  return toLower(out.str());
}

string trim(const string& value) {
  const auto begin = find_if_not(value.begin(), value.end(), [](unsigned char ch) {
    return isspace(ch) != 0;
  });
  const auto end = find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
    return isspace(ch) != 0;
  }).base();

  if (begin >= end) {
    return {};
  }

  return string(begin, end);
}

string toLower(string value) {
  transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(tolower(ch));
  });
  return value;
}

string nowTimestampString(time_t value) {
  tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &value);
#else
  localtime_r(&value, &tm);
#endif
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &tm);
  return buffer;
}

string makeId() {
  const auto stamp = chrono::system_clock::now().time_since_epoch().count();
  mt19937_64 rng(static_cast<mt19937_64::result_type>(stamp));
  uniform_int_distribution<unsigned long long> dist;
  ostringstream out;
  out << hex << setw(10) << setfill('0') << (stamp & 0xfffffffffULL) << '-' << setw(10) << setfill('0')
      << (dist(rng) & 0xfffffffffULL);
  return out.str();
}

string himsCategoryPrefix(const string& category) {
  return normalizeHimsPrefix(category);
}

string makeHimsId(const string& category, size_t sequence) {
  ostringstream out;
  out << "HIMS:" << himsCategoryPrefix(category) << '-' << uppercase << setw(5) << setfill('0') << sequence;
  return out.str();
}

bool isHimsId(const string& value) {
  string prefix;
  size_t sequence = 0;
  return parseHimsIdValue(value, prefix, sequence);
}

void ensureInventoryIdentifiers(vector<InventoryItem>& items) {
  unordered_map<string, size_t> nextSequenceByPrefix;
  unordered_set<string> usedIds;

  for (const auto& item : items) {
    if (!trim(item.himsId).empty()) {
      usedIds.insert(toLower(trim(item.himsId)));
      string prefix;
      size_t sequence = 0;
      if (parseHimsIdValue(item.himsId, prefix, sequence)) {
        auto& nextSequence = nextSequenceByPrefix[prefix];
        nextSequence = max(nextSequence, sequence + 1);
      }
    }
  }

  for (auto& item : items) {
    if (item.createdAt == 0) {
      item.createdAt = item.lastUpdated == 0 ? nowEpoch() : item.lastUpdated;
    }

    auto normalizedId = trim(item.himsId);
    if (!normalizedId.empty() && isHimsId(normalizedId)) {
      item.himsId = normalizedId;
      continue;
    }

    const auto prefix = himsCategoryPrefix(item.category);
    auto& nextSequence = nextSequenceByPrefix[prefix];
    if (nextSequence == 0) {
      nextSequence = 1;
    }

    string candidate;
    do {
      candidate = makeHimsId(item.category, nextSequence++);
    } while (usedIds.count(toLower(candidate)) != 0);

    item.himsId = move(candidate);
    usedIds.insert(toLower(item.himsId));
  }
}

string join(const vector<string>& values, char delimiter) {
  ostringstream out;
  for (size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out << delimiter;
    }
    out << values[index];
  }
  return out.str();
}

vector<string> split(const string& value, char delimiter) {
  vector<string> values;
  string current;
  istringstream input(value);

  while (getline(input, current, delimiter)) {
    current = trim(current);
    if (!current.empty()) {
      values.push_back(current);
    }
  }

  return values;
}

vector<string> tokenizeQuery(const string& query) {
  return splitTokensRespectingQuotes(trim(query));
}

bool containsInsensitive(string_view haystack, string_view needle) {
  if (needle.empty()) {
    return true;
  }

  string loweredHaystack(haystack);
  string loweredNeedle(needle);
  loweredHaystack = toLower(move(loweredHaystack));
  loweredNeedle = toLower(move(loweredNeedle));
  return loweredHaystack.find(loweredNeedle) != string::npos;
}

}  // namespace hims
