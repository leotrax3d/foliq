// SPDX-License-Identifier: GPL-2.0-or-later
// Onboarding wizard (GTK3): profile -> library folder -> subjects/colours/paper -> look & input -> intro.
#include "Onboarding.h"

#include <cmath>
#include <map>
#include <memory>

#include "control/Control.h"
#include "gui/MainWindow.h"
#include "util/XojMsgBox.h"
#include "Navigation.h"
#include "Storage.h"

namespace foliq {
namespace {
struct Row {
    GtkWidget *check, *swatch, *paper;
    std::string name, color;
};
struct State {
    Profile profile = Profile::Pupil;
    std::string library;
    std::vector<std::unique_ptr<Row>> rows;
    GtkWidget *list = nullptr, *pathLabel = nullptr, *stack = nullptr, *back = nullptr, *next = nullptr,
              *stepLabel = nullptr, *custom = nullptr;
    int theme = 0;
    bool touch = false;
    std::string accent;
    int page = 0;
};
const char* PAGES[] = {"profil", "ort", "faecher", "ansicht", "intro"};

GtkWidget* title(const char* text, const char* sub) {
    auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    auto* t = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(t), "foliq-dialog-title");
    gtk_label_set_xalign(GTK_LABEL(t), 0);
    gtk_box_pack_start(GTK_BOX(box), t, false, false, 0);
    if (sub) {
        auto* s = gtk_label_new(sub);
        gtk_label_set_xalign(GTK_LABEL(s), 0);
        gtk_label_set_line_wrap(GTK_LABEL(s), true);
        gtk_style_context_add_class(gtk_widget_get_style_context(s), "dim-label");
        gtk_box_pack_start(GTK_BOX(box), s, false, false, 0);
    }
    return box;
}
gboolean drawSwatch(GtkWidget* w, cairo_t* cr, gpointer) {
    auto* color = static_cast<std::string*>(g_object_get_data(G_OBJECT(w), "color"));
    Rgb rgb;
    double size = std::min(gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w));
    if (color && parseHex(*color, rgb)) {
        cairo_arc(cr, size / 2, size / 2, size / 2 - 2, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, rgb.r, rgb.g, rgb.b);
        cairo_fill(cr);
    }
    return true;
}
}  // namespace

