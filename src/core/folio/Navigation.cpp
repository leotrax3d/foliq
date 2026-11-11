// SPDX-License-Identifier: GPL-2.0-or-later
#include "Navigation.h"

#include <stdexcept>
#include <algorithm>
#include "enums/ActionType.enum.h"
#include "enums/ActionGroup.enum.h"

#include <zlib.h>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/settings/Settings.h"
#include "gui/MainWindow.h"
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
    gtk_box_pack_start(GTK_BOX(container), outer, true, true, 0);
    gtk_paned_pack1(GTK_PANED(outer), split, false, false);
    editorHost = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
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
        auto* column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_size_request(column, 120, -1);
        gtk_container_set_border_width(GTK_CONTAINER(column), 8);
        auto* heading = gtk_label_new(view == books ? "NOTIZBÜCHER" : "SEITEN");
        gtk_label_set_xalign(GTK_LABEL(heading), 0);
        gtk_box_pack_start(GTK_BOX(column), heading, false, false, 4);
        auto* add = gtk_button_new_with_label(view == books ? "+ Neu" : "+ Seite");
        atk_object_set_name(gtk_widget_get_accessible(add), view == books ? "Notizbuchaktionen" : "Seitenaktionen");
        gtk_widget_set_tooltip_text(add, "Anlegen, Umbenennen, Verschieben und weitere Aktionen");
        g_object_set_data(G_OBJECT(add), "view", view);
        g_signal_connect(add, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer p) {
            auto* self = static_cast<Navigation*>(p);
            self->selectTree(GTK_WIDGET(g_object_get_data(G_OBJECT(b), "view")));
            auto* menu = gtk_menu_new();
            for (const auto* label: {"Notizbuch", "Gruppe", "Abschnitt", "Seite", "Umbenennen", "Verschieben", "↑", "↓",
                                    "Duplizieren", "Löschen", "Wiederherstellen", "Öffnen/Schließen", "Papierkorb"}) {
                auto* item = gtk_menu_item_new_with_label(label);
                if (std::string(label) == "↑" || std::string(label) == "↓")
                    atk_object_set_name(gtk_widget_get_accessible(item), std::string(label) == "↑" ? "Nach oben verschieben" : "Nach unten verschieben");
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                g_signal_connect(item, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer p) {
                    auto* self = static_cast<Navigation*>(p);
                    try { self->action(gtk_menu_item_get_label(item)); } catch (const std::exception& e) { self->error(e); }
                }), self);
            }
            gtk_widget_show_all(menu);
            g_signal_connect_swapped(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), menu);
#if GTK_CHECK_VERSION(3, 22, 0)
            gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(b), GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, nullptr);
#else
            gtk_menu_popup(GTK_MENU(menu), nullptr, nullptr, nullptr, nullptr, 0, gtk_get_current_event_time());
