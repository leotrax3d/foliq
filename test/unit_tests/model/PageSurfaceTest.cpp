// SPDX-License-Identifier: GPL-2.0-or-later
#include <cmath>
#include <fstream>
#include <limits>

#include <gtest/gtest.h>
#include <poppler.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Stroke.h"
#include "model/SurfaceState.h"
#include "pdf/base/XojCairoPdfExport.h"
#include "undo/SurfacePanUndoAction.h"
#include "util/PathUtil.h"
#include "view/DocumentView.h"

namespace {
PageRef samplePage() {
    auto p = std::make_shared<XojPage>(595.276, 841.89);
    p->setBackgroundType(PageType(PageTypeFormat::Plain));
    PageSurface state;
    state.infinite = true;
    state.cameraX = -500;
    state.cameraY = -600;
    p->setSurface(state);
    auto* stroke = new Stroke();
    stroke->setWidth(4);
    stroke->setColor(Colors::black);
    stroke->addPoint(Point(-100, -200, 1));
    stroke->addPoint(Point(-80, -180, 3));
    stroke->addPoint(Point(-60, -160));
    p->getSelectedLayer()->addElement(stroke);
    return p;
}
fs::path tempFile(const char* name) { return Util::getTmpDirSubfolder() / name; }
}  // namespace

TEST(PageSurface, ZoomAnchorsInAllQuadrants) {
    for (double camera: {-900000., -321.75, 0., 952.125, 900000.}) {
        for (double zoom: {.05, .5, 1., 3., 20.}) {
            double world = PageSurface::worldCoordinate(412.25, 10, camera, zoom);
            auto next = PageSurface::zoomedCamera(world, 412.25, 10, zoom * 1.25);
            EXPECT_NEAR(PageSurface::worldCoordinate(412.25, 10, next, zoom * 1.25), world, 1e-8);
        }
    }
    EXPECT_THROW(PageSurface::worldCoordinate(0, 0, 0, 0), std::invalid_argument);
}

TEST(PageSurface, ValidatesFiniteValuesAndLimits) {
    PageSurface s;
    s.cameraX = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(s.validate(), std::invalid_argument);
    s.cameraX = 1000001;
    EXPECT_THROW(s.validate(), std::invalid_argument);
    s.cameraX = -1000000;
    EXPECT_NO_THROW(s.validate());
    s.tileWidth = 0;
    EXPECT_THROW(s.validate(), std::invalid_argument);
}

TEST(PageSurface, LazyTilesCoverNegativeCropWithoutGaps) {
    PageSurface s;
    s.tileWidth = 100;
    s.tileHeight = 70;
    SurfaceRect area{-250, -100, 250, 141};
    ASSERT_EQ(s.tileCount(area), 9);
    double total = 0;
    for (size_t i = 0; i < 9; ++i) {
        auto t = s.tile(area, i);
        total += t.width * t.height;
        EXPECT_GE(t.x, area.x);
        EXPECT_GE(t.y, area.y);
        EXPECT_LE(t.x + t.width, area.x + area.width);
        EXPECT_LE(t.y + t.height, area.y + area.height);
    }
    EXPECT_DOUBLE_EQ(total, area.width * area.height);
    EXPECT_THROW(s.tile(area, 9), std::out_of_range);
    EXPECT_THROW(s.tileCount({-1000000, -1000000, 2000000, 2000000}), std::invalid_argument);
    s.tiled = false;
    EXPECT_EQ(s.tileCount(area), 1);
    EXPECT_THROW(s.tileCount({0, 0, 14401, 2}), std::invalid_argument);
}

TEST(PageSurface, ConversionPreservesOriginalPressureAndOverflow) {
    auto original = samplePage();
    SurfaceState paper(*original);
    paper.surface.infinite = false;
    paper.width = 100;
    paper.height = 100;
    auto copy = convertedSurfaceCopy(*original, paper);
    auto* a = static_cast<Stroke*>(original->getSelectedLayer()->getElements()[0]);
    auto* b = static_cast<Stroke*>(copy->getSelectedLayer()->getElements()[0]);
    ASSERT_NE(a, b);
    EXPECT_DOUBLE_EQ(a->getPoint(0).x, -100);
    EXPECT_DOUBLE_EQ(b->getPoint(0).x, 400);
    EXPECT_DOUBLE_EQ(b->getPoint(0).y, 400);
    EXPECT_DOUBLE_EQ(b->getPoint(1).z, a->getPoint(1).z);
    EXPECT_GT(b->getPoint(0).x, copy->getWidth());
    EXPECT_TRUE(original->isInfinite());
    EXPECT_FALSE(copy->isInfinite());
    SurfaceState back(*copy);
    back.surface.infinite = true;
    auto again = convertedSurfaceCopy(*copy, back);
    EXPECT_EQ(again->getSelectedLayer()->getElements().size(), 1);
    EXPECT_DOUBLE_EQ(static_cast<Stroke*>(again->getSelectedLayer()->getElements()[0])->getPoint(0).x, 400);
}

