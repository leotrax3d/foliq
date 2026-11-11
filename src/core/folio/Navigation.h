// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <gtk/gtk.h>

#include "Catalog.h"
class Control;
namespace foliq {
class Navigation {
public:
    Navigation(Control* control, GtkWidget* container, GtkWindow* window);
    ~Navigation();
    bool rememberPosition();
    GtkWidget* editorContainer() const { return editorHost; }

private:
    Control* control;
    GtkWindow* window;
    std::unique_ptr<Catalog> catalog;
    GtkWidget *tree, *status, *panel;
    GtkTreeStore* store;
    GtkWidget *editorHost, *books, *pages, *split, *outer, *ribbon, *legacyToggle, *drawOptions, *undoButton, *redoButton;
    GtkTreeStore *bookStore, *pageStore;
    std::string section, dragId, pendingSelection;
    std::vector<std::string> savedExpanded;
    std::vector<std::pair<GtkWidget*, int>> toolButtons;
    std::vector<std::pair<GtkWidget*, const char*>> mirroredButtons;
    bool viewWritable = false;
    bool updatingToolButtons = false;
    bool refreshing = false, focus = false;
    int theme = 0;
    void selectTree(GtkWidget* view);
    void setupTree(GtkWidget* view);
    void chrome(GtkWidget* container);
    void saveView();
    void loadView();
    void applyTheme();
    void updateStatus();
    guint timer = 0;
    guint statusTimer = 0;
    guint restoreIdle = 0;
    bool trashView = false;
    bool failedSave = false;
    std::string selected() const;
    void refresh(bool pagesOnly = false);
    void action(const std::string& action);
    void activate();
    bool flush();
    bool tick();
    void error(const std::exception& e);
    std::string prompt(const char* title, const std::string& value = {});
};
}  // namespace foliq
