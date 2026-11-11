// SPDX-License-Identifier: GPL-2.0-or-later
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

#include <glib.h>

namespace foliq {
bool parseHex(const std::string& hex, Rgb& out) {
    if (hex.size() != 7 || hex[0] != '#')
        return false;
    unsigned v[3];
    for (int i = 0; i < 3; ++i) {
        unsigned value = 0;
        for (int j = 0; j < 2; ++j) {
            char c = hex[1 + 2 * i + j];
            int d = g_ascii_xdigit_value(c);
            if (d < 0)
                return false;
            value = value * 16 + static_cast<unsigned>(d);
        }
        v[i] = value;
    }
    out = {v[0] / 255.0, v[1] / 255.0, v[2] / 255.0};
    return true;
}
std::string toHex(const Rgb& c) {
    auto ch = [](double v) { return static_cast<unsigned>(std::lround(std::clamp(v, 0.0, 1.0) * 255)); };
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", ch(c.r), ch(c.g), ch(c.b));
    return buf;
}
double luminance(const Rgb& c) {
    auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}
double contrast(const Rgb& a, const Rgb& b) {
    double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}
static Rgb mix(const Rgb& a, const Rgb& b, double t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
static uint32_t packed(const std::string& hex) {
    Rgb c;
    parseHex(hex, c);
    auto ch = [](double v) { return static_cast<uint32_t>(std::lround(v * 255)); };
    return 0xff000000u | (ch(c.r) << 16) | (ch(c.g) << 8) | ch(c.b);
}
uint32_t Palette::canvasRgb() const { return packed(canvas); }
uint32_t Palette::accentRgb() const { return packed(accentVisible); }

Palette makePalette(bool dark, const std::string& accent) {
    Palette p;
    p.dark = dark;
    Rgb a;
    if (!parseHex(accent, a)) {
        parseHex("#005fb8", a);
        p.accent = "#005fb8";
    } else
        p.accent = accent;
    p.background = dark ? "#202020" : "#f3f3f3";
    p.canvas = dark ? "#191919" : "#e7e7e9";
    p.text = dark ? "#ffffff" : "#1b1b1b";
    Rgb bg, white{1, 1, 1}, black{0, 0, 0};
    parseHex(p.background, bg);
    // Indicators and outlines need >= 3:1 against the shell (WCAG 1.4.11).
    Rgb visible = a;
    for (int i = 1; i <= 20 && contrast(visible, bg) < 3.0; ++i) visible = mix(a, dark ? white : black, i * 0.05);
    p.accentVisible = toHex(visible);
    p.onAccent = contrast(white, visible) >= contrast(black, visible) ? "#ffffff" : "#000000";
    return p;
}
std::string Palette::css() const {
    Rgb a;
    parseHex(accentVisible, a);
    auto rgba = [&](double alpha) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.2f)", int(std::lround(a.r * 255)),
                      int(std::lround(a.g * 255)), int(std::lround(a.b * 255)), alpha);
        return std::string(buf);
    };
    Rgb hover = mix(a, dark ? Rgb{1, 1, 1} : Rgb{0, 0, 0}, 0.12);
    std::string s;
    auto def = [&](const char* name, const std::string& value) {
        s += std::string("@define-color ") + name + " " + value + ";\n";
    };
    def("foliq_accent", accentVisible);
    def("foliq_accent_hover", toHex(hover));
    def("foliq_on_accent", onAccent);
    def("foliq_accent_subtle", rgba(dark ? 0.26 : 0.13));
    if (dark) {
        def("foliq_focus", "#ffffff");
        def("foliq_bg", "#202020");
        def("foliq_layer", "#272727");
        def("foliq_card", "#2c2c2c");
        def("foliq_text", "#ffffff");
        def("foliq_text_secondary", "#c8c8c8");
        def("foliq_text_disabled", "#787878");
        def("foliq_stroke", "rgba(255,255,255,0.08)");
        def("foliq_stroke_strong", "rgba(255,255,255,0.16)");
        def("foliq_hover", "rgba(255,255,255,0.06)");
        def("foliq_pressed", "rgba(255,255,255,0.10)");
        def("foliq_danger", "#ff99a4");
        def("foliq_danger_bg", "#442726");
        def("foliq_success", "#6ccb5f");
    }
    return s;
}
const std::vector<std::pair<std::string, std::string>>& accentChoices() {
    static const std::vector<std::pair<std::string, std::string>> choices = {
            {"Blau", "#005fb8"},  {"Petrol", "#03787c"}, {"Grün", "#107c10"},   {"Violett", "#7a4bc8"},
            {"Pflaume", "#9a0089"}, {"Rot", "#c42b1c"},  {"Orange", "#ca5010"}, {"Graphit", "#4c5562"},
    };
    return choices;
}
static Palette& palette() {
    static Palette p = makePalette(false, "#005fb8");
    return p;
}
const Palette& currentPalette() { return palette(); }
void setCurrentPalette(const Palette& p) { palette() = p; }

