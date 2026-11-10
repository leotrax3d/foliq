// SPDX-License-Identifier: GPL-2.0-or-later
#include "Catalog.h"

#include <algorithm>
#include <set>
#include <stdexcept>

#include <glib.h>
namespace foliq {
namespace {
std::string uuid() {
    auto* p = g_uuid_string_random();
    std::string s(p);
    g_free(p);
    return s;
}
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
struct KeyFile {
    GKeyFile* p = g_key_file_new();
    ~KeyFile() { g_key_file_unref(p); }
    std::string string(const char* group, const char* key) {
        GError* error = nullptr;
        auto* s = g_key_file_get_string(p, group, key, &error);
        if (error) {
            std::string m(error->message);
            g_error_free(error);
            throw std::runtime_error(m);
        }
        std::string result(s);
        g_free(s);
        return result;
    }
    int integer(const char* group, const char* key) {
        auto s = string(group, key);
        size_t end = 0;
        auto n = std::stoi(s, &end);
        require(end == s.size(), "Ungültige Metadatenzahl");
        return n;
    }
};
}  // namespace
Catalog::Catalog(fs::path root, Writer writer): root(fs::absolute(root).lexically_normal()), writer(std::move(writer)) {
    fs::create_directories(this->root / "documents");
    lock = std::make_unique<FileLock>(this->root / "catalog.lock");
    auto path = this->root / "catalog.ini";
    expected = fingerprint(path);
    if (fs::exists(path))
        load(read(path));
    else {
        require(!fs::exists(fs::path(path).concat(".bak")),
                "Katalog fehlt; Sicherung catalog.ini.bak manuell wiederherstellen.");
        commit([] {});
    }
}
const Node& Catalog::get(const std::string& id) const { return entries.at(id); }
std::vector<Node> Catalog::children(const std::string& parent) const {
    std::vector<Node> result;
    for (const auto& e: entries)
        if (e.second.parent == parent)
            result.push_back(e.second);
    std::sort(result.begin(), result.end(), [](const Node& a, const Node& b) { return a.order < b.order; });
    return result;
}
bool Catalog::trashed(const std::string& id) const {
    auto* n = &get(id);
    while (true) {
        if (n->trash)
            return true;
        if (n->parent.empty())
            return false;
        n = &get(n->parent);
    }
}
fs::path Catalog::document(const std::string& id) const {
    require(get(id).kind == Kind::Page, "Kein Seitendokument");
    return root / "documents" / (id + ".xopp");
}
void Catalog::validate() const {
    std::set<std::pair<std::string, int>> positions;
    for (const auto& e: entries) {
        const auto& n = e.second;
        require(g_uuid_string_is_valid(n.id.c_str()) && n.id == e.first, "Ungültige ID");
        require(!n.name.empty() && n.name.size() <= 1024 && n.name.find('\0') == std::string::npos &&
                        g_utf8_validate(n.name.c_str(), -1, nullptr),
                "Ungültiger Name (1–1024 UTF-8-Bytes)");
        require(n.order >= 0 && positions.emplace(n.parent, n.order).second, "Ungültige Reihenfolge");
        require(n.kind >= Kind::Notebook && n.kind <= Kind::Page, "Ungültiger Knotentyp");
        if (n.parent.empty())
            require(n.kind == Kind::Notebook, "Nur Notizbücher auf oberster Ebene");
        else {
            auto parent = entries.find(n.parent);
            require(parent != entries.end(), "Fehlender Elternknoten");
            auto pk = parent->second.kind;
            require((n.kind == Kind::Group && (pk == Kind::Group || pk == Kind::Notebook)) ||
                            (n.kind == Kind::Section && (pk == Kind::Group || pk == Kind::Notebook)) ||
                            (n.kind == Kind::Page && (pk == Kind::Section || pk == Kind::Page)),
                    "Ungültige Hierarchie");
        }
        std::set<std::string> seen;
        auto p = n.id;
        while (!p.empty()) {
            require(seen.insert(p).second, "Zyklus im Katalog");
            p = get(p).parent;
        }
        if (n.kind == Kind::Page)
            require(fs::is_regular_file(document(n.id)) && !fs::is_symlink(document(n.id)),
                    "Seitendatei fehlt oder ist ein Symlink");
    }
    require(lastPageNo >= 0, "Ungültige letzte Position");
    if (!lastId.empty())
        require(get(lastId).kind == Kind::Page, "Ungültige letzte Seite");
}
std::string Catalog::serialize() const {
    KeyFile k;
    g_key_file_set_integer(k.p, "catalog", "version", 1);
    g_key_file_set_integer(k.p, "catalog", "revision", revision);
    g_key_file_set_string(k.p, "catalog", "last", lastId.c_str());
    g_key_file_set_integer(k.p, "catalog", "page", lastPageNo);
    for (const auto& e: entries) {
        const auto& n = e.second;
        auto* group = n.id.c_str();
        g_key_file_set_string(k.p, group, "name", n.name.c_str());
        g_key_file_set_string(k.p, group, "parent", n.parent.c_str());
        g_key_file_set_integer(k.p, group, "kind", static_cast<int>(n.kind));
        g_key_file_set_integer(k.p, group, "order", n.order);
        g_key_file_set_integer(k.p, group, "trash", n.trash);
        g_key_file_set_integer(k.p, group, "open", n.opened);
    }
    gsize size;
    auto* bytes = g_key_file_to_data(k.p, &size, nullptr);
    std::string result(bytes, size);
    g_free(bytes);
    return result;
}
void Catalog::load(const std::string& bytes) {
    KeyFile k;
    GError* error = nullptr;
    if (!g_key_file_load_from_data(k.p, bytes.data(), bytes.size(), G_KEY_FILE_NONE, &error)) {
        std::string message(error->message);
        g_error_free(error);
        throw std::runtime_error(message);
    }
    require(k.integer("catalog", "version") == 1,
            "Nicht unterstützte Katalogversion; Original und Backup bleiben erhalten.");
    auto rev = k.integer("catalog", "revision");
    require(rev >= 0, "Ungültige Revision");
    revision = rev;
    lastId = k.string("catalog", "last");
    lastPageNo = k.integer("catalog", "page");
    gsize count;
    auto** groups = g_key_file_get_groups(k.p, &count);
    std::vector<std::string> names;
    for (gsize i = 0; i < count; ++i) names.emplace_back(groups[i]);
    g_strfreev(groups);
    entries.clear();
    for (const auto& name: names) {
        if (name == "catalog")
            continue;
        auto* g = name.c_str();
        auto trash = k.integer(g, "trash"), opened = k.integer(g, "open");
        require((trash == 0 || trash == 1) && (opened == 0 || opened == 1), "Ungültige Flags");
        Node n{name,
               k.string(g, "parent"),
               k.string(g, "name"),
               static_cast<Kind>(k.integer(g, "kind")),
               k.integer(g, "order"),
               trash != 0,
               opened != 0};
        entries.emplace(name, n);
    }
    validate();
}
void Catalog::commit(const std::function<void()>& mutation) {
    const auto previous = entries;
    const auto last = lastId;
    const auto page = lastPageNo;
    const auto rev = revision;
    try {
        require(fingerprint(root / "catalog.ini") == expected,
                "Katalog außerhalb der App geändert; kein Überschreiben.");
        mutation();
        validate();
        require(revision < 2147483647u, "Katalogrevision erschöpft");
        ++revision;
        writer(root / "catalog.ini", serialize());
        expected = fingerprint(root / "catalog.ini");
    } catch (...) {
        entries = previous;
        lastId = last;
        lastPageNo = page;
        revision = rev;
        throw;
    }
}
std::string Catalog::add(Kind kind, const std::string& parent, const std::string& name, const std::string& xopp) {
    const auto id = uuid();
    // Publish content before metadata. An interruption can leave only an unreferenced complete file.
    if (kind == Kind::Page) {
        require(!xopp.empty(), "Leeres Seitendokument");
        writer(root / "documents" / (id + ".xopp"), xopp);
    }
    commit([&] {
        auto siblings = children(parent);
        int order = siblings.empty() ? 0 : siblings.back().order + 1;
        entries.emplace(id, Node{id, parent, name, kind, order});
    });
    return id;
}
void Catalog::rename(const std::string& id, const std::string& name) {
    commit([&] { entries.at(id).name = name; });
}
void Catalog::move(const std::string& id, const std::string& parent, int position) {
    commit([&] {
        auto siblings = children(parent);
        siblings.erase(std::remove_if(siblings.begin(), siblings.end(), [&](const Node& n) { return n.id == id; }),
                       siblings.end());
        position = std::max(0, std::min(position, static_cast<int>(siblings.size())));
        auto n = get(id);
        n.parent = parent;
        siblings.insert(siblings.begin() + position, n);
        for (size_t i = 0; i < siblings.size(); ++i) {
            auto& node = entries.at(siblings[i].id);
            node.parent = parent;
            node.order = static_cast<int>(i);
        }
    });
}
std::string Catalog::duplicate(const std::string& id) {
    std::string result;
    commit([&] {
        std::function<void(const std::string&, const std::string&)> copy = [&](const std::string& old,
                                                                               const std::string& parent) {
            auto n = get(old);
            const auto childrenCopy = children(old);
            n.id = uuid();
            n.parent = parent;
            if (old == id) {
                n.trash = false;
                auto siblings = children(parent);
                n.order = siblings.empty() ? 0 : siblings.back().order + 1;
                n.name += " (Kopie)";
                result = n.id;
            }
            if (n.kind == Kind::Page) {
                auto source = document(old);
                auto target = root / "documents" / (n.id + ".xopp");
                const auto prefix = source.filename().u8string() + ".bg_";
                for (const auto& file: fs::directory_iterator(root / "documents")) {
                    auto name = file.path().filename().u8string();
                    if (name.compare(0, prefix.size(), prefix) == 0 && fs::is_regular_file(file.path()))
                        writer(fs::path(target).concat(name.substr(source.filename().u8string().size())),
                               read(file.path()));
                }
                writer(target, read(source));
            }
            entries.emplace(n.id, n);
            for (const auto& child: childrenCopy) copy(child.id, n.id);
        };
        copy(id, get(id).parent);
    });
    return result;
}
void Catalog::trash(const std::string& id, bool deleted) {
    commit([&] {
        if (!deleted && !get(id).parent.empty())
            require(!trashed(get(id).parent), "Zuerst den übergeordneten Papierkorbeintrag wiederherstellen.");
        entries.at(id).trash = deleted;
    });
}
void Catalog::open(const std::string& id, bool opened) {
    commit([&] {
        require(get(id).kind == Kind::Notebook, "Kein Notizbuch");
        entries.at(id).opened = opened;
    });
}
void Catalog::position(const std::string& id, int page) {
    if (lastId == id && lastPageNo == page)
        return;
    commit([&] {
        lastId = id;
        lastPageNo = page;
    });
}
}  // namespace foliq
