// SPDX-License-Identifier: GPL-2.0-or-later
#include "SurfaceChangeUndoAction.h"

#include "control/Control.h"
#include "model/Document.h"
bool SurfaceChangeUndoAction::undo(Control* control) {
    auto index = control->getDocument()->indexOf(page);
    if (index == npos)
        return false;
    other.swapWith(*page);
    control->firePageSizeChanged(index);
    control->firePageChanged(index);
    return true;
}
