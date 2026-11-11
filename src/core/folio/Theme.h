// SPDX-License-Identifier: GPL-2.0-or-later
// Foliq design tokens: colour palette, contrast-safe accent derivation and the
// view preferences format (view.ini). GTK-free so it can be unit-tested.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace foliq {
struct Rgb {
    double r = 0, g = 0, b = 0;  // 0..1
};
bool parseHex(const std::string& hex, Rgb& out);  // strict "#rrggbb"
std::string toHex(const Rgb& c);
double luminance(const Rgb& c);  // WCAG relative luminance
double contrast(const Rgb& a, const Rgb& b);

/// Shell colours; the paper colour always comes from the document.
struct Palette {
    bool dark = false;
    std::string accent;         // as chosen by the user
    std::string accentVisible;  // adjusted to reach >= 3:1 against the shell background
    std::string onAccent;       // text/icon colour on solid accent (black or white, higher contrast)
    std::string background, canvas, text;
    uint32_t canvasRgb() const;
    uint32_t accentRgb() const;
    std::string css() const;  // @define-color overrides for ui/xournalpp.css tokens
};
Palette makePalette(bool dark, const std::string& accent);

/// Built-in accent and section colour choices (own selection, not Microsoft assets).
const std::vector<std::pair<std::string, std::string>>& accentChoices();

/// Process-wide current palette for drawing code (canvas, current page outline).
const Palette& currentPalette();
void setCurrentPalette(const Palette& p);

/// view.ini: user-specific view options. Version 2 adds ribbon/navigation pinning,
/// density, motion, accent and section colours. Version 1 files are migrated in memory
/// and rewritten as version 2 on the next regular save.
struct ViewOptions {
    static constexpr int VERSION = 2;
    int books = 200, width = 420, theme = 0, tab = 0;
    bool focus = false, ribbonPinned = true, navPinned = true, touch = false, reducedMotion = false;
    std::string section, accent = "#005fb8";
    std::vector<std::string> expanded;
    std::map<std::string, std::string> sectionColors;  // node id -> #rrggbb
};
/// Throws std::runtime_error for damaged files or unknown (newer) versions.
ViewOptions parseView(const std::string& bytes);
std::string serializeView(const ViewOptions& v);
}  // namespace foliq
