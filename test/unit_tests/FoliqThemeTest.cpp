// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>

#include "folio/Theme.h"
using namespace foliq;

TEST(FoliqThemeTest, ParsesStrictHexOnly) {
    Rgb c;
    EXPECT_TRUE(parseHex("#005fb8", c));
    EXPECT_EQ(toHex(c), "#005fb8");
    for (const char* bad: {"", "005fb8", "#05f", "#00zzb8", "#005fb8ff", "red"}) EXPECT_FALSE(parseHex(bad, c)) << bad;
}

TEST(FoliqThemeTest, AccentIndicatorsKeepThreeToOneContrastForAnyChoice) {
    // Includes deliberately problematic user colours: near-white, near-black, yellow.
    for (const char* accent: {"#005fb8", "#ffffff", "#000000", "#ffff00", "#f3f3f3", "#202020", "#7a4bc8"}) {
        for (bool dark: {false, true}) {
            auto p = makePalette(dark, accent);
            Rgb bg, vis, on;
            ASSERT_TRUE(parseHex(p.background, bg));
            ASSERT_TRUE(parseHex(p.accentVisible, vis));
            ASSERT_TRUE(parseHex(p.onAccent, on));
            EXPECT_GE(contrast(vis, bg), 3.0) << accent << " dark=" << dark;
            EXPECT_GE(contrast(on, vis), 3.0) << accent << " dark=" << dark;
            EXPECT_NE(p.css().find("@define-color foliq_accent "), std::string::npos);
        }
    }
    EXPECT_EQ(makePalette(false, "nonsense").accent, "#005fb8");
}

TEST(FoliqThemeTest, ViewV1MigratesWithDefaultsAndV2RoundTrips) {
    auto v1 = parseView("[view]\nversion=1\nbooks=180\nwidth=400\ntheme=2\nfocus=1\nsection=abc\ntab=2\nexpanded=a;b;\n");
    EXPECT_EQ(v1.books, 180);
    EXPECT_TRUE(v1.focus);
    EXPECT_TRUE(v1.ribbonPinned);
    EXPECT_FALSE(v1.touch);
    EXPECT_EQ(v1.accent, "#005fb8");
    EXPECT_EQ(v1.expanded, (std::vector<std::string>{"a", "b"}));
    v1.touch = true;
    v1.ribbonPinned = false;
    v1.accent = "#107c10";
    v1.sectionColors["abc"] = "#c42b1c";
    auto again = parseView(serializeView(v1));
    EXPECT_EQ(serializeView(again), serializeView(v1));
    EXPECT_TRUE(again.touch);
    EXPECT_FALSE(again.ribbonPinned);
    EXPECT_EQ(again.sectionColors.at("abc"), "#c42b1c");
}

TEST(FoliqThemeTest, RejectsUnknownVersionsAndInvalidValues) {
    EXPECT_THROW(parseView("[view]\nversion=3\nbooks=180\nwidth=400\ntheme=0\nfocus=0\ntab=0\n"), std::runtime_error);
    EXPECT_THROW(parseView("not a key file"), std::runtime_error);
    const std::string base = "[view]\nversion=2\nbooks=180\nwidth=400\ntheme=0\nfocus=0\ntab=0\nribbon=1\nnavpin=1\n"
                             "touch=0\nmotion=0\n";
    EXPECT_NO_THROW(parseView(base + "accent=#123456\ncolors=\n"));
    EXPECT_THROW(parseView(base + "accent=blue\n"), std::runtime_error);
    EXPECT_THROW(parseView(base + "accent=#123456\ncolors=x=#12\n"), std::runtime_error);
    EXPECT_THROW(parseView("[view]\nversion=2\nbooks=180\nwidth=400\ntheme=0\nfocus=0\ntab=0\naccent=#123456\n"),
                 std::runtime_error);  // v2 requires its new keys
    EXPECT_THROW(parseView(base + "accent=#123456\n" + "touch=2\n"), std::runtime_error);
}
