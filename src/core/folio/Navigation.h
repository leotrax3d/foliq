// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <gtk/gtk.h>

#include "Catalog.h"
#include "Theme.h"
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
    // Fluent shell (see docs/design-references/FOLIQ-DESIGN-SYSTEM.md)
    ViewOptions options;
    GtkWidget *overlay = nullptr, *commandBar = nullptr, *ribbonPanel = nullptr, *ribbonStack = nullptr,
              *navToggle = nullptr, *ribbonPinButton = nullptr, *menuButton = nullptr, *crumb = nullptr,
              *appMenu = nullptr, *focusButton = nullptr, *touchButton = nullptr, *motionButton = nullptr;
    GtkCssProvider* paletteProvider = nullptr;
    std::vector<GtkWidget*> tabButtons, ribbonImages, themeButtons;
    bool ribbonTemporary = false, navFloating = false, updatingChrome = false;
    void showTab(int index, bool userAction);
    void setRibbonPinned(bool pinned);
    void hideTemporary();
    void setNavVisible(bool visible);
    void setNavPinned(bool pinned);
    void setFocusMode(bool on);
    void setDensity(bool touch);
    void applyPalette();
    void moveMenubar();
    void accentPopover(GtkWidget* anchor, const std::string& target);
    void updateCrumb();
    bool onKey(GdkEventKey* event);
    void onPress(double x, double y);
    void cycleFocus(bool backwards);
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
