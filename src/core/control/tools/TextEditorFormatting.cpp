// Foliq — GPL-2.0-or-later
#include <algorithm>
#include <functional>
#include <numeric>

#include "control/Control.h"
#include "control/ScrollHandler.h"
#include "model/Document.h"
#include "model/Text.h"
#include "util/XojMsgBox.h"

#include "TextEditor.h"

namespace {
void navigateToPage(Control* control, size_t page) {
    auto* payload = new std::pair<Control*, size_t>(control, page);
    g_idle_add_full(
            G_PRIORITY_DEFAULT_IDLE,
            +[](gpointer p) -> gboolean {
                auto* v = static_cast<std::pair<Control*, size_t>*>(p);
                v->first->getScrollHandler()->scrollToPage(v->second);
                return G_SOURCE_REMOVE;
            },
            payload, +[](gpointer p) { delete static_cast<std::pair<Control*, size_t>*>(p); });
}
void bounds(GtkTextBuffer* buffer, GtkTextIter& a, GtkTextIter& b) {
    if (!gtk_text_buffer_get_selection_bounds(buffer, &a, &b)) {
        gtk_text_buffer_get_iter_at_mark(buffer, &a, gtk_text_buffer_get_insert(buffer));
        b = a;
        gtk_text_iter_set_line_offset(&a, 0);
        gtk_text_iter_forward_to_line_end(&b);
    }
}
void button(GtkWidget* box, const char* label, std::function<void()> fn) {
    auto* b = gtk_button_new_with_label(label);
    gtk_widget_set_can_focus(b, false);  // preserve the editing selection
    gtk_box_pack_start(GTK_BOX(box), b, false, false, 0);
    auto* callback = new std::function<void()>(std::move(fn));
    g_signal_connect_data(
            b, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) { (*static_cast<std::function<void()>*>(p))(); }),
            callback, +[](gpointer p, GClosure*) { delete static_cast<std::function<void()>*>(p); }, G_CONNECT_DEFAULT);
}
}  // namespace

void TextEditor::recordHistory() {
    auto rich = RichText::fromBuffer(buffer.get(), tableColumns);
    if (!rich.valid()) {  // Never leave a malformed table in a live document.
        if (!history.empty())
            restoreHistory();
        XojMsgBox::showErrorToUser(control->getGtkWindow(),
                                   "Änderung nicht übernommen: Text zu groß oder ungültige Tabellenstruktur. Tabellen "
                                   "bitte über den Zelleneditor bearbeiten.");
        return;
    }
    textElement->setRichText(rich);
    auto same = [&](const EditionState& s) {
        return s.rich.encode() == rich.encode() && s.font == textElement->getFontName() &&
               s.size == textElement->getFontSize() && s.color == textElement->getColor();
    };
    if (!history.empty() && same(history[historyIndex]))
        return;
    if (!history.empty())
        history.resize(historyIndex + 1);
    GtkTextIter cursor, anchor;
    gtk_text_buffer_get_iter_at_mark(buffer.get(), &cursor, gtk_text_buffer_get_insert(buffer.get()));
    gtk_text_buffer_get_iter_at_mark(buffer.get(), &anchor, gtk_text_buffer_get_selection_bound(buffer.get()));
    history.push_back({std::move(rich), textElement->getFontName(), textElement->getFontSize(), textElement->getColor(),
                       gtk_text_iter_get_offset(&cursor), gtk_text_iter_get_offset(&anchor)});
    size_t bytes = 0;
    for (const auto& s: history) bytes += s.rich.text.size() + s.rich.spans.size() * sizeof(RichText::Span);
    while (history.size() > 1 && (history.size() > 100 || bytes > 32 * 1024 * 1024)) {
        bytes -= history.front().rich.text.size() + history.front().rich.spans.size() * sizeof(RichText::Span);
        history.erase(history.begin());
    }
    historyIndex = history.size() - 1;
    control->fireEnableAction(ACTION_UNDO, canUndoEdition());
    control->fireEnableAction(ACTION_REDO, canRedoEdition());
}