#endif
        }), this);
        gtk_box_pack_start(GTK_BOX(column), add, false, false, 0);
        auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scroll), view);
        gtk_box_pack_start(GTK_BOX(column), scroll, true, true, 0);
        if (view == books) gtk_paned_pack1(GTK_PANED(split), column, true, false);
        else gtk_paned_pack2(GTK_PANED(split), column, true, false);
        setupTree(view);
    }
    chrome(container);
    statusTimer = g_timeout_add(250, +[](gpointer p) -> gboolean {
        static_cast<Navigation*>(p)->updateStatus(); return true;
    }, this);
    gtk_widget_show_all(outer);
    gtk_widget_show_all(ribbon);
    try {
        const auto* overrideRoot = g_getenv("FOLIQ_LIBRARY");
        auto root = overrideRoot ? fs::u8path(overrideRoot) : fs::u8path(g_get_user_data_dir()) / "foliq" / "library";
        catalog = std::make_unique<Catalog>(root);
        gtk_widget_set_tooltip_text(status, root.u8string().c_str());
        try { loadView(); } catch (const std::exception& e) { error(e); }
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
                        auto id = self->catalog->last();
                        if (!id.empty() && !self->catalog->trashed(id) &&
                            self->control->getDocument()->getFilepath().empty() &&
                            self->control->getDocument()->getPdfPageCount() == 0 &&
                            !self->control->getUndoRedoHandler()->isChanged()) {
                            auto n = self->catalog->get(id);
                            while (!n.parent.empty()) n = self->catalog->get(n.parent);
                            if (n.opened)
                                self->control->openFile(self->catalog->document(id), self->catalog->lastPage());
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
    if (statusTimer) g_source_remove(statusTimer);
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
    if (refreshing || !catalog) return;
    refreshing = true;
    auto* active = tree;
    for (auto* view: {books, pages}) {
        if (pagesOnly && view == books) continue;
        selectTree(view);
        auto selection = pendingSelection.empty() ? selected() : pendingSelection;
        std::vector<std::string> expanded = savedExpanded;
        gtk_tree_view_map_expanded_rows(GTK_TREE_VIEW(view), +[](GtkTreeView* v, GtkTreePath* path, gpointer p) {
            GtkTreeIter it; gchar* id = nullptr;
            auto* m = gtk_tree_view_get_model(v);
            if (gtk_tree_model_get_iter(m, &it, path)) {
                gtk_tree_model_get(m, &it, 1, &id, -1);
                static_cast<std::vector<std::string>*>(p)->push_back(id); g_free(id);
            }
        }, &expanded);
        gtk_tree_store_clear(store);
        std::function<void(const std::string&, GtkTreeIter*)> append = [&](const std::string& parent, GtkTreeIter* row) {
            for (const auto& n: catalog->children(parent)) {
                if ((!trashView && catalog->trashed(n.id)) || (view == books && n.kind == Kind::Page)) continue;
                auto name = n.name + (catalog->trashed(n.id) ? " [Papierkorb]" : "") +
                            (n.kind == Kind::Notebook && !n.opened ? " [geschlossen]" : "");
                GtkTreeIter it; gtk_tree_store_append(store, &it, row);
                gtk_tree_store_set(store, &it, 0, name.c_str(), 1, n.id.c_str(), -1);
                if (trashView || n.kind != Kind::Notebook || n.opened) append(n.id, &it);
                auto* path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &it);
                if (std::find(expanded.begin(), expanded.end(), n.id) != expanded.end())
                    gtk_tree_view_expand_row(GTK_TREE_VIEW(view), path, false);
                if (n.id == selection) {
                    if (!pendingSelection.empty()) gtk_tree_view_expand_to_path(GTK_TREE_VIEW(view), path);
                    gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), &it);
                }
                gtk_tree_path_free(path);
            }
        };
        if (view == books || !section.empty()) append(view == books ? "" : section, nullptr);
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
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), entry);
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
        if (control->openFile(catalog->document(id)))
            catalog->position(id, 0);
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
        if (kind == Kind::Page && id.empty()) id = section;
        auto created = catalog->add(kind, kind == Kind::Notebook ? "" : id, name, kind == Kind::Page ? blankDocument() : "");
        if (kind == Kind::Section) section = created;
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
                    kind != Kind::Notebook && (targetKind == Kind::Notebook || targetKind == Kind::Group);
                bool descendant = false; auto ancestor = e.second;
                while (true) {
                    if (ancestor.id == id) { descendant = true; break; }
                    if (ancestor.parent.empty()) break;
                    ancestor = catalog->get(ancestor.parent);
                }
                if (compatible && !descendant && !catalog->trashed(e.first)) {
                    std::string label = e.second.name;
                    auto ancestor = e.second;
                    while (!ancestor.parent.empty()) { ancestor = catalog->get(ancestor.parent); label = ancestor.name + " / " + label; }
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
                if (value) target = value; else ok = false;
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
    g_object_set(cell, "ellipsize", PANGO_ELLIPSIZE_END, "ypad", 9, nullptr);
    auto* col = gtk_tree_view_column_new_with_attributes("Titel", cell, "text", 0, nullptr);
    gtk_tree_view_column_set_expand(col, true);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    atk_object_set_name(gtk_widget_get_accessible(view), view == books ? "Notizbücher und Abschnitte" : "Seiten und Unterseiten");
    gtk_widget_set_tooltip_text(view, "Pfeile: navigieren/einklappen · Eingabe: öffnen · F2: umbenennen · Alt+↑/↓: sortieren. Aktionen im Menü oberhalb.");
    g_signal_connect(view, "focus-in-event", G_CALLBACK(+[](GtkWidget* v, GdkEventFocus*, gpointer p) -> gboolean {
        static_cast<Navigation*>(p)->selectTree(v); return false;
    }), this);
    g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed", G_CALLBACK(+[](GtkTreeSelection* sel, gpointer p) {
        auto* self = static_cast<Navigation*>(p);
        if (self->refreshing || !self->catalog) return;
        self->selectTree(GTK_WIDGET(gtk_tree_selection_get_tree_view(sel)));
        auto id = self->selected();
        if (id.empty()) return;
        if (self->tree == self->books) {
            const auto& n = self->catalog->get(id);
            self->section = n.kind == Kind::Section ? id : "";
            self->refresh(true);
        }
    }), this);
    g_signal_connect(view, "row-activated", G_CALLBACK(+[](GtkTreeView* v, GtkTreePath*, GtkTreeViewColumn*, gpointer p) {
        auto* self = static_cast<Navigation*>(p); self->selectTree(GTK_WIDGET(v));
        try { self->activate(); } catch (const std::exception& e) { self->error(e); }
    }), this);
    g_signal_connect(view, "key-press-event", G_CALLBACK(+[](GtkWidget* v, GdkEventKey* e, gpointer p) -> gboolean {
        auto* self = static_cast<Navigation*>(p); self->selectTree(v);
        const char* action = e->keyval == GDK_KEY_F2 ? "Umbenennen" :
            (e->state & GDK_MOD1_MASK) && e->keyval == GDK_KEY_Up ? "↑" :
            (e->state & GDK_MOD1_MASK) && e->keyval == GDK_KEY_Down ? "↓" : nullptr;
        if (!action) return false;
        try { self->action(action); } catch (const std::exception& ex) { self->error(ex); }
        return true;
    }), this);
    static const GtkTargetEntry target = {const_cast<gchar*>("application/x-foliq-node"), GTK_TARGET_SAME_APP, 0};
    gtk_drag_source_set(view, GDK_BUTTON1_MASK, &target, 1, GDK_ACTION_MOVE);
    gtk_drag_dest_set(view, GTK_DEST_DEFAULT_ALL, &target, 1, GDK_ACTION_MOVE);
    g_signal_connect(view, "drag-data-get", G_CALLBACK(+[](GtkWidget* v, GdkDragContext*, GtkSelectionData* data, guint, guint, gpointer p) {
        auto* self = static_cast<Navigation*>(p); self->selectTree(v); self->dragId = self->selected();
        gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8,
            reinterpret_cast<const guchar*>(self->dragId.data()), static_cast<int>(self->dragId.size()));
    }), this);
    g_signal_connect(view, "drag-data-received", G_CALLBACK(+[](GtkWidget* v, GdkDragContext* ctx, gint x, gint y, GtkSelectionData* data, guint, guint time, gpointer p) {
        auto* self = static_cast<Navigation*>(p); bool success = false;
        try {
            if (!self->catalog || gtk_selection_data_get_length(data) <= 0) throw std::runtime_error("Kein Verschiebeziel.");
            std::string id(reinterpret_cast<const char*>(gtk_selection_data_get_data(data)), gtk_selection_data_get_length(data));
            GtkTreePath* path = nullptr; GtkTreeViewDropPosition pos;
            if (!gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(v), x, y, &path, &pos)) throw std::runtime_error("Auf einen Zieleintrag ziehen.");
            GtkTreeIter it; auto* model = gtk_tree_view_get_model(GTK_TREE_VIEW(v));
            gtk_tree_model_get_iter(model, &it, path); gtk_tree_path_free(path);
            gchar* value; gtk_tree_model_get(model, &it, 1, &value, -1); std::string targetId(value); g_free(value);
            auto targetNode = self->catalog->get(targetId);
            std::string parent = targetId; int index = static_cast<int>(self->catalog->children(parent).size());
            if (pos == GTK_TREE_VIEW_DROP_BEFORE || pos == GTK_TREE_VIEW_DROP_AFTER) {
                parent = targetNode.parent;
                auto siblings = self->catalog->children(parent);
                auto target = std::find_if(siblings.begin(), siblings.end(), [&](const Node& n) { return n.id == targetId; });
                index = static_cast<int>(std::distance(siblings.begin(), target)) + (pos == GTK_TREE_VIEW_DROP_AFTER ? 1 : 0);
                auto source = std::find_if(siblings.begin(), siblings.end(), [&](const Node& n) { return n.id == id; });
                if (source != siblings.end() && std::distance(siblings.begin(), source) < index) --index;
            }
            self->catalog->move(id, parent, index);
            if (self->catalog->get(id).kind == Kind::Page) {
                auto ancestor = self->catalog->get(id);
                while (ancestor.kind == Kind::Page) ancestor = self->catalog->get(ancestor.parent);
                self->section = ancestor.id; self->selectTree(self->pages);
            } else self->selectTree(self->books);
            self->pendingSelection = id; self->refresh(); success = true;
        } catch (const std::exception& e) { self->error(e); }
        gtk_drag_finish(ctx, success, false, time);
    }), this);
}
void Navigation::chrome(GtkWidget* container) {
    auto* mainBox = gtk_widget_get_parent(gtk_widget_get_parent(container));
    ribbon = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(ribbon, "foliq-ribbon");
    gtk_container_set_border_width(GTK_CONTAINER(ribbon), 6);
    gtk_box_pack_start(GTK_BOX(mainBox), ribbon, false, false, 0);
    gtk_box_reorder_child(GTK_BOX(mainBox), ribbon, 1);
    auto* quick = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(ribbon), quick, false, false, 0);
    auto* brand = gtk_label_new("Foliq"); gtk_box_pack_start(GTK_BOX(quick), brand, false, false, 12);
    auto button = [&](GtkWidget* box, const char* label, const char* icon, ActionType action) {
        bool isTool = action >= ACTION_TOOL_PEN && action <= ACTION_TOOL_HAND;
        auto* b = isTool ? gtk_toggle_button_new_with_label(label) : gtk_button_new_with_label(label);
        if (isTool) toolButtons.emplace_back(b, action - ACTION_TOOL_PEN + TOOL_PEN);
        if (action == ACTION_CUT || action == ACTION_COPY || action == ACTION_PASTE)
            mirroredButtons.emplace_back(b, action == ACTION_CUT ? "menuEditCut" : action == ACTION_COPY ? "menuEditCopy" : "menuEditPaste");
        if (icon) { gtk_button_set_image(GTK_BUTTON(b), gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON)); gtk_button_set_always_show_image(GTK_BUTTON(b), true); }
        if (action == ACTION_ZOOM_IN || action == ACTION_ZOOM_OUT)
            atk_object_set_name(gtk_widget_get_accessible(b), action == ACTION_ZOOM_IN ? "Vergrößern" : "Verkleinern");
        g_object_set_data(G_OBJECT(b), "action", GINT_TO_POINTER(action));
        gtk_box_pack_start(GTK_BOX(box), b, false, false, 0);
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer p) {
            auto a = static_cast<ActionType>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "action")));
            auto* self = static_cast<Navigation*>(p);
            if (self->updatingToolButtons)
                return;
            self->control->actionPerformed(a, a >= ACTION_TOOL_PEN && a <= ACTION_TOOL_HAND ? GROUP_TOOL : GROUP_NOGROUP, nullptr, true);
            self->updateStatus();
        }), this); return b;
    };
    undoButton = button(quick, "Rückgängig", "xopp-edit-undo", ACTION_UNDO);
    redoButton = button(quick, "Wiederholen", "xopp-edit-redo", ACTION_REDO);
    auto* findButton = button(quick, "Suchen", nullptr, ACTION_SEARCH);
    gtk_widget_set_tooltip_text(findButton, "Im aktuellen Dokument suchen (Strg+F)");
    button(quick, "Speichern", "xopp-document-save", ACTION_SAVE);
    status = gtk_label_new("Lokal"); gtk_label_set_ellipsize(GTK_LABEL(status), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(quick), status, true, true, 8);
    auto* focusButton = gtk_toggle_button_new_with_label("Fokus");
    gtk_widget_set_tooltip_text(focusButton, "Notizbuch- und Seitennavigation ausblenden");
    gtk_box_pack_end(GTK_BOX(quick), focusButton, false, false, 0);
    g_signal_connect(focusButton, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
        auto* self = static_cast<Navigation*>(p); self->focus = gtk_toggle_button_get_active(b);
        gtk_widget_set_no_show_all(self->panel, self->focus);
        gtk_widget_set_visible(self->panel, !self->focus);
    }), this);
    g_object_set_data(G_OBJECT(ribbon), "focus", focusButton);
    auto* tabs = gtk_notebook_new();
    g_object_set_data(G_OBJECT(ribbon), "tabs", tabs); gtk_notebook_set_scrollable(GTK_NOTEBOOK(tabs), true);
    gtk_box_pack_start(GTK_BOX(ribbon), tabs, false, false, 0);
    auto page = [&](const char* name) {
        auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        auto* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); gtk_container_set_border_width(GTK_CONTAINER(box), 6);
        gtk_container_add(GTK_CONTAINER(scroll), box);
        gtk_notebook_append_page(GTK_NOTEBOOK(tabs), scroll, gtk_label_new(name)); return box;
    };
    auto* home = page("Start");
    button(home, "Text", "xopp-tool-text", ACTION_TOOL_TEXT);
    button(home, "Auswahl", "xopp-select-rect", ACTION_TOOL_SELECT_RECT);
    button(home, "Ausschneiden", "xopp-edit-cut", ACTION_CUT);
    button(home, "Kopieren", "xopp-edit-copy", ACTION_COPY);
    button(home, "Einfügen", "xopp-edit-paste", ACTION_PASTE);
    auto* insert = page("Einfügen");
    button(insert, "Bild", "xopp-tool-image", ACTION_TOOL_IMAGE);
    button(insert, "PDF annotieren", "xopp-document-open", ACTION_ANNOTATE_PDF);
    button(insert, "Dokumentseite", "xopp-page-add", ACTION_NEW_PAGE_AFTER);
    auto* surface = button(insert, "Seitentyp / Papier", nullptr, ACTION_PAPER_FORMAT);
    gtk_widget_set_tooltip_text(surface, "Papierformat, freie Arbeitsfläche, Lineatur, Farbe und PDF-Ausschnitt");
    auto* draw = page("Zeichnen");
    button(draw, "Stift", "xopp-tool-pencil", ACTION_TOOL_PEN);
    button(draw, "Radierer", "xopp-tool-eraser", ACTION_TOOL_ERASER);
    button(draw, "Textmarker", "xopp-tool-highlighter", ACTION_TOOL_HIGHLIGHTER);
    button(draw, "Lasso", "xopp-select-lasso", ACTION_TOOL_SELECT_REGION);
    button(draw, "Hand", "xopp-hand", ACTION_TOOL_HAND);
    drawOptions = gtk_check_button_new_with_label("Weitere Werkzeuge");
    gtk_box_pack_start(GTK_BOX(draw), drawOptions, false, false, 0);
    auto* view = page("Ansicht");
    button(view, "Einpassen", nullptr, ACTION_ZOOM_FIT);
    button(view, "100 %", nullptr, ACTION_ZOOM_100);
    button(view, "−", nullptr, ACTION_ZOOM_OUT); button(view, "+", nullptr, ACTION_ZOOM_IN);
    legacyToggle = gtk_check_button_new_with_label("Alle Werkzeuge");
    gtk_widget_set_tooltip_text(legacyToggle, "Bestehende konfigurierbare Werkzeugleisten einblenden");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(legacyToggle), control->getSettings()->isToolbarVisible());
    gtk_box_pack_start(GTK_BOX(view), legacyToggle, false, false, 0);
    for (auto* toggle: {legacyToggle, drawOptions}) g_signal_connect(toggle, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
        auto* self = static_cast<Navigation*>(p);
        if (self->control->getWindow()) self->control->getWindow()->setToolbarVisible(gtk_toggle_button_get_active(b));
    }), this);
    auto* themes = gtk_combo_box_text_new();
    for (const auto* name: {"System", "Hell", "Dunkel"}) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(themes), name);
    theme = static_cast<int>(control->getSettings()->getThemeVariant());
    gtk_combo_box_set_active(GTK_COMBO_BOX(themes), theme);
    atk_object_set_name(gtk_widget_get_accessible(themes), "Farbschema");
    gtk_box_pack_start(GTK_BOX(view), themes, false, false, 0);
    g_object_set_data(G_OBJECT(ribbon), "theme", themes);
    g_signal_connect(themes, "changed", G_CALLBACK(+[](GtkComboBox* b, gpointer p) {
        auto* self = static_cast<Navigation*>(p); self->theme = gtk_combo_box_get_active(b); self->applyTheme();
    }), this);

}
void Navigation::applyTheme() {
    control->getSettings()->setThemeVariant(static_cast<ThemeVariant>(theme));
    if (control->getWindow()) control->getWindow()->updateColorscheme();
}
void Navigation::updateStatus() {
    if (control->getWindow()) {
        for (const auto& entry: mirroredButtons)
            gtk_widget_set_sensitive(entry.first, gtk_widget_get_sensitive(control->getWindow()->get(entry.second)));
        bool visible = control->getSettings()->isToolbarVisible();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(legacyToggle), visible);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(drawOptions), visible);
    }
    updatingToolButtons = true;
    for (const auto& entry: toolButtons)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entry.first), control->getToolHandler()->getToolType() == entry.second);
    updatingToolButtons = false;
    auto* undo = control->getUndoRedoHandler();
    gtk_widget_set_sensitive(undoButton, undo->canUndo()); gtk_widget_set_sensitive(redoButton, undo->canRedo());
    gtk_label_set_text(GTK_LABEL(status), failedSave ? "Speicherfehler · Änderungen im Editor" :
        undo->isChanged() ? "Ungespeicherte Änderungen" :
        control->getDocument()->getFilepath().empty() ? "Neue Notiz · noch nicht gespeichert" : "Lokal · gespeichert");
}
void Navigation::saveView() {
    if (!catalog || !viewWritable) return;
    std::string bytes = "[view]\nversion=1\nbooks=" + std::to_string(gtk_paned_get_position(GTK_PANED(split))) +
        "\nwidth=" + std::to_string(gtk_paned_get_position(GTK_PANED(outer))) + "\ntheme=" + std::to_string(theme) +
        "\nfocus=" + std::to_string(focus) + "\n";
    bytes += "section=" + section + "\ntab=" + std::to_string(gtk_notebook_get_current_page(GTK_NOTEBOOK(g_object_get_data(G_OBJECT(ribbon), "tabs")))) + "\nexpanded=";
    std::string expanded;
    for (auto* view: {books, pages}) gtk_tree_view_map_expanded_rows(GTK_TREE_VIEW(view), +[](GtkTreeView* v, GtkTreePath* path, gpointer data) {
        auto* m = gtk_tree_view_get_model(v); GtkTreeIter it; gchar* id;
        if (gtk_tree_model_get_iter(m, &it, path)) { gtk_tree_model_get(m, &it, 1, &id, -1);
            *static_cast<std::string*>(data) += std::string(id) + ";"; g_free(id); }
    }, &expanded);
    bytes += expanded + "\n";
    auto path = catalog->directory() / "view.ini";
    if (!fs::exists(path) || read(path) != bytes) atomicWrite(path, bytes);
}
void Navigation::loadView() {
    gtk_paned_set_position(GTK_PANED(split), 200); gtk_paned_set_position(GTK_PANED(outer), 420);
    auto path = catalog->directory() / "view.ini";
    if (!fs::exists(path)) { viewWritable = true; return; }
    std::unique_ptr<GKeyFile, decltype(&g_key_file_unref)> owner(g_key_file_new(), g_key_file_unref);
    auto* key = owner.get(); auto bytes = read(path);
    auto integer = [&](const char* name) {
        GError* error = nullptr; auto value = g_key_file_get_integer(key, "view", name, &error);
        if (error) { g_error_free(error); throw std::runtime_error("Fehlende/ungültige Ansichtsoption in view.ini."); }
        return value;
    };
    if (!g_key_file_load_from_data(key, bytes.data(), bytes.size(), G_KEY_FILE_NONE, nullptr) ||
        integer("version") != 1) {
        throw std::runtime_error("Unbekannte oder beschädigte Ansichtsoptionen (view.ini).");
    }
    int left = integer("books");
    int width = integer("width");
    theme = integer("theme");
    int focusValue = integer("focus");
    focus = focusValue != 0;
    auto* selectedSection = g_key_file_get_string(key, "view", "section", nullptr);
    if (selectedSection && catalog->nodes().count(selectedSection) && catalog->get(selectedSection).kind == Kind::Section)
        section = selectedSection;
    g_free(selectedSection);
    gsize count = 0; auto** expanded = g_key_file_get_string_list(key, "view", "expanded", &count, nullptr);
    for (gsize i = 0; i < count; ++i) savedExpanded.push_back(expanded[i]);
    g_strfreev(expanded);
    int tab = integer("tab");
    gtk_notebook_set_current_page(GTK_NOTEBOOK(g_object_get_data(G_OBJECT(ribbon), "tabs")), std::max(0, std::min(3, tab)));
    if (focusValue < 0 || focusValue > 1 || tab < 0 || tab > 3 || left < 120 || width < 240 || width > 16384 || left >= width || theme < 0 || theme > 2)
        throw std::runtime_error("Ungültige Ansichtsoptionen (view.ini).");
    viewWritable = true;
    gtk_paned_set_position(GTK_PANED(split), left); gtk_paned_set_position(GTK_PANED(outer), width);
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_object_get_data(G_OBJECT(ribbon), "theme")), theme);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(ribbon), "focus")), focus);
}
}  // namespace foliq
