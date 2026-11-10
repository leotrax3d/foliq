// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <gtk/gtk.h>

#include "Catalog.h"
class Control;
namespace foliq {
class Navigation {
public:
    Navigation(Control* control, GtkWidget* container);
    ~Navigation();
    bool rememberPosition();

private:
    Control* control;
    std::unique_ptr<Catalog> catalog;
    GtkWidget *tree, *status, *panel;
    GtkTreeStore* store;
    guint timer = 0;
    guint restoreIdle = 0;
    bool trashView = false;
    bool failedSave = false;
    std::string selected() const;
    void refresh();
    void action(const std::string& action);
    void activate();
    bool flush();
    bool tick();
    void error(const std::exception& e);
    std::string prompt(const char* title, const std::string& value = {});
};
}  // namespace foliq
