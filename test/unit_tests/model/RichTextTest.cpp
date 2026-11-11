// Foliq — GPL-2.0-or-later
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

#include <cairo-pdf.h>
#include <config-test.h>
#include <gtest/gtest.h>
#include <poppler.h>
#include <zlib.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Image.h"
#include "model/RichText.h"
#include "model/SurfaceState.h"
#include "model/TexImage.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "pdf/base/XojCairoPdfExport.h"
#include "undo/TextBoxUndoAction.h"
#include "util/PathUtil.h"
#include "util/serializing/BinObjectEncoding.h"
#include "util/serializing/ObjectInputStream.h"
#include "util/serializing/ObjectOutputStream.h"
#include "view/ImageView.h"
#include "view/View.h"

namespace {
RichText sample() {
    RichText r;
    r.text = "Grüße ÄÖÜ äöü ß €\nMehrzeilig: é 😀\nZweiter Absatz";
    r.spans = {{0, 5, "bold", "1"},       {6, 9, "italic", "1"},     {0, 9, "background", "#ffe080"},
               {0, 5, "size", "24"},      {6, 9, "underline", "1"},  {10, 14, "foreground", "#c02040"},
               {10, 14, "font", "Serif"}, {10, 14, "link", "page:2"}};
    return r;
}
std::string fileData(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}
void writeGzip(const fs::path& path, const std::string& xml) {
    auto f = gzopen(path.string().c_str(), "wb");
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(gzwrite(f, xml.data(), static_cast<unsigned int>(xml.size())), static_cast<int>(xml.size()));
    ASSERT_EQ(gzclose(f), Z_OK);
}
}  // namespace

TEST(FoliqRichText, UnicodeStylesVersionAndValidation) {
    auto r = sample();
    ASSERT_TRUE(r.valid());
    auto copy = RichText::decode(r.encode());
    EXPECT_EQ(copy.text, r.text);
    EXPECT_EQ(copy.encode(), r.encode());
    auto* attrs = copy.attributes();
    auto* it = pango_attr_list_get_iterator(attrs);
    int start, end;
    pango_attr_iterator_range(it, &start, &end);
    EXPECT_EQ(start, 0);
    EXPECT_EQ(end, 7);  // five Unicode code points in "Grüße", seven UTF-8 bytes
    pango_attr_iterator_destroy(it);
    pango_attr_list_unref(attrs);
    r.spans[0].end = 10000;
    EXPECT_FALSE(r.valid());
    EXPECT_THROW(r.encode(), std::runtime_error);
    auto encodeRaw = [](const char* raw) {
        char* encoded = g_base64_encode(reinterpret_cast<const guchar*>(raw), strlen(raw));
        std::string result(encoded);
        g_free(encoded);
        return result;
    };
    EXPECT_THROW(RichText::decode(encodeRaw("[rich]\nversion=99\n")), std::runtime_error);
    EXPECT_THROW(RichText::decode("invalid"), std::runtime_error);
    r = sample();
    r.spans[0].kind = "unknown";
    EXPECT_FALSE(r.valid());
    r = sample();
    r.spans.back().value = "javascript:alert(1)";
    EXPECT_FALSE(r.valid());
    r.spans.back().value = "page:1e3";
    EXPECT_FALSE(r.valid());
    r = sample();
    r.text = std::string("x\xff", 2);
    EXPECT_FALSE(r.valid());
    r = sample();
    r.text += '\x01';
    EXPECT_FALSE(r.valid());
}

TEST(FoliqRichText, GtkTagsSurviveEditingAndFormattedClipboardSerialization) {
    gtk_init_check(nullptr, nullptr);
    auto* buffer = gtk_text_buffer_new(nullptr);
    auto r = sample();
    r.toBuffer(buffer);
    auto before = RichText::fromBuffer(buffer);
    EXPECT_EQ(before.text, r.text);
    EXPECT_EQ(before.spans.size(), r.spans.size());
    GtkTextIter a, b;
    gtk_text_buffer_get_iter_at_offset(buffer, &a, 2);
    gtk_text_buffer_insert(buffer, &a, "ä", -1);
    auto after = RichText::fromBuffer(buffer);
    EXPECT_EQ(after.text.substr(0, 9), "Gräüße");
    auto bold = std::find_if(after.spans.begin(), after.spans.end(), [](const auto& s) { return s.kind == "bold"; });
    ASSERT_NE(bold, after.spans.end());
    EXPECT_EQ(bold->end, 6);
    auto format = gtk_text_buffer_register_serialize_tagset(buffer, "foliq-v1");
    auto* target = gtk_text_buffer_new(nullptr);
    auto targetFormat = gtk_text_buffer_register_deserialize_tagset(target, "foliq-v1");
    gtk_text_buffer_deserialize_set_can_create_tags(target, targetFormat, true);
    gtk_text_buffer_get_bounds(buffer, &a, &b);
    gsize len = 0;
    guchar* bytes = gtk_text_buffer_serialize(buffer, buffer, format, &a, &b, &len);
    gtk_text_buffer_get_start_iter(target, &a);
    GError* error = nullptr;
    ASSERT_TRUE(gtk_text_buffer_deserialize(target, target, targetFormat, &a, bytes, len, &error));
    ASSERT_EQ(error, nullptr);
    EXPECT_EQ(RichText::fromBuffer(target).text, after.text);
    EXPECT_EQ(RichText::fromBuffer(target).spans.size(), after.spans.size());
    g_free(bytes);
    g_object_unref(target);
    g_object_unref(buffer);
}

