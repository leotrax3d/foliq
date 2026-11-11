// SPDX-License-Identifier: GPL-2.0-or-later
#include "Navigation.h"

#include "Onboarding.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <zlib.h>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/settings/Settings.h"
#include "control/tools/ImageHandler.h"
#include "control/tools/TextEditor.h"
#include "enums/ActionGroup.enum.h"
#include "enums/ActionType.enum.h"
#include "gui/MainWindow.h"
#include "gui/XournalView.h"
#include "model/Document.h"
#include "util/XojMsgBox.h"
namespace foliq {
static std::string blankDocument() {
    const std::string xml =
            "<?xml version=\"1.0\"?><xournal creator=\"Foliq\" fileversion=\"4\"><title>Foliq</title><page "
            "width=\"595.28\" height=\"841.89\"><background type=\"solid\" color=\"#ffffffff\" "
            "style=\"plain\"/><layer/></page></xournal>";
    z_stream z{};
    if (deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("gzip init fehlgeschlagen");
    std::string result(1024, '\0');
    z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(xml.data()));
    z.avail_in = static_cast<uInt>(xml.size());
    z.next_out = reinterpret_cast<Bytef*>(&result[0]);
    z.avail_out = static_cast<uInt>(result.size());
    int ret = deflate(&z, Z_FINISH);
    result.resize(z.total_out);
    deflateEnd(&z);
    if (ret != Z_STREAM_END)
        throw std::runtime_error("gzip fehlgeschlagen");
    return result;
}
Navigation::Navigation(Control* control, GtkWidget* container, GtkWindow* window): control(control), window(window) {
    // Keep the existing editor and its input pipeline intact, only reparent its container.
    auto* children = gtk_container_get_children(GTK_CONTAINER(container));
    auto* editor = GTK_WIDGET(children->data);
    g_list_free(children);
    g_object_ref(editor);
    gtk_container_remove(GTK_CONTAINER(container), editor);
    outer = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    panel = split;
    gtk_widget_set_name(panel, "foliq-navigation");
    // Every toplevel (dialogs, message boxes, popups) joins the design system.
    static bool hooked = false;
    if (!hooked) {
        hooked = true;
        g_signal_add_emission_hook(
                g_signal_lookup("realize", GTK_TYPE_WIDGET), 0,
                +[](GSignalInvocationHint*, guint, const GValue* values, gpointer) -> gboolean {
                    auto* w = g_value_get_object(&values[0]);
                    if (GTK_IS_WINDOW(w)) {
                        auto* ctx = gtk_widget_get_style_context(GTK_WIDGET(w));
                        gtk_style_context_add_class(ctx, "foliq-app");
                        if (currentPalette().dark)
                            gtk_style_context_add_class(ctx, "darkMode");
                    }
                    return true;
                },
                nullptr, nullptr);
    }
    // The overlay hosts temporary (floating) ribbon and navigation without reflowing the canvas.
    overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(overlay), outer);
    gtk_box_pack_start(GTK_BOX(container), overlay, true, true, 0);
    gtk_paned_pack1(GTK_PANED(outer), split, false, false);
    editorHost = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(editorHost, "foliq-editor-host");
    gtk_box_pack_start(GTK_BOX(editorHost), editor, true, true, 0);
    gtk_paned_pack2(GTK_PANED(outer), editorHost, true, false);
    g_object_unref(editor);
    bookStore = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    pageStore = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    books = gtk_tree_view_new_with_model(GTK_TREE_MODEL(bookStore));
    pages = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pageStore));
    tree = books;
    store = bookStore;
    for (auto* view: {books, pages}) {
        auto* column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_size_request(column, 120, -1);
        gtk_container_set_border_width(GTK_CONTAINER(column), 6);
        auto* headRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        auto* heading = gtk_label_new(view == books ? "Notizbücher" : "Seiten");
        gtk_style_context_add_class(gtk_widget_get_style_context(heading), "foliq-pane-title");
        gtk_label_set_xalign(GTK_LABEL(heading), 0);
        gtk_box_pack_start(GTK_BOX(headRow), heading, true, true, 0);
        gtk_box_pack_start(GTK_BOX(column), headRow, false, false, 0);
        auto* add = gtk_button_new_from_icon_name("foliq-plus", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_focus_on_click(add, false);
        ribbonImages.push_back(gtk_button_get_image(GTK_BUTTON(add)));
        atk_object_set_name(gtk_widget_get_accessible(add), view == books ? "Notizbuchaktionen" : "Seitenaktionen");
        gtk_widget_set_tooltip_text(add, view == books ? "Notizbuch, Gruppe, Abschnitt anlegen und weitere Aktionen"
                                                       : "Seite/Unterseite anlegen und weitere Aktionen");
        g_object_set_data(G_OBJECT(add), "view", view);
        g_signal_connect(add, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer p) {
                             auto* self = static_cast<Navigation*>(p);
                             self->selectTree(GTK_WIDGET(g_object_get_data(G_OBJECT(b), "view")));
                             auto* menu = gtk_menu_new();
                             for (const auto* label:
                                  {"Notizbuch", "Gruppe", "Abschnitt", "Seite", "Umbenennen", "Verschieben",
                                   "Nach oben", "Nach unten",
                                   "Duplizieren", "Farbe…", "Löschen", "Wiederherstellen", "Öffnen/Schließen",
                                   "Papierkorb"}) {
                                 if (std::string(label) == "Umbenennen" || std::string(label) == "Duplizieren" ||
                                     std::string(label) == "Wiederherstellen")
                                     gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
                                 auto* item = gtk_menu_item_new_with_label(label);
                                 if (std::string(label) == "Nach oben" || std::string(label) == "Nach unten")
                                     atk_object_set_name(gtk_widget_get_accessible(item),
                                                         std::string(label) == "Nach oben" ? "Nach oben verschieben" :
                                                                                             "Nach unten verschieben");
                                 gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                                 g_signal_connect(item, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer p) {
                                                      auto* self = static_cast<Navigation*>(p);
                                                      try {
                                                          if (std::string(gtk_menu_item_get_label(item)) ==
                                                              "Farbe…") {
                                                              auto id = self->selected();
                                                              if (id.empty())
                                                                  throw std::runtime_error(
                                                                          "Zuerst einen Eintrag auswählen.");
                                                              self->accentPopover(self->tree, id);
                                                              return;
                                                          }
                                                          std::string chosen = gtk_menu_item_get_label(item);
                                                          if (chosen == "Nach oben")
                                                              chosen = "↑";
                                                          else if (chosen == "Nach unten")
                                                              chosen = "↓";
                                                          self->action(chosen);
                                                      } catch (const std::exception& e) {
                                                          self->error(e);
                                                      }
                                                  }),
                                                  self);
                             }
                             gtk_widget_show_all(menu);
                             g_signal_connect_swapped(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), menu);
#if GTK_CHECK_VERSION(3, 22, 0)
                             gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(b), GDK_GRAVITY_SOUTH_WEST,
                                                      GDK_GRAVITY_NORTH_WEST, nullptr);
#else
            gtk_menu_popup(GTK_MENU(menu), nullptr, nullptr, nullptr, nullptr, 0, gtk_get_current_event_time());