void TextEditor::restoreHistory() {
    restoringHistory = true;
    const auto& state = history[historyIndex];
    state.rich.toBuffer(buffer.get());
    tableColumns = state.rich.columns;
    textElement->setRichText(state.rich);
    textElement->setFont(XojFont(state.font, state.size));
    textElement->setColor(state.color);
    textElement->updatePangoFont(layout.get());
    GtkTextIter a, b;
    gtk_text_buffer_get_iter_at_offset(buffer.get(), &a, state.cursor);
    gtk_text_buffer_get_iter_at_offset(buffer.get(), &b, state.anchor);
    gtk_text_buffer_select_range(buffer.get(), &a, &b);
    layoutStatus = LayoutStatus::NEEDS_COMPLETE_UPDATE;
    restoringHistory = false;
    repaintEditor();
}
bool TextEditor::canUndoEdition() const { return historyIndex > 0; }
bool TextEditor::canRedoEdition() const { return historyIndex + 1 < history.size(); }
bool TextEditor::undoEdition(bool redo) {
    resetImContext();
    if (redo ? canRedoEdition() : canUndoEdition()) {
        if (redo)
            ++historyIndex;
        else
            --historyIndex;
        restoreHistory();
    }
    control->fireEnableAction(ACTION_UNDO, canUndoEdition());
    control->fireEnableAction(ACTION_REDO, canRedoEdition());
    return true;
}

void TextEditor::applyStyles(std::initializer_list<std::pair<std::string, std::string>> styles) {
    bool wasRestoring = restoringHistory;
    restoringHistory = true;
    for (const auto& style: styles) applyStyle(style.first, style.second);
    restoringHistory = wasRestoring;
    contentsChanged(true);
    repaintEditor();
}

void TextEditor::applyStyle(const std::string& kind, const std::string& value, bool toggle) {
    GtkTextIter a, b;
    bounds(buffer.get(), a, b);
    auto* tag = RichText::tag(buffer.get(), kind, value);
    if (!tag)
        return;
    if (gtk_text_iter_equal(&a, &b)) {
        if (toggle && pendingStyles.count(kind))
            pendingStyles.erase(kind);
        else
            pendingStyles[kind] = value;
        return;
    }
    bool remove = toggle && gtk_text_iter_has_tag(&a, tag);
    // One value per property in a range. Unrelated properties survive.
    auto rich = RichText::fromBuffer(buffer.get());
    for (const auto& span: rich.spans)
        if (span.kind == kind) {
            auto* old = RichText::tag(buffer.get(), span.kind, span.value);
            gtk_text_buffer_remove_tag(buffer.get(), old, &a, &b);
        }
    if (!remove)
        gtk_text_buffer_apply_tag(buffer.get(), tag, &a, &b);
    contentsChanged(true);
    repaintEditor();
}

void TextEditor::paragraph(const std::string& prefix) {
    if (tableColumns)
        return;
    GtkTextIter a, b;
    bounds(buffer.get(), a, b);
    int first = gtk_text_iter_get_line(&a);
    int last = gtk_text_iter_get_line(&b);
    if (last > first && gtk_text_iter_starts_line(&b))
        --last;
    gtk_text_buffer_begin_user_action(buffer.get());
    for (int line = last; line >= first; --line) {
        gtk_text_buffer_get_iter_at_line(buffer.get(), &a, line);
        b = a;
        gtk_text_iter_forward_to_line_end(&b);
        char* raw = gtk_text_buffer_get_text(buffer.get(), &a, &b, true);
        std::string text(raw);
        g_free(raw);
        if (prefix == "outdent") {
            if (text.compare(0, 4, "    ") == 0) {
                b = a;
                gtk_text_iter_forward_chars(&b, 4);
                gtk_text_buffer_delete(buffer.get(), &a, &b);
            }
        } else {
            std::string insert = prefix == "number" ? std::to_string(line - first + 1) + ". " : prefix;
            if (prefix == "☐ " && text.compare(0, std::string("☐ ").size(), "☐ ") == 0) {
                b = a;
                gtk_text_iter_forward_chars(&b, 2);
                gtk_text_buffer_delete(buffer.get(), &a, &b);
                insert = "☑ ";
            } else if (prefix == "☐ " && text.compare(0, std::string("☑ ").size(), "☑ ") == 0) {
                b = a;
                gtk_text_iter_forward_chars(&b, 2);
                gtk_text_buffer_delete(buffer.get(), &a, &b);
            }
            gtk_text_buffer_insert(buffer.get(), &a, insert.c_str(), -1);
        }
    }
    gtk_text_buffer_end_user_action(buffer.get());
    contentsChanged(true);
    repaintEditor();
}