TEST(FoliqRichText, TableStructureLayoutAndReediting) {
    RichText table;
    table.setCells({{"Käse", "Größe"}, {"Äpfel", "12 €"}, {"", "ß"}});
    ASSERT_TRUE(table.valid());
    table.spans.push_back({0, 4, "bold", "1"});
    auto restored = RichText::decode(table.encode());
    EXPECT_EQ(restored.cells()[1][0], "Äpfel");
    EXPECT_EQ(restored.cells()[2][0], "");
    EXPECT_EQ(restored.columns, 2);
    auto cells = restored.cells();
    cells.insert(cells.begin() + 1, {"Neue Zeile", "ö"});
    for (auto& row: cells) row.insert(row.begin(), "Spalte");
    cells.erase(cells.begin() + 2);
    restored.setCells(cells);
    ASSERT_TRUE(restored.valid());
    EXPECT_EQ(restored.columns, 3);
    EXPECT_EQ(restored.cells().size(), 3);
    Text element;
    element.setRichText(restored);
    auto layout = element.createPangoLayout();
    EXPECT_GT(restored.tableWidth(layout.get()), 0);
    EXPECT_GE(element.getElementWidth(), restored.tableWidth(layout.get()));
    restored.columns = 2;
    EXPECT_FALSE(restored.valid());
}

TEST(FoliqRichText, ObjectClipboardCloneAndProportionalResize) {
    Text original;
    original.setRichText(sample());
    ObjectOutputStream out(new BinObjectEncoding());
    original.serialize(out);
    auto* raw = out.getStr();
    ObjectInputStream in;
    ASSERT_TRUE(in.read(raw->str, static_cast<int>(raw->len)));
    Text restored;
    restored.readSerialized(in);
    EXPECT_EQ(restored.getRichText().encode(), original.getRichText().encode());
    std::unique_ptr<Text> clone(original.clone());
    clone->scale(0, 0, 2, 2, 0, false);
    EXPECT_EQ(clone->getFontSize(), original.getFontSize() * 2);
    EXPECT_EQ(g_ascii_strtod(clone->getRichText().spans[3].value.c_str(), nullptr), 48);
    EXPECT_EQ(original.getRichText().spans[3].value, "24");
    clone->setText("Neu");
    EXPECT_TRUE(clone->getRichText().valid());
    EXPECT_TRUE(clone->getRichText().spans.empty());
}

TEST(FoliqRichText, RealXoppRoundtripAndLongMultilineText) {
    // Use the upstream mixed-content fixture to retain real stroke/formula coverage.
    LoadHandler loader;
    auto* doc = loader.loadDocument(GET_TESTFILE("packaged_xopp/suite.xopp"));
    ASSERT_NE(doc, nullptr) << loader.getLastError();
    auto page = doc->getPage(0);
    auto* layer = page->getSelectedLayer();
    auto* text = new Text();
    auto rich = sample();
    rich.text += std::string(4096, 'x') + "\nEnde Äöß\r\nWindows\rZeile\tTab";
    text->setRichText(rich);
    text->setX(43);
    text->setY(91);
    layer->addElement(text);
    auto* table = new Text();
    RichText t;
    t.setCells({{"Überschrift", "Wert"}, {"Grüße", "42"}});
    table->setRichText(t);
    table->setY(150);
    layer->addElement(table);
    auto file = Util::getTmpDirSubfolder() / "foliq-rich-roundtrip.xopp";
    SaveHandler save;
    save.prepareSave(doc);
    save.saveTo(file);
    ASSERT_TRUE(save.getErrorMessage().empty()) << save.getErrorMessage();
    LoadHandler second;
    auto* restored = second.loadDocument(file);
    ASSERT_NE(restored, nullptr) << second.getLastError();
    auto& elements = restored->getPage(0)->getSelectedLayer()->getElements();
    auto* text2 = dynamic_cast<Text*>(elements[elements.size() - 2]);
    auto* table2 = dynamic_cast<Text*>(elements.back());
    ASSERT_NE(text2, nullptr);
    ASSERT_NE(table2, nullptr);
    EXPECT_EQ(text2->getRichText().encode(), rich.encode());
    EXPECT_EQ(text2->getX(), 43);
    EXPECT_EQ(text2->getY(), 91);
    EXPECT_EQ(table2->getRichText().encode(), t.encode());
    EXPECT_EQ(table2->getRichText().cells()[1][0], "Grüße");
    EXPECT_EQ(second.getFileVersion(), 5);
    fs::remove(file);
}

