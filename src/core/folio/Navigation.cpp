// SPDX-License-Identifier: GPL-2.0-or-later
#include "Navigation.h"

#include <stdexcept>

#include <zlib.h>

#include "control/Control.h"
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
Navigation::Navigation(Control* control, GtkWidget* container): control(control) {
    panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(panel, 250, -1);
    gtk_box_pack_start(GTK_BOX(container), panel, false, false, 0);
    gtk_box_reorder_child(GTK_BOX(container), panel, 0);
    gtk_box_pack_start(GTK_BOX(panel), gtk_label_new("Foliq · Lokale Notizbücher"), false, false, 0);
    auto* commands = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(commands), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(commands), 3);
    gtk_box_pack_start(GTK_BOX(panel), commands, false, false, 0);
    for (const auto* label: {"Notizbuch", "Gruppe", "Abschnitt", "Seite", "Umbenennen", "Verschieben", "↑", "↓",
                             "Duplizieren", "Löschen", "Wiederherstellen", "Öffnen/Schließen", "Papierkorb"}) {
        auto* button = gtk_button_new_with_label(label);
        gtk_container_add(GTK_CONTAINER(commands), button);
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer data) {
                             auto* self = static_cast<Navigation*>(data);
                             try {
                                 self->action(gtk_button_get_label(b));
                             } catch (const std::exception& e) {
                                 self->error(e);
                             }
                         }),
                         this);
    }
    store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Notizbücher / Seiten",
                                                gtk_cell_renderer_text_new(), "text", 0, nullptr);
    gtk_widget_set_tooltip_text(tree, "Seite mit Doppelklick oder Eingabe öffnen. Verschieben: Ziel aus Liste wählen.");
    auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(panel), scroll, true, true, 0);
    g_signal_connect(tree, "row-activated", G_CALLBACK(+[](GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*, gpointer p) {
                         auto* self = static_cast<Navigation*>(p);
                         try {
                             self->activate();
                         } catch (const std::exception& e) {
                             self->error(e);
                         }
                     }),
                     this);
    status = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(status), true);
    gtk_box_pack_start(GTK_BOX(panel), status, false, false, 0);
    gtk_widget_show_all(panel);
    try {
        const auto* overrideRoot = g_getenv("FOLIQ_LIBRARY");
        auto root = overrideRoot ? fs::u8path(overrideRoot) : fs::u8path(g_get_user_data_dir()) / "foliq" / "library";
        catalog = std::make_unique<Catalog>(root);
        gtk_widget_set_tooltip_text(status, root.u8string().c_str());
        refresh();
        timer = g_timeout_add_seconds(
                5, +[](gpointer p) -> gboolean { return static_cast<Navigation*>(p)->tick(); }, this);
        // Restore after Control and initial startup document have been initialized.
        restoreIdle = g_idle_add(
                +[](gpointer p) -> gboolean {
                    auto* self = static_cast<Navigation*>(p);
                    self->restoreIdle = 0;
                    try {
                        auto id = self->catalog->last();
                        if (!id.empty() && !self->catalog->trashed(id) &&
                            self->control->getDocument()->getFilepath().empty() &&
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
        gtk_widget_set_sensitive(commands, false);
    }
}
Navigation::~Navigation() {
    if (restoreIdle)
        g_source_remove(restoreIdle);
    if (timer)
        g_source_remove(timer);
    g_object_unref(store);
}
void Navigation::error(const std::exception& e) {
    gtk_label_set_text(GTK_LABEL(status), (std::string("Speicherfehler: ") + e.what()).c_str());
    XojMsgBox::showErrorToUser(control->getGtkWindow(), e.what());
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
void Navigation::refresh() {
    auto selection = selected();
    gtk_tree_store_clear(store);
    std::function<void(const std::string&, GtkTreeIter*)> append = [&](const std::string& parent, GtkTreeIter* row) {
        for (const auto& n: catalog->children(parent)) {
            bool deleted = catalog->trashed(n.id);
            GtkTreeIter iter;
            if (!trashView && deleted)
                continue;
            // Keep ancestors visible in trash view, with explicit deleted markers.
            auto name = n.name + (deleted ? " [Papierkorb]" : "") +
                        (n.kind == Kind::Notebook && !n.opened ? " [geschlossen]" : "");
            gtk_tree_store_append(store, &iter, row);
            gtk_tree_store_set(store, &iter, 0, name.c_str(), 1, n.id.c_str(), -1);
            if (n.id == selection)
                gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)), &iter);
            if (trashView || n.kind != Kind::Notebook || n.opened)
                append(n.id, &iter);
        }
    };
    append("", nullptr);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));
    gtk_label_set_text(GTK_LABEL(status), trashView ? "Papierkorbansicht (inklusive Elternpfade)" :
                                                      "Lokal gespeichert · Autosave alle 5 s");
}
std::string Navigation::prompt(const char* title, const std::string& value) {
    auto* dlg = gtk_dialog_new_with_buttons(title, control->getGtkWindow(), GTK_DIALOG_MODAL, "Abbrechen",
                                            GTK_RESPONSE_CANCEL, "OK", GTK_RESPONSE_OK, nullptr);
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
        catalog->open(id, !n.opened);
        refresh();
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
        catalog->add(kind, kind == Kind::Notebook ? "" : id, name, kind == Kind::Page ? blankDocument() : "");
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
            auto* dlg =
                    gtk_dialog_new_with_buttons("Ziel wählen", control->getGtkWindow(), GTK_DIALOG_MODAL, "Abbrechen",
                                                GTK_RESPONSE_CANCEL, "Verschieben", GTK_RESPONSE_OK, nullptr);
            auto* combo = gtk_combo_box_text_new();
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "", "Oberste Ebene (Notizbuch)");
            for (const auto& e: catalog->nodes())
                if (!catalog->trashed(e.first)) {
                    std::string label = e.second.name + " · " + e.first;
                    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), e.first.c_str(), label.c_str());
                }
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
            gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), combo);
            gtk_widget_show_all(dlg);
            std::string target;
            bool ok = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK;
            if (ok)
                target = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
            gtk_widget_destroy(dlg);
            if (ok)
                catalog->move(id, target, static_cast<int>(catalog->children(target).size()));
        } else if (action == "Duplizieren") {
            if (!flush())
                return;
            catalog->duplicate(id);
        } else if (action == "Löschen" || action == "Öffnen/Schließen") {
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
            else
                catalog->open(id, !catalog->get(id).opened);
        } else if (action == "Wiederherstellen")
            catalog->trash(id, false);
    }
    refresh();
}
}  // namespace foliq