TEST(PageSurface, SettingsUndoRedoSwapsCompleteState) {
    auto p = samplePage();
    SurfaceState before(*p);
    auto next = before;
    next.width = 300;
    next.height = 200;
    next.surface.cameraX = 120;
    next.surface.tiled = false;
    next.surface.crop = {-50, -70, 500, 300};
    next.background = PageType(PageTypeFormat::Graph);
    next.background.config = "r1=25";
    next.color = Color(0x123456);
    next.apply(*p);
    before.swapWith(*p);
    EXPECT_DOUBLE_EQ(p->getWidth(), 595.276);
    EXPECT_DOUBLE_EQ(p->getSurface().cameraX, -500);
    before.swapWith(*p);
    EXPECT_DOUBLE_EQ(p->getWidth(), 300);
    EXPECT_EQ(p->getBackgroundColor(), Color(0x123456));
    EXPECT_DOUBLE_EQ(p->getSurface().crop.x, -50);
    EXPECT_EQ(p->getBackgroundType().config, "r1=25");
    EXPECT_FALSE(p->getSurface().tiled);
}

TEST(PageSurface, PanHistoryMergesOnlySamePage) {
    auto a = samplePage(), b = samplePage();
    SurfacePanUndoAction first(a, 0, 0), same(a, 1, 2), other(b, 1, 2);
    EXPECT_TRUE(first.merge(same));
    EXPECT_FALSE(first.merge(other));
}

TEST(PageSurface, SaveLoadRoundtripAndLegacyMigration) {
    Document doc(nullptr);
    auto p = samplePage();
    doc.addPage(p);
    auto s = p->getSurface();
    s.crop = {-500, -100, 1000, 2000};
    s.exportContent = false;
    s.tileWidth = 300;
    p->setSurface(s);
    auto path = tempFile("surface-roundtrip.xopp");
    SaveHandler save;
    save.prepareSave(&doc);
    save.saveTo(path);
    ASSERT_TRUE(save.getErrorMessage().empty()) << save.getErrorMessage();
    LoadHandler load;
    auto* restored = load.loadDocument(path);
    ASSERT_NE(restored, nullptr);
    auto q = restored->getPage(0);
    ASSERT_TRUE(q->isInfinite());
    EXPECT_DOUBLE_EQ(q->getSurface().cameraX, -500);
    EXPECT_DOUBLE_EQ(q->getSurface().crop.y, -100);
    EXPECT_DOUBLE_EQ(q->getSurface().tileWidth, 300);
    EXPECT_FALSE(q->getSurface().exportContent);
    auto* stroke = static_cast<Stroke*>(q->getSelectedLayer()->getElements()[0]);
    EXPECT_DOUBLE_EQ(stroke->getPoint(0).x, -100);
    EXPECT_DOUBLE_EQ(stroke->getPoint(1).z, 3);
    auto paper = std::make_shared<XojPage>(300, 500);
    Document legacy(nullptr);
    legacy.addPage(paper);
    save.prepareSave(&legacy);
    save.saveTo(path);
    LoadHandler old;
    auto* migrated = old.loadDocument(path);
    ASSERT_NE(migrated, nullptr);
    EXPECT_FALSE(migrated->getPage(0)->isInfinite());
    fs::remove(path);
}

TEST(PageSurface, UnknownSurfaceVersionAndCorruptionAreRejected) {
    for (const char* version: {"2", "99", "0", "broken"}) {
        auto path = tempFile("surface-newer.xopp");
        std::ofstream out(path);
        out << "<?xml version=\"1.0\"?><xournal creator=\"Foliq\" fileversion=\"4\"><page width=\"500\" height=\"700\" "
               "foliq-surface-version=\""
            << version
            << "\"><background type=\"solid\" color=\"#ffffffff\" style=\"plain\"/><layer/></page></xournal>";
        out.close();
        LoadHandler load;
        EXPECT_EQ(load.loadDocument(path), nullptr);
        fs::remove(path);
    }
}

