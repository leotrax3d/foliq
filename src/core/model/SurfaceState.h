// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>

#include "XojPage.h"
struct SurfaceState {
    PageSurface surface;
    double width, height;
    PageType background;
    Color color;
    explicit SurfaceState(const XojPage& p):
            surface(p.getSurface()),
            width(p.getWidth()),
            height(p.getHeight()),
            background(p.getBackgroundType()),
            color(p.getBackgroundColor()) {}
    void swapWith(XojPage& p) {
        SurfaceState current(p);
        apply(p);
        *this = std::move(current);
    }
    void apply(XojPage& p) const {
        surface.validate();
        if (!std::isfinite(width) || !std::isfinite(height) || width < 1 || height < 1 || width > 14400 ||
            height > 14400)
            throw std::invalid_argument("Paper/window dimensions must be between 1 and 14400 pt");
        p.setSurface(surface);
        p.setSize(width, height);
        p.setBackgroundType(background);
        p.setBackgroundColor(color);
    }
};

inline std::shared_ptr<XojPage> convertedSurfaceCopy(const XojPage& source, SurfaceState destination) {
    auto copy = std::make_shared<XojPage>(source);
    if (source.isInfinite() && !destination.surface.infinite) {
        for (auto* layer: *copy->getLayers())
            for (auto* element: layer->getElements())
                element->move(-source.getSurface().cameraX, -source.getSurface().cameraY);
        destination.surface.cameraX = destination.surface.cameraY = 0;
    }
    destination.apply(*copy);
    return copy;
}
