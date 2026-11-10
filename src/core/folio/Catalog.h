// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "Storage.h"
namespace foliq {
enum class Kind { Notebook, Group, Section, Page };
struct Node {
    std::string id, parent, name;
    Kind kind;
    int order = 0;
    bool trash = false;
    bool opened = true;
};
class Catalog {
public:
    using Writer = std::function<void(const fs::path&, const std::string&)>;
    explicit Catalog(fs::path root, Writer writer = atomicWrite);
    const std::map<std::string, Node>& nodes() const { return entries; }
    const Node& get(const std::string& id) const;
    std::vector<Node> children(const std::string& parent) const;
    bool trashed(const std::string& id) const;
    std::string add(Kind kind, const std::string& parent, const std::string& name, const std::string& xopp = {});
    void rename(const std::string& id, const std::string& name);
    void move(const std::string& id, const std::string& parent, int position);
    std::string duplicate(const std::string& id);
    void trash(const std::string& id, bool deleted);
    void open(const std::string& id, bool opened);
    void position(const std::string& id, int page);
    std::string last() const { return lastId; }
    int lastPage() const { return lastPageNo; }
    fs::path document(const std::string& id) const;
    fs::path directory() const { return root; }

private:
    fs::path root;
    std::unique_ptr<FileLock> lock;
    Writer writer;
    std::map<std::string, Node> entries;
    std::string lastId, expected;
    int lastPageNo = 0;
    unsigned revision = 0;
    void validate() const;
    std::string serialize() const;
    void load(const std::string& bytes);
    void commit(const std::function<void()>& mutation);
};
}  // namespace foliq
