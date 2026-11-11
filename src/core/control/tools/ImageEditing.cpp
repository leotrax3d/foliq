// Foliq — GPL-2.0-or-later
#include <array>

#include "control/Control.h"
#include "control/tools/EditSelection.h"
#include "gui/MainWindow.h"
#include "gui/XournalView.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/XojPage.h"
#include "undo/TextBoxUndoAction.h"
#include "undo/UndoRedoHandler.h"
#include "util/XojMsgBox.h"

#include "ImageHandler.h"

void ImageHandler::editSelectedImage(Control* control) {
    auto* selection = control->getWindow()->getXournal()->getSelection();
    if (!selection || selection->getElements().size() != 1 ||
        selection->getElements().front()->getType() != ELEMENT_IMAGE) {
        XojMsgBox::showErrorToUser(control->getGtkWindow(), "Zuerst genau ein Bild mit dem Auswahlwerkzeug auswählen.");
        return;
    }
    auto* original = static_cast<Image*>(selection->getElements().front());
    auto page = selection->getSourcePage();
    auto* layer = selection->getSourceLayer();
    auto crop = original->getCrop();
    auto* dlg = gtk_dialog_new_with_buttons("Bildgröße und Zuschnitt", control->getGtkWindow(), GTK_DIALOG_MODAL,
                                            "Abbrechen", GTK_RESPONSE_CANCEL, "Übernehmen", GTK_RESPONSE_OK, nullptr);
    auto* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 16);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), grid, true, true, 0);
    std::array<const char*, 5> names{"Breite (Seitenpunkte)", "Links (%)", "Oben (%)", "Rechts (%)", "Unten (%)"};
    std::array<double, 5> values{original->getElementWidth(), crop.left * 100, crop.top * 100, crop.right * 100,
                                 crop.bottom * 100};
    std::array<GtkWidget*, 5> fields;
    for (size_t i = 0; i < names.size(); ++i) {
        auto* label = gtk_label_new(names[i]);
        fields[i] = gtk_spin_button_new_with_range(i ? 0 : 1, i ? 100 : 10000, .1);
        atk_object_set_name(gtk_widget_get_accessible(fields[i]), names[i]);
        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(fields[i]), 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields[i]), values[i]);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), fields[i]);
        gtk_grid_attach(GTK_GRID(grid), label, 0, static_cast<int>(i), 1, 1);
        gtk_grid_attach(GTK_GRID(grid), fields[i], 1, static_cast<int>(i), 1, 1);
    }
    gtk_grid_attach(GTK_GRID(grid),
                    gtk_label_new("Zuschnitt relativ zum Original. 0/0/100/100 stellt es wieder her.\nSeitenverhältnis "
                                  "des sichtbaren Ausschnitts bleibt erhalten; Position bleibt gleich."),
                    0, 5, 2, 1);
    gtk_widget_show_all(dlg);
    while (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        for (auto* field: fields) gtk_spin_button_update(GTK_SPIN_BUTTON(field));
        auto replacement = std::unique_ptr<Image>(static_cast<Image*>(original->clone()));
        Image::Crop next{gtk_spin_button_get_value(GTK_SPIN_BUTTON(fields[1])) / 100,
                         gtk_spin_button_get_value(GTK_SPIN_BUTTON(fields[2])) / 100,
                         gtk_spin_button_get_value(GTK_SPIN_BUTTON(fields[3])) / 100,
                         gtk_spin_button_get_value(GTK_SPIN_BUTTON(fields[4])) / 100};
        if (!replacement->setCrop(next)) {
            XojMsgBox::showErrorToUser(GTK_WINDOW(dlg), "Der rechte/untere Rand muss hinter dem linken/oberen liegen. "
                                                        "Der Ausschnitt muss mindestens 0,1 % groß sein.");
            continue;
        }
        double width = gtk_spin_button_get_value(GTK_SPIN_BUTTON(fields[0]));
        double ratio = (original->getElementWidth() / original->getElementHeight()) *
                       ((next.right - next.left) / (crop.right - crop.left)) /
                       ((next.bottom - next.top) / (crop.bottom - crop.top));
        replacement->setWidth(width);
        replacement->setHeight(width / ratio);
        if (next.left == crop.left && next.top == crop.top && next.right == crop.right && next.bottom == crop.bottom &&
            width == original->getElementWidth())
            break;
        control->clearSelectionEndText();
        auto* doc = control->getDocument();
        doc->lock();
        auto index = layer->removeElement(original, false);
        auto* changed = replacement.release();
        layer->insertElement(changed, index);
        doc->unlock();
        page->fireElementChanged(changed);
        page->firePageChanged();
        control->getUndoRedoHandler()->addUndoAction(
                std::make_unique<TextBoxUndoAction>(page, layer, changed, original));
        break;
    }
    gtk_widget_destroy(dlg);
}