void TextEditor::editLink() {
    auto* dlg = gtk_dialog_new_with_buttons("Link oder Seitenverweis", control->getGtkWindow(), GTK_DIALOG_MODAL,
                                            "Abbrechen", GTK_RESPONSE_CANCEL, "Entfernen", GTK_RESPONSE_REJECT,
                                            "Übernehmen", GTK_RESPONSE_OK, nullptr);
    auto* entry = gtk_entry_new();
    GtkTextIter current;
    gtk_text_buffer_get_iter_at_mark(buffer.get(), &current, gtk_text_buffer_get_insert(buffer.get()));
    int position = gtk_text_iter_get_offset(&current);
    for (const auto& span: RichText::fromBuffer(buffer.get()).spans)
        if (span.kind == "link" && span.start <= position && position <= span.end)
            gtk_entry_set_text(GTK_ENTRY(entry), span.value.c_str());
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "https://… oder page:2 (Seite im aktuellen Dokument)");
    gtk_widget_set_size_request(entry, 520, -1);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), entry, false, false, 12);
    gtk_widget_show_all(dlg);
    int response = gtk_dialog_run(GTK_DIALOG(dlg));
    if (response == GTK_RESPONSE_REJECT) {
        GtkTextIter a, b;
        bounds(buffer.get(), a, b);
        for (const auto& span: RichText::fromBuffer(buffer.get()).spans)
            if (span.kind == "link")
                gtk_text_buffer_remove_tag(buffer.get(), RichText::tag(buffer.get(), span.kind, span.value), &a, &b);
        contentsChanged(true);
        repaintEditor();
    } else if (response == GTK_RESPONSE_OK) {
        std::string uri = gtk_entry_get_text(GTK_ENTRY(entry));
        if (RichText::tag(buffer.get(), "link", uri))
            applyStyle("link", uri);
        else
            XojMsgBox::showErrorToUser(
                    GTK_WINDOW(dlg),
                    "Bitte http://, https://, mailto: oder page: mit einer ganzen Seitenzahl verwenden.");
    }
    gtk_widget_destroy(dlg);
}

void TextEditor::followLink() {
    GtkTextIter it;
    gtk_text_buffer_get_iter_at_mark(buffer.get(), &it, gtk_text_buffer_get_insert(buffer.get()));
    int pos = gtk_text_iter_get_offset(&it);
    auto rich = RichText::fromBuffer(buffer.get());
    for (const auto& span: rich.spans)
        if (span.kind == "link" && span.start <= pos && pos <= span.end) {
            if (span.value.compare(0, 5, "page:") == 0) {
                auto target = static_cast<size_t>(g_ascii_strtoull(span.value.c_str() + 5, nullptr, 10));
                if (target <= control->getDocument()->getPageCount()) {
                    if (dialogOpen)
                        pendingLinkPage = target;
                    else
                        navigateToPage(control, target - 1);
                } else
                    XojMsgBox::showErrorToUser(control->getGtkWindow(),
                                               "Die verknüpfte Seite existiert in diesem Dokument nicht.");
            } else {
                GError* error = nullptr;
                gtk_show_uri_on_window(control->getGtkWindow(), span.value.c_str(), GDK_CURRENT_TIME, &error);
                if (error) {
                    XojMsgBox::showErrorToUser(control->getGtkWindow(), error->message);
                    g_error_free(error);
                }
            }
            return;
        }
}

