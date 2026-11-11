// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstddef>

// Width/height on a free page describe its navigable window, never world bounds.
struct SurfaceRect {
    double x = 0, y = 0, width = 595.276, height = 841.89;
};
struct PageSurface {
    static constexpr int VERSION = 1;
    // Keeps existing GTK integer window coordinates safe at supported zoom levels.
    static constexpr double COORDINATE_LIMIT = 1000000;
    bool infinite = false;
    double cameraX = 0, cameraY = 0;
    bool exportContent = true;
    SurfaceRect crop;
    bool tiled = true;
    double tileWidth = 595.276, tileHeight = 841.89;

    static double worldCoordinate(double screen, double viewportOrigin, double camera, double zoom) {
        if (!std::isfinite(zoom) || zoom <= 0)
            throw std::invalid_argument("Invalid zoom");
        return camera + (screen - viewportOrigin) / zoom;
    }
    static double zoomedCamera(double anchorWorld, double anchorScreen, double viewportOrigin, double newZoom) {
        return anchorWorld - worldCoordinate(anchorScreen, viewportOrigin, 0, newZoom);
    }
    void validate() const {
        auto coordinate = [](double v) { return std::isfinite(v) && std::abs(v) <= COORDINATE_LIMIT; };
        auto length = [](double v) { return std::isfinite(v) && v >= 1 && v <= 14400; };
        if (!coordinate(cameraX) || !coordinate(cameraY) || !coordinate(crop.x) || !coordinate(crop.y) ||
            !std::isfinite(crop.width) || !std::isfinite(crop.height) || crop.width < 1 || crop.height < 1 ||
            !coordinate(crop.x + crop.width) || !coordinate(crop.y + crop.height) || !length(tileWidth) ||
            !length(tileHeight)) {
            throw std::invalid_argument("Invalid Foliq surface coordinates or PDF dimensions");
        }
    }
    // Lazy page addressing avoids allocating a list proportional to empty world space.
    size_t tileCount(const SurfaceRect& area) const {
        validate();
        if (!std::isfinite(area.x) || !std::isfinite(area.y) || !std::isfinite(area.width) ||
            !std::isfinite(area.height) || area.width <= 0 || area.height <= 0)
            throw std::invalid_argument("Invalid PDF crop");
        double n = tiled ? std::ceil(area.width / tileWidth) * std::ceil(area.height / tileHeight) : 1;
        if (!std::isfinite(n) || n > 10000 || (!tiled && (area.width > 14400 || area.height > 14400)))
            throw std::invalid_argument(
                    "PDF crop exceeds 10000 tiles or 14400 pt single-page limit; choose a smaller crop");
        return static_cast<size_t>(n);
    }
    SurfaceRect tile(const SurfaceRect& area, size_t index) const {
        if (index >= tileCount(area))
            throw std::out_of_range("PDF tile");
        if (!tiled)
            return area;
        auto columns = static_cast<size_t>(std::ceil(area.width / tileWidth));
        double dx = static_cast<double>(index % columns) * tileWidth,
               dy = static_cast<double>(index / columns) * tileHeight;
        return {area.x + dx, area.y + dy, std::min(tileWidth, area.width - dx), std::min(tileHeight, area.height - dy)};
    }
};
