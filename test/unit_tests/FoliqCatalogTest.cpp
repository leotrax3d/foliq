// SPDX-License-Identifier: GPL-2.0-or-later
#include <glib.h>
#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "folio/Catalog.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/XojPage.h"
using namespace foliq;
class FoliqCatalogTest: public ::testing::Test {
protected:
    fs::path root;
    void SetUp() override {
        auto* id = g_uuid_string_random();
        root = fs::temp_directory_path() / id;
        g_free(id);
        fs::create_directories(root);
    }
    void TearDown() override { fs::remove_all(root); }
    std::string pageBytes() {
        Document doc(nullptr);
        auto page = std::make_shared<XojPage>(595, 842);
        doc.addPage(page);
        SaveHandler save;
        save.prepareSave(&doc);
        save.saveTo(root / "fixture.xopp");
        EXPECT_TRUE(save.getErrorMessage().empty());
        return read(root / "fixture.xopp");
    }
};
TEST_F(FoliqCatalogTest, RestartMoveTrashAndPositionKeepDocumentIdentity) {
    std::string book, section, page, group;
    const auto bytes = pageBytes();
    {
        Catalog c(root);
        book = c.add(Kind::Notebook, "", "CON: /日本語.? ");
        group = c.add(Kind::Group, book, "Gruppe");
        auto nested = c.add(Kind::Group, group, "Verschachtelt");
        section = c.add(Kind::Section, nested, "Abschnitt");
        page = c.add(Kind::Page, section, "NUL", bytes);
        auto sub = c.add(Kind::Page, page, "Unterseite", bytes);
        auto path = c.document(page);
        c.move(section, book, 0);
        c.rename(page, "Neu");
        EXPECT_EQ(path, c.document(page));
        EXPECT_EQ(bytes, read(path));
        c.position(sub, 2);
        c.trash(section, true);
        EXPECT_TRUE(c.trashed(sub));
        c.open(book, false);
    }
    {
        Catalog c(root);
        EXPECT_FALSE(c.get(book).opened);
        EXPECT_TRUE(c.trashed(page));
        EXPECT_EQ(c.lastPage(), 2);
        EXPECT_EQ(c.get(c.last()).parent, page);
        c.trash(section, false);
        EXPECT_FALSE(c.trashed(c.last()));
        EXPECT_EQ(read(c.document(page)), bytes);
        c.open(book, true);
        LoadHandler load;
        auto* doc = load.loadDocument(c.document(page));
        ASSERT_NE(doc, nullptr);
        EXPECT_EQ(doc->getPageCount(), 1);
    }
}
TEST_F(FoliqCatalogTest, OrderingDuplicateAndCycleRollback) {
    Catalog c(root);
    auto b = c.add(Kind::Notebook, "", "B");
    auto g = c.add(Kind::Group, b, "G");
    auto nested = c.add(Kind::Group, g, "G2");
    auto s = c.add(Kind::Section, g, "S");
    auto p = c.add(Kind::Page, s, "P", pageBytes());
    auto sub = c.add(Kind::Page, p, "Sub", pageBytes());
    atomicWrite(fs::path(c.document(p)).concat(".bg_test.png"), "image");
    auto copy = c.duplicate(g);
    auto copyS = c.children(copy).back();
    auto copyP = c.children(copyS.id).front();
    EXPECT_NE(copyP.id, p);
    EXPECT_EQ(read(c.document(copyP.id)), read(c.document(p)));
    EXPECT_EQ(read(fs::path(c.document(copyP.id)).concat(".bg_test.png")), "image");
    EXPECT_EQ(c.children(copyP.id).size(), 1);
    EXPECT_NE(c.children(copyP.id)[0].id, sub);
    const auto before = read(root / "catalog.ini");
    EXPECT_THROW(c.move(g, nested, 0), std::exception);
    EXPECT_EQ(read(root / "catalog.ini"), before);
    EXPECT_THROW(c.move(b, s, 0), std::exception);
    EXPECT_THROW(c.rename(p, ""), std::exception);
    c.move(copy, b, 0);
    EXPECT_EQ(c.children(b).front().id, copy);
}
TEST_F(FoliqCatalogTest, FailedMetadataWriteDoesNotPublishOrLoseContent) {
    bool fail = false;
    Catalog c(root, [&](const fs::path& path, const std::string& bytes) {
        if (fail && path.filename() == "catalog.ini")
            throw std::runtime_error("Simulierte volle Platte");
        atomicWrite(path, bytes);
    });
    auto b = c.add(Kind::Notebook, "", "B");
    auto s = c.add(Kind::Section, b, "S");
    auto p = c.add(Kind::Page, s, "P", pageBytes());
    auto before = read(root / "catalog.ini");
    fail = true;
    EXPECT_THROW(c.rename(p, "Verloren"), std::exception);
    EXPECT_EQ(c.get(p).name, "P");
    EXPECT_THROW(c.trash(s, true), std::exception);
    EXPECT_FALSE(c.trashed(p));
    EXPECT_THROW(c.duplicate(s), std::exception);
    EXPECT_EQ(read(root / "catalog.ini"), before);
    EXPECT_EQ(c.nodes().size(), 3);
    EXPECT_TRUE(fs::exists(c.document(p)));
    fail = false;
    c.rename(p, "Erhalten");
}
TEST_F(FoliqCatalogTest, LocksExternalEditsAndUnknownVersionsFailClosed) {
    {
        Catalog c(root);
        EXPECT_THROW(Catalog second(root), std::exception);
        auto b = c.add(Kind::Notebook, "", "B");
        auto bytes = read(root / "catalog.ini");
        atomicWrite(root / "catalog.ini", bytes + "\n# externe Änderung\n");
        EXPECT_THROW(c.rename(b, "X"), std::exception);
    }
    auto bytes = read(root / "catalog.ini");
    auto pos = bytes.find("version=1");
    ASSERT_NE(pos, std::string::npos);
    bytes.replace(pos, 9, "version=99");
    atomicWrite(root / "catalog.ini", bytes);
    auto backup = read(root / "catalog.ini.bak");
    EXPECT_THROW(Catalog c(root), std::exception);
    EXPECT_EQ(read(root / "catalog.ini"), bytes);
    EXPECT_EQ(read(root / "catalog.ini.bak"), backup);
    bytes.replace(bytes.find("version=99"), 10, "version=0");
    atomicWrite(root / "catalog.ini", bytes);
    EXPECT_THROW(Catalog c(root), std::exception);  // no legacy Foliq schema exists; never guess a migration
}
TEST_F(FoliqCatalogTest, InterruptedStageAndBackupFailureKeepLastCompleteRevision) {
    auto target = root / "note.xopp";
    atomicWrite(target, "complete");
    atomicWrite(fs::path(target).concat(".saving"), "incomplete gzip");
    EXPECT_EQ(read(target), "complete");
    fs::create_directory(fs::path(target).concat(".bak"));
    EXPECT_THROW(atomicWrite(target, "new"), std::exception);
    EXPECT_EQ(read(target), "complete");
    fs::remove(fs::path(target).concat(".bak"));
    atomicWrite(target, "new");
    EXPECT_EQ(read(target), "new");
    EXPECT_EQ(read(fs::path(target).concat(".bak")), "complete");
}
TEST_F(FoliqCatalogTest, DocumentConflictDetection) {
    auto p = root / "document.xopp";
    atomicWrite(p, "old");
    EXPECT_THROW(checkDocument(p), std::exception);
    watchDocument(p);
    checkDocument(p);
    atomicWrite(p, "other instance");
    EXPECT_THROW(checkDocument(p), std::exception);
    watchDocument(p);
    checkDocument(p);
    fs::remove(p);
    EXPECT_THROW(checkDocument(p), std::exception);
}
TEST_F(FoliqCatalogTest, MissingOrCorruptCatalogNeverReinitializesBackup) {
    {
        Catalog c(root);
        c.add(Kind::Notebook, "", "B");
    }
    auto backup = read(root / "catalog.ini.bak");
    fs::remove(root / "catalog.ini");
    EXPECT_THROW(Catalog c(root), std::exception);
    EXPECT_FALSE(fs::exists(root / "catalog.ini"));
    EXPECT_EQ(read(root / "catalog.ini.bak"), backup);
    atomicWrite(root / "catalog.ini", "broken");
    EXPECT_THROW(Catalog c(root), std::exception);
    EXPECT_EQ(read(root / "catalog.ini"), "broken");
}
#ifdef _WIN32
TEST_F(FoliqCatalogTest, WindowsLockedTargetKeepsOriginal) {
    auto p = root / "locked.xopp";
    atomicWrite(p, "original");
    {
        FileLock external(p);
        EXPECT_THROW(atomicWrite(p, "replacement"), std::exception);
    }
    EXPECT_EQ(read(p), "original");
}
#endif
TEST_F(FoliqCatalogTest, CorruptRelationsAndMissingDocumentAreRejectedWithoutRewriting) {
    std::string group, nested, page;
    {
        Catalog c(root);
        auto book = c.add(Kind::Notebook, "", "B");
        group = c.add(Kind::Group, book, "G");
        nested = c.add(Kind::Group, group, "Nested");
        auto section = c.add(Kind::Section, nested, "S");
        page = c.add(Kind::Page, section, "P", pageBytes());
    }
    auto original = read(root / "catalog.ini");
    auto* key = g_key_file_new();
    ASSERT_TRUE(g_key_file_load_from_data(key, original.data(), original.size(), G_KEY_FILE_NONE, nullptr));
    g_key_file_set_string(key, group.c_str(), "parent", nested.c_str());
    gsize size;
    auto* raw = g_key_file_to_data(key, &size, nullptr);
    const std::string corrupt(raw, size);
    g_free(raw);
    g_key_file_unref(key);
    atomicWrite(root / "catalog.ini", corrupt);
    EXPECT_THROW(Catalog c(root), std::exception);
    EXPECT_EQ(read(root / "catalog.ini"), corrupt);
    atomicWrite(root / "catalog.ini", original);
    fs::remove(root / "documents" / (page + ".xopp"));
    EXPECT_THROW(Catalog c(root), std::exception);
    EXPECT_EQ(read(root / "catalog.ini"), original);
}