TEST(FoliqRichText, NewerContentRefusedWithoutModifyingFile) {
    auto file = Util::getTmpDirSubfolder() / "foliq-future-content.xopp";
    writeGzip(file, "<xournal fileversion=\"5\" foliq-content-version=\"99\"><page width=\"595\" "
                    "height=\"842\"><layer/></page></xournal>");
    auto original = fileData(file);
    LoadHandler loader;
    EXPECT_EQ(loader.loadDocument(file), nullptr);
    EXPECT_NE(loader.getLastError().find("Unsupported Foliq"), std::string::npos);
    EXPECT_EQ(fileData(file), original);
    fs::remove(file);
}

TEST(FoliqRichText, ReplacementUndoRedoPreservesStackingOrder) {
    PageRef page = std::make_shared<XojPage>(595, 842);
    auto* layer = page->getSelectedLayer();
    auto* below = new Text();
    below->setText("below");
    auto* old = new Text();
    old->setRichText(sample());
    auto* above = new Text();
    above->setText("above");
    layer->addElement(below);
    layer->addElement(old);
    layer->addElement(above);
    auto* replacement = old->clone();
    replacement->setText("changed");
    auto index = layer->removeElement(old, false);
    layer->insertElement(replacement, index);
    TextBoxUndoAction action(page, layer, replacement, old);
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(action.undo(nullptr));
        EXPECT_EQ(layer->getElements()[1], old);
        ASSERT_TRUE(action.redo(nullptr));
        EXPECT_EQ(layer->getElements()[1], replacement);
        EXPECT_EQ(layer->getElements()[0], below);
        EXPECT_EQ(layer->getElements()[2], above);
    }
}

TEST(FoliqRichText, NonDestructiveImageCropValidationAndSerialization) {
    Image image;
    image.setImage(fileData(GET_TESTFILE("images/r90.jpg")));
    image.getImage();
    image.setWidth(130);
    image.setHeight(500);
    ASSERT_TRUE(image.setCrop({.1, .2, .8, .9}));
    EXPECT_FALSE(image.setCrop({.9, 0, .1, 1}));
    EXPECT_FALSE(image.setCrop({0, 0, std::numeric_limits<double>::quiet_NaN(), 1}));
    EXPECT_TRUE(image.rescaleOnlyAspectRatio());
    ObjectOutputStream out(new BinObjectEncoding());
    image.serialize(out);
    auto* raw = out.getStr();
    ObjectInputStream in;
    ASSERT_TRUE(in.read(raw->str, static_cast<int>(raw->len)));
    Image restored;
    restored.readSerialized(in);
    EXPECT_EQ(restored.getCrop().left, .1);
    EXPECT_EQ(restored.getCrop().bottom, .9);
    EXPECT_EQ(restored.getRawDataLength(), image.getRawDataLength());
    EXPECT_EQ(restored.getImageSize(), Image::NOSIZE);
    auto clone = std::unique_ptr<Image>(static_cast<Image*>(image.clone()));
    EXPECT_EQ(clone->getCrop().right, .8);
    EXPECT_TRUE(clone->setCrop({0, 0, 1, 1}));
    EXPECT_EQ(clone->getRawDataLength(), image.getRawDataLength());
    // Reusing a reader target for a legacy image must clear the previous crop.
    ObjectOutputStream plain(new BinObjectEncoding());
    clone->serialize(plain);
    auto* plainData = plain.getStr();
    ObjectInputStream plainIn;
    ASSERT_TRUE(plainIn.read(plainData->str, static_cast<int>(plainData->len)));
    restored.readSerialized(plainIn);
    EXPECT_EQ(restored.getCrop().left, 0);
    EXPECT_EQ(restored.getCrop().right, 1);
}