TEST(PageSurface, BoundedRenderingAtDistantAndNegativeCoordinates) {
    auto p = samplePage();
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    auto* cr = cairo_create(surface);
    // Original negative stroke becomes visible inside a bounded target.
    cairo_translate(cr, 110, 210);
    DocumentView view;
    view.drawPage(p, cr, true);
    EXPECT_EQ(cairo_status(cr), CAIRO_STATUS_SUCCESS);
    cairo_surface_flush(surface);
    auto* bytes = cairo_image_surface_get_data(surface);
    bool nonWhite = false;
    for (int i = 0; i < 64 * 64 * 4; i += 4) nonWhite |= bytes[i] < 200;
    EXPECT_TRUE(nonWhite);
    // Filled translucent shape spanning huge empty space must allocate only a clip-sized mask.
    auto* huge = new Stroke();
    huge->setColor(Colors::black);
    huge->setToolType(StrokeTool::HIGHLIGHTER);
    huge->setFill(120);
    huge->setWidth(2);
    huge->addPoint(Point(-900000, -900000));
    huge->addPoint(Point(900000, 900000));
    huge->addPoint(Point(-900000, 900000));
    huge->addPoint(Point(-900000, -900000));
    p->getSelectedLayer()->addElement(huge);
    cairo_identity_matrix(cr);
    cairo_translate(cr, -500000, -500000);
    view.drawPage(p, cr, true);
    EXPECT_EQ(cairo_status(cr), CAIRO_STATUS_SUCCESS);
    cairo_surface_flush(surface);
    EXPECT_LT(cairo_image_surface_get_data(surface)[(20 * 64 + 5) * 4], 200);
    EXPECT_EQ(cairo_image_surface_get_width(surface), 64);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

TEST(PageSurface, PdfExportTilingIncludesNegativeCoordinates) {
    Document doc(nullptr);
    auto p = samplePage();
    doc.addPage(p);
    auto s = p->getSurface();
    s.exportContent = false;
    s.crop = {-120, -220, 250, 150};
    s.tileWidth = 100;
    s.tileHeight = 100;
    p->setSurface(s);
    auto path = tempFile("surface-tiled.pdf");
    XojCairoPdfExport pdf(&doc, nullptr);
    ASSERT_TRUE(pdf.createPdf(path, false)) << pdf.getLastError();
    GError* error = nullptr;
    auto uri = std::string("file://") + path.string();
    auto* result = poppler_document_new_from_file(uri.c_str(), nullptr, &error);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(poppler_document_get_n_pages(result), 6);
    auto* first = poppler_document_get_page(result, 0);
    double w, h;
    poppler_page_get_size(first, &w, &h);
    EXPECT_DOUBLE_EQ(w, 100);
    EXPECT_DOUBLE_EQ(h, 100);
    auto* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
    auto* cr = cairo_create(target);
    poppler_page_render(first, cr);
    cairo_surface_flush(target);
    auto* data = cairo_image_surface_get_data(target);
    bool marked = false;
    for (int i = 0; i < 100 * 100 * 4; i += 4) marked |= data[i] < 100;
    EXPECT_TRUE(marked);
    cairo_destroy(cr);
    cairo_surface_destroy(target);
    g_object_unref(first);
    auto* last = poppler_document_get_page(result, 5);
    poppler_page_get_size(last, &w, &h);
    EXPECT_DOUBLE_EQ(w, 50);
    EXPECT_DOUBLE_EQ(h, 50);
    g_object_unref(last);
    g_object_unref(result);
    fs::remove(path);
}

TEST(PageSurface, PdfPreflightDoesNotOverwriteDestinationOnExcessiveTiles) {
    Document doc(nullptr);
    auto p = samplePage();
    doc.addPage(p);
    auto s = p->getSurface();
    s.exportContent = false;
    s.crop = {-900000, -900000, 1800000, 1800000};
    p->setSurface(s);
    auto path = tempFile("surface-protected.pdf");
    {
        std::ofstream out(path);
        out << "original";
    }
    XojCairoPdfExport pdf(&doc, nullptr);
    EXPECT_FALSE(pdf.createPdf(path, false));
    std::ifstream in(path);
    std::string value;
    in >> value;
    EXPECT_EQ(value, "original");
    fs::remove(path);
}

TEST(PageSurface, StrictSurfaceMetadataValidationPreservesInput) {
    const std::string attributes =
            " foliq-surface-version=\"1\" foliq-camera-x=\"-500\" foliq-camera-y=\"-600\" foliq-export-content=\"1\" "
            "foliq-crop-x=\"-500\" foliq-crop-y=\"-600\" foliq-crop-width=\"700\" foliq-crop-height=\"700\" "
            "foliq-tiled=\"1\" foliq-tile-width=\"350\" foliq-tile-height=\"350\"";
    for (auto change: {std::make_pair("foliq-camera-x=\"-500\"", "foliq-camera-x=\"nan\""),
                       std::make_pair("foliq-camera-y=\"-600\"", "foliq-camera-y=\"3px\""),
                       std::make_pair("foliq-tiled=\"1\"", "foliq-tiled=\"2\""),
                       std::make_pair("foliq-tile-width=\"350\"", "foliq-tile-width=\"0\""),
                       std::make_pair("foliq-crop-height=\"700\"", "")}) {
        auto a = attributes;
        a.replace(a.find(change.first), std::string(change.first).size(), change.second);
        auto payload = "<?xml version=\"1.0\"?><xournal creator=\"Foliq\" fileversion=\"4\"><page width=\"500\" "
                       "height=\"700\"" +
                       a + "><background type=\"solid\" color=\"#ffffffff\" style=\"plain\"/><layer/></page></xournal>";
        auto path = tempFile("surface-invalid.xopp");
        {
            std::ofstream out(path);
            out << payload;
        }
        LoadHandler load;
        EXPECT_EQ(load.loadDocument(path), nullptr);
        std::ifstream in(path);
        std::string after((std::istreambuf_iterator<char>(in)), {});
        EXPECT_EQ(after, payload);
        fs::remove(path);
    }
}

TEST(PageSurface, SingleCropAndMixedPaperPdfExport) {
    Document doc(nullptr);
    auto p = samplePage();
    doc.addPage(p);
    auto paper = std::make_shared<XojPage>(300, 400);
    doc.addPage(paper);
    auto s = p->getSurface();
    s.tiled = false;
    s.exportContent = false;
    s.crop = {-120, -220, 80, 90};
    p->setSurface(s);
    auto path = tempFile("surface-mixed.pdf");
    XojCairoPdfExport pdf(&doc, nullptr);
    ASSERT_TRUE(pdf.createPdf(path, false));
    GError* error = nullptr;
    auto uri = std::string("file://") + path.string();
    auto* result = poppler_document_new_from_file(uri.c_str(), nullptr, &error);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(poppler_document_get_n_pages(result), 2);
    auto* first = poppler_document_get_page(result, 0);
    double w, h;
    poppler_page_get_size(first, &w, &h);
    EXPECT_DOUBLE_EQ(w, 80);
    EXPECT_DOUBLE_EQ(h, 90);
    g_object_unref(first);
    auto* second = poppler_document_get_page(result, 1);
    poppler_page_get_size(second, &w, &h);
    EXPECT_DOUBLE_EQ(w, 300);
    EXPECT_DOUBLE_EQ(h, 400);
    g_object_unref(second);
    g_object_unref(result);
    fs::remove(path);
}

TEST(PageSurface, NegativeOnlyStrokeBoundsDoNotExpandToOrigin) {
    auto p = samplePage();
    auto bounds = p->getContentBounds();
    EXPECT_LT(bounds.x + bounds.width, 0);
    EXPECT_LT(bounds.y + bounds.height, 0);
    EXPECT_LT(bounds.width, 60);
    EXPECT_LT(bounds.height, 60);
    auto* stroke = static_cast<Stroke*>(p->getSelectedLayer()->getElements()[0]);
    auto snapping = stroke->getSnappedBounds();
    EXPECT_LT(snapping.x + snapping.width, 0);
    EXPECT_LT(snapping.y + snapping.height, 0);
}

TEST(PageSurface, LargeSparsePenCrossesDistantViewport) {
    auto p = std::make_shared<XojPage>(595, 842);
    PageSurface s;
    s.infinite = true;
    p->setSurface(s);
    p->setBackgroundType(PageType(PageTypeFormat::Plain));
    auto* line = new Stroke();
    line->setColor(Colors::black);
    line->setWidth(4);
    line->addPoint(Point(-900000, -900000));
    line->addPoint(Point(900000, 900000));
    p->getSelectedLayer()->addElement(line);
    auto* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    auto* cr = cairo_create(target);
    cairo_translate(cr, -500000, -500000);
    DocumentView view;
    view.drawPage(p, cr, true);
    cairo_surface_flush(target);
    EXPECT_LT(cairo_image_surface_get_data(target)[(20 * 64 + 20) * 4], 100);
    cairo_destroy(cr);
    cairo_surface_destroy(target);
}

TEST(PageSurface, BackgroundSuppressionKeepsFreeExportTransparent) {
    auto p = samplePage();
    p->setBackgroundColor(Color(0xff0000));
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 32, 32);
    auto* cr = cairo_create(surface);
    DocumentView view;
    view.drawPage(p, cr, true, true, true, true);
    cairo_surface_flush(surface);
    EXPECT_EQ(cairo_image_surface_get_data(surface)[3], 0);
    view.drawPage(p, cr, true);
    cairo_surface_flush(surface);
    EXPECT_EQ(cairo_image_surface_get_data(surface)[3], 255);
    EXPECT_EQ(cairo_image_surface_get_data(surface)[2], 255);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}