ViewOptions parseView(const std::string& bytes) {
    std::unique_ptr<GKeyFile, decltype(&g_key_file_unref)> owner(g_key_file_new(), g_key_file_unref);
    auto* key = owner.get();
    if (!g_key_file_load_from_data(key, bytes.data(), bytes.size(), G_KEY_FILE_NONE, nullptr))
        throw std::runtime_error("Beschädigte Ansichtsoptionen (view.ini).");
    auto integer = [&](const char* name, bool required, int fallback) {
        GError* error = nullptr;
        if (!required && !g_key_file_has_key(key, "view", name, nullptr))
            return fallback;
        auto value = g_key_file_get_integer(key, "view", name, &error);
        if (error) {
            g_error_free(error);
            throw std::runtime_error("Fehlende/ungültige Ansichtsoption in view.ini.");
        }
        return value;
    };
    auto boolean = [&](const char* name, bool required, bool fallback) {
        int v = integer(name, required, fallback ? 1 : 0);
        if (v != 0 && v != 1)
            throw std::runtime_error("Ungültige Ansichtsoptionen (view.ini).");
        return v == 1;
    };
    ViewOptions v;
    int version = integer("version", true, 0);
    if (version != 1 && version != 2)
        throw std::runtime_error("Unbekannte Version der Ansichtsoptionen (view.ini); Datei bleibt unverändert.");
    bool v2 = version == 2;
    v.books = integer("books", true, 0);
    v.width = integer("width", true, 0);
    v.theme = integer("theme", true, 0);
    v.focus = boolean("focus", true, false);
    v.tab = integer("tab", true, 0);
    v.ribbonPinned = boolean("ribbon", v2, true);
    v.navPinned = boolean("navpin", v2, true);
    v.touch = boolean("touch", v2, false);
    v.reducedMotion = boolean("motion", v2, false);
    if (v.tab < 0 || v.tab > 3 || v.books < 120 || v.width < 240 || v.width > 16384 || v.books >= v.width ||
        v.theme < 0 || v.theme > 2)
        throw std::runtime_error("Ungültige Ansichtsoptionen (view.ini).");
    if (auto* s = g_key_file_get_string(key, "view", "section", nullptr)) {
        v.section = s;
        g_free(s);
    }
    gsize count = 0;
    if (auto** list = g_key_file_get_string_list(key, "view", "expanded", &count, nullptr)) {
        for (gsize i = 0; i < count; ++i) v.expanded.emplace_back(list[i]);
        g_strfreev(list);
    }
    if (v2) {
        auto* accent = g_key_file_get_string(key, "view", "accent", nullptr);
        Rgb rgb;
        bool ok = accent && parseHex(accent, rgb);
        if (ok)
            v.accent = accent;
        g_free(accent);
        if (!ok)
            throw std::runtime_error("Ungültige Akzentfarbe in view.ini.");
        count = 0;
        if (auto** list = g_key_file_get_string_list(key, "view", "colors", &count, nullptr)) {
            for (gsize i = 0; i < count; ++i) {
                std::string entry = list[i];
                auto eq = entry.find('=');
                Rgb c;
                if (eq == std::string::npos || eq == 0 || !parseHex(entry.substr(eq + 1), c)) {
                    g_strfreev(list);
                    throw std::runtime_error("Ungültige Abschnittsfarbe in view.ini.");
                }
                v.sectionColors[entry.substr(0, eq)] = entry.substr(eq + 1);
            }
            g_strfreev(list);
        }
    }
    return v;
}
std::string serializeView(const ViewOptions& v) {
    std::string s = "[view]\nversion=" + std::to_string(ViewOptions::VERSION) + "\nbooks=" + std::to_string(v.books) +
                    "\nwidth=" + std::to_string(v.width) + "\ntheme=" + std::to_string(v.theme) +
                    "\nfocus=" + std::to_string(int(v.focus)) + "\nsection=" + v.section +
                    "\ntab=" + std::to_string(v.tab) + "\nexpanded=";
    for (const auto& id: v.expanded) s += id + ";";
    s += "\nribbon=" + std::to_string(int(v.ribbonPinned)) + "\nnavpin=" + std::to_string(int(v.navPinned)) +
         "\ntouch=" + std::to_string(int(v.touch)) + "\nmotion=" + std::to_string(int(v.reducedMotion)) +
         "\naccent=" + v.accent + "\ncolors=";
    for (const auto& e: v.sectionColors) s += e.first + "=" + e.second + ";";
    return s + "\n";
}
}  // namespace foliq