#endif
                         }),
                         this);
        gtk_box_pack_end(GTK_BOX(headRow), add, false, false, 0);
        if (view == books) {
            auto* pin = gtk_toggle_button_new();
            gtk_button_set_image(GTK_BUTTON(pin), gtk_image_new_from_icon_name("foliq-pin", GTK_ICON_SIZE_BUTTON));
            ribbonImages.push_back(gtk_button_get_image(GTK_BUTTON(pin)));
            gtk_widget_set_focus_on_click(pin, false);
            gtk_widget_set_tooltip_text(pin, "Navigation anheften: neben der Seite statt schwebend");
            atk_object_set_name(gtk_widget_get_accessible(pin), "Navigation anheften");
            gtk_box_pack_end(GTK_BOX(headRow), pin, false, false, 0);
            g_object_set_data(G_OBJECT(panel), "pin", pin);
            g_signal_connect(pin, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                                 auto* self = static_cast<Navigation*>(p);
                                 if (!self->updatingChrome)
                                     self->setNavPinned(gtk_toggle_button_get_active(b));
                             }),
                             this);
        }
        auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scroll), view);
        gtk_box_pack_start(GTK_BOX(column), scroll, true, true, 0);
        if (view == books)
            gtk_paned_pack1(GTK_PANED(split), column, true, false);
        else
            gtk_paned_pack2(GTK_PANED(split), column, true, false);
        setupTree(view);
    }
    chrome(container);
    statusTimer = g_timeout_add(
            250,
            +[](gpointer p) -> gboolean {
                static_cast<Navigation*>(p)->updateStatus();
                return true;
            },
            this);
    gtk_widget_show_all(overlay);
    gtk_widget_show_all(ribbon);
    try {
        const auto* overrideRoot = g_getenv("FOLIQ_LIBRARY");
        auto root = overrideRoot ? fs::u8path(overrideRoot) : fs::u8path(g_get_user_data_dir()) / "foliq" / "library";
        if (!overrideRoot) {
            auto config = libraryConfigPath();
            if (!fs::exists(config))
                firstRun = true;  // fresh installation: onboarding after startup
            else {
                try {
                    root = fs::u8path(parseLibraryConfig(read(config)).path);
                } catch (const std::exception& e) {
                    // Damaged/unknown library.ini is reported and left untouched; default library is used.
                    error(e);
                }
            }
        }
        catalog = std::make_unique<Catalog>(root);
        gtk_widget_set_tooltip_text(status, root.u8string().c_str());
        try {
            loadView();
        } catch (const std::exception& e) {
            error(e);
        }
        refresh();
        timer = g_timeout_add_seconds(
                5, +[](gpointer p) -> gboolean { return static_cast<Navigation*>(p)->tick(); }, this);
        // Restore after Control and initial startup document have been initialized.
        restoreIdle = g_idle_add(
                +[](gpointer p) -> gboolean {
                    auto* self = static_cast<Navigation*>(p);
                    self->restoreIdle = 0;
                    try {
                        self->applyTheme();
                        if (self->firstRun) {
                            self->firstRun = false;
                            g_idle_add(
                                    +[](gpointer p) -> gboolean {
                                        static_cast<Navigation*>(p)->onboarding(true);
                                        return false;
                                    },
                                    self);
                        }
                        auto id = self->catalog->last();
                        if (!id.empty() && !self->catalog->trashed(id) &&
                            self->control->getDocument()->getFilepath().empty() &&
                            self->control->getDocument()->getPdfPageCount() == 0 &&
                            !self->control->getUndoRedoHandler()->isChanged()) {
                            auto n = self->catalog->get(id);
                            while (!n.parent.empty()) n = self->catalog->get(n.parent);
                            if (n.opened)
                                self->control->openFile(self->catalog->document(id), self->catalog->lastPage());
                            // Restore the navigation position: show the page list of the last page.
                            auto owner = self->catalog->get(id);
                            while (owner.kind == Kind::Page && !owner.parent.empty())
                                owner = self->catalog->get(owner.parent);
                            if (owner.kind == Kind::Section) {
                                self->section = owner.id;
                                self->pendingSelection = owner.id;
                                self->selectTree(self->books);
                                self->refresh();
                                self->pendingSelection = id;
                                self->selectTree(self->pages);
                                self->refresh(true);
                            }
                        }
                    } catch (const std::exception& e) {
                        self->error(e);
                    }
                    return false;
                },
                this);
    } catch (const std::exception& e) {
        error(e);
        gtk_widget_set_sensitive(panel, false);
    }
}
Navigation::~Navigation() {
    if (statusTimer)
        g_source_remove(statusTimer);
    if (restoreIdle)
        g_source_remove(restoreIdle);
    if (timer)
        g_source_remove(timer);
    g_object_unref(bookStore);
    g_object_unref(pageStore);
}
void Navigation::error(const std::exception& e) {
    gtk_label_set_text(GTK_LABEL(status), (std::string("Aktion fehlgeschlagen: ") + e.what()).c_str());
    XojMsgBox::showErrorToUser(window, e.what());
}
std::string Navigation::selected() const {
    GtkTreeIter iter;
    GtkTreeModel* model;
    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)), &model, &iter))
        return {};
    gchar* id;
    gtk_tree_model_get(model, &iter, 1, &id, -1);
    std::string result(id);
    g_free(id);
    return result;
}
void Navigation::selectTree(GtkWidget* view) {
    tree = view;
    store = view == books ? bookStore : pageStore;
}
void Navigation::refresh(bool pagesOnly) {
    if (refreshing || !catalog)
        return;
    refreshing = true;
    auto* active = tree;
    for (auto* view: {books, pages}) {
        if (pagesOnly && view == books)
            continue;
        selectTree(view);
        auto selection = pendingSelection.empty() ? selected() : pendingSelection;
        std::vector<std::string> expanded = savedExpanded;
        gtk_tree_view_map_expanded_rows(
                GTK_TREE_VIEW(view),
                +[](GtkTreeView* v, GtkTreePath* path, gpointer p) {
                    GtkTreeIter it;
                    gchar* id = nullptr;
                    auto* m = gtk_tree_view_get_model(v);
                    if (gtk_tree_model_get_iter(m, &it, path)) {
                        gtk_tree_model_get(m, &it, 1, &id, -1);
                        static_cast<std::vector<std::string>*>(p)->push_back(id);
                        g_free(id);
                    }
                },
                &expanded);
        gtk_tree_store_clear(store);
        std::function<void(const std::string&, GtkTreeIter*)> append = [&](const std::string& parent,
                                                                           GtkTreeIter* row) {
            for (const auto& n: catalog->children(parent)) {
                if ((!trashView && catalog->trashed(n.id)) || (view == books && n.kind == Kind::Page))
                    continue;
                auto name = n.name + (catalog->trashed(n.id) ? " [Papierkorb]" : "") +
                            (n.kind == Kind::Notebook && !n.opened ? " [geschlossen]" : "");
                GtkTreeIter it;
                gtk_tree_store_append(store, &it, row);
                gtk_tree_store_set(store, &it, 0, name.c_str(), 1, n.id.c_str(), -1);
                if (trashView || n.kind != Kind::Notebook || n.opened)
                    append(n.id, &it);
                auto* path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &it);
                if (std::find(expanded.begin(), expanded.end(), n.id) != expanded.end())
                    gtk_tree_view_expand_row(GTK_TREE_VIEW(view), path, false);
                if (n.id == selection) {
                    if (!pendingSelection.empty())
                        gtk_tree_view_expand_to_path(GTK_TREE_VIEW(view), path);
                    gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), &it);
                }
                gtk_tree_path_free(path);
            }
        };
        if (view == books || !section.empty())
            append(view == books ? "" : section, nullptr);
    }
    pendingSelection.clear();
    savedExpanded.clear();
    selectTree(active);
    refreshing = false;
    updateStatus();
}
std::string Navigation::prompt(const char* title, const std::string& value) {
    auto* dlg = gtk_dialog_new_with_buttons(title, window, GTK_DIALOG_MODAL, "Abbrechen", GTK_RESPONSE_CANCEL, "OK",
                                            GTK_RESPONSE_OK, nullptr);
    auto* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), value.c_str());
    gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 36);
    atk_object_set_name(gtk_widget_get_accessible(entry), title);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_style_context_add_class(
            gtk_widget_get_style_context(gtk_dialog_get_widget_for_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK)),
            "suggested-action");
    auto* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_set_spacing(GTK_BOX(content), 10);
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    auto* heading = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(heading), "foliq-dialog-title");
    gtk_container_add(GTK_CONTAINER(content), heading);
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dlg);
    std::string result;
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
        result = gtk_entry_get_text(GTK_ENTRY(entry));
    gtk_widget_destroy(dlg);
    return result;
}
bool Navigation::flush() {
    if (control->getUndoRedoHandler()->isChanged() && !control->getDocument()->getFilepath().empty()) {
        if (!control->save(true)) {
            failedSave = true;
            gtk_label_set_text(GTK_LABEL(status), "Speicherfehler · Änderungen bleiben im Editor");
            return false;
        }
    }
    failedSave = false;
    return true;
}
bool Navigation::rememberPosition() {
    if (!catalog)
        return true;
    try {
        saveView();
        auto path = control->getDocument()->getFilepath();
        for (const auto& e: catalog->nodes())
            if (e.second.kind == Kind::Page && catalog->document(e.first) == path) {
                catalog->position(e.first, static_cast<int>(control->getCurrentPageNo()));
                break;
            }
        return true;
    } catch (const std::exception& e) {
        error(e);
        return false;
    }
}
bool Navigation::tick() {
    updateStatus();
    if (!catalog)
        return true;
    try {
        auto path = control->getDocument()->getFilepath();
        for (const auto& e: catalog->nodes()) {
            if (e.second.kind == Kind::Page && catalog->document(e.first) == path) {
                if (!control->getUndoRedoHandler()->isChanged())
                    failedSave = false;
                if (!failedSave && !flush())
                    return true;
                catalog->position(e.first, static_cast<int>(control->getCurrentPageNo()));
                break;
            }
        }
    } catch (const std::exception& e) {
        failedSave = true;
        error(e);
    }
    return true;
}
void Navigation::activate() {
    auto id = selected();
    if (id.empty() || catalog->trashed(id))
        return;
    const auto& n = catalog->get(id);
    if (n.kind == Kind::Page) {
        if (!rememberPosition() || !flush())
            return;
        if (control->openFile(catalog->document(id))) {
            catalog->position(id, 0);
            if (navFloating)
                setNavVisible(false);
        }
    } else if (n.kind == Kind::Notebook) {
        action("Öffnen/Schließen");
    }
}
void Navigation::action(const std::string& action) {
    if (!catalog)
        return;
    if (action == "Papierkorb") {
        trashView = !trashView;
        refresh();
        return;
    }
    auto id = selected();
    if (action == "Notizbuch" || action == "Gruppe" || action == "Abschnitt" || action == "Seite") {
        Kind kind = action == "Notizbuch" ? Kind::Notebook :
                    action == "Gruppe"    ? Kind::Group :
                    action == "Abschnitt" ? Kind::Section :
                                            Kind::Page;
        auto name = prompt("Name");
        if (name.empty())
            return;
        if (kind == Kind::Page && id.empty())
            id = section;
        auto created =
                catalog->add(kind, kind == Kind::Notebook ? "" : id, name, kind == Kind::Page ? blankDocument() : "");
        if (kind == Kind::Section)
            section = created;
        pendingSelection = created;
        selectTree(kind == Kind::Page ? pages : books);
    } else {
        if (id.empty())
            throw std::runtime_error("Zuerst einen Eintrag auswählen.");
        if (action == "Umbenennen") {
            auto name = prompt("Umbenennen", catalog->get(id).name);
            if (!name.empty())
                catalog->rename(id, name);
        } else if (action == "↑" || action == "↓") {
            auto n = catalog->get(id);
            auto siblings = catalog->children(n.parent);
            int pos = 0;
            for (size_t i = 0; i < siblings.size(); ++i)
                if (siblings[i].id == id)
                    pos = static_cast<int>(i);
            catalog->move(id, n.parent, pos + (action == "↑" ? -1 : 1));
        } else if (action == "Verschieben") {
            auto* dlg = gtk_dialog_new_with_buttons("Ziel wählen", window, GTK_DIALOG_MODAL, "Abbrechen",
                                                    GTK_RESPONSE_CANCEL, "Verschieben", GTK_RESPONSE_OK, nullptr);
            auto* combo = gtk_combo_box_text_new();
            auto kind = catalog->get(id).kind;
            if (kind == Kind::Notebook)
                gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "", "Oberste Ebene (Notizbuch)");
            for (const auto& e: catalog->nodes()) {
                auto targetKind = e.second.kind;
                bool compatible = kind == Kind::Page ? targetKind == Kind::Page || targetKind == Kind::Section :
                                                       kind != Kind::Notebook && (targetKind == Kind::Notebook ||
                                                                                  targetKind == Kind::Group);
                bool descendant = false;
                auto ancestor = e.second;
                while (true) {
                    if (ancestor.id == id) {
                        descendant = true;
                        break;
                    }
                    if (ancestor.parent.empty())
                        break;
                    ancestor = catalog->get(ancestor.parent);
                }
                if (compatible && !descendant && !catalog->trashed(e.first)) {
                    std::string label = e.second.name;
                    auto ancestor = e.second;
                    while (!ancestor.parent.empty()) {
                        ancestor = catalog->get(ancestor.parent);
                        label = ancestor.name + " / " + label;
                    }
                    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), e.first.c_str(), label.c_str());
                }
            }
            auto* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(combo));
            for (auto* c = cells; c; c = c->next)
                g_object_set(c->data, "ellipsize", PANGO_ELLIPSIZE_END, "max-width-chars", 50, nullptr);
            g_list_free(cells);
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
            gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), combo);
            gtk_widget_show_all(dlg);
            std::string target;
            bool ok = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK;
            if (ok) {
                const auto* value = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
                if (value)
                    target = value;
                else
                    ok = false;
            }
            gtk_widget_destroy(dlg);
            if (ok)
                catalog->move(id, target, static_cast<int>(catalog->children(target).size()));
        } else if (action == "Duplizieren") {
            if (!flush())
                return;
            catalog->duplicate(id);
        } else if (action == "Löschen" || action == "Öffnen/Schließen") {
            if (action == "Öffnen/Schließen" && catalog->get(id).kind != Kind::Notebook)
                throw std::runtime_error("Zum Öffnen/Schließen ein Notizbuch auswählen.");
            if (!rememberPosition() || !flush())
                return;
            // Close active managed editor first, so deleted/closed nodes cannot be autosaved accidentally.
            auto path = control->getDocument()->getFilepath();
            bool contained = false;
            for (const auto& e: catalog->nodes())
                if (e.second.kind == Kind::Page && catalog->document(e.first) == path) {
                    auto n = e.second;
                    while (true) {
                        if (n.id == id) {
                            contained = true;
                            break;
                        }
                        if (n.parent.empty())
                            break;
                        n = catalog->get(n.parent);
                    }
                }
            if (contained && !control->newFile())
                return;
            if (action == "Löschen")
                catalog->trash(id, true);
            else {
                catalog->open(id, !catalog->get(id).opened);
                auto last = catalog->last();
                if (catalog->get(id).opened && !last.empty() && !catalog->trashed(last)) {
                    auto ancestor = catalog->get(last);
                    while (!ancestor.parent.empty()) ancestor = catalog->get(ancestor.parent);
                    if (ancestor.id == id)
                        control->openFile(catalog->document(last), catalog->lastPage());
                }
            }
        } else if (action == "Wiederherstellen")
            catalog->trash(id, false);
    }
    refresh();
}
void Navigation::setupTree(GtkWidget* view) {
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), false);
    auto* cell = gtk_cell_renderer_text_new();
    g_object_set(cell, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 7, nullptr);
    auto* col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Titel");
    // Notebook/section colour (user decision 2026-11-11): the whole row is tinted, not a dot.
    // Text keeps the theme colour; tint strength is limited so contrast stays >= 4.5:1, and a
    // selected row gets a stronger tint plus bold text so selection never depends on hue alone.
    gtk_tree_view_column_set_cell_data_func(
            col, cell,
            +[](GtkTreeViewColumn*, GtkCellRenderer* r, GtkTreeModel* m, GtkTreeIter* it, gpointer p) {
                auto* self = static_cast<Navigation*>(p);
                gchar* id = nullptr;
                gtk_tree_model_get(m, it, 1, &id, -1);
                auto found = id ? self->options.sectionColors.find(id) : self->options.sectionColors.end();
                auto* view = self->books;
                for (auto* v: {self->books, self->pages})
                    if (gtk_tree_view_get_model(GTK_TREE_VIEW(v)) == m)
                        view = v;
                bool selected = gtk_tree_selection_iter_is_selected(
                        gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), it);
                Rgb rgb;
                if (found != self->options.sectionColors.end() && parseHex(found->second, rgb)) {
                    bool dark = currentPalette().dark;
                    GdkRGBA tint{rgb.r, rgb.g, rgb.b, selected ? (dark ? 0.55 : 0.42) : (dark ? 0.32 : 0.22)};
                    g_object_set(r, "cell-background-rgba", &tint, "cell-background-set", TRUE, "weight",
                                 selected ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL, "weight-set", TRUE, nullptr);
                } else
                    g_object_set(r, "cell-background-set", FALSE, "weight",
                                 selected ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL, "weight-set", TRUE, nullptr);
                g_free(id);
            },
            this, nullptr);
    gtk_tree_view_column_pack_start(col, cell, true);
    gtk_tree_view_column_add_attribute(col, cell, "text", 0);
    gtk_tree_view_column_set_expand(col, true);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    atk_object_set_name(gtk_widget_get_accessible(view),
                        view == books ? "Notizbücher und Abschnitte" : "Seiten und Unterseiten");
    gtk_widget_set_tooltip_text(view, "Pfeile: navigieren/einklappen · Eingabe: öffnen · F2: umbenennen · Alt+↑/↓: "
                                      "sortieren. Aktionen im Menü oberhalb.");
    g_signal_connect(view, "focus-in-event", G_CALLBACK(+[](GtkWidget* v, GdkEventFocus*, gpointer p) -> gboolean {
                         static_cast<Navigation*>(p)->selectTree(v);
                         return false;
                     }),
                     this);
    g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed",
                     G_CALLBACK(+[](GtkTreeSelection* sel, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         if (self->refreshing || !self->catalog)
                             return;
                         self->selectTree(GTK_WIDGET(gtk_tree_selection_get_tree_view(sel)));
                         auto id = self->selected();
                         if (id.empty())
                             return;
                         if (self->tree == self->books) {
                             const auto& n = self->catalog->get(id);
                             self->section = n.kind == Kind::Section ? id : "";
                             self->refresh(true);
                         }
                     }),
                     this);
    g_signal_connect(view, "row-activated",
                     G_CALLBACK(+[](GtkTreeView* v, GtkTreePath*, GtkTreeViewColumn*, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         self->selectTree(GTK_WIDGET(v));
                         try {
                             self->activate();
                         } catch (const std::exception& e) {
                             self->error(e);
                         }
                     }),
                     this);
    g_signal_connect(view, "key-press-event", G_CALLBACK(+[](GtkWidget* v, GdkEventKey* e, gpointer p) -> gboolean {
                         auto* self = static_cast<Navigation*>(p);
                         self->selectTree(v);
                         const char* action = e->keyval == GDK_KEY_F2                                 ? "Umbenennen" :
                                              (e->state & GDK_MOD1_MASK) && e->keyval == GDK_KEY_Up   ? "↑" :
                                              (e->state & GDK_MOD1_MASK) && e->keyval == GDK_KEY_Down ? "↓" :
                                                                                                        nullptr;
                         if (!action)
                             return false;
                         try {
                             self->action(action);
                         } catch (const std::exception& ex) {
                             self->error(ex);
                         }
                         return true;
                     }),
                     this);
    static const GtkTargetEntry target = {const_cast<gchar*>("application/x-foliq-node"), GTK_TARGET_SAME_APP, 0};
    gtk_drag_source_set(view, GDK_BUTTON1_MASK, &target, 1, GDK_ACTION_MOVE);
    gtk_drag_dest_set(view, GTK_DEST_DEFAULT_ALL, &target, 1, GDK_ACTION_MOVE);
    g_signal_connect(view, "drag-data-get",
                     G_CALLBACK(+[](GtkWidget* v, GdkDragContext*, GtkSelectionData* data, guint, guint, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         self->selectTree(v);
                         self->dragId = self->selected();
                         gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8,
                                                reinterpret_cast<const guchar*>(self->dragId.data()),
                                                static_cast<int>(self->dragId.size()));
                     }),
                     this);
    g_signal_connect(view, "drag-data-received",
                     G_CALLBACK(+[](GtkWidget* v, GdkDragContext* ctx, gint x, gint y, GtkSelectionData* data, guint,
                                    guint time, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         bool success = false;
                         try {
                             if (!self->catalog || gtk_selection_data_get_length(data) <= 0)
                                 throw std::runtime_error("Kein Verschiebeziel.");
                             std::string id(reinterpret_cast<const char*>(gtk_selection_data_get_data(data)),
                                            gtk_selection_data_get_length(data));
                             GtkTreePath* path = nullptr;
                             GtkTreeViewDropPosition pos;
                             if (!gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(v), x, y, &path, &pos))
                                 throw std::runtime_error("Auf einen Zieleintrag ziehen.");
                             GtkTreeIter it;
                             auto* model = gtk_tree_view_get_model(GTK_TREE_VIEW(v));
                             gtk_tree_model_get_iter(model, &it, path);
                             gtk_tree_path_free(path);
                             gchar* value;
                             gtk_tree_model_get(model, &it, 1, &value, -1);
                             std::string targetId(value);
                             g_free(value);
                             auto targetNode = self->catalog->get(targetId);
                             std::string parent = targetId;
                             int index = static_cast<int>(self->catalog->children(parent).size());
                             if (pos == GTK_TREE_VIEW_DROP_BEFORE || pos == GTK_TREE_VIEW_DROP_AFTER) {
                                 parent = targetNode.parent;
                                 auto siblings = self->catalog->children(parent);
                                 auto target = std::find_if(siblings.begin(), siblings.end(),
                                                            [&](const Node& n) { return n.id == targetId; });
                                 index = static_cast<int>(std::distance(siblings.begin(), target)) +
                                         (pos == GTK_TREE_VIEW_DROP_AFTER ? 1 : 0);
                                 auto source = std::find_if(siblings.begin(), siblings.end(),
                                                            [&](const Node& n) { return n.id == id; });
                                 if (source != siblings.end() && std::distance(siblings.begin(), source) < index)
                                     --index;
                             }
                             self->catalog->move(id, parent, index);
                             if (self->catalog->get(id).kind == Kind::Page) {
                                 auto ancestor = self->catalog->get(id);
                                 while (ancestor.kind == Kind::Page) ancestor = self->catalog->get(ancestor.parent);
                                 self->section = ancestor.id;
                                 self->selectTree(self->pages);
                             } else
                                 self->selectTree(self->books);
                             self->pendingSelection = id;
                             self->refresh();
                             success = true;
                         } catch (const std::exception& e) {
                             self->error(e);
                         }
                         gtk_drag_finish(ctx, success, false, time);
                     }),
                     this);
}
// ---------------------------------------------------------------------------
// Fluent shell: command bar, collapsible ribbon, floating panels, palette.
// Design tokens and rationale: docs/design-references/FOLIQ-DESIGN-SYSTEM.md
// ---------------------------------------------------------------------------
static GtkWidget* iconImage(const char* icon, std::vector<GtkWidget*>& images) {
    auto* image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON);
    gtk_image_set_pixel_size(GTK_IMAGE(image), 16);
    images.push_back(image);
    return image;
}
void Navigation::chrome(GtkWidget* container) {
    auto* mainBox = gtk_widget_get_parent(gtk_widget_get_parent(container));
    ribbon = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(ribbon, "foliq-ribbon");
    gtk_box_pack_start(GTK_BOX(mainBox), ribbon, false, false, 0);
    gtk_box_reorder_child(GTK_BOX(mainBox), ribbon, 1);
    commandBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_name(commandBar, "foliq-commandbar");
    gtk_box_pack_start(GTK_BOX(ribbon), commandBar, false, false, 0);

    // Commands never take keyboard focus on click: the active text/ink input keeps it.
    auto prepare = [&](GtkWidget* b, const char* label, const char* icon, bool showLabel) {
        gtk_widget_set_focus_on_click(b, false);
        if (icon) {
            gtk_button_set_image(GTK_BUTTON(b), iconImage(icon, ribbonImages));
            gtk_button_set_always_show_image(GTK_BUTTON(b), true);
        }
        if (showLabel || !icon)
            gtk_button_set_label(GTK_BUTTON(b), label);
        else
            gtk_widget_set_tooltip_text(b, label);
        atk_object_set_name(gtk_widget_get_accessible(b), label);
        return b;
    };
    auto button = [&](GtkWidget* box, const char* label, const char* icon, ActionType action, bool showLabel = false) {
        bool isTool = action >= ACTION_TOOL_PEN && action <= ACTION_TOOL_HAND;
        auto* b = isTool ? gtk_toggle_button_new() : gtk_button_new();
        prepare(b, label, icon, showLabel);
        if (isTool)
            toolButtons.emplace_back(b, action - ACTION_TOOL_PEN + TOOL_PEN);
        if (action == ACTION_CUT || action == ACTION_COPY || action == ACTION_PASTE)
            mirroredButtons.emplace_back(b, action == ACTION_CUT  ? "menuEditCut" :
                                            action == ACTION_COPY ? "menuEditCopy" :
                                                                    "menuEditPaste");
        g_object_set_data(G_OBJECT(b), "action", GINT_TO_POINTER(action));
        gtk_box_pack_start(GTK_BOX(box), b, false, false, 0);
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer p) {
                             auto a =
                                     static_cast<ActionType>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "action")));
                             auto* self = static_cast<Navigation*>(p);
                             if (self->updatingToolButtons)
                                 return;
                             self->control->actionPerformed(
                                     a, a >= ACTION_TOOL_PEN && a <= ACTION_TOOL_HAND ? GROUP_TOOL : GROUP_NOGROUP,
                                     nullptr, true);
                             // A temporary ribbon closes after a command, like a flyout.
                             self->hideTemporary();
                             self->updateStatus();
                         }),
                         this);
        return b;
    };

    // --- Command bar: [nav] Foliq  Start Einfügen Zeichnen Ansicht   crumb/status   search undo redo save ⋯ pin
    navToggle = gtk_toggle_button_new();
    prepare(navToggle, "Navigation", "foliq-panel-left", false);
    gtk_widget_set_tooltip_text(navToggle, "Navigation ein-/ausblenden (Strg+Umschalt+N)");
    gtk_box_pack_start(GTK_BOX(commandBar), navToggle, false, false, 0);
    g_signal_connect(navToggle, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         if (!self->updatingChrome)
                             self->setNavVisible(gtk_toggle_button_get_active(b));
                     }),
                     this);
    auto* brand = gtk_label_new("Foliq");
    gtk_widget_set_name(brand, "foliq-brand");
    gtk_box_pack_start(GTK_BOX(commandBar), brand, false, false, 4);
    auto* tabStrip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_name(tabStrip, "foliq-tabstrip");
    atk_object_set_name(gtk_widget_get_accessible(tabStrip), "Menüband-Registerkarten");
    gtk_box_pack_start(GTK_BOX(commandBar), tabStrip, false, false, 8);
    const char* tabNames[] = {"Start", "Einfügen", "Zeichnen", "Ansicht"};
    for (int i = 0; i < 4; ++i) {
        auto* tab = gtk_toggle_button_new_with_label(tabNames[i]);
        gtk_widget_set_focus_on_click(tab, false);
        atk_object_set_role(gtk_widget_get_accessible(tab), ATK_ROLE_PAGE_TAB);
        g_object_set_data(G_OBJECT(tab), "index", GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(tabStrip), tab, false, false, 0);
        tabButtons.push_back(tab);
        g_signal_connect(tab, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer p) {
                             auto* self = static_cast<Navigation*>(p);
                             if (!self->updatingChrome)
                                 self->showTab(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "index")), true);
                         }),
                         this);
    }
    auto* middle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(commandBar), middle, true, true, 0);
    crumb = gtk_label_new("");
    gtk_widget_set_name(crumb, "foliq-crumb");
    gtk_label_set_ellipsize(GTK_LABEL(crumb), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_width_chars(GTK_LABEL(crumb), 8);
    gtk_label_set_xalign(GTK_LABEL(crumb), 1);
    gtk_box_pack_start(GTK_BOX(middle), crumb, true, true, 0);
    status = gtk_label_new("Lokal");
    gtk_widget_set_name(status, "foliq-status");
    gtk_label_set_ellipsize(GTK_LABEL(status), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(middle), status, false, false, 0);

    auto* findButton = gtk_button_new();
    gtk_widget_set_name(findButton, "foliq-search-button");
    gtk_widget_set_focus_on_click(findButton, false);
    auto* findBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(findBox), iconImage("foliq-search", ribbonImages), false, false, 0);
    auto* findLabel = gtk_label_new("Suchen");
    gtk_box_pack_start(GTK_BOX(findBox), findLabel, false, false, 0);
    gtk_container_add(GTK_CONTAINER(findButton), findBox);
    atk_object_set_name(gtk_widget_get_accessible(findButton), "Suchen");
    gtk_widget_set_tooltip_text(findButton, "Im aktuellen Dokument suchen (Strg+F)");
    g_object_set_data(G_OBJECT(findButton), "action", GINT_TO_POINTER(ACTION_SEARCH));
    g_signal_connect(findButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) {
                         static_cast<Navigation*>(p)->control->actionPerformed(ACTION_SEARCH, GROUP_NOGROUP, nullptr,
                                                                               true);
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(commandBar), findButton, false, false, 6);
    undoButton = button(commandBar, "Rückgängig", "xopp-edit-undo", ACTION_UNDO, false);
    redoButton = button(commandBar, "Wiederholen", "xopp-edit-redo", ACTION_REDO, false);
    gtk_widget_set_tooltip_text(undoButton, "Rückgängig (Strg+Z)");
    gtk_widget_set_tooltip_text(redoButton, "Wiederholen (Strg+Y)");
    auto* save = button(commandBar, "Speichern", "xopp-document-save", ACTION_SAVE, false);
    gtk_widget_set_tooltip_text(save, "Speichern (Strg+S) – Seiten im Notizbuch speichern automatisch");
    menuButton = gtk_button_new();
    prepare(menuButton, "Menü", "foliq-more", false);
    gtk_widget_set_tooltip_text(menuButton, "Alle Befehle: Datei, Bearbeiten, Ansicht, Journal, Extras, Hilfe (F10)");
    gtk_box_pack_start(GTK_BOX(commandBar), menuButton, false, false, 0);
    g_signal_connect(menuButton, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         if (!self->appMenu)
                             return;
#if GTK_CHECK_VERSION(3, 22, 0)
                         gtk_menu_popup_at_widget(GTK_MENU(self->appMenu), GTK_WIDGET(b), GDK_GRAVITY_SOUTH_EAST,
                                                  GDK_GRAVITY_NORTH_EAST, nullptr);
#else
                         gtk_menu_popup(GTK_MENU(self->appMenu), nullptr, nullptr, nullptr, nullptr, 0,
                                        gtk_get_current_event_time());
#endif
                     }),
                     this);
    fullscreenButton = gtk_button_new();
    prepare(fullscreenButton, "Vollbild (F11)", "xopp-fullscreen", false);
    gtk_box_pack_start(GTK_BOX(commandBar), fullscreenButton, false, false, 0);
    g_signal_connect(fullscreenButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         bool on = !self->fullscreen;
                         self->control->actionPerformed(ACTION_FULLSCREEN, GROUP_FULLSCREEN, nullptr, on);
                         self->applyFullscreen(on);  // also without a window manager state event
                     }),
                     this);
    ribbonPinButton = gtk_toggle_button_new();
    prepare(ribbonPinButton, "Menüband anheften", "foliq-chevron-up", false);
    gtk_widget_set_tooltip_text(ribbonPinButton, "Menüband anheften oder einklappen (Strg+F1)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ribbonPinButton), true);
    gtk_box_pack_start(GTK_BOX(commandBar), ribbonPinButton, false, false, 0);
    g_signal_connect(ribbonPinButton, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         if (!self->updatingChrome)
                             self->setRibbonPinned(gtk_toggle_button_get_active(b));
                     }),
                     this);

    // --- Ribbon panel: one rounded layer with grouped commands per tab.
    ribbonPanel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(ribbonPanel, "foliq-ribbon-panel");
    gtk_box_pack_start(GTK_BOX(ribbon), ribbonPanel, false, false, 0);
    ribbonStack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(ribbonStack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(ribbonStack), 120);
    gtk_box_pack_start(GTK_BOX(ribbonPanel), ribbonStack, false, false, 0);
    g_object_set_data(G_OBJECT(ribbon), "tabs", ribbonStack);
    auto page = [&](const char* name) {
        auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), true);
        auto* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_container_add(GTK_CONTAINER(scroll), box);
        gtk_stack_add_named(GTK_STACK(ribbonStack), scroll, name);
        return box;
    };
    auto separator = [](GtkWidget* box) {
        gtk_box_pack_start(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), false, false, 0);
    };
    auto custom = [&](GtkWidget* box, const char* label, const char* icon, GCallback cb) {
        auto* b = prepare(gtk_button_new(), label, icon, false);
        gtk_box_pack_start(GTK_BOX(box), b, false, false, 0);
        g_signal_connect(b, "clicked", cb, this);
        return b;
    };

    auto* home = page("Start");
    button(home, "Einfügen", "xopp-edit-paste", ACTION_PASTE);
    button(home, "Ausschneiden", "xopp-edit-cut", ACTION_CUT, false);
    button(home, "Kopieren", "xopp-edit-copy", ACTION_COPY, false);
    separator(home);
    button(home, "Text", "foliq-type", ACTION_TOOL_TEXT);
    auto* format = custom(home, "Formatieren", "foliq-text-format", G_CALLBACK(+[](GtkButton*, gpointer p) {
                              auto* self = static_cast<Navigation*>(p);
                              self->hideTemporary();
                              if (auto* editor = self->control->getTextEditor())
                                  editor->showFormattingDialog();
                              else
                                  XojMsgBox::showErrorToUser(self->control->getGtkWindow(),
                                                             "Zuerst mit dem Textwerkzeug einen Textcontainer auf der "
                                                             "Seite anklicken.");
                          }));
    gtk_widget_set_tooltip_text(format, "Text formatieren: Textcontainer anklicken, dann formatieren (F4)");
    separator(home);
    button(home, "Auswahl", "xopp-select-rect", ACTION_TOOL_SELECT_RECT);
    button(home, "Lasso", "xopp-select-lasso", ACTION_TOOL_SELECT_REGION);

    auto* insert = page("Einfügen");
    button(insert, "Seite", "xopp-page-add", ACTION_NEW_PAGE_AFTER);
    auto* surface = button(insert, "Seitentyp und Papier", "foliq-file-cog", ACTION_PAPER_FORMAT);
    gtk_widget_set_tooltip_text(surface, "Papierformat, freie Arbeitsfläche, Lineatur, Farbe und PDF-Ausschnitt");
    separator(insert);
    button(insert, "Bild", "xopp-tool-image", ACTION_TOOL_IMAGE);
    custom(insert, "Zuschneiden", "foliq-crop", G_CALLBACK(+[](GtkButton*, gpointer p) {
               static_cast<Navigation*>(p)->hideTemporary();
               ImageHandler::editSelectedImage(static_cast<Navigation*>(p)->control);
           }));
    button(insert, "Formel", "foliq-sigma", ACTION_TEX);
    separator(insert);
    button(insert, "PDF annotieren", "xopp-document-open", ACTION_ANNOTATE_PDF);

    auto* draw = page("Zeichnen");
    button(draw, "Stift", "xopp-tool-pencil", ACTION_TOOL_PEN);
    button(draw, "Textmarker", "xopp-tool-highlighter", ACTION_TOOL_HIGHLIGHTER);
    button(draw, "Radierer", "xopp-tool-eraser", ACTION_TOOL_ERASER);
    separator(draw);
    button(draw, "Lasso", "xopp-select-lasso", ACTION_TOOL_SELECT_REGION, false);
    button(draw, "Hand", "xopp-hand", ACTION_TOOL_HAND);
    separator(draw);
    // Shape assistance (existing Xournal++ drawing types): one active at a time, click again = freehand.
    struct ShapeEntry {
        const char* label;
        const char* icon;
        ActionType action;
        DrawingType type;
    };
    for (const auto& e: {ShapeEntry{"Formerkennung: Freihand wird zu Linie, Rechteck, Dreieck, Kreis oder Pfeil",
                                    "xopp-shape-recognizer", ACTION_SHAPE_RECOGNIZER, DRAWING_TYPE_STROKE_RECOGNIZER},
                         ShapeEntry{"Gerade Linie", "xopp-draw-line", ACTION_RULER, DRAWING_TYPE_LINE},
                         ShapeEntry{"Rechteck", "xopp-draw-rect", ACTION_TOOL_DRAW_RECT, DRAWING_TYPE_RECTANGLE},
                         ShapeEntry{"Ellipse", "xopp-draw-ellipse", ACTION_TOOL_DRAW_ELLIPSE, DRAWING_TYPE_ELLIPSE},
                         ShapeEntry{"Pfeil", "xopp-draw-arrow", ACTION_TOOL_DRAW_ARROW, DRAWING_TYPE_ARROW}}) {
        auto* b = prepare(gtk_toggle_button_new(), e.label, e.icon, false);
        g_object_set_data(G_OBJECT(b), "action", GINT_TO_POINTER(e.action));
        shapeButtons.emplace_back(b, e.type);
        gtk_box_pack_start(GTK_BOX(draw), b, false, false, 0);
        g_signal_connect(b, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                             auto* self = static_cast<Navigation*>(p);
                             if (self->updatingToolButtons)
                                 return;
                             auto a = static_cast<ActionType>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "action")));
                             self->control->actionPerformed(a, GROUP_RULER, nullptr, gtk_toggle_button_get_active(b));
                             self->hideTemporary();
                             self->updateStatus();
                         }),
                         this);
    }
    separator(draw);
    // Measuring tools: existing geometry tools; clicking again removes them from the page.
    button(draw, "Geodreieck / Lineal (erneut klicken zum Entfernen)", "xopp-setsquare", ACTION_SETSQUARE);
    button(draw, "Zirkel (erneut klicken zum Entfernen)", "xopp-compass", ACTION_COMPASS);
    separator(draw);
    drawOptions = gtk_toggle_button_new();
    prepare(drawOptions, "Farben und Stärken", "foliq-sliders", false);
    gtk_widget_set_tooltip_text(drawOptions, "Werkzeugleisten für Farben, Strichstärken und Spezialwerkzeuge");
    gtk_box_pack_start(GTK_BOX(draw), drawOptions, false, false, 0);

    auto* view = page("Ansicht");
    button(view, "Einpassen", "foliq-fit", ACTION_ZOOM_FIT);
    button(view, "100 %", "foliq-percent", ACTION_ZOOM_100);
    button(view, "Verkleinern", "foliq-zoom-out", ACTION_ZOOM_OUT, false);
    button(view, "Vergrößern", "foliq-zoom-in", ACTION_ZOOM_IN, false);
    separator(view);
    focusButton = gtk_toggle_button_new();
    prepare(focusButton, "Fokusmodus", "foliq-focus", false);
    gtk_widget_set_tooltip_text(focusButton, "Navigation ausblenden und Menüband einklappen (Strg+Umschalt+F)");
    gtk_box_pack_start(GTK_BOX(view), focusButton, false, false, 0);
    g_object_set_data(G_OBJECT(ribbon), "focus", focusButton);
    g_signal_connect(focusButton, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         if (!self->updatingChrome)
                             self->setFocusMode(gtk_toggle_button_get_active(b));
                     }),
                     this);
    separator(view);
    // Design: segmented System/Hell/Dunkel, density, accent, motion.
    GSList* group = nullptr;
    for (const auto* name: {"System", "Hell", "Dunkel"}) {
        auto* b = gtk_radio_button_new_with_label(group, name);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(b));
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), false);
        gtk_widget_set_focus_on_click(b, false);
        g_object_set_data(G_OBJECT(b), "index", GINT_TO_POINTER(themeButtons.size()));
        gtk_box_pack_start(GTK_BOX(view), b, false, false, 0);
        themeButtons.push_back(b);
        g_signal_connect(b, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                             auto* self = static_cast<Navigation*>(p);
                             if (self->updatingChrome || !gtk_toggle_button_get_active(b))
                                 return;
                             self->theme = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "index"));
                             self->applyTheme();
                         }),
                         this);
    }
    theme = static_cast<int>(control->getSettings()->getThemeVariant());
    separator(view);
    group = nullptr;
    for (const auto* name: {"Kompakt", "Berührung"}) {
        auto* b = gtk_radio_button_new_with_label(group, name);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(b));
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), false);
        gtk_widget_set_focus_on_click(b, false);
        gtk_widget_set_tooltip_text(b, std::string(name) == "Kompakt" ?
                                               "Kompakt für Maus und Tastatur" :
                                               "Berührungsfreundlich: größere Trefferflächen für Stift und Touch");
        gtk_box_pack_start(GTK_BOX(view), b, false, false, 0);
        if (std::string(name) == "Berührung") {
            touchButton = b;
            g_signal_connect(b, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                                 auto* self = static_cast<Navigation*>(p);
                                 if (!self->updatingChrome)
                                     self->setDensity(gtk_toggle_button_get_active(b));
                             }),
                             this);
        }
    }
    separator(view);
    custom(view, "Akzentfarbe", "foliq-sun-moon", G_CALLBACK(+[](GtkButton* b, gpointer p) {
               static_cast<Navigation*>(p)->accentPopover(GTK_WIDGET(b), {});
           }));
    custom(view, "Einrichtung starten (Profil, Speicherort, Fächer)", "foliq-file-cog",
           G_CALLBACK(+[](GtkButton*, gpointer p) {
               auto* self = static_cast<Navigation*>(p);
               self->hideTemporary();
               self->onboarding(false);
           }));
    motionButton = gtk_check_button_new_with_label("Weniger Bewegung");
    gtk_widget_set_focus_on_click(motionButton, false);
    gtk_widget_set_tooltip_text(motionButton, "Animationen und Übergänge abschalten");
    gtk_box_pack_start(GTK_BOX(view), motionButton, false, false, 4);
    g_signal_connect(motionButton, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         if (self->updatingChrome)
                             return;
                         self->options.reducedMotion = gtk_toggle_button_get_active(b);
                         self->applyPalette();
                     }),
                     this);
    legacyToggle = gtk_check_button_new_with_label("Alle Werkzeugleisten");
    gtk_widget_set_focus_on_click(legacyToggle, false);
    gtk_widget_set_tooltip_text(legacyToggle, "Bestehende konfigurierbare Werkzeugleisten einblenden");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(legacyToggle), control->getSettings()->isToolbarVisible());
    gtk_box_pack_start(GTK_BOX(view), legacyToggle, false, false, 4);
    for (auto* toggle: {legacyToggle, drawOptions})
        g_signal_connect(toggle, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                             auto* self = static_cast<Navigation*>(p);
                             if (self->control->getWindow() && !self->updatingChrome)
                                 self->control->getWindow()->setToolbarVisible(gtk_toggle_button_get_active(b));
                         }),
                         this);

    // Responsive command bar: below ~1100 px, secondary text (brand, path, search label) yields
    // space so the window still shrinks to small sizes without clipping commands.
    g_object_set_data(G_OBJECT(commandBar), "brand", brand);
    g_object_set_data(G_OBJECT(commandBar), "findLabel", findLabel);
    g_object_set_data(G_OBJECT(commandBar), "findButton", findButton);
    g_signal_connect(window, "size-allocate", G_CALLBACK(+[](GtkWidget* w, GdkRectangle* a, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         bool narrow = a->width < 1100;
                         auto* bar = self->commandBar;
                         auto set = [&](const char* key, bool visible) {
                             auto* widget = GTK_WIDGET(g_object_get_data(G_OBJECT(bar), key));
                             if (gtk_widget_get_visible(widget) != visible)
                                 gtk_widget_set_visible(widget, visible);
                         };
                         set("brand", !narrow);
                         set("findLabel", !narrow);
                         if (gtk_widget_get_visible(self->crumb) == narrow)
                             gtk_widget_set_visible(self->crumb, !narrow);
                         auto* ctx = gtk_widget_get_style_context(
                                 GTK_WIDGET(g_object_get_data(G_OBJECT(bar), "findButton")));
                         if (narrow)
                             gtk_style_context_add_class(ctx, "foliq-narrow");
                         else
                             gtk_style_context_remove_class(ctx, "foliq-narrow");
                     }),
                     this);

    // Fullscreen pill: floating, top centre; the same commands as the ribbon (no new tool logic).
    fullscreenPill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_name(fullscreenPill, "foliq-fullscreen-pill");
    gtk_widget_set_halign(fullscreenPill, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(fullscreenPill, GTK_ALIGN_START);
    gtk_widget_set_margin_top(fullscreenPill, 10);
    atk_object_set_name(gtk_widget_get_accessible(fullscreenPill), "Vollbild-Werkzeuge");
    button(fullscreenPill, "Stift", "xopp-tool-pencil", ACTION_TOOL_PEN);
    button(fullscreenPill, "Textmarker", "xopp-tool-highlighter", ACTION_TOOL_HIGHLIGHTER);
    button(fullscreenPill, "Radierer", "xopp-tool-eraser", ACTION_TOOL_ERASER);
    button(fullscreenPill, "Lasso", "xopp-select-lasso", ACTION_TOOL_SELECT_REGION);
    gtk_box_pack_start(GTK_BOX(fullscreenPill), gtk_separator_new(GTK_ORIENTATION_VERTICAL), false, false, 2);
    button(fullscreenPill, "Rückgängig (Strg+Z)", "xopp-edit-undo", ACTION_UNDO);
    auto* showRibbon = gtk_toggle_button_new();
    prepare(showRibbon, "Menüband im Vollbild ein-/ausblenden", "foliq-chevron-down", false);
    gtk_box_pack_start(GTK_BOX(fullscreenPill), showRibbon, false, false, 0);
    g_signal_connect(showRibbon, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         gtk_widget_set_visible(self->ribbon, gtk_toggle_button_get_active(b));
                     }),
                     this);
    g_object_set_data(G_OBJECT(fullscreenPill), "ribbonToggle", showRibbon);
    auto* leave = gtk_button_new();
    prepare(leave, "Vollbild beenden (Esc)", "foliq-focus", false);
    gtk_box_pack_start(GTK_BOX(fullscreenPill), leave, false, false, 0);
    g_signal_connect(leave, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         self->control->actionPerformed(ACTION_FULLSCREEN, GROUP_FULLSCREEN, nullptr, false);
                         self->applyFullscreen(false);
                     }),
                     this);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), fullscreenPill);
    gtk_widget_show_all(fullscreenPill);
    gtk_widget_set_no_show_all(fullscreenPill, true);
    gtk_widget_hide(fullscreenPill);
    // F11, menu and icon all end up in GTK's window state: follow it, whatever triggered it.
    g_signal_connect(window, "window-state-event",
                     G_CALLBACK(+[](GtkWidget*, GdkEventWindowState* e, gpointer p) -> gboolean {
                         if (e->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
                             static_cast<Navigation*>(p)->applyFullscreen(e->new_window_state &
                                                                          GDK_WINDOW_STATE_FULLSCREEN);
                         return false;
                     }),
                     this);

    // Shell-wide keyboard handling, capture-phase presses for light dismiss, menubar relocation.
    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* e, gpointer p) -> gboolean {
                         return static_cast<Navigation*>(p)->onKey(e);
                     }),
                     this);
    auto* press = gtk_gesture_multi_press_new(GTK_WIDGET(window));
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(press), GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(press), 0);
    g_object_set_data_full(G_OBJECT(window), "foliq-press", press, g_object_unref);
    g_signal_connect(press, "pressed", G_CALLBACK(+[](GtkGestureMultiPress*, gint, gdouble x, gdouble y, gpointer p) {
                         static_cast<Navigation*>(p)->onPress(x, y);
                     }),
                     this);
    // Relocate the menubar after Control has bound the menu accelerators (Control::initWindow).
    g_idle_add(
            +[](gpointer p) -> gboolean {
                static_cast<Navigation*>(p)->moveMenubar();
                return false;
            },
            this);
    showTab(0, false);
    setRibbonPinned(true);
    updatingChrome = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(navToggle), true);
    if (auto* pin = GTK_WIDGET(g_object_get_data(G_OBJECT(panel), "pin")))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pin), true);
    updatingChrome = false;
    applyPalette();
}

