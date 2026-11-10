#include "folio/Storage.h"
#include "util/OutputStream.h"
#include "SaveJob.h"

#include <cmath>   // for ceil
#include <memory>  // for __shared_ptr_access

#include <cairo.h>  // for cairo_create, cairo_destroy
#include <glib.h>   // for g_warning, g_error

#include "control/Control.h"              // for Control
#include "control/jobs/BlockingJob.h"     // for BlockingJob
#include "control/xojfile/SaveHandler.h"  // for SaveHandler
#include "model/Document.h"               // for Document
#include "model/PageRef.h"                // for PageRef
#include "model/PageType.h"               // for PageType
#include "model/XojPage.h"                // for XojPage
#include "pdf/base/XojPdfPage.h"          // for XojPdfPageSPtr, XojPdfPage
#include "util/PathUtil.h"                // for clearExtensions, safeRename...
#include "util/XojMsgBox.h"               // for XojMsgBox
#include "util/i18n.h"                    // for FS, _, _F
#include "view/DocumentView.h"            // for DocumentView

#include "filesystem.h"  // for path, filesystem_error, remove


SaveJob::SaveJob(Control* control): BlockingJob(control, _("Save")) {}

SaveJob::~SaveJob() = default;

void SaveJob::run() {
    save();

    if (this->control->getWindow()) {
        callAfterRun();
    }
}

void SaveJob::afterRun() {
    if (!this->lastError.empty()) {
        XojMsgBox::showErrorToUser(control->getGtkWindow(), this->lastError);
    } else {
        this->control->resetSavedStatus();
    }
}

void SaveJob::updatePreview(Control* control) {
    const int previewSize = 128;

    Document* doc = control->getDocument();

    doc->lock();

    if (doc->getPageCount() > 0) {
        PageRef page = doc->getPage(0);

        double width = page->getWidth();
        double height = page->getHeight();

        double zoom = 1;

        if (width < height) {
            zoom = previewSize / height;
        } else {
            zoom = previewSize / width;
        }
        width *= zoom;
        height *= zoom;

        cairo_surface_t* crBuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(std::ceil(width)),
                                                               static_cast<int>(std::ceil(height)));

        cairo_t* cr = cairo_create(crBuffer);
        cairo_scale(cr, zoom, zoom);

        // We don't have access to a PdfCache on which DocumentView relies for PDF backgrounds.
        // We thus print the PDF background by hand.
        if (page->getBackgroundType().isPdfPage()) {
            auto pgNo = page->getPdfPageNr();
            XojPdfPageSPtr popplerPage = doc->getPdfPage(pgNo);
            if (popplerPage) {
                popplerPage->render(cr);
            }
        }

        DocumentView view;
        view.drawPage(page, cr, true /* don't render erasable */, true /* Don't rerender the pdf background */);
        cairo_destroy(cr);
        doc->setPreview(crBuffer);
        cairo_surface_destroy(crBuffer);
    } else {
        doc->setPreview(nullptr);
    }

    doc->unlock();
}

auto SaveJob::save() -> bool {
    updatePreview(control);
    Document* doc = this->control->getDocument();
    SaveHandler h;

    doc->lock();
    h.prepareSave(doc);
    fs::path target = doc->getFilepath();
    doc->unlock();

    Util::safeReplaceExtension(target, "xopp");

    // Keep the original in place until the entire gzip stream is closed successfully.
    fs::path stage = fs::path(target).concat(".saving");
    try {
        foliq::FileLock lock(fs::path(target).concat(".lock"));
        foliq::checkDocument(target);
        doc->lock();
        try {
            GzOutputStream out(stage);
            h.saveTo(&out, target, this->control);
            out.close();
            if (!out.getLastError().empty()) throw std::runtime_error(out.getLastError());
            if (!h.getErrorMessage().empty()) throw std::runtime_error(h.getErrorMessage());
        } catch (...) { doc->unlock(); throw; }
        doc->unlock();
        foliq::atomicWrite(target, foliq::read(stage));
        foliq::acceptDocument(target);
        doc->setFilepath(target);
        doc->setCreateBackupOnSave(true);
        fs::remove(stage);
    } catch (const std::exception& e) {
        this->lastError = std::string("Speichern fehlgeschlagen: ") + e.what();
        return false;
    }

    return true;
}
