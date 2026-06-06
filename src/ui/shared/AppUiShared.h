// HIMS - Hardware Inventory Management System
// Shared terminal UI formatting and inventory detail helpers.

#pragma once

#include "core/Inventory.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace hims {

using namespace std;

inline constexpr const char* kColorReset = "\x1b[0m";
inline constexpr const char* kColorTitle = "\x1b[38;5;81m";
inline constexpr const char* kColorAccent = "\x1b[38;5;49m";
inline constexpr const char* kColorInfo = "\x1b[38;5;117m";
inline constexpr const char* kColorSuccess = "\x1b[38;5;114m";
inline constexpr const char* kColorLink = "\x1b[38;5;87m";
inline constexpr const char* kColorLabel = "\x1b[38;5;111m";
inline constexpr const char* kColorMuted = "\x1b[38;5;243m";
inline constexpr const char* kColorWarn = "\x1b[38;5;214m";
inline constexpr const char* kColorDanger = "\x1b[38;5;203m";
inline constexpr const char* kColorDim = "\x1b[38;5;245m";
inline constexpr const char* kColorSelect = "\x1b[48;5;236m";
inline constexpr const char* kBgPanelLeft = "\x1b[48;5;233m";
inline constexpr const char* kBgPanelRight = "\x1b[48;5;235m";
inline constexpr const char* kBgRowDark = "\x1b[48;5;232m";
inline constexpr const char* kBgRowLight = "\x1b[48;5;236m";
inline constexpr const char* kBgRowSelected = "\x1b[48;5;24m";

struct DetailField {
  string label;
  string value;
  ftxui::Color labelColor;
  ftxui::Color valueColor;
};

ftxui::Color uiTitleColor();
ftxui::Color uiAccentColor();
ftxui::Color uiInfoColor();
ftxui::Color uiSuccessColor();
ftxui::Color uiLinkColor();
ftxui::Color uiLabelColor();
ftxui::Color uiWarnColor();
ftxui::Color uiDangerColor();
ftxui::Color uiMutedColor();
ftxui::Color uiDimColor();
ftxui::Color uiPanelLeftBg();
ftxui::Color uiPanelRightBg();
ftxui::Color uiRowDarkBg();
ftxui::Color uiRowLightBg();
ftxui::Color uiRowSelectedBg();

string padRight(string value, int width);
string styleText(const string& text, const char* fg = nullptr, const char* bg = nullptr);
string styleCell(const string& text, int width, const char* fg = nullptr, const char* bg = nullptr);

ftxui::Element styledText(const string& text, optional<ftxui::Color> fg = nullopt,
                          optional<ftxui::Color> bg = nullopt);
ftxui::Element fullLine(const string& text, optional<ftxui::Color> fg = nullopt,
                        optional<ftxui::Color> bg = nullopt);
ftxui::Element bulletLine(const string& label, const string& value, ftxui::Color labelColor,
                          ftxui::Color valueColor);
ftxui::Element panel(const string& title, ftxui::Elements body, optional<ftxui::Color> titleColor = nullopt,
                     optional<ftxui::Color> borderColor = nullopt);
ftxui::Element quantityBadge(int quantity, bool selected = false);

string displayCategory(const string& category);
string ellipsize(const string& value, size_t maxLength);
vector<string> splitLines(const string& text);
vector<string> wrapText(const string& text, int width);
void renderColumns(ostringstream& out, const vector<string>& leftLines,
                   const vector<string>& rightLines, int leftWidth, int rightWidth, int maxRows,
                   int gap = 4, const char* leftFg = nullptr, const char* leftBg = nullptr,
                   const char* rightFg = nullptr, const char* rightBg = nullptr);
void appendWrapped(vector<string>& lines, const string& text, int width);
void appendWrappedStyled(vector<string>& lines, const string& text, int width, const char* fg);

string joinTags(const vector<string>& tags);
string renderTags(const vector<string>& tags);
string renderParameters(const vector<Parameter>& parameters);
string renderUrl(const string& url);

string normalizeKey(string value);
bool categoryContains(const InventoryItem& item, initializer_list<const char*> needles);
bool parameterLabelMatches(const string& lhs, const string& rhs);
bool looksLikePackagingValue(const string& value);
const Parameter* findParameter(const vector<Parameter>& parameters, initializer_list<const char*> names);
optional<string> parameterValue(const InventoryItem& item, initializer_list<const char*> names);
string prettyLabel(const string& label);

vector<DetailField> electricalFieldsForItem(const InventoryItem& item);
vector<DetailField> stockPreviewFields(const InventoryItem& item);
vector<DetailField> detailCoreFields(const InventoryItem& item);
ftxui::Element detailFieldLine(const DetailField& field, int width);

}  // namespace hims