void Navigation::openLibrary(const fs::path& root) {
    if (!rememberPosition() || !flush())
        throw std::runtime_error("Aktuelle Seite konnte nicht gespeichert werden; Speicherort nicht gewechselt.");
    saveView();
    catalog.reset();  // releases the old library lock
    catalog = std::make_unique<Catalog>(root);
    section.clear();
    options.sectionColors.clear();
    viewWritable = false;
    try {
        loadView();
    } catch (const std::exception& e) {
        error(e);
    }
    gtk_widget_set_tooltip_text(status, root.u8string().c_str());
    gtk_widget_set_sensitive(panel, true);
    refresh();
}
void Navigation::applyFullscreen(bool on) {
    if (on == fullscreen)
        return;
    fullscreen = on;
    hideTemporary();
    auto* ribbonToggle = GTK_WIDGET(g_object_get_data(G_OBJECT(fullscreenPill), "ribbonToggle"));
    if (on) {
        focusBeforeFullscreen = focus;
        setFocusMode(true);  // hides navigation, collapses ribbon (preferences untouched)
        gtk_widget_hide(ribbon);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ribbonToggle), false);
        gtk_widget_set_no_show_all(fullscreenPill, false);
        gtk_widget_show_all(fullscreenPill);
    } else {
        gtk_widget_hide(fullscreenPill);
        gtk_widget_set_no_show_all(fullscreenPill, true);
        gtk_widget_show(ribbon);
        setFocusMode(focusBeforeFullscreen);
    }
    auto* image = gtk_button_get_image(GTK_BUTTON(fullscreenButton));
    gtk_image_set_from_icon_name(GTK_IMAGE(image), on ? "foliq-focus" : "xopp-fullscreen", GTK_ICON_SIZE_BUTTON);
    atk_object_set_name(gtk_widget_get_accessible(fullscreenButton), on ? "Vollbild beenden" : "Vollbild (F11)");
}
void Navigation::moveMenubar() {
    // All upstream menus remain available (and keep their accelerators) in one "⋯" menu.
    // The menu is attached to the visible button, so GTK still activates the accelerators.
    auto* mw = control->getWindow();
    if (!mw)
        return;
    auto* bar = mw->get("mainMenubar");
    if (!bar || !GTK_IS_MENU_SHELL(bar))
        return;
    appMenu = gtk_menu_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(appMenu), "foliq-menu");
    gtk_menu_attach_to_widget(GTK_MENU(appMenu), menuButton, nullptr);
    GList* items = gtk_container_get_children(GTK_CONTAINER(bar));
    for (GList* l = items; l; l = l->next) {
        auto* item = GTK_WIDGET(l->data);
        g_object_ref(item);
        gtk_container_remove(GTK_CONTAINER(bar), item);
        gtk_menu_shell_append(GTK_MENU_SHELL(appMenu), item);
        g_object_unref(item);
    }
    g_list_free(items);
    auto* setup = gtk_menu_item_new_with_label("Foliq einrichten …");
    g_signal_connect(setup, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer p) {
                         static_cast<Navigation*>(p)->onboarding(false);
                     }),
                     this);
    gtk_menu_shell_prepend(GTK_MENU_SHELL(appMenu), gtk_separator_menu_item_new());
    gtk_menu_shell_prepend(GTK_MENU_SHELL(appMenu), setup);
    gtk_widget_show_all(appMenu);
    gtk_widget_set_no_show_all(bar, true);
    gtk_widget_hide(bar);
}