void TextEditor::showFormattingDialog() {
    if (dialogOpen)
        return;
    if (tableColumns) {
        editTable();
        return;
    }
    dialogOpen = true;
    auto* dlg = gtk_dialog_new_with_buttons("Text bearbeiten", control->getGtkWindow(), GTK_DIALOG_MODAL, "Fertig",
                                            GTK_RESPONSE_CLOSE, nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 820, 520);
    auto* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    auto toolbarRow = [content] {
        auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_container_add(GTK_CONTAINER(scroll), row);
        gtk_box_pack_start(GTK_BOX(content), scroll, false, false, 4);
        return row;
    };
    auto* toolbar = toolbarRow();
    button(toolbar, "Fett", [this] { applyStyle("bold", "1", true); });
    button(toolbar, "Kursiv", [this] { applyStyle("italic", "1", true); });
    button(toolbar, "Unterstrichen", [this] { applyStyle("underline", "1", true); });
    button(toolbar, "Schrift", [this, dlg] {
        auto* choose = gtk_font_chooser_dialog_new("Schrift auswählen", GTK_WINDOW(dlg));
        if (gtk_dialog_run(GTK_DIALOG(choose)) == GTK_RESPONSE_OK) {
            auto* desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(choose));
            applyStyles(
                    {{"font", pango_font_description_get_family(desc)},
                     {"size", std::to_string(static_cast<double>(pango_font_description_get_size(desc)) / PANGO_SCALE)},
                     {"bold", pango_font_description_get_weight(desc) >= PANGO_WEIGHT_SEMIBOLD ? "1" : "0"},
                     {"italic", pango_font_description_get_style(desc) != PANGO_STYLE_NORMAL ? "1" : "0"}});
            pango_font_description_free(desc);
        }
        gtk_widget_destroy(choose);
    });
    for (const auto& item:
         {std::pair<const char*, const char*>("Textfarbe", "foreground"), {"Hintergrund", "background"}}) {
        std::string kind = item.second;
        button(toolbar, item.first, [this, dlg, kind] {
            auto* choose = gtk_color_chooser_dialog_new("Farbe auswählen", GTK_WINDOW(dlg));
            gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(choose), false);
            if (gtk_dialog_run(GTK_DIALOG(choose)) == GTK_RESPONSE_OK) {
                GdkRGBA c;
                gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(choose), &c);
                char* raw = gdk_rgba_to_string(&c);
                applyStyle(kind, raw);
                g_free(raw);
            }
            gtk_widget_destroy(choose);
        });
    }
    auto* blocks = toolbarRow();
    button(blocks, "Absatz", [this] {
        GtkTextIter a, b;
        bounds(buffer.get(), a, b);
        gtk_text_buffer_remove_tag(buffer.get(), RichText::tag(buffer.get(), "bold", "1"), &a, &b);
        applyStyle("size", "12");
    });
    button(blocks, "Überschrift 1", [this] { applyStyles({{"size", "24"}, {"bold", "1"}}); });
    button(blocks, "Überschrift 2", [this] { applyStyles({{"size", "18"}, {"bold", "1"}}); });
    button(blocks, "• Liste", [this] { paragraph("• "); });
    button(blocks, "1. Liste", [this] { paragraph("number"); });
    button(blocks, "☐ Aufgabe", [this] { paragraph("☐ "); });
    button(blocks, "Einrücken", [this] { paragraph("    "); });
    button(blocks, "Ausrücken", [this] { paragraph("outdent"); });
    auto* links = toolbarRow();
    button(links, "Link / Seite", [this] { editLink(); });
    button(links, "Link öffnen", [this, dlg] {
        followLink();
        if (pendingLinkPage)
            gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_CLOSE);
    });
    button(links, "Tabelle…", [this, dlg] {
        editTable();
        if (tableColumns)
            gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_CLOSE);
    });
    button(links, "Formatierung löschen", [this] {
        GtkTextIter a, b;
        bounds(buffer.get(), a, b);
        gtk_text_buffer_remove_all_tags(buffer.get(), &a, &b);
        pendingStyles.clear();
        contentsChanged(true);
        repaintEditor();
    });
    button(links, "Rückgängig", [this] { undoEdition(); });
    button(links, "Wiederholen", [this] { undoEdition(true); });
    auto* hint = gtk_label_new(
            "Auswahl formatieren; ohne Auswahl gilt die aktuelle Zeile. Strg+B/I/U · Strg+K · Strg+Z/Y\nTabellen "
            "ersetzen diesen Textcontainer. Tabellenzellen bleiben separat bearbeitbar.");
    gtk_box_pack_start(GTK_BOX(content), hint, false, false, 6);
    auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    auto* view = gtk_text_view_new_with_buffer(buffer.get());
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 12);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_box_pack_start(GTK_BOX(content), scroll, true, true, 0);
    auto changed = g_signal_connect(buffer.get(), "end-user-action", G_CALLBACK(+[](GtkTextBuffer*, gpointer p) {
                                        auto* self = static_cast<TextEditor*>(p);
                                        self->contentsChanged(true);
                                        self->repaintEditor();
                                    }),
                                    this);
    g_signal_connect(view, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer p) -> gboolean {
                         auto* self = static_cast<TextEditor*>(p);
                         if (event->state & GDK_CONTROL_MASK) {
                             auto key = gdk_keyval_to_lower(event->keyval);
                             if (key == GDK_KEY_v && (event->state & GDK_SHIFT_MASK)) {
                                 self->pasteFromClipboard(true);
                                 return true;
                             }
                             if (key == GDK_KEY_z || key == GDK_KEY_y) {
                                 self->undoEdition(key == GDK_KEY_y || (event->state & GDK_SHIFT_MASK));
                                 return true;
                             }
                             if (!(event->state & GDK_SHIFT_MASK) &&
                                 (key == GDK_KEY_b || key == GDK_KEY_i || key == GDK_KEY_u)) {
                                 self->applyStyle(key == GDK_KEY_b ? "bold" :
                                                  key == GDK_KEY_i ? "italic" :
                                                                     "underline",
                                                  "1", true);
                                 return true;
                             }
                         }
                         return false;
                     }),
                     this);
    gtk_widget_show_all(dlg);
    gtk_widget_grab_focus(view);
    gtk_dialog_run(GTK_DIALOG(dlg));
    g_signal_handler_disconnect(buffer.get(), changed);
    gtk_widget_destroy(dlg);
    dialogOpen = false;
    contentsChanged(true);
    repaintEditor();
    if (pendingLinkPage) {
        auto target = pendingLinkPage;
        pendingLinkPage = 0;
        navigateToPage(control, target - 1);
    }
}

