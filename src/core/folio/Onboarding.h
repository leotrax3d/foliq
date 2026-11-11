// SPDX-License-Identifier: GPL-2.0-or-later
// Onboarding: profiles, subject templates, 16-colour palette and the library location
// file (config/foliq/library.ini). GTK-free parts are unit-tested (FoliqOnboardingTest).
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "filesystem.h"

namespace foliq {
enum class Profile { Pupil, Student, Teacher, Work, Empty };

/// 16 distinct, mid-saturation colours for notebooks/subjects (own selection).
const std::vector<std::pair<std::string, std::string>>& subjectPalette();
/// Paper styles offered in onboarding: label -> XOPP background style ("free" = free surface).
const std::vector<std::pair<std::string, std::string>>& paperStyles();

std::vector<std::string> subjectsFor(Profile profile);
std::vector<std::string> sectionsFor(Profile profile);
/// Suggested paper style (XOPP style key) for a subject name.
std::string defaultPaperFor(const std::string& subject);

struct Subject {
    std::string name, color, paper;
};
struct PlannedNotebook {
    std::string name, color, paper;
    std::vector<std::string> sections;
};
/// One notebook per subject (user decision 2026-11-11), with the profile's sections.
/// Empty/duplicate names are dropped, invalid colours/papers replaced by defaults.
std::vector<PlannedNotebook> plan(Profile profile, const std::vector<Subject>& subjects);

/// Gzipped XOPP with one A4 page in the given style (plain, lined, graph, dotted or free).
std::string pageDocument(const std::string& style);

/// Library location, version 1. Missing file = first run.
struct LibraryConfig {
    static constexpr int VERSION = 1;
    std::string path;
};
LibraryConfig parseLibraryConfig(const std::string& bytes);  // throws on damage/unknown version
std::string serializeLibraryConfig(const LibraryConfig& c);
fs::path libraryConfigPath();
}  // namespace foliq
