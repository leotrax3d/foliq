// Foliq — GPL-2.0-or-later
#pragma once

#include <string>
#include <vector>

#include <gtk/gtk.h>

/** Local rich-text v1. Offsets count Unicode code points, never UTF-8 bytes.
 * Tables are rectangular, single-paragraph cells separated by TAB/LF.
 * The plain text remains independently readable by XOPP readers.
 */
struct RichText {
    struct Span {
        int start = 0;
        int end = 0;
        std::string kind;
        std::string value;
    };
    std::string text;
    std::vector<Span> spans;
    int columns = 0;

    bool valid() const;
    std::string encode() const;
    static RichText decode(const std::string& data);  // throws on unsupported/corrupt data
    static RichText fromBuffer(GtkTextBuffer* buffer, int columns = 0);
    void toBuffer(GtkTextBuffer* buffer) const;
    static GtkTextTag* tag(GtkTextBuffer* buffer, const std::string& kind, const std::string& value);
    PangoAttrList* attributes() const;  // caller owns
    void configureLayout(PangoLayout* layout, double fontSize) const;
    double tableWidth(PangoLayout* layout) const;
    void drawTable(cairo_t* cr, PangoLayout* layout) const;
    void scale(double factor);
    std::vector<std::vector<std::string>> cells() const;
    void setCells(const std::vector<std::vector<std::string>>& cells);
};