void TextEditor::editTable() {
    auto rich = RichText::fromBuffer(buffer.get(), tableColumns);
    std::vector<std::vector<RichText>> rows;
    if (tableColumns) {
        int offset = 0;
        for (const auto& row: rich.cells()) {
            rows.emplace_back();
            for (const auto& cell: row) {
                RichText value;
                value.text = cell;
                int len = static_cast<int>(g_utf8_strlen(cell.c_str(), -1));
                for (auto span: rich.spans) {
                    span.start = std::max(span.start, offset) - offset;
                    span.end = std::min(span.end, offset + len) - offset;
                    if (span.end > span.start)
                        value.spans.push_back(std::move(span));
                }
                rows.back().push_back(std::move(value));
                offset += len + 1;
            }
        }
    } else {
        rows.resize(2, std::vector<RichText>(2));
        rows[0][0].text = rich.text;
        std::replace(rows[0][0].text.begin(), rows[0][0].text.end(), '\n', ' ');
        std::replace(rows[0][0].text.begin(), rows[0][0].text.end(), '\t', ' ');
        rows[0][0].spans = rich.spans;
    }
    bool finished = false;
    while (!finished) {
        auto* dlg =
                gtk_dialog_new_with_buttons("Tabelle bearbeiten", control->getGtkWindow(), GTK_DIALOG_MODAL,
                                            "Abbrechen", GTK_RESPONSE_CANCEL, "Übernehmen", GTK_RESPONSE_OK, nullptr);
        gtk_window_set_default_size(GTK_WINDOW(dlg), 740, 440);
        auto* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
        auto* actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_box_pack_start(GTK_BOX(content), actions, false, false, 4);
        auto* rowChoice = gtk_spin_button_new_with_range(1, static_cast<double>(rows.size()), 1);
        auto* colChoice = gtk_spin_button_new_with_range(1, static_cast<double>(rows[0].size()), 1);
        atk_object_set_name(gtk_widget_get_accessible(rowChoice), "Zeile");
        atk_object_set_name(gtk_widget_get_accessible(colChoice), "Spalte");
        gtk_box_pack_start(GTK_BOX(actions), gtk_label_new("Zeile"), false, false, 0);
        gtk_box_pack_start(GTK_BOX(actions), rowChoice, false, false, 0);
        gtk_box_pack_start(GTK_BOX(actions), gtk_label_new("Spalte"), false, false, 0);
        gtk_box_pack_start(GTK_BOX(actions), colChoice, false, false, 0);
        button(actions, "Zeile +", [dlg] { gtk_dialog_response(GTK_DIALOG(dlg), 1); });
        button(actions, "Zeile −", [dlg] { gtk_dialog_response(GTK_DIALOG(dlg), 2); });
        button(actions, "Spalte +", [dlg] { gtk_dialog_response(GTK_DIALOG(dlg), 3); });
        button(actions, "Spalte −", [dlg] { gtk_dialog_response(GTK_DIALOG(dlg), 4); });
        auto* hint = gtk_label_new(
                "Eine Textzeile je Zelle · Tab wechselt die Zelle · Änderungen erst mit Übernehmen\nSpalten sind "
                "gleich breit und wachsen mit dem Inhalt. Maximal 32 Spalten / 1000 Zeilen.");
        gtk_box_pack_start(GTK_BOX(content), hint, false, false, 6);
        auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        auto* grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
        gtk_container_add(GTK_CONTAINER(scroll), grid);
        gtk_box_pack_start(GTK_BOX(content), scroll, true, true, 0);
        std::vector<std::vector<GtkWidget*>> editors;
        for (size_t r = 0; r < rows.size(); ++r) {
            editors.emplace_back();
            for (size_t c = 0; c < rows[r].size(); ++c) {
                auto* view = gtk_text_view_new();
                gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(view), false);
                gtk_widget_set_size_request(view, 160, 40);
                auto* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
                rows[r][c].toBuffer(buf);
                gtk_text_buffer_register_serialize_tagset(buf, "foliq-v1");
                auto fmt = gtk_text_buffer_register_deserialize_tagset(buf, "foliq-v1");
                gtk_text_buffer_deserialize_set_can_create_tags(buf, fmt, true);
                std::string name = "Zelle " + std::to_string(r + 1) + ", " + std::to_string(c + 1);
                atk_object_set_name(gtk_widget_get_accessible(view), name.c_str());
                gtk_grid_attach(GTK_GRID(grid), view, static_cast<int>(c), static_cast<int>(r), 1, 1);
                editors.back().push_back(view);
            }
        }
        gtk_widget_show_all(dlg);
        int response = gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_spin_button_update(GTK_SPIN_BUTTON(rowChoice));
        gtk_spin_button_update(GTK_SPIN_BUTTON(colChoice));
        int rowIndex = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rowChoice)) - 1;
        int colIndex = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(colChoice)) - 1;
        bool valid = true;
        for (size_t r = 0; r < rows.size(); ++r)
            for (size_t c = 0; c < rows[r].size(); ++c) {
                auto value = RichText::fromBuffer(gtk_text_view_get_buffer(GTK_TEXT_VIEW(editors[r][c])));
                if (value.text.find_first_of("\n\t\r") != std::string::npos || !value.valid())
                    valid = false;
                rows[r][c] = std::move(value);
            }
        if (!valid && response != GTK_RESPONSE_CANCEL && response != GTK_RESPONSE_DELETE_EVENT) {
            XojMsgBox::showErrorToUser(
                    GTK_WINDOW(dlg),
                    "Tabellenzellen benötigen jeweils eine Textzeile ohne Tabulator. Bitte Zeilenumbrüche entfernen.");
            // Preserve entered data for correction; never commit a malformed table.
            gtk_widget_destroy(dlg);
            continue;
        }
        gtk_widget_destroy(dlg);
        if (response == 1 && rows.size() < 1000)
            rows.insert(rows.begin() + rowIndex + 1, std::vector<RichText>(rows[0].size()));
        else if (response == 2 && rows.size() > 1)
            rows.erase(rows.begin() + rowIndex);
        else if (response == 3 && rows[0].size() < 32)
            for (auto& row: rows) row.insert(row.begin() + colIndex + 1, RichText{});
        else if (response == 4 && rows[0].size() > 1)
            for (auto& row: rows) row.erase(row.begin() + colIndex);
        else if (response == GTK_RESPONSE_OK) {
            RichText result;
            result.columns = static_cast<int>(rows[0].size());
            int offset = 0;
            for (size_t r = 0; r < rows.size(); ++r) {
                if (r) {
                    result.text += '\n';
                    ++offset;
                }
                for (size_t c = 0; c < rows[r].size(); ++c) {
                    if (c) {
                        result.text += '\t';
                        ++offset;
                    }
                    const auto& cell = rows[r][c];
                    for (auto span: cell.spans) {
                        span.start += offset;
                        span.end += offset;
                        result.spans.push_back(std::move(span));
                    }
                    result.text += cell.text;
                    offset += static_cast<int>(g_utf8_strlen(cell.text.c_str(), -1));
                }
            }
            if (!result.valid()) {
                XojMsgBox::showErrorToUser(control->getGtkWindow(),
                                           "Die Tabelle überschreitet die unterstützte Größe.");
                continue;
            }
            tableColumns = result.columns;
            result.toBuffer(buffer.get());
            contentsChanged(true);
            repaintEditor();
            finished = true;
        } else if (response < 0)
            finished = true;
    }
}
