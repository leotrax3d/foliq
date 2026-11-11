// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model/SurfaceState.h"

#include "UndoAction.h"
class SurfaceChangeUndoAction: public UndoAction {
public:
    SurfaceChangeUndoAction(const PageRef& page, SurfaceState before):
            UndoAction("SurfaceChangeUndoAction"), other(std::move(before)) {
        this->page = page;
    }
    bool undo(Control* control) override;
    bool redo(Control* control) override { return undo(control); }
    std::string getText() override { return "Page surface settings"; }

private:
    SurfaceState other;
};
