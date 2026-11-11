#include "StrokeViewHelper.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "model/LineStyle.h"
#include "model/Point.h"
#include "util/LoopUtil.h"
#include "util/PairView.h"
#include "util/Range.h"
#include "util/Util.h"  // for cairo_set_dash_from_vector

namespace {
bool needsClipping(cairo_t* cr, const std::vector<Point>& pts) {
    for (const auto& p: pts) {
        double x = p.x, y = p.y;
        cairo_user_to_device(cr, &x, &y);
        if (std::abs(x) > 16000 || std::abs(y) > 16000)
            return true;
    }
    return false;
}
Range clipBounds(cairo_t* cr, double padding = 0) {
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    return {x1 - padding, y1 - padding, x2 + padding, y2 + padding};
}
// Liang-Barsky, in double world coordinates, before Cairo's fixed-point rasterizer.
bool clippedSegment(const Point& p, const Point& q, const Range& r, double& lo, double& hi) {
    lo = 0;
    hi = 1;
    double dx = q.x - p.x, dy = q.y - p.y;
    auto boundary = [&](double direction, double distance) {
        if (direction == 0)
            return distance >= 0;
        double t = distance / direction;
        if (direction < 0)
            lo = std::max(lo, t);
        else
            hi = std::min(hi, t);
        return lo <= hi;
    };
    return boundary(-dx, p.x - r.minX) && boundary(dx, r.maxX - p.x) && boundary(-dy, p.y - r.minY) &&
           boundary(dy, r.maxY - p.y);
}
}  // namespace

void xoj::view::StrokeViewHelper::pathToCairo(cairo_t* cr, const std::vector<Point>& pts) {
    for_first_then_each(
            pts, [cr](auto const& first) { cairo_move_to(cr, first.x, first.y); },
            [cr](auto const& other) { cairo_line_to(cr, other.x, other.y); });
}

void xoj::view::StrokeViewHelper::filledPathToCairo(cairo_t* cr, const std::vector<Point>& pts) {
    if (!needsClipping(cr, pts)) {
        pathToCairo(cr, pts);
        return;
    }
    auto r = clipBounds(cr, 1);
    std::vector<Point> polygon = pts;
    for (int edge = 0; edge < 4 && !polygon.empty(); ++edge) {
        auto distance = [&](const Point& p) {
            return edge == 0 ? p.x - r.minX : edge == 1 ? r.maxX - p.x : edge == 2 ? p.y - r.minY : r.maxY - p.y;
        };
        std::vector<Point> output;
        Point previous = polygon.back();
        double previousDistance = distance(previous);
        for (const auto& current: polygon) {
            double d = distance(current);
            if ((d >= 0) != (previousDistance >= 0)) {
                double t = previousDistance / (previousDistance - d);
                output.emplace_back(previous.x + t * (current.x - previous.x),
                                    previous.y + t * (current.y - previous.y));
            }
            if (d >= 0)
                output.push_back(current);
            previous = current;
            previousDistance = d;
        }
        polygon = std::move(output);
    }
    pathToCairo(cr, polygon);
}

/**
 * No pressure sensitivity, one line is drawn
 */
void xoj::view::StrokeViewHelper::drawNoPressure(cairo_t* cr, const std::vector<Point>& pts, const double strokeWidth,
                                                 const LineStyle& lineStyle, double dashOffset) {
    cairo_set_line_width(cr, strokeWidth);

    const auto& dashes = lineStyle.getDashes();
    Util::cairo_set_dash_from_vector(cr, dashes, dashOffset);

    if (!needsClipping(cr, pts)) {
        pathToCairo(cr, pts);
        cairo_stroke(cr);
        return;
    }
    auto r = clipBounds(cr, strokeWidth * 2 + 2);
    for (const auto& [p, q]: PairView(pts)) {
        double lo, hi;
        double length = p.lineLengthTo(q);
        if (clippedSegment(p, q, r, lo, hi)) {
            Util::cairo_set_dash_from_vector(cr, dashes, dashOffset + lo * length);
            cairo_move_to(cr, p.x + lo * (q.x - p.x), p.y + lo * (q.y - p.y));
            cairo_line_to(cr, p.x + hi * (q.x - p.x), p.y + hi * (q.y - p.y));
            cairo_stroke(cr);
        }
        dashOffset += length;
    }
}

/**
 * Draw a stroke with pressure, for this multiple lines with different widths needs to be drawn
 */
double xoj::view::StrokeViewHelper::drawWithPressure(cairo_t* cr, const std::vector<Point>& pts,
                                                     const LineStyle& lineStyle, double dashOffset) {
    const auto& dashes = lineStyle.getDashes();

    // Clip per-segment before entering Cairo's fixed-point rasterizer. Preserve the
    // phase accumulated along skipped portions of dashed, pressure-sensitive paths.
    for (const auto& [p, q]: PairView(pts)) {
        assert(p.z > 0.0);
        double length = p.lineLengthTo(q), lo, hi;
        if (clippedSegment(p, q, clipBounds(cr, p.z * 2 + 2), lo, hi)) {
            Util::cairo_set_dash_from_vector(cr, dashes, dashOffset + lo * length);
            cairo_set_line_width(cr, p.z);
            cairo_move_to(cr, p.x + lo * (q.x - p.x), p.y + lo * (q.y - p.y));
            cairo_line_to(cr, p.x + hi * (q.x - p.x), p.y + hi * (q.y - p.y));
            cairo_stroke(cr);
        }
        dashOffset += length;
    }
    return dashOffset;
}