void Navigation::showTab(int index, bool userAction) {
    index = std::max(0, std::min(3, index));
    bool wasActive = gtk_stack_get_visible_child_name(GTK_STACK(ribbonStack)) &&
                     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tabButtons[index]));
    updatingChrome = true;
    for (size_t i = 0; i < tabButtons.size(); ++i)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tabButtons[i]), static_cast<int>(i) == index);
    updatingChrome = false;
    const char* names[] = {"Start", "Einfügen", "Zeichnen", "Ansicht"};
    gtk_stack_set_visible_child_name(GTK_STACK(ribbonStack), names[index]);
    options.tab = index;
    if (!userAction)
        return;
    bool docked = !ribbonTemporary && gtk_widget_get_visible(ribbonPanel);
    if (!docked) {
        // Collapsed ribbon: the tab opens it temporarily as a floating layer over the canvas;
        // clicking the open tab again closes it.
        if (ribbonTemporary && wasActive)
            hideTemporary();
        else if (!ribbonTemporary) {
            ribbonTemporary = true;
            g_object_ref(ribbonPanel);
            gtk_container_remove(GTK_CONTAINER(ribbon), ribbonPanel);
            gtk_overlay_add_overlay(GTK_OVERLAY(overlay), ribbonPanel);
            g_object_unref(ribbonPanel);
            gtk_widget_set_valign(ribbonPanel, GTK_ALIGN_START);
            gtk_widget_set_halign(ribbonPanel, GTK_ALIGN_FILL);
            gtk_widget_set_margin_start(ribbonPanel, 8);
            gtk_widget_set_margin_end(ribbonPanel, 8);
            gtk_style_context_add_class(gtk_widget_get_style_context(ribbonPanel), "foliq-temporary");
            gtk_widget_set_no_show_all(ribbonPanel, false);
            gtk_widget_show_all(ribbonPanel);
        }
    }
}
void Navigation::hideTemporary() {
    if (navFloating && gtk_widget_get_visible(panel) && !options.navPinned)
        setNavVisible(false);
    if (!ribbonTemporary)
        return;
    ribbonTemporary = false;
    gtk_widget_hide(ribbonPanel);
    gtk_widget_set_no_show_all(ribbonPanel, true);
    g_object_ref(ribbonPanel);
    gtk_container_remove(GTK_CONTAINER(overlay), ribbonPanel);
    gtk_box_pack_start(GTK_BOX(ribbon), ribbonPanel, false, false, 0);
    g_object_unref(ribbonPanel);
    gtk_widget_set_margin_start(ribbonPanel, 0);
    gtk_widget_set_margin_end(ribbonPanel, 0);
    gtk_style_context_remove_class(gtk_widget_get_style_context(ribbonPanel), "foliq-temporary");
}
void Navigation::setRibbonPinned(bool pinned) {
    hideTemporary();
    options.ribbonPinned = pinned;
    updatingChrome = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ribbonPinButton), pinned);
    updatingChrome = false;
    auto* image = gtk_button_get_image(GTK_BUTTON(ribbonPinButton));
    gtk_image_set_from_icon_name(GTK_IMAGE(image), pinned ? "foliq-chevron-up" : "foliq-chevron-down",
                                 GTK_ICON_SIZE_BUTTON);
    const char* name = pinned ? "Menüband einklappen" : "Menüband anheften";
    atk_object_set_name(gtk_widget_get_accessible(ribbonPinButton), name);
    gtk_widget_set_tooltip_text(ribbonPinButton, (std::string(name) + " (Strg+F1)").c_str());
    gtk_widget_set_no_show_all(ribbonPanel, !pinned);
    gtk_widget_set_visible(ribbonPanel, pinned);
    if (pinned)
        gtk_widget_show_all(ribbonPanel);
}
void Navigation::setNavVisible(bool visible) {
    if (navFloating && !visible) {
        // Return the floating panel to its docked slot (hidden).
        g_object_ref(panel);
        gtk_container_remove(GTK_CONTAINER(overlay), panel);
        gtk_paned_pack1(GTK_PANED(outer), panel, false, false);
        g_object_unref(panel);
        navFloating = false;
        gtk_widget_set_size_request(panel, -1, -1);
        gtk_widget_set_halign(panel, GTK_ALIGN_FILL);
        gtk_widget_set_valign(panel, GTK_ALIGN_FILL);
        gtk_paned_set_position(GTK_PANED(outer), options.width);
        gtk_paned_set_position(GTK_PANED(split), options.books);
        gtk_style_context_remove_class(gtk_widget_get_style_context(panel), "foliq-floating");
    }
    if (visible && !options.navPinned && !navFloating) {
        // Unpinned navigation floats over the canvas and closes after a page is opened.
        if (gtk_widget_get_visible(panel)) {
            options.width = gtk_paned_get_position(GTK_PANED(outer));
            options.books = gtk_paned_get_position(GTK_PANED(split));
        }
        g_object_ref(panel);
        gtk_container_remove(GTK_CONTAINER(outer), panel);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), panel);
        g_object_unref(panel);
        gtk_widget_set_halign(panel, GTK_ALIGN_START);
        gtk_widget_set_valign(panel, GTK_ALIGN_FILL);
        gtk_widget_set_size_request(panel, std::max(320, options.width), -1);
        gtk_style_context_add_class(gtk_widget_get_style_context(panel), "foliq-floating");
        navFloating = true;
    } else if (!navFloating)
        gtk_widget_set_size_request(panel, -1, -1);
    if (!visible && !navFloating && gtk_widget_get_visible(panel)) {
        options.width = gtk_paned_get_position(GTK_PANED(outer));
        options.books = gtk_paned_get_position(GTK_PANED(split));
    }
    gtk_widget_set_no_show_all(panel, !visible);
    gtk_widget_set_visible(panel, visible);
    if (visible)
        gtk_widget_show_all(panel);
    updatingChrome = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(navToggle), visible);
    updatingChrome = false;
    if (visible && navFloating)
        gtk_widget_grab_focus(books);
}
void Navigation::setNavPinned(bool pinned) {
    bool visible = gtk_widget_get_visible(panel);
    if (navFloating)
        setNavVisible(false);
    options.navPinned = pinned;
    updatingChrome = true;
    if (auto* pin = GTK_WIDGET(g_object_get_data(G_OBJECT(panel), "pin")))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pin), pinned);
    updatingChrome = false;
    setNavVisible(pinned ? (visible && !focus) : false);
}
void Navigation::setFocusMode(bool on) {
    focus = on;
    updatingChrome = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(focusButton), on);
    updatingChrome = false;
    setNavVisible(!on && options.navPinned);
    // Focus mode collapses the ribbon without forgetting the user's pin preference.
    bool pinned = options.ribbonPinned;
    setRibbonPinned(!on && pinned);
    options.ribbonPinned = pinned;
}
void Navigation::setDensity(bool touch) {
    options.touch = touch;
    auto* ctx = gtk_widget_get_style_context(GTK_WIDGET(window));
    if (touch)
        gtk_style_context_add_class(ctx, "foliq-touch");
    else
        gtk_style_context_remove_class(ctx, "foliq-touch");
    for (auto* image: ribbonImages) gtk_image_set_pixel_size(GTK_IMAGE(image), touch ? 20 : 16);
    for (auto* view: {books, pages}) {
        auto* col = gtk_tree_view_get_column(GTK_TREE_VIEW(view), 0);
        GList* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(col));
        for (GList* c = cells; c; c = c->next) g_object_set(c->data, "ypad", touch ? 11 : 7, nullptr);
        g_list_free(cells);
        gtk_tree_view_columns_autosize(GTK_TREE_VIEW(view));
    }
    updatingChrome = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(touchButton), touch);
    if (!touch) {
        GSList* group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(touchButton));
        for (GSList* g = group; g; g = g->next)
            if (g->data != touchButton)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g->data), true);
    }
    updatingChrome = false;
}
void Navigation::accentPopover(GtkWidget* anchor, const std::string& target) {
    auto* pop = gtk_popover_new(anchor);
    auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 4);
    auto* title = gtk_label_new(target.empty() ? "Akzentfarbe" : "Abschnittsfarbe");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "foliq-pane-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_pack_start(GTK_BOX(box), title, false, false, 0);
    auto* grid = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid), 4);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(grid), GTK_SELECTION_NONE);
    gtk_box_pack_start(GTK_BOX(box), grid, false, false, 0);
    auto choices = accentChoices();
    if (!target.empty())
        choices.insert(choices.begin(), {"Keine", ""});
    const std::string current = target.empty() ? options.accent :
                                options.sectionColors.count(target) ? options.sectionColors.at(target) :
                                                                       "";
    for (const auto& choice: choices) {
        auto* b = gtk_button_new();
        auto* swatch = gtk_drawing_area_new();
        gtk_widget_set_size_request(swatch, 28, 28);
        auto* color = new std::string(choice.second);
        g_object_set_data_full(G_OBJECT(b), "color", color, [](gpointer p) { delete static_cast<std::string*>(p); });
        g_object_set_data(G_OBJECT(swatch), "color", color);
        g_object_set_data(G_OBJECT(swatch), "selected", GINT_TO_POINTER(choice.second == current));
        g_signal_connect(swatch, "draw", G_CALLBACK(+[](GtkWidget* w, cairo_t* cr, gpointer) -> gboolean {
                             auto* c = static_cast<std::string*>(g_object_get_data(G_OBJECT(w), "color"));
                             double size = std::min(gtk_widget_get_allocated_width(w),
                                                    gtk_widget_get_allocated_height(w));
                             Rgb rgb;
                             cairo_arc(cr, size / 2, size / 2, size / 2 - 3, 0, 2 * M_PI);
                             if (parseHex(*c, rgb)) {
                                 cairo_set_source_rgb(cr, rgb.r, rgb.g, rgb.b);
                                 cairo_fill_preserve(cr);
                             }
                             GdkRGBA fg;
                             gtk_style_context_get_color(gtk_widget_get_style_context(w), GTK_STATE_FLAG_NORMAL, &fg);
                             cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 0.35);
                             cairo_set_line_width(cr, 1);
                             cairo_stroke(cr);
                             if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "selected"))) {
                                 // Selection ring uses the text colour: visible with any chosen colour.
                                 cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 1);
                                 cairo_set_line_width(cr, 2);
                                 cairo_arc(cr, size / 2, size / 2, size / 2 - 1, 0, 2 * M_PI);
                                 cairo_stroke(cr);
                             }
                             return true;
                         }),
                         nullptr);
        gtk_container_add(GTK_CONTAINER(b), swatch);
        gtk_widget_set_tooltip_text(b, choice.first.c_str());
        atk_object_set_name(gtk_widget_get_accessible(b),
                            (choice.first + (choice.second == current ? " (ausgewählt)" : "")).c_str());
        g_object_set_data_full(G_OBJECT(b), "target", new std::string(target),
                               [](gpointer p) { delete static_cast<std::string*>(p); });
        g_object_set_data(G_OBJECT(b), "popover", pop);
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer p) {
                             auto* self = static_cast<Navigation*>(p);
                             auto color = *static_cast<std::string*>(g_object_get_data(G_OBJECT(b), "color"));
                             auto t = *static_cast<std::string*>(g_object_get_data(G_OBJECT(b), "target"));
                             if (t.empty())
                                 self->options.accent = color;
                             else if (color.empty())
                                 self->options.sectionColors.erase(t);
                             else
                                 self->options.sectionColors[t] = color;
                             gtk_widget_hide(GTK_WIDGET(g_object_get_data(G_OBJECT(b), "popover")));
                             self->applyPalette();
                             gtk_widget_queue_draw(self->books);
                             gtk_widget_queue_draw(self->pages);
                             self->saveView();
                         }),
                         this);
        gtk_container_add(GTK_CONTAINER(grid), b);
    }
    gtk_container_add(GTK_CONTAINER(pop), box);
    gtk_widget_show_all(box);
    g_signal_connect(pop, "closed", G_CALLBACK(+[](GtkPopover* p, gpointer) {
                         g_idle_add(
                                 +[](gpointer w) -> gboolean {
                                     gtk_widget_destroy(GTK_WIDGET(w));
                                     return false;
                                 },
                                 p);
                     }),
                     nullptr);
    gtk_popover_popup(GTK_POPOVER(pop));
}
void Navigation::applyPalette() {
    bool dark = control->getWindow() && control->getWindow()->isDarkTheme();
    auto palette = makePalette(dark, options.accent);
    setCurrentPalette(palette);
    if (!paletteProvider) {
        paletteProvider = gtk_css_provider_new();
        // Above the application stylesheet so token overrides win; still below user CSS.
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(paletteProvider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    }
    auto css = palette.css();
    if (options.reducedMotion)
        css += "* { transition: none; animation: none; }\n";
    gtk_css_provider_load_from_data(paletteProvider, css.c_str(), -1, nullptr);
    auto* ctx = gtk_widget_get_style_context(GTK_WIDGET(window));
    gtk_style_context_add_class(ctx, "foliq");
    gtk_stack_set_transition_duration(GTK_STACK(ribbonStack), options.reducedMotion ? 0 : 120);
    updatingChrome = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(motionButton), options.reducedMotion);
    updatingChrome = false;
    if (control->getWindow()) {
        gtk_widget_queue_draw(GTK_WIDGET(window));
        if (auto* xournal = control->getWindow()->getXournal())
            gtk_widget_queue_draw(xournal->getWidget());
    }
}
void Navigation::applyTheme() {
    control->getSettings()->setThemeVariant(static_cast<ThemeVariant>(theme));
    if (control->getWindow())
        control->getWindow()->updateColorscheme();
    updatingChrome = true;
    if (theme >= 0 && theme < static_cast<int>(themeButtons.size()))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(themeButtons[theme]), true);
    updatingChrome = false;
    applyPalette();
}
void Navigation::onPress(double x, double y) {
    // Light dismiss for temporary layers; the press itself continues to the canvas
    // (not claimed), so an ink stroke started here is not lost or delayed.
    auto inside = [&](GtkWidget* w) {
        if (!w || !gtk_widget_get_visible(w))
            return false;
        int wx = 0, wy = 0;
        if (!gtk_widget_translate_coordinates(GTK_WIDGET(window), w, int(x), int(y), &wx, &wy))
            return false;
        return wx >= 0 && wy >= 0 && wx < gtk_widget_get_allocated_width(w) && wy < gtk_widget_get_allocated_height(w);
    };
    if (ribbonTemporary && !inside(ribbonPanel) && !inside(commandBar))
        g_idle_add(
                +[](gpointer p) -> gboolean {
                    static_cast<Navigation*>(p)->hideTemporary();
                    return false;
                },
                this);
    else if (navFloating && gtk_widget_get_visible(panel) && !inside(panel) && !inside(commandBar))
        g_idle_add(
                +[](gpointer p) -> gboolean {
                    static_cast<Navigation*>(p)->setNavVisible(false);
                    return false;
                },
                this);
}
void Navigation::cycleFocus(bool backwards) {
    // F6 cycles regions like Windows apps: navigation → pages → ribbon tabs → editor.
    std::vector<GtkWidget*> regions;
    if (gtk_widget_get_visible(panel)) {
        regions.push_back(books);
        regions.push_back(pages);
    }
    regions.push_back(tabButtons[options.tab]);
    regions.push_back(editorHost);
    auto* focusWidget = gtk_window_get_focus(window);
    int current = -1;
    for (size_t i = 0; i < regions.size(); ++i)
        if (focusWidget && (focusWidget == regions[i] || gtk_widget_is_ancestor(focusWidget, regions[i])))
            current = static_cast<int>(i);
    int n = static_cast<int>(regions.size());
    int next = backwards ? (current <= 0 ? n - 1 : current - 1) : (current + 1) % n;
    auto* target = regions[next];
    if (target == editorHost) {
        gtk_widget_child_focus(editorHost, GTK_DIR_TAB_FORWARD);
    } else {
        gtk_widget_set_can_focus(target, true);
        gtk_widget_grab_focus(target);
    }
}
bool Navigation::onKey(GdkEventKey* e) {
    auto mods = e->state & gtk_accelerator_get_default_mod_mask();
    if (e->keyval == GDK_KEY_Escape && (ribbonTemporary || (navFloating && gtk_widget_get_visible(panel)))) {
        hideTemporary();
        if (navFloating)
            setNavVisible(false);
        return true;
    }
    if (e->keyval == GDK_KEY_Escape && fullscreen && mods == 0 && !control->getTextEditor()) {
        control->actionPerformed(ACTION_FULLSCREEN, GROUP_FULLSCREEN, nullptr, false);
        applyFullscreen(false);
        return true;
    }
    if (e->keyval == GDK_KEY_F1 && mods == GDK_CONTROL_MASK) {
        setRibbonPinned(!options.ribbonPinned);
        return true;
    }
    if (e->keyval == GDK_KEY_F6 && (mods == 0 || mods == GDK_SHIFT_MASK)) {
        cycleFocus(mods == GDK_SHIFT_MASK);
        return true;
    }
    if (e->keyval == GDK_KEY_F10 && mods == 0 && appMenu) {
        gtk_menu_popup_at_widget(GTK_MENU(appMenu), menuButton, GDK_GRAVITY_SOUTH_EAST, GDK_GRAVITY_NORTH_EAST,
                                 reinterpret_cast<GdkEvent*>(e));
        gtk_menu_shell_select_first(GTK_MENU_SHELL(appMenu), false);
        return true;
    }
    if ((e->keyval == GDK_KEY_N || e->keyval == GDK_KEY_n) && mods == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
        setNavVisible(!gtk_widget_get_visible(panel));
        return true;
    }
    if ((e->keyval == GDK_KEY_F || e->keyval == GDK_KEY_f) && mods == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
        setFocusMode(!focus);
        return true;
    }
    return false;
}
void Navigation::updateCrumb() {
    if (!catalog)
        return;
    auto path = control->getDocument()->getFilepath();
    for (const auto& e: catalog->nodes())
        if (e.second.kind == Kind::Page && catalog->document(e.first) == path) {
            std::vector<std::string> parts;
            for (auto n = e.second;; n = catalog->get(n.parent)) {
                parts.insert(parts.begin(), n.name);
                if (n.parent.empty())
                    break;
            }
            std::string text;
            for (size_t i = 0; i < parts.size(); ++i) text += (i ? "  ›  " : "") + parts[i];
            gtk_label_set_text(GTK_LABEL(crumb), text.c_str());
            gtk_widget_set_tooltip_text(crumb, text.c_str());
            return;
        }
    gtk_label_set_text(GTK_LABEL(crumb), path.empty() ? "" : path.filename().u8string().c_str());
}
void Navigation::updateStatus() {
    if (control->getWindow()) {
        for (const auto& entry: mirroredButtons)
            gtk_widget_set_sensitive(entry.first, gtk_widget_get_sensitive(control->getWindow()->get(entry.second)));
        bool visible = control->getSettings()->isToolbarVisible();
        updatingChrome = true;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(legacyToggle), visible);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(drawOptions), visible);
        updatingChrome = false;
    }
    updatingToolButtons = true;
    for (const auto& entry: toolButtons)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entry.first),
                                     control->getToolHandler()->getToolType() == entry.second);
    auto drawing = control->getToolHandler()->getDrawingType();
    for (const auto& entry: shapeButtons)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entry.first), drawing == entry.second);
    updatingToolButtons = false;
    auto* undo = control->getUndoRedoHandler();
    auto* textEditor = control->getTextEditor();
    gtk_widget_set_sensitive(undoButton, textEditor ? textEditor->canUndoEdition() : undo->canUndo());
    gtk_widget_set_sensitive(redoButton, textEditor ? textEditor->canRedoEdition() : undo->canRedo());
    bool changed = undo->isChanged() || (textEditor && textEditor->canUndoEdition());
    // Success stays quiet; a failed save is shown prominently until resolved.
    const char* text = failedSave ? "Speicherfehler · Änderungen nur im Editor" :
                       changed    ? "Änderungen…" :
                       control->getDocument()->getFilepath().empty() ? "Nicht gespeichert" :
                                                                        "Gespeichert";
    gtk_label_set_text(GTK_LABEL(status), text);
    auto* ctx = gtk_widget_get_style_context(status);
    if (failedSave)
        gtk_style_context_add_class(ctx, "foliq-error");
    else
        gtk_style_context_remove_class(ctx, "foliq-error");
    atk_object_set_name(gtk_widget_get_accessible(status), (std::string("Speicherstatus: ") + text).c_str());
    updateCrumb();
}
void Navigation::saveView() {
    if (!catalog || !viewWritable)
        return;
    if (!navFloating && gtk_widget_get_visible(panel)) {
        options.books = gtk_paned_get_position(GTK_PANED(split));
        options.width = gtk_paned_get_position(GTK_PANED(outer));
    }
    options.theme = theme;
    options.focus = focus;
    options.section = section;
    options.expanded.clear();
    for (auto* view: {books, pages})
        gtk_tree_view_map_expanded_rows(
                GTK_TREE_VIEW(view),
                +[](GtkTreeView* v, GtkTreePath* path, gpointer data) {
                    auto* m = gtk_tree_view_get_model(v);
                    GtkTreeIter it;
                    gchar* id;
                    if (gtk_tree_model_get_iter(m, &it, path)) {
                        gtk_tree_model_get(m, &it, 1, &id, -1);
                        static_cast<std::vector<std::string>*>(data)->push_back(id);
                        g_free(id);
                    }
                },
                &options.expanded);
    // Section colours of deleted nodes are dropped; trashed nodes keep theirs.
    for (auto it = options.sectionColors.begin(); it != options.sectionColors.end();)
        it = catalog->nodes().count(it->first) ? std::next(it) : options.sectionColors.erase(it);
    auto bytes = serializeView(options);
    auto path = catalog->directory() / "view.ini";
    if (!fs::exists(path) || read(path) != bytes)
        atomicWrite(path, bytes);
}
void Navigation::loadView() {
    gtk_paned_set_position(GTK_PANED(split), 200);
    gtk_paned_set_position(GTK_PANED(outer), 420);
    auto path = catalog->directory() / "view.ini";
    if (!fs::exists(path)) {
        viewWritable = true;
        return;
    }
    // Unknown/newer or damaged files raise an error and are never overwritten.
    auto v = parseView(read(path));
    viewWritable = true;
    options = v;
    theme = v.theme;
    if (!v.section.empty() && catalog->nodes().count(v.section) && catalog->get(v.section).kind == Kind::Section)
        section = v.section;
    savedExpanded = v.expanded;
    gtk_paned_set_position(GTK_PANED(split), v.books);
    gtk_paned_set_position(GTK_PANED(outer), v.width);
    showTab(v.tab, false);
    setDensity(v.touch);
    updatingChrome = true;
    if (auto* pin = GTK_WIDGET(g_object_get_data(G_OBJECT(panel), "pin")))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pin), v.navPinned);
    updatingChrome = false;
    setRibbonPinned(v.ribbonPinned);
    if (v.focus)
        setFocusMode(true);
    else if (!v.navPinned)
        setNavVisible(false);
    applyPalette();
}
}  // namespace foliq
