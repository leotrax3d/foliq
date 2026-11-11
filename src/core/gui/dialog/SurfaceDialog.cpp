// SPDX-License-Identifier: GPL-2.0-or-later
#include "SurfaceDialog.h"

#include <array>
#include <sstream>

#include "model/BackgroundConfig.h"
#include "util/Util.h"
#include "util/i18n.h"

bool editPageSurface(GtkWindow* parent, SurfaceState& state) {
    auto* dialog = gtk_dialog_new_with_buttons(_("Page surface and PDF crop"), parent, GTK_DIALOG_MODAL, _("Cancel"),
                                               GTK_RESPONSE_CANCEL, _("Apply"), GTK_RESPONSE_OK, nullptr);
    auto* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 7);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    auto* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_size_request(scroller, std::clamp(gtk_widget_get_allocated_width(GTK_WIDGET(parent)) - 80, 480, 820),
                                std::clamp(gtk_widget_get_allocated_height(GTK_WIDGET(parent)) - 160, 320, 600));
    gtk_container_add(GTK_CONTAINER(scroller), grid);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), scroller);
    int row = 0;
    auto add = [&](const char* title, GtkWidget* widget) {
        auto* label = gtk_label_new(title);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), widget, 1, row++, 1, 1);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);
        atk_object_set_name(gtk_widget_get_accessible(widget), title);
    };
    auto combo = [&](const char* title, const std::initializer_list<const char*>& items, int selected) {
        auto* widget = gtk_combo_box_text_new();
        for (auto item: items) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), item);
        gtk_combo_box_set_active(GTK_COMBO_BOX(widget), selected);
        add(title, widget);
        return widget;
    };
    auto spin = [&](const char* title, double value, double min, double max) {
        auto* widget = gtk_spin_button_new_with_range(min, max, 1);
        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(widget), 2);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), value);
        add(title, widget);
        return widget;
    };
    auto toggle = [&](const char* title, bool value) {
        auto* widget = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), value);
        add(title, widget);
        return widget;
    };
    auto* mode = combo(_("Page type"), {_("Paper"), _("Free surface")}, state.surface.infinite ? 1 : 0);
    auto* format = combo(_("Paper format / free view window"), {_("Custom"), "A4", "A5", "A3", "Letter"}, 0);
    auto* orientation = combo(_("Orientation"), {_("Portrait"), _("Landscape")}, state.width > state.height ? 1 : 0);
    auto* width = spin(_("Width (pt)"), state.width, 1, 14400);
    auto* height = spin(_("Height (pt)"), state.height, 1, 14400);
    struct FormatWidgets {
        GtkWidget *format, *orientation, *width, *height;
    } fw{format, orientation, width, height};
    auto updateFormat = +[](GtkComboBox*, gpointer data) {
        auto& w = *static_cast<FormatWidgets*>(data);
        int f = gtk_combo_box_get_active(GTK_COMBO_BOX(w.format));
        double width = gtk_spin_button_get_value(GTK_SPIN_BUTTON(w.width));
        double height = gtk_spin_button_get_value(GTK_SPIN_BUTTON(w.height));
        const double sizes[][2] = {{0, 0}, {595.276, 841.89}, {419.528, 595.276}, {841.89, 1190.55}, {612, 792}};
        if (f > 0) {
            width = sizes[f][0];
            height = sizes[f][1];
        }
        if ((width > height) != (gtk_combo_box_get_active(GTK_COMBO_BOX(w.orientation)) == 1))
            std::swap(width, height);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(w.width), width);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(w.height), height);
    };
    g_signal_connect(format, "changed", G_CALLBACK(updateFormat), &fw);
    g_signal_connect(orientation, "changed", G_CALLBACK(updateFormat), &fw);
    int bg = state.background.format == PageTypeFormat::Plain ? 1 :
             state.background.format == PageTypeFormat::Lined ? 2 :
             state.background.format == PageTypeFormat::Graph ? 3 :
                                                                0;
    auto* ruling = combo(_("Ruling"), {_("Keep existing background"), _("Plain"), _("Lined"), _("Grid")}, bg);
    double raster = 14.17;
    BackgroundConfig(state.background.config).loadValue("r1", raster);
    auto* spacing = spin(_("Line / grid spacing (pt)"), raster, 2, 1000);
    auto color = Util::rgb_to_GdkRGBA(state.color);
    auto* colorButton = gtk_color_button_new_with_rgba(&color);
    add(_("Background color"), colorButton);
    auto* content = toggle(_("PDF: entire content (all layers)"), state.surface.exportContent);
    auto* x = spin(_("PDF crop X (pt)"), state.surface.crop.x, -985600, 985600);
    auto* y = spin(_("PDF crop Y (pt)"), state.surface.crop.y, -985600, 985600);
    auto* cw = spin(_("PDF crop width (pt)"), state.surface.crop.width, 1, 1000000);
    auto* ch = spin(_("PDF crop height (pt)"), state.surface.crop.height, 1, 1000000);
    auto* tiled = toggle(_("PDF: split into tiles (row by row)"), state.surface.tiled);
    auto* tw = spin(_("PDF tile width (pt)"), state.surface.tileWidth, 1, 14400);
    auto* th = spin(_("PDF tile height (pt)"), state.surface.tileHeight, 1, 14400);
    auto* info = gtk_label_new(_(
            "Changing the page type creates a copy; the original stays in this document.\n"
            "Paper to free: coordinates and pressure stay unchanged. PDF/image backgrounds stay anchored at (0,0).\n"
            "Free to paper: the current view origin moves to (0,0), without scaling. Content outside the paper\n"
            "is retained in the copy but clipped on screen and in PDF. Undo removes the copy.\n"
            "Free surfaces support plain, lined and grid backgrounds. Other rulings become a grid in the copy.\n"
            "Free ruling uses a uniform global pattern; paper margins and custom line styles remain in the original.\n"
            "Pan with the Hand tool or configured touch gestures; zoom with Ctrl+wheel or pinch.\n"
            "Coordinate range: +/-1,000,000 pt. PDF: at most 10,000 tiles, single pages at most 14,400 pt.\n"
            "Older Xournal++ ignores free-surface metadata. Keep this Foliq original. PDF outlines are omitted\n"
            "when free surfaces are present because tiling changes page numbers. PDF crop applies to free pages."));
    gtk_label_set_xalign(GTK_LABEL(info), 0);
    gtk_label_set_line_wrap(GTK_LABEL(info), true);
    gtk_label_set_max_width_chars(GTK_LABEL(info), 85);
    gtk_grid_attach(GTK_GRID(grid), info, 0, row++, 2, 1);
    gtk_widget_show_all(dialog);
    bool accepted = false;
    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        SurfaceState candidate = state;
        candidate.surface.infinite = gtk_combo_box_get_active(GTK_COMBO_BOX(mode)) == 1;
        candidate.width = gtk_spin_button_get_value(GTK_SPIN_BUTTON(width));
        candidate.height = gtk_spin_button_get_value(GTK_SPIN_BUTTON(height));
        int choice = gtk_combo_box_get_active(GTK_COMBO_BOX(ruling));
        if (choice) {
            const PageTypeFormat types[] = {PageTypeFormat::Plain, PageTypeFormat::Plain, PageTypeFormat::Lined,
                                            PageTypeFormat::Graph};
            if (candidate.background.format != types[choice])
                candidate.background = PageType(types[choice]);
            std::istringstream previous(candidate.background.config);
            std::string token, config;
            while (std::getline(previous, token, ',')) {
                if (!token.empty() && token.compare(0, 3, "r1=") != 0)
                    config += token + ",";
            }
            candidate.background.config =
                    config + "r1=" + std::to_string(gtk_spin_button_get_value(GTK_SPIN_BUTTON(spacing)));
        }
        if (candidate.surface.infinite && !candidate.background.isSpecial() &&
            candidate.background.format != PageTypeFormat::Plain &&
            candidate.background.format != PageTypeFormat::Lined &&
            candidate.background.format != PageTypeFormat::Graph)
            candidate.background = PageType(PageTypeFormat::Graph);
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(colorButton), &color);
        candidate.color = Color(static_cast<unsigned>(std::lround(color.red * 255)) << 16 |
                                static_cast<unsigned>(std::lround(color.green * 255)) << 8 |
                                static_cast<unsigned>(std::lround(color.blue * 255)));
        auto& s = candidate.surface;
        s.exportContent = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(content));
        s.crop = {gtk_spin_button_get_value(GTK_SPIN_BUTTON(x)), gtk_spin_button_get_value(GTK_SPIN_BUTTON(y)),
                  gtk_spin_button_get_value(GTK_SPIN_BUTTON(cw)), gtk_spin_button_get_value(GTK_SPIN_BUTTON(ch))};
        s.tiled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tiled));
        s.tileWidth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(tw));
        s.tileHeight = gtk_spin_button_get_value(GTK_SPIN_BUTTON(th));
        try {
            s.validate();
            if (!s.exportContent)
                s.tileCount(s.crop);
            state = candidate;
            accepted = true;
            break;
        } catch (const std::exception& e) {
            auto* error = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_OK, "%s", e.what());
            gtk_dialog_run(GTK_DIALOG(error));
            gtk_widget_destroy(error);
        }
    }
    gtk_widget_destroy(dialog);
    return accepted;
}
