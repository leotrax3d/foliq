// SPDX-License-Identifier: GPL-2.0-or-later
#include <memory>
#include <set>

#include <glib.h>
#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "folio/Onboarding.h"
#include "folio/Storage.h"
#include "folio/Theme.h"
#include "model/Document.h"
#include "model/XojPage.h"
using namespace foliq;

TEST(FoliqOnboardingTest, PaletteHasSixteenDistinctValidColours) {
    std::set<std::string> values;
    for (const auto& c: subjectPalette()) {
        Rgb rgb;
        EXPECT_TRUE(parseHex(c.second, rgb)) << c.first;
        values.insert(c.second);
    }
    EXPECT_EQ(subjectPalette().size(), 16u);
    EXPECT_EQ(values.size(), 16u);
}

TEST(FoliqOnboardingTest, ProfilesOfferCommonSubjectsAndSections) {
    auto pupil = subjectsFor(Profile::Pupil);
    for (const char* s: {"Deutsch", "Mathematik", "Englisch", "Biologie", "Physik", "Geschichte", "Sport"})
        EXPECT_NE(std::find(pupil.begin(), pupil.end(), s), pupil.end()) << s;
    EXPECT_TRUE(subjectsFor(Profile::Empty).empty());
    EXPECT_EQ(sectionsFor(Profile::Pupil).size(), 3u);
    EXPECT_EQ(defaultPaperFor("Mathematik"), "graph");
    EXPECT_EQ(defaultPaperFor("Deutsch"), "lined");
}

TEST(FoliqOnboardingTest, PlanDropsEmptyAndDuplicateAndRepairsValues) {
    auto p = plan(Profile::Pupil, {{"Mathe", "#d13438", "graph"},
                                   {"  Mathe ", "#000000", "lined"},
                                   {"   ", "#000000", "lined"},
                                   {"Kunst", "nonsense", "wave"}});
    ASSERT_EQ(p.size(), 2u);
    EXPECT_EQ(p[0].name, "Mathe");
    EXPECT_EQ(p[0].color, "#d13438");
    EXPECT_EQ(p[1].paper, "plain");          // invalid paper -> subject default
    EXPECT_EQ(p[1].color, subjectPalette()[1].second);  // invalid colour -> palette
    EXPECT_EQ(p[1].sections, sectionsFor(Profile::Pupil));
}

TEST(FoliqOnboardingTest, PageDocumentsLoadInAllStyles) {
    auto* id = g_uuid_string_random();
    auto dir = fs::temp_directory_path() / id;
    g_free(id);
    fs::create_directories(dir);
    for (const auto& style: paperStyles()) {
        auto file = dir / (style.second + ".xopp");
        atomicWrite(file, pageDocument(style.second));
        LoadHandler handler;
        auto* doc = handler.loadDocument(file);  // owned by the LoadHandler
        ASSERT_TRUE(doc) << style.second << ": " << handler.getLastError();
        ASSERT_EQ(doc->getPageCount(), 1u);
        EXPECT_EQ(doc->getPage(0)->isInfinite(), style.second == "free");
    }
    fs::remove_all(dir);
}

TEST(FoliqOnboardingTest, LibraryConfigRoundTripAndRejection) {
    LibraryConfig c;
    c.path = (fs::temp_directory_path() / "Foliq Bibliothek").u8string();
    EXPECT_EQ(parseLibraryConfig(serializeLibraryConfig(c)).path, c.path);
    EXPECT_THROW(parseLibraryConfig("[library]\nversion=2\npath=/x\n"), std::runtime_error);
    EXPECT_THROW(parseLibraryConfig("[library]\nversion=1\npath=relative\n"), std::runtime_error);
    EXPECT_THROW(parseLibraryConfig("garbage"), std::runtime_error);
}
