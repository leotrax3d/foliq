// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>

#include <glib.h>
#include <zlib.h>

#include "Onboarding.h"
#include "Theme.h"

namespace foliq {
const std::vector<std::pair<std::string, std::string>>& subjectPalette() {
    static const std::vector<std::pair<std::string, std::string>> p = {
            {"Rot", "#d13438"},     {"Koralle", "#e8603c"}, {"Orange", "#ca5010"},  {"Bernstein", "#c19c00"},
            {"Limette", "#6b9e1f"}, {"Grün", "#107c10"},    {"Mint", "#00a37a"},    {"Petrol", "#03787c"},
            {"Türkis", "#0099bc"},  {"Blau", "#005fb8"},    {"Indigo", "#4f52b2"},  {"Violett", "#7a4bc8"},
            {"Pflaume", "#9a0089"}, {"Pink", "#c2185b"},    {"Braun", "#8e562e"},   {"Graphit", "#4c5562"},
    };
    return p;
}
const std::vector<std::pair<std::string, std::string>>& paperStyles() {
    static const std::vector<std::pair<std::string, std::string>> s = {
            {"Liniert", "lined"}, {"Kariert", "graph"}, {"Gepunktet", "dotted"}, {"Blanko", "plain"},
            {"Freie Fläche", "free"}};
    return s;
}
std::vector<std::string> subjectsFor(Profile profile) {
    switch (profile) {
        case Profile::Pupil:
        case Profile::Teacher:
            return {"Deutsch",   "Mathematik", "Englisch",  "Französisch", "Spanisch",     "Latein",
                    "Biologie",  "Chemie",     "Physik",    "Informatik",  "Geschichte",   "Erdkunde",
                    "Politik",   "Wirtschaft", "Religion",  "Ethik",       "Philosophie",  "Kunst",
                    "Musik",     "Sport",      "Pädagogik", "Psychologie", "Sachunterricht", "Technik"};
        case Profile::Student:
            return {"Mathematik",   "Statistik",    "Informatik",  "Physik",         "Chemie",
                    "Biologie",     "Medizin",      "Psychologie", "BWL",            "VWL",
                    "Rechtswissenschaft", "Maschinenbau", "Elektrotechnik", "Bauingenieurwesen",
                    "Germanistik",  "Anglistik",    "Geschichte",  "Soziologie",     "Pädagogik",
                    "Philosophie",  "Architektur",  "Kunstgeschichte"};
        case Profile::Work:
            return {"Projekte", "Besprechungen", "Ideen",  "Privat",     "Finanzen",
                    "Reisen",   "Rezepte",       "Gesundheit", "Weiterbildung", "Haushalt"};
        case Profile::Empty:
            break;
    }
    return {};
}
std::vector<std::string> sectionsFor(Profile profile) {
    switch (profile) {
        case Profile::Pupil:
            return {"Mitschriften", "Hausaufgaben", "Arbeiten & Tests"};
        case Profile::Student:
            return {"Vorlesung", "Übung", "Klausurvorbereitung"};
        case Profile::Teacher:
            return {"Unterrichtsplanung", "Material", "Korrekturen"};
        case Profile::Work:
            return {"Notizen", "Aufgaben"};
        case Profile::Empty:
            break;
    }
    return {};
}
std::string defaultPaperFor(const std::string& subject) {
    static const std::set<std::string> grid = {"Mathematik", "Physik",  "Chemie",         "Informatik", "Statistik",
                                               "Technik",    "Maschinenbau", "Elektrotechnik", "Bauingenieurwesen",
                                               "Wirtschaft", "BWL",     "VWL",            "Finanzen"};
    static const std::set<std::string> blank = {"Kunst", "Architektur", "Ideen"};
    return grid.count(subject) ? "graph" : blank.count(subject) ? "plain" : "lined";
}
std::vector<PlannedNotebook> plan(Profile profile, const std::vector<Subject>& subjects) {
    std::vector<PlannedNotebook> result;
    std::set<std::string> seen;
    size_t index = 0;
    for (const auto& s: subjects) {
        auto name = s.name;
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        if (name.empty() || name.size() > 200 || !seen.insert(name).second)
            continue;
        Rgb rgb;
        auto color = parseHex(s.color, rgb) ? s.color : subjectPalette()[index % 16].second;
        bool paperOk = std::any_of(paperStyles().begin(), paperStyles().end(),
                                   [&](const auto& p) { return p.second == s.paper; });
        result.push_back({name, color, paperOk ? s.paper : defaultPaperFor(name), sectionsFor(profile)});
        ++index;
    }
    return result;
}
std::string pageDocument(const std::string& style) {
    bool free = style == "free";
    std::string bg = free ? "plain" : style;
    if (bg != "plain" && bg != "lined" && bg != "graph" && bg != "dotted")
        bg = "plain";
    std::string page = free ? "<page width=\"595.28\" height=\"841.89\" foliq-surface-version=\"1\" foliq-camera-x=\"0\" "
                              "foliq-camera-y=\"0\" foliq-export-content=\"1\" foliq-crop-x=\"0\" foliq-crop-y=\"0\" "
                              "foliq-crop-width=\"595.28\" foliq-crop-height=\"841.89\" foliq-tiled=\"1\" "
                              "foliq-tile-width=\"595.276\" foliq-tile-height=\"841.89\">" :
                              "<page width=\"595.28\" height=\"841.89\">";
    const std::string xml = std::string("<?xml version=\"1.0\"?><xournal creator=\"Foliq\" fileversion=\"") +
                            (free ? "5" : "4") + "\"><title>Foliq</title>" + page +
                            "<background type=\"solid\" color=\"#ffffffff\" style=\"" + bg +
                            "\"/><layer/></page></xournal>";
    z_stream z{};
    if (deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("gzip init fehlgeschlagen");
    std::string out(deflateBound(&z, xml.size()) + 64, '\0');
    z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(xml.data()));
    z.avail_in = static_cast<uInt>(xml.size());
    z.next_out = reinterpret_cast<Bytef*>(&out[0]);
    z.avail_out = static_cast<uInt>(out.size());
    int ret = deflate(&z, Z_FINISH);
    out.resize(z.total_out);
    deflateEnd(&z);
    if (ret != Z_STREAM_END)
        throw std::runtime_error("gzip fehlgeschlagen");
    return out;
}
LibraryConfig parseLibraryConfig(const std::string& bytes) {
    std::unique_ptr<GKeyFile, decltype(&g_key_file_unref)> owner(g_key_file_new(), g_key_file_unref);
    auto* k = owner.get();
    if (!g_key_file_load_from_data(k, bytes.data(), bytes.size(), G_KEY_FILE_NONE, nullptr))
        throw std::runtime_error("Beschädigte Bibliotheks-Einstellung (library.ini).");
    GError* error = nullptr;
    int version = g_key_file_get_integer(k, "library", "version", &error);
    if (error) {
        g_error_free(error);
        throw std::runtime_error("Beschädigte Bibliotheks-Einstellung (library.ini).");
    }
    if (version != LibraryConfig::VERSION)
        throw std::runtime_error("Unbekannte Version von library.ini; Datei bleibt unverändert.");
    auto* path = g_key_file_get_string(k, "library", "path", nullptr);
    LibraryConfig c;
    if (path)
        c.path = path;
    g_free(path);
    if (c.path.empty() || !fs::u8path(c.path).is_absolute())
        throw std::runtime_error("Ungültiger Bibliothekspfad in library.ini.");
    return c;
}
std::string serializeLibraryConfig(const LibraryConfig& c) {
    std::unique_ptr<GKeyFile, decltype(&g_key_file_unref)> owner(g_key_file_new(), g_key_file_unref);
    g_key_file_set_integer(owner.get(), "library", "version", LibraryConfig::VERSION);
    g_key_file_set_string(owner.get(), "library", "path", c.path.c_str());
    gsize size = 0;
    auto* data = g_key_file_to_data(owner.get(), &size, nullptr);
    std::string s(data, size);
    g_free(data);
    return s;
}
fs::path libraryConfigPath() { return fs::u8path(g_get_user_config_dir()) / "foliq" / "library.ini"; }
}  // namespace foliq