void Navigation::onboarding(bool firstRun) {
    State st;
    st.library = catalog ? catalog->directory().u8string() :
                           (fs::u8path(g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS) ? g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS) : g_get_home_dir()) /
                            "Foliq")
                                   .u8string();
    st.theme = theme;
    st.touch = options.touch;
    st.accent = options.accent;

    auto* dlg = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dlg), "Foliq einrichten");
    gtk_window_set_transient_for(GTK_WINDOW(dlg), window);
    gtk_window_set_modal(GTK_WINDOW(dlg), true);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 780, 600);
    auto* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    gtk_box_set_spacing(GTK_BOX(content), 12);
    st.stepLabel = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(st.stepLabel), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(st.stepLabel), "dim-label");
    gtk_box_pack_start(GTK_BOX(content), st.stepLabel, false, false, 0);
    st.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(st.stack), options.reducedMotion ? GTK_STACK_TRANSITION_TYPE_NONE :
                                                                              GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_box_pack_start(GTK_BOX(content), st.stack, true, true, 0);

    // 1 Profile ------------------------------------------------------------------------------
    auto* p1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start(GTK_BOX(p1), title("Willkommen bei Foliq", "Wofür möchtest du Foliq nutzen? Daraus entsteht "
                                                                  "eine passende Notizbuch-Vorlage. Alles bleibt "
                                                                  "lokal und lässt sich später ändern."),
                       false, false, 0);
    struct ProfileInfo {
        Profile p;
        const char *name, *icon, *desc;
    };
    GSList* group = nullptr;
    for (const auto& info: {ProfileInfo{Profile::Pupil, "Schüler/in", "foliq-type",
                                        "Ein Notizbuch pro Fach: Mitschriften, Hausaufgaben, Arbeiten & Tests"},
                            ProfileInfo{Profile::Student, "Student/in", "xopp-document-open",
                                        "Ein Notizbuch pro Modul: Vorlesung, Übung, Klausurvorbereitung"},
                            ProfileInfo{Profile::Teacher, "Lehrkraft", "xopp-tool-pencil",
                                        "Ein Notizbuch pro Fach: Unterrichtsplanung, Material, Korrekturen"},
                            ProfileInfo{Profile::Work, "Beruf / Privat", "foliq-sliders",
                                        "Projekte, Besprechungen, Ideen, Privates …"},
                            ProfileInfo{Profile::Empty, "Leer starten", "foliq-plus",
                                        "Keine Vorlage – nur Speicherort und Darstellung festlegen"}}) {
        auto* b = gtk_radio_button_new(group);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(b));
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), false);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "foliq-choice");
        auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        auto* img = gtk_image_new_from_icon_name(info.icon, GTK_ICON_SIZE_DND);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 28);
        gtk_box_pack_start(GTK_BOX(row), img, false, false, 4);
        auto* texts = title(info.name, info.desc);
        gtk_box_pack_start(GTK_BOX(row), texts, true, true, 0);
        gtk_container_add(GTK_CONTAINER(b), row);
        atk_object_set_name(gtk_widget_get_accessible(b), info.name);
        g_object_set_data(G_OBJECT(b), "profile", GINT_TO_POINTER(static_cast<int>(info.p)));
        g_signal_connect(b, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                             if (gtk_toggle_button_get_active(b))
                                 static_cast<State*>(p)->profile =
                                         static_cast<Profile>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "profile")));
                         }),
                         &st);
        gtk_box_pack_start(GTK_BOX(p1), b, false, false, 0);
    }
    gtk_stack_add_named(GTK_STACK(st.stack), p1, PAGES[0]);

    // 2 Library folder -----------------------------------------------------------------------
    auto* p2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start(GTK_BOX(p2),
                       title("Wo sollen deine Notizen gespeichert werden?",
                             "Foliq legt dort Notizbücher, Seiten und Sicherungen ab. Ein Ordner in einem "
                             "Cloud-Sync-Verzeichnis ist möglich, Foliq selbst sendet nichts ins Internet."),
                       false, false, 0);
    auto* pathRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    st.pathLabel = gtk_label_new(st.library.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(st.pathLabel), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_xalign(GTK_LABEL(st.pathLabel), 0);
    gtk_label_set_selectable(GTK_LABEL(st.pathLabel), true);
    gtk_box_pack_start(GTK_BOX(pathRow), st.pathLabel, true, true, 0);
    auto* choose = gtk_button_new_with_label("Ordner wählen …");
    gtk_style_context_add_class(gtk_widget_get_style_context(choose), "foliq-standard");
    gtk_box_pack_start(GTK_BOX(pathRow), choose, false, false, 0);
    g_object_set_data(G_OBJECT(choose), "dialog", dlg);
    g_signal_connect(choose, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer p) {
                         auto* st = static_cast<State*>(p);
                         // Native folder picker on Windows (GtkFileChooserNative -> IFileDialog).
                         auto* chooser = gtk_file_chooser_native_new(
                                 "Speicherort für Foliq wählen",
                                 GTK_WINDOW(g_object_get_data(G_OBJECT(b), "dialog")),
                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Auswählen", "Abbrechen");
                         gtk_file_chooser_set_create_folders(GTK_FILE_CHOOSER(chooser), true);
                         if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
                             auto* f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
                             if (f) {
                                 st->library = f;
                                 gtk_label_set_text(GTK_LABEL(st->pathLabel), f);
                                 g_free(f);
                             }
                         }
                         g_object_unref(chooser);
                     }),
                     &st);
    gtk_box_pack_start(GTK_BOX(p2), pathRow, false, false, 0);
    auto* hint = gtk_label_new("Tipp: Einen leeren oder einen vorhandenen Foliq-Ordner wählen. Vorhandene "
                               "Notizbücher bleiben erhalten; die Vorlage wird ergänzt.");
    gtk_label_set_line_wrap(GTK_LABEL(hint), true);
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(hint), "dim-label");
    gtk_box_pack_start(GTK_BOX(p2), hint, false, false, 0);
    gtk_stack_add_named(GTK_STACK(st.stack), p2, PAGES[1]);

    // 3 Subjects -------------------------------------------------------------------------------
    auto* p3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(p3),
                       title("Fächer auswählen", "Jedes gewählte Fach wird ein eigenes Notizbuch. Farbe und "
                                                 "Papier kannst du direkt festlegen."),
                       false, false, 0);
    auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    st.list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(scroll), st.list);
    gtk_box_pack_start(GTK_BOX(p3), scroll, true, true, 0);
    auto* addRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    st.custom = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(st.custom), "Eigenes Fach, z. B. Darstellendes Spiel");
    atk_object_set_name(gtk_widget_get_accessible(st.custom), "Eigenes Fach");
    gtk_box_pack_start(GTK_BOX(addRow), st.custom, true, true, 0);
    auto* addButton = gtk_button_new_with_label("Hinzufügen");
    gtk_style_context_add_class(gtk_widget_get_style_context(addButton), "foliq-standard");
    gtk_box_pack_start(GTK_BOX(addRow), addButton, false, false, 0);
    gtk_box_pack_start(GTK_BOX(p3), addRow, false, false, 0);
    gtk_stack_add_named(GTK_STACK(st.stack), p3, PAGES[2]);

    // Row factory (also used for custom subjects).
    static auto addSubject = +[](State* st, const std::string& name, bool checked) {
        auto row = std::make_unique<Row>();
        row->name = name;
        row->color = subjectPalette()[st->rows.size() % 16].second;
        auto* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        row->check = gtk_check_button_new_with_label(name.c_str());
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(row->check), checked);
        gtk_box_pack_start(GTK_BOX(box), row->check, true, true, 0);
        row->swatch = gtk_menu_button_new();
        auto* area = gtk_drawing_area_new();
        gtk_widget_set_size_request(area, 20, 20);
        g_object_set_data(G_OBJECT(area), "color", &row->color);
        g_signal_connect(area, "draw", G_CALLBACK(drawSwatch), nullptr);
        gtk_container_add(GTK_CONTAINER(row->swatch), area);
        gtk_widget_set_tooltip_text(row->swatch, "Farbe wählen");
        atk_object_set_name(gtk_widget_get_accessible(row->swatch), (name + ": Farbe").c_str());
        // 16-colour popover.
        auto* pop = gtk_popover_new(row->swatch);
        auto* grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
        gtk_container_set_border_width(GTK_CONTAINER(grid), 6);
        for (size_t i = 0; i < subjectPalette().size(); ++i) {
            auto* b = gtk_button_new();
            auto* a = gtk_drawing_area_new();
            gtk_widget_set_size_request(a, 26, 26);
            auto* c = const_cast<std::string*>(&subjectPalette()[i].second);
            g_object_set_data(G_OBJECT(a), "color", c);
            g_signal_connect(a, "draw", G_CALLBACK(drawSwatch), nullptr);
            gtk_container_add(GTK_CONTAINER(b), a);
            gtk_widget_set_tooltip_text(b, subjectPalette()[i].first.c_str());
            atk_object_set_name(gtk_widget_get_accessible(b), subjectPalette()[i].first.c_str());
            g_object_set_data(G_OBJECT(b), "row", row.get());
            g_object_set_data(G_OBJECT(b), "color", c);
            g_object_set_data(G_OBJECT(b), "pop", pop);
            g_object_set_data(G_OBJECT(b), "area", area);
            g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
                                 auto* r = static_cast<Row*>(g_object_get_data(G_OBJECT(b), "row"));
                                 r->color = *static_cast<std::string*>(g_object_get_data(G_OBJECT(b), "color"));
                                 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r->check), true);
                                 gtk_widget_queue_draw(GTK_WIDGET(g_object_get_data(G_OBJECT(b), "area")));
                                 gtk_popover_popdown(GTK_POPOVER(g_object_get_data(G_OBJECT(b), "pop")));
                             }),
                             nullptr);
            gtk_grid_attach(GTK_GRID(grid), b, static_cast<int>(i % 8), static_cast<int>(i / 8), 1, 1);
        }
        gtk_widget_show_all(grid);
        gtk_container_add(GTK_CONTAINER(pop), grid);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(row->swatch), pop);
        gtk_box_pack_start(GTK_BOX(box), row->swatch, false, false, 0);
        row->paper = gtk_combo_box_text_new();
        for (const auto& s: paperStyles()) gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(row->paper), s.second.c_str(),
                                                                     s.first.c_str());
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(row->paper), defaultPaperFor(name).c_str());
        atk_object_set_name(gtk_widget_get_accessible(row->paper), (name + ": Papier").c_str());
        gtk_box_pack_start(GTK_BOX(box), row->paper, false, false, 0);
        gtk_widget_show_all(box);
        gtk_box_pack_start(GTK_BOX(st->list), box, false, false, 0);
        st->rows.push_back(std::move(row));
    };
    g_signal_connect(addButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) {
                         auto* st = static_cast<State*>(p);
                         std::string name = gtk_entry_get_text(GTK_ENTRY(st->custom));
                         if (name.find_first_not_of(" \t") == std::string::npos)
                             return;
                         addSubject(st, name, true);
                         gtk_entry_set_text(GTK_ENTRY(st->custom), "");
                     }),
                     &st);
    g_signal_connect(st.custom, "activate", G_CALLBACK(+[](GtkEntry*, gpointer b) { gtk_button_clicked(GTK_BUTTON(b)); }),
                     addButton);

    // 4 Look & input ------------------------------------------------------------------------
    auto* p4 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_box_pack_start(GTK_BOX(p4), title("Darstellung und Eingabe", "Alles lässt sich später unter „Ansicht“ ändern."),
                       false, false, 0);
    auto segmented = [&](const char* heading, std::vector<const char*> labels, int active, GCallback cb) {
        auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        auto* h = gtk_label_new(heading);
        gtk_label_set_xalign(GTK_LABEL(h), 0);
        gtk_style_context_add_class(gtk_widget_get_style_context(h), "foliq-pane-title");
        gtk_box_pack_start(GTK_BOX(box), h, false, false, 0);
        auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        GSList* g = nullptr;
        for (size_t i = 0; i < labels.size(); ++i) {
            auto* b = gtk_radio_button_new_with_label(g, labels[i]);
            g = gtk_radio_button_get_group(GTK_RADIO_BUTTON(b));
            gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), false);
            gtk_style_context_add_class(gtk_widget_get_style_context(b), "foliq-standard");
            g_object_set_data(G_OBJECT(b), "index", GINT_TO_POINTER(i));
            if (static_cast<int>(i) == active)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), true);
            g_signal_connect(b, "toggled", cb, &st);
            gtk_box_pack_start(GTK_BOX(row), b, false, false, 0);
        }
        gtk_box_pack_start(GTK_BOX(box), row, false, false, 0);
        gtk_box_pack_start(GTK_BOX(p4), box, false, false, 0);
    };
    segmented("Design", {"System", "Hell", "Dunkel"}, st.theme, G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                  if (gtk_toggle_button_get_active(b))
                      static_cast<State*>(p)->theme = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "index"));
              }));
    segmented("Eingabe", {"Kompakt (Maus und Tastatur)", "Berührung (Stift und Touch)"}, st.touch ? 1 : 0,
              G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                  if (gtk_toggle_button_get_active(b))
                      static_cast<State*>(p)->touch = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "index")) == 1;
              }));
    {
        auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        auto* h = gtk_label_new("Akzentfarbe");
        gtk_label_set_xalign(GTK_LABEL(h), 0);
        gtk_style_context_add_class(gtk_widget_get_style_context(h), "foliq-pane-title");
        gtk_box_pack_start(GTK_BOX(box), h, false, false, 0);
        auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        GSList* g = nullptr;
        for (const auto& c: accentChoices()) {
            auto* b = gtk_radio_button_new(g);
            g = gtk_radio_button_get_group(GTK_RADIO_BUTTON(b));
            gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), false);
            auto* a = gtk_drawing_area_new();
            gtk_widget_set_size_request(a, 24, 24);
            g_object_set_data(G_OBJECT(a), "color", const_cast<std::string*>(&c.second));
            g_signal_connect(a, "draw", G_CALLBACK(drawSwatch), nullptr);
            gtk_container_add(GTK_CONTAINER(b), a);
            gtk_widget_set_tooltip_text(b, c.first.c_str());
            atk_object_set_name(gtk_widget_get_accessible(b), c.first.c_str());
            g_object_set_data(G_OBJECT(b), "color", const_cast<std::string*>(&c.second));
            if (c.second == st.accent)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), true);
            g_signal_connect(b, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer p) {
                                 if (gtk_toggle_button_get_active(b))
                                     static_cast<State*>(p)->accent =
                                             *static_cast<std::string*>(g_object_get_data(G_OBJECT(b), "color"));
                             }),
                             &st);
            gtk_box_pack_start(GTK_BOX(row), b, false, false, 0);
        }
        gtk_box_pack_start(GTK_BOX(box), row, false, false, 0);
        gtk_box_pack_start(GTK_BOX(p4), box, false, false, 0);
    }
    gtk_stack_add_named(GTK_STACK(st.stack), p4, PAGES[3]);

    // 5 Intro ------------------------------------------------------------------------------------
    auto* p5 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(p5), title("So funktioniert Foliq", nullptr), false, false, 0);
    struct Tip {
        const char *icon, *head, *text;
    };
    for (const auto& t: {Tip{"foliq-panel-left", "Navigation",
                             "Links: Notizbücher und Abschnitte, daneben die Seiten. Strg+Umschalt+N blendet sie "
                             "ein und aus, F6 wechselt zwischen den Bereichen."},
                         Tip{"foliq-chevron-up", "Menüband",
                             "Start, Einfügen, Zeichnen und Ansicht. Mit Strg+F1 einklappen – dann öffnet ein Klick "
                             "auf die Registerkarte es kurz über der Seite."},
                         Tip{"foliq-maximize", "Vollbild und Fokus",
                             "Das Vollbild-Symbol oben rechts (F11) lässt nur eine kleine Werkzeugleiste übrig, "
                             "Escape beendet es. Fokusmodus: Strg+Umschalt+F."},
                         Tip{"foliq-more", "Alle Befehle",
                             "Das ⋯-Menü (F10) enthält sämtliche weiteren Funktionen. Dieser Assistent lässt "
                             "sich dort und unter Ansicht → Darstellung jederzeit erneut starten."}}) {
        auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        auto* img = gtk_image_new_from_icon_name(t.icon, GTK_ICON_SIZE_DND);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 24);
        gtk_widget_set_valign(img, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(row), img, false, false, 4);
        gtk_box_pack_start(GTK_BOX(row), title(t.head, t.text), true, true, 0);
        gtk_box_pack_start(GTK_BOX(p5), row, false, false, 4);
    }
    gtk_stack_add_named(GTK_STACK(st.stack), p5, PAGES[4]);

    // Footer -------------------------------------------------------------------------------------
    gtk_dialog_add_button(GTK_DIALOG(dlg), firstRun ? "Überspringen" : "Abbrechen", GTK_RESPONSE_CANCEL);
    st.back = gtk_dialog_add_button(GTK_DIALOG(dlg), "Zurück", 1);
    st.next = gtk_dialog_add_button(GTK_DIALOG(dlg), "Weiter", 2);
    gtk_style_context_add_class(gtk_widget_get_style_context(st.next), "suggested-action");
    auto show = [](State* st) {
        bool noSubjects = st->profile == Profile::Empty;
        if (noSubjects && st->page == 2)
            st->page = st->page == 2 ? 3 : st->page;
        gtk_stack_set_visible_child_name(GTK_STACK(st->stack), PAGES[st->page]);
        gtk_widget_set_sensitive(st->back, st->page > 0);
        gtk_button_set_label(GTK_BUTTON(st->next), st->page == 4 ? "Fertig" : "Weiter");
        int total = noSubjects ? 4 : 5;
        int shown = st->page + 1 - (noSubjects && st->page > 2 ? 1 : 0);
        gtk_label_set_text(GTK_LABEL(st->stepLabel),
                           ("Schritt " + std::to_string(shown) + " von " + std::to_string(total)).c_str());
    };
    gtk_widget_show_all(dlg);
    show(&st);
    int lastProfile = -1;
    bool done = false;
    while (true) {
        int r = gtk_dialog_run(GTK_DIALOG(dlg));
        if (r == 1) {
            st.page = std::max(0, st.page - 1);
            if (st.profile == Profile::Empty && st.page == 2)
                st.page = 1;
        } else if (r == 2) {
            if (st.page == 1) {
                // Validate the folder now, not after the subjects page.
                std::error_code ec;
                fs::create_directories(fs::u8path(st.library), ec);
                if (ec || !fs::is_directory(fs::u8path(st.library))) {
                    XojMsgBox::showErrorToUser(GTK_WINDOW(dlg), "Der Ordner kann nicht angelegt oder verwendet "
                                                                "werden. Bitte einen anderen Speicherort wählen.");
                    continue;
                }
            }
            if (st.page == 0 && lastProfile != static_cast<int>(st.profile)) {
                // (Re)build the subject list for the chosen profile.
                st.rows.clear();
                GList* kids = gtk_container_get_children(GTK_CONTAINER(st.list));
                for (GList* k = kids; k; k = k->next) gtk_widget_destroy(GTK_WIDGET(k->data));
                g_list_free(kids);
                for (const auto& s: subjectsFor(st.profile)) addSubject(&st, s, false);
                lastProfile = static_cast<int>(st.profile);
            }
            if (st.page == 4) {
                done = true;
                break;
            }
            ++st.page;
        } else
            break;
        show(&st);
    }
    gtk_widget_hide(dlg);
    try {
        if (done) {
            std::vector<Subject> subjects;
            for (const auto& row: st.rows)
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(row->check))) {
                    auto* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(row->paper));
                    subjects.push_back({row->name, row->color, id ? id : "lined"});
                }
            // Library location: persisted first (atomic), then (re)opened.
            auto root = fs::u8path(st.library);
            if (!g_getenv("FOLIQ_LIBRARY")) {
                fs::create_directories(libraryConfigPath().parent_path());
                atomicWrite(libraryConfigPath(), serializeLibraryConfig({root.u8string()}));
            }
            if (!catalog || fs::absolute(catalog->directory()).lexically_normal() !=
                                    fs::absolute(root).lexically_normal())
                openLibrary(root);
            options.accent = st.accent;
            theme = st.theme;
            setDensity(st.touch);
            applyTheme();
            std::string firstPage;
            for (const auto& nb: plan(st.profile, subjects)) {
                auto book = catalog->add(Kind::Notebook, "", nb.name);
                options.sectionColors[book] = nb.color;
                for (const auto& sec: nb.sections) {
                    auto section = catalog->add(Kind::Section, book, sec);
                    options.sectionColors[section] = nb.color;
                    auto page = catalog->add(Kind::Page, section, "Erste Seite", pageDocument(nb.paper));
                    if (firstPage.empty())
                        firstPage = page;
                }
            }
            if (!firstPage.empty()) {
                // Land on the first subject's first page, with its section's page list visible.
                section = catalog->get(firstPage).parent;
                pendingSelection = section;
                selectTree(books);
                refresh();
                pendingSelection = firstPage;
                selectTree(pages);
                refresh(true);
                if (flush() && control->openFile(catalog->document(firstPage)))
                    catalog->position(firstPage, 0);
            } else
                refresh();
            saveView();
        } else if (firstRun && !g_getenv("FOLIQ_LIBRARY") && catalog) {
            // Skipped: remember the default location so the wizard does not reappear.
            fs::create_directories(libraryConfigPath().parent_path());
            atomicWrite(libraryConfigPath(), serializeLibraryConfig({catalog->directory().u8string()}));
        }
    } catch (const std::exception& e) {
        error(e);
    }
    gtk_widget_destroy(dlg);
}
}  // namespace foliq