namespace {
cairo_status_t appendBytes(void* p, const unsigned char* bytes, unsigned int length) {
    static_cast<std::string*>(p)->append(reinterpret_cast<const char*>(bytes), length);
    return CAIRO_STATUS_SUCCESS;
}
std::string formulaPdf() {
    std::string result;
    auto* surface = cairo_pdf_surface_create_for_stream(appendBytes, &result, 80, 30);
    auto* cr = cairo_create(surface);
    cairo_move_to(cr, 5, 20);
    cairo_show_text(cr, "x squared");
    cairo_show_page(cr);
    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_surface_destroy(surface);
    return result;
}
}  // namespace

TEST(FoliqRichText, CropRendererClipsWithoutDestroyingOriginalPixels) {
    auto* source = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 20);
    auto* cr = cairo_create(source);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0, 0, 1);
    cairo_rectangle(cr, 50, 0, 50, 20);
    cairo_fill(cr);
    cairo_destroy(cr);
    std::string png;
    ASSERT_EQ(cairo_surface_write_to_png_stream(source, appendBytes, &png), CAIRO_STATUS_SUCCESS);
    cairo_surface_destroy(source);
    Image image;
    image.setImage(std::move(png));
    image.setX(10);
    image.setY(5);
    image.setWidth(50);
    image.setHeight(20);
    ASSERT_TRUE(image.setCrop({.5, 0, 1, 1}));
    auto* dest = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 80, 35);
    cr = cairo_create(dest);
    xoj::view::ImageView view(&image);
    view.draw(xoj::view::Context::createDefault(cr));
    cairo_destroy(cr);
    cairo_surface_flush(dest);
    auto* pixels = reinterpret_cast<uint32_t*>(cairo_image_surface_get_data(dest));
    int stride = cairo_image_surface_get_stride(dest) / 4;
    EXPECT_EQ(pixels[15 * stride + 30], 0xff0000ffU);                 // selected blue half
    EXPECT_EQ(pixels[15 * stride + 5], 0U);                           // outside the object is untouched
    EXPECT_EQ(cairo_image_surface_get_width(image.getImage()), 100);  // red pixels remain available
    cairo_surface_destroy(dest);
}

TEST(FoliqRichText, FormulaSourceAndOriginalImageSurviveXoppAndClipboard) {
    LoadHandler loader;
    auto* doc = loader.loadDocument(GET_TESTFILE("packaged_xopp/suite.xopp"));
    ASSERT_NE(doc, nullptr);
    auto* layer = doc->getPage(0)->getSelectedLayer();
    auto* formula = new TexImage();
    ASSERT_TRUE(formula->loadData(formulaPdf()));
    formula->setText("\\frac{a}{b} + x^2");
    formula->setX(50);
    formula->setY(220);
    layer->addElement(formula);
    auto* image = new Image();
    image->setImage(fileData(GET_TESTFILE("images/r90.jpg")));
    image->getImage();
    image->setWidth(50);
    image->setHeight(80);
    image->setCrop({.1, .2, .8, .9});
    layer->addElement(image);
    auto file = Util::getTmpDirSubfolder() / "foliq-formula-image.xopp";
    SaveHandler save;
    save.prepareSave(doc);
    save.saveTo(file);
    ASSERT_TRUE(save.getErrorMessage().empty());
    LoadHandler second;
    auto* restored = second.loadDocument(file);
    ASSERT_NE(restored, nullptr) << second.getLastError();
    auto& elements = restored->getPage(0)->getSelectedLayer()->getElements();
    auto* formula2 = dynamic_cast<TexImage*>(elements[elements.size() - 2]);
    auto* image2 = dynamic_cast<Image*>(elements.back());
    ASSERT_NE(formula2, nullptr);
    ASSERT_NE(image2, nullptr);
    EXPECT_EQ(formula2->getText(), formula->getText());
    EXPECT_EQ(formula2->getBinaryData(), formula->getBinaryData());
    EXPECT_NE(formula2->getPdf(), nullptr);
    EXPECT_EQ(image2->getCrop().left, .1);
    EXPECT_EQ(image2->getCrop().bottom, .9);
    EXPECT_EQ(cairo_image_surface_get_width(image2->getImage()), 130);
    ObjectOutputStream out(new BinObjectEncoding());
    formula->serialize(out);
    auto* data = out.getStr();
    ObjectInputStream input;
    ASSERT_TRUE(input.read(data->str, static_cast<int>(data->len)));
    TexImage formula3;
    formula3.readSerialized(input);
    EXPECT_EQ(formula3.getText(), formula->getText());
    EXPECT_NE(formula3.getPdf(), nullptr);
    EXPECT_TRUE(formula3.rescaleOnlyAspectRatio());
    fs::remove(file);
}

