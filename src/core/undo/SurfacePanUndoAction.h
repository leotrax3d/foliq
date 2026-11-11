// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>

#include "UndoAction.h"
class SurfacePanUndoAction: public UndoAction {
public:
    SurfacePanUndoAction(const PageRef& p, double x, double y, double zoom = 0):
            UndoAction("SurfacePanUndoAction"), x(x), y(y), zoom(zoom), time(std::chrono::steady_clock::now()) {
        page = p;
    }
    bool undo(Control* control) override;
    bool redo(Control* control) override { return undo(control); }
    std::string getText() override { return zoom > 0 ? "Zoom free surface" : "Pan free surface"; }
    bool merge(const UndoAction& next) override;

private:
    double x, y, zoom;
    std::chrono::steady_clock::time_point time;
};
