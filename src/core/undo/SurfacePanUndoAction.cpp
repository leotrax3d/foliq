// SPDX-License-Identifier: GPL-2.0-or-later
#include "SurfacePanUndoAction.h"

#include "control/Control.h"
#include "control/zoom/ZoomControl.h"
#include "model/Document.h"
#include "model/XojPage.h"
bool SurfacePanUndoAction::undo(Control* control) {
    auto index = control->getDocument()->indexOf(page);
    if (index == npos)
        return false;
    if (zoom > 0) {
        auto* z = control->getZoomControl();
        double current = z->getZoom();
        control->getDocument()->unlock();
        z->setZoomFromSurfaceHistory(zoom);
        control->getDocument()->lock();
        zoom = current;
    }
    auto state = page->getSurface();
    std::swap(state.cameraX, x);
    std::swap(state.cameraY, y);
    page->setSurface(state);
    control->firePageChanged(index);
    return true;
}
bool SurfacePanUndoAction::merge(const UndoAction& next) {
    auto* p = dynamic_cast<const SurfacePanUndoAction*>(&next);
    if (!p || p->page != page || (zoom > 0) != (p->zoom > 0) || p->time - time > std::chrono::milliseconds(350))
        return false;
    time = p->time;
    return true;
}