TEST(FoliqRichText, CorruptRichContentAndNewerCropRefused) {
    auto file = Util::getTmpDirSubfolder() / "foliq-damaged-content.xopp";
    auto rich = sample();
    writeGzip(file, "<xournal fileversion=\"5\"><page width=\"595\" height=\"842\"><layer><text font=\"Sans\" "
                    "size=\"12\" x=\"0\" y=\"0\" color=\"#000000ff\" foliq-rich=\"" +
                            rich.encode() + "\">mismatch</text></layer></page></xournal>");
    auto original = fileData(file);
    LoadHandler loader;
    EXPECT_EQ(loader.loadDocument(file), nullptr);
    EXPECT_NE(loader.getLastError().find("mismatch"), std::string::npos);
    EXPECT_EQ(fileData(file), original);
    writeGzip(file, "<xournal fileversion=\"5\"><page width=\"595\" height=\"842\"><layer><image left=\"0\" top=\"0\" "
                    "right=\"100\" bottom=\"100\" foliq-crop-version=\"99\"></image></layer></page></xournal>");
    original = fileData(file);
    LoadHandler imageLoader;
    EXPECT_EQ(imageLoader.loadDocument(file), nullptr);
    EXPECT_EQ(fileData(file), original);
    fs::remove(file);
}


TEST(FoliqRichText, FreeCoordinatesConversionAndTiledPdfPreserveEditableContent) {
    Document doc(nullptr);
    auto page = std::make_shared<XojPage>(595, 842);
    PageSurface surface;
    surface.infinite = true;
    surface.cameraX = -200;
    surface.cameraY = -150;
    surface.exportContent = false;
    surface.crop = {-300, -300, 600, 600};
    surface.tileWidth = 300;
    surface.tileHeight = 300;
    surface.tiled = true;
    page->setSurface(surface);
    auto* rich = new Text();
    rich->setRichText(sample());
    rich->setX(-250);
    rich->setY(-250);
    page->getSelectedLayer()->addElement(rich);
    auto* table = new Text();
    RichText cells;
    cells.setCells({{"Zelle Grüße", "Öß"}, {"Nach Neustart", "42"}});
    table->setRichText(cells);
    table->setX(-100);
    table->setY(100);
    page->getSelectedLayer()->addElement(table);
    auto* formula = new TexImage();
    ASSERT_TRUE(formula->loadData(formulaPdf()));
    formula->setText("x^2");
    formula->setX(50);
    formula->setY(-150);
    page->getSelectedLayer()->addElement(formula);
    doc.addPage(page);
    SurfaceState paper(*page);
    paper.surface.infinite = false;
    auto copy = convertedSurfaceCopy(*page, paper);
    auto* copied = dynamic_cast<Text*>(copy->getSelectedLayer()->getElements()[0]);
    ASSERT_NE(copied, nullptr);
    EXPECT_EQ(copied->getX(), -50);
    EXPECT_EQ(copied->getY(), -100);
    EXPECT_EQ(copied->getRichText().encode(), rich->getRichText().encode());
    auto path = Util::getTmpDirSubfolder() / "foliq-free-rich.xopp";
    SaveHandler save;
    save.prepareSave(&doc);
    save.saveTo(path);
    LoadHandler loader;
    auto* restored = loader.loadDocument(path);
    ASSERT_NE(restored, nullptr) << loader.getLastError();
    EXPECT_TRUE(restored->getPage(0)->isInfinite());
    EXPECT_EQ(restored->getPage(0)->getSurface().cameraX, -200);
    auto* restoredTable = dynamic_cast<Text*>(restored->getPage(0)->getSelectedLayer()->getElements()[1]);
    ASSERT_NE(restoredTable, nullptr);
    EXPECT_EQ(restoredTable->getRichText().cells()[0][0], "Zelle Grüße");
    EXPECT_EQ(restoredTable->getX(), -100);
    auto pdfPath = Util::getTmpDirSubfolder() / "foliq-free-rich.pdf";
    XojCairoPdfExport pdf(restored, nullptr);
    ASSERT_TRUE(pdf.createPdf(pdfPath, false)) << pdf.getLastError();
    auto uri = std::string("file://") + pdfPath.string();
    auto* result = poppler_document_new_from_file(uri.c_str(), nullptr, nullptr);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(poppler_document_get_n_pages(result), 4);
    auto* first = poppler_document_get_page(result, 0);
    char* text = poppler_page_get_text(first);
    EXPECT_NE(std::string(text).find("Grüße"), std::string::npos);
    g_free(text);
    g_object_unref(first);
    g_object_unref(result);
    fs::remove(path);
    fs::remove(pdfPath);
}
