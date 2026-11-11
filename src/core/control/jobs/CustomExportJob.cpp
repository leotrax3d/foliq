#include "CustomExportJob.h"

#include <memory>   // for unique_ptr
#include <utility>  // for move, pair

#include <gtk/gtk.h>  // for GTK_WINDOW

#include "control/Control.h"                   // for Control
#include "control/jobs/BaseExportJob.h"        // for BaseExportJob::ExportType
#include "control/xojfile/XojExportHandler.h"  // for XojExportHandler
#include "gui/MainWindow.h"                    // for MainWindow
#include "gui/dialog/ExportDialog.h"           // for ExportDialog
#include "model/Document.h"
#include "model/Image.h"
#include "model/Text.h"
#include "model/XojPage.h"                 // for Document
#include "pdf/base/XojPdfExport.h"         // for XojPdfExport
#include "pdf/base/XojPdfExportFactory.h"  // for XojPdfExportFactory
#include "util/PathUtil.h"                 // for clearExtensions
#include "util/XojMsgBox.h"                // for XojMsgBox
#include "util/i18n.h"                     // for _, FS, _F

#include "ImageExport.h"  // for ImageExport, EXPORT_GR...
#include "SaveJob.h"      // for SaveJob


CustomExportJob::CustomExportJob(Control* control): BaseExportJob(control, _("Custom Export")) {
    // Supported filters
    filters.insert({_("PDF files"), ExportType(".pdf")});
    filters.insert({_("PNG graphics"), ExportType(".png")});
    filters.insert({_("SVG graphics"), ExportType(".svg")});
    filters.insert({_("Xournal (Compatibility)"), ExportType(".xoj")});
}

CustomExportJob::~CustomExportJob() = default;

void CustomExportJob::addFilterToDialog() {
    // Runs on every filter inside the filters map
    for (auto& filter: filters) {
        addFileFilterToDialog(filter.first, "*" + filter.second.extension);  // Adds * for the pattern
    }
}

auto CustomExportJob::testAndSetFilepath(const fs::path& file) -> bool {
    if (!BaseExportJob::testAndSetFilepath(std::move(file))) {
        return false;
    }

    // Extract the file filter selected
    this->chosenFilterName = BaseExportJob::getFilterName();
    const auto& chosenFilter = filters.at(this->chosenFilterName);

    // Remove any pre-existing extension and adds the chosen one
    Util::clearExtensions(filepath, chosenFilter.extension);
    filepath += chosenFilter.extension;

    return checkOverwriteBackgroundPDF(filepath);
}

auto CustomExportJob::showFilechooser() -> bool {
    if (!BaseExportJob::showFilechooser()) {
        return false;
    }

    if (filepath.extension() == ".xoj") {
        exportTypeXoj = true;
        return true;
    }

    Document* doc = control->getDocument();
    doc->lock();
    ExportDialog dlg(control->getGladeSearchPath());
    if (filepath.extension() == ".pdf") {
        dlg.showProgressiveMode();
        format = EXPORT_GRAPHICS_PDF;
    } else if (filepath.extension() == ".svg") {
        dlg.removeQualitySetting();
        format = EXPORT_GRAPHICS_SVG;
    } else if (filepath.extension() == ".png") {
        format = EXPORT_GRAPHICS_PNG;
    }

    dlg.initPages(control->getCurrentPageNo() + 1, doc->getPageCount());

    dlg.show(GTK_WINDOW(control->getWindow()->getWindow()));

    if (!dlg.isConfirmed()) {
        doc->unlock();
        return false;
    }

    exportRange = dlg.getRange();
    progressiveMode = dlg.progressiveMode();
    exportBackground = dlg.getBackgroundType();

    if (format == EXPORT_GRAPHICS_PNG) {
        pngQualityParameter = dlg.getPngQualityParameter();
    }

    doc->unlock();
    return true;
}

/**
 * Create one Graphics file per page
 */
void CustomExportJob::exportGraphics() {
    ImageExport imgExport(control->getDocument(), filepath, format, exportBackground, exportRange);
    if (format == EXPORT_GRAPHICS_PNG) {
        imgExport.setQualityParameter(pngQualityParameter);
    }
    imgExport.exportGraphics(control);
    errorMsg = imgExport.getLastErrorMsg();
}

void CustomExportJob::run() {
    if (exportTypeXoj) {
        SaveJob::updatePreview(control);
        Document* doc = this->control->getDocument();

        // Legacy Xournal cannot round-trip rich content. Refuse before opening the target.
        for (size_t p = 0; p < doc->getPageCount(); ++p) {
            for (auto* layer: *doc->getPage(p)->getLayers())
                for (auto* element: layer->getElements()) {
                    bool rich = false;
                    if (auto* text = dynamic_cast<Text*>(element))
                        rich = !text->getRichText().spans.empty() || text->getRichText().columns;
                    if (auto* image = dynamic_cast<Image*>(element)) {
                        auto crop = image->getCrop();
                        rich = crop.left != 0 || crop.top != 0 || crop.right != 1 || crop.bottom != 1;
                    }
                    if (rich) {
                        this->lastError =
                                _("This document contains rich text, tables or image crops that legacy Xournal cannot "
                                  "preserve. Save as XOPP for editing, or export as PDF, SVG or PNG.");
                        callAfterRun();
                        return;
                    }
                }
        }
        XojExportHandler h;
        doc->lock();
        h.prepareSave(doc);
        h.saveTo(filepath, this->control);
        doc->unlock();

        if (!h.getErrorMessage().empty()) {
            this->lastError = FS(_F("Save file error: {1}") % h.getErrorMessage());

            callAfterRun();
        }
    } else if (format == EXPORT_GRAPHICS_PDF) {
        // don't lock the page here for the whole flow, else we get a dead lock...
        // the ui is blocked, so there should be no changes...
        Document* doc = control->getDocument();

        std::unique_ptr<XojPdfExport> pdfe = XojPdfExportFactory::createExport(doc, control);

        pdfe->setExportBackground(exportBackground);

        if (!pdfe->createPdf(this->filepath, exportRange, progressiveMode)) {
            this->errorMsg = pdfe->getLastError();
        }

    } else {
        exportGraphics();
    }
}

void CustomExportJob::afterRun() {
    if (!this->lastError.empty()) {
        XojMsgBox::showErrorToUser(control->getGtkWindow(), this->lastError);
    }
}
