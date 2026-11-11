// Foliq — GPL-2.0-or-later
#include "RichText.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

#include <pango/pangocairo.h>

namespace {
constexpr size_t MAX_TEXT = 8 * 1024 * 1024;
constexpr size_t MAX_SPANS = 100000;
const std::string PREFIX = "foliq:";
std::string stringValue(GKeyFile* file, const char* group, const char* key) {
    GError* error = nullptr;
    char* value = g_key_file_get_string(file, group, key, &error);
    if (error) {
        std::string message = error->message;
        g_error_free(error);
        throw std::runtime_error(message);
    }
    std::string result(value);
    g_free(value);
    return result;
}
double number(const std::string& value) {
    char* end = nullptr;
    double result = g_ascii_strtod(value.c_str(), &end);
    if (value.empty() || !end || end == value.c_str() || *end || !std::isfinite(result))
        return -1;
    return result;
}
std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> result;
    size_t begin = 0;
    for (size_t end = s.find(sep); end != std::string::npos; end = s.find(sep, begin)) {
        result.push_back(s.substr(begin, end - begin));
        begin = end + 1;
    }
    result.push_back(s.substr(begin));
    return result;
}
bool validStyle(const std::string& kind, const std::string& value) {
    if (value.empty() || value.size() > 2048 || !g_utf8_validate(value.c_str(), -1, nullptr))
        return false;
    if (kind == "bold" || kind == "italic" || kind == "underline")
        return value == "1" || value == "0";
    if (kind == "size")
        return number(value) >= 1 && number(value) <= 1000;
    if (kind == "font")
        return true;
    if (kind == "foreground" || kind == "background") {
        GdkRGBA color;
        return gdk_rgba_parse(&color, value.c_str());
    }
    if (kind == "link")
        return g_str_has_prefix(value.c_str(), "https://") || g_str_has_prefix(value.c_str(), "http://") ||
               g_str_has_prefix(value.c_str(), "mailto:") ||
               (g_str_has_prefix(value.c_str(), "page:") && value.size() > 5 && value.size() <= 14 &&
                std::all_of(value.begin() + 5, value.end(), [](char c) { return g_ascii_isdigit(c); }) &&
                number(value.substr(5)) >= 1);
    return false;
}
}  // namespace

bool RichText::valid() const {
    if (text.size() > MAX_TEXT || !g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr) ||
        text.find('\0') != std::string::npos || spans.size() > MAX_SPANS || columns < 0 || columns > 32)
        return false;
    // XML 1.0 text must remain round-trippable, including clipboard input.
    for (const char* p = text.c_str(); *p; p = g_utf8_next_char(p)) {
        auto ch = g_utf8_get_char(p);
        if ((ch < 32 && ch != '\t' && ch != '\n' && ch != '\r') || ch == 0xfffe || ch == 0xffff)
            return false;
    }
    auto length = g_utf8_strlen(text.c_str(), -1);
    for (const auto& span: spans) {
        if (span.start < 0 || span.end <= span.start || span.end > length || !validStyle(span.kind, span.value))
            return false;
    }
    if (columns) {
        if (text.find('\r') != std::string::npos)
            return false;
        auto rows = cells();
        if (rows.size() > 1000)
            return false;
        for (const auto& row: rows)
            if (row.size() != static_cast<size_t>(columns))
                return false;
    }
    return true;
}

std::string RichText::encode() const {
    if (!valid())
        throw std::runtime_error("Invalid rich text");
    GKeyFile* file = g_key_file_new();
    g_key_file_set_integer(file, "rich", "version", 1);
    g_key_file_set_string(file, "rich", "text", text.c_str());
    g_key_file_set_integer(file, "rich", "columns", columns);
    g_key_file_set_integer(file, "rich", "count", static_cast<int>(spans.size()));
    auto ordered = spans;
    std::sort(ordered.begin(), ordered.end(), [](const Span& a, const Span& b) {
        return std::tie(a.start, a.end, a.kind, a.value) < std::tie(b.start, b.end, b.kind, b.value);
    });
    for (size_t i = 0; i < ordered.size(); ++i) {
        auto group = "span" + std::to_string(i);
        const auto& span = ordered[i];
        g_key_file_set_integer(file, group.c_str(), "start", span.start);
        g_key_file_set_integer(file, group.c_str(), "end", span.end);
        g_key_file_set_string(file, group.c_str(), "kind", span.kind.c_str());
        g_key_file_set_string(file, group.c_str(), "value", span.value.c_str());
    }
    gsize length = 0;
    char* raw = g_key_file_to_data(file, &length, nullptr);
    char* encoded = g_base64_encode(reinterpret_cast<const guchar*>(raw), length);
    std::string result(encoded);
    g_free(encoded);
    g_free(raw);
    g_key_file_unref(file);
    return result;
}

RichText RichText::decode(const std::string& data) {
    if (data.size() > 32 * MAX_TEXT)
        throw std::runtime_error("Rich text exceeds size limit");
    gsize length = 0;
    guchar* raw = g_base64_decode(data.c_str(), &length);
    GKeyFile* file = g_key_file_new();
    bool loaded = g_key_file_load_from_data(file, reinterpret_cast<char*>(raw), length, G_KEY_FILE_NONE, nullptr);
    g_free(raw);
    try {
        if (!loaded || stringValue(file, "rich", "version") != "1")
            throw std::runtime_error("Unsupported or damaged Foliq rich-text version; file was not opened");
        RichText result;
        result.text = stringValue(file, "rich", "text");
        auto integer = [&](const char* group, const char* key) {
            auto s = stringValue(file, group, key);
            double n = number(s);
            if (n < 0 || n > static_cast<double>(MAX_TEXT) || std::floor(n) != n)
                throw std::runtime_error("Invalid rich-text index");
            return static_cast<int>(n);
        };
        result.columns = integer("rich", "columns");
        int count = integer("rich", "count");
        if (count > static_cast<int>(MAX_SPANS))
            throw std::runtime_error("Too many rich-text spans");
        for (int i = 0; i < count; ++i) {
            auto group = "span" + std::to_string(i);
            result.spans.push_back({integer(group.c_str(), "start"), integer(group.c_str(), "end"),
                                    stringValue(file, group.c_str(), "kind"),
                                    stringValue(file, group.c_str(), "value")});
        }
        if (!result.valid())
            throw std::runtime_error("Invalid rich-text content or table");
        g_key_file_unref(file);
        return result;
    } catch (...) {
        g_key_file_unref(file);
        throw;
    }
}

GtkTextTag* RichText::tag(GtkTextBuffer* buffer, const std::string& kind, const std::string& value) {
    if (!validStyle(kind, value))
        return nullptr;
    std::string name = PREFIX + kind + ":" + value;
    auto* table = gtk_text_buffer_get_tag_table(buffer);
    auto* result = gtk_text_tag_table_lookup(table, name.c_str());
    if (result)
        return result;
    result = gtk_text_buffer_create_tag(buffer, name.c_str(), nullptr);
    if (kind == "bold")
        g_object_set(result, "weight", value == "1" ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, nullptr);
    else if (kind == "italic")
        g_object_set(result, "style", value == "1" ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL, nullptr);
    else if (kind == "underline")
        g_object_set(result, "underline", value == "1" ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE, nullptr);
    else if (kind == "size")
        g_object_set(result, "size-points", number(value), nullptr);
    else if (kind == "font")
        g_object_set(result, "family", value.c_str(), nullptr);
    else if (kind == "link")
        g_object_set(result, "underline", PANGO_UNDERLINE_SINGLE, "foreground", "#2060c0", nullptr);
    else
        g_object_set(result, kind.c_str(), value.c_str(), nullptr);
    return result;
}

RichText RichText::fromBuffer(GtkTextBuffer* buffer, int columns) {
    RichText result;
    result.columns = columns;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char* raw = gtk_text_buffer_get_text(buffer, &start, &end, true);
    result.text = raw;
    g_free(raw);
    struct Context {
        GtkTextBuffer* buffer;
        RichText* result;
    } context{buffer, &result};
    gtk_text_tag_table_foreach(
            gtk_text_buffer_get_tag_table(buffer),
            [](GtkTextTag* tag, gpointer data) {
                auto* ctx = static_cast<Context*>(data);
                char* rawName = nullptr;
                g_object_get(tag, "name", &rawName, nullptr);
                std::string name = rawName ? rawName : "";
                g_free(rawName);
                if (name.compare(0, PREFIX.size(), PREFIX))
                    return;
                size_t sep = name.find(':', PREFIX.size());
                if (sep == std::string::npos)
                    return;
                std::string kind = name.substr(PREFIX.size(), sep - PREFIX.size());
                std::string value = name.substr(sep + 1);
                if (!validStyle(kind, value))
                    return;
                GtkTextIter it;
                gtk_text_buffer_get_start_iter(ctx->buffer, &it);
                while (!gtk_text_iter_is_end(&it)) {
                    if (!gtk_text_iter_has_tag(&it, tag)) {
                        if (!gtk_text_iter_forward_to_tag_toggle(&it, tag))
                            break;
                    }
                    int begin = gtk_text_iter_get_offset(&it);
                    gtk_text_iter_forward_to_tag_toggle(&it, tag);
                    int finish = gtk_text_iter_get_offset(&it);
                    if (finish > begin)
                        ctx->result->spans.push_back({begin, finish, kind, value});
                }
            },
            &context);
    return result;
}

void RichText::toBuffer(GtkTextBuffer* buffer) const {
    gtk_text_buffer_set_text(buffer, text.c_str(), static_cast<int>(text.size()));
    for (const auto& span: spans) {
        GtkTextIter a, b;
        gtk_text_buffer_get_iter_at_offset(buffer, &a, span.start);
        gtk_text_buffer_get_iter_at_offset(buffer, &b, span.end);
        if (auto* t = tag(buffer, span.kind, span.value))
            gtk_text_buffer_apply_tag(buffer, t, &a, &b);
    }
}

PangoAttrList* RichText::attributes() const {
    auto* list = pango_attr_list_new();
    for (const auto& span: spans) {
        if (span.start < 0 || span.end > g_utf8_strlen(text.c_str(), -1))
            continue;
        auto add = [&](PangoAttribute* a) {
            a->start_index = static_cast<guint>(g_utf8_offset_to_pointer(text.c_str(), span.start) - text.c_str());
            a->end_index = static_cast<guint>(g_utf8_offset_to_pointer(text.c_str(), span.end) - text.c_str());
            pango_attr_list_change(list, a);
        };
        if (span.kind == "bold")
            add(pango_attr_weight_new(span.value == "1" ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL));
        else if (span.kind == "italic")
            add(pango_attr_style_new(span.value == "1" ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL));
        else if (span.kind == "underline")
            add(pango_attr_underline_new(span.value == "1" ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE));
        else if (span.kind == "font")
            add(pango_attr_family_new(span.value.c_str()));
        else if (span.kind == "size")
            add(pango_attr_size_new_absolute(static_cast<int>(number(span.value) * PANGO_SCALE)));
        else if (span.kind == "foreground" || span.kind == "background" || span.kind == "link") {
            GdkRGBA c;
            gdk_rgba_parse(&c, span.kind == "link" ? "#2060c0" : span.value.c_str());
            auto r = static_cast<guint16>(c.red * 65535);
            auto g = static_cast<guint16>(c.green * 65535);
            auto b = static_cast<guint16>(c.blue * 65535);
            add(span.kind == "background" ? pango_attr_background_new(r, g, b) : pango_attr_foreground_new(r, g, b));
            if (span.kind == "link")
                add(pango_attr_underline_new(PANGO_UNDERLINE_SINGLE));
        }
    }
    return list;
}

void RichText::scale(double factor) {
    for (auto& span: spans)
        if (span.kind == "size")
            span.value = std::to_string(std::clamp(number(span.value) * factor, 1.0, 1000.0));
}

std::vector<std::vector<std::string>> RichText::cells() const {
    std::vector<std::vector<std::string>> result;
    for (const auto& line: split(text, '\n')) result.push_back(split(line, '\t'));
    return result;
}
void RichText::setCells(const std::vector<std::vector<std::string>>& rows) {
    text.clear();
    spans.clear();
    columns = rows.empty() ? 0 : static_cast<int>(rows.front().size());
    for (size_t r = 0; r < rows.size(); ++r) {
        if (r)
            text += '\n';
        for (size_t c = 0; c < rows[r].size(); ++c) {
            if (c)
                text += '\t';
            text += rows[r][c];
        }
    }
}

void RichText::configureLayout(PangoLayout* layout, double fontSize) const {
    auto* attrs = attributes();
    pango_layout_set_attributes(layout, attrs);
    pango_attr_list_unref(attrs);
    if (!columns) {
        pango_layout_set_tabs(layout, nullptr);
        return;
    }
    // Equal-width columns; widths grow to fit cell contents. No implicit wrapping or clipping.
    double width = fontSize * 8;
    auto* measure = pango_layout_copy(layout);
    int offset = 0;
    for (const auto& row: cells())
        for (const auto& cell: row) {
            RichText part;
            part.text = cell;
            int length = static_cast<int>(g_utf8_strlen(cell.c_str(), -1));
            for (auto span: spans) {
                span.start = std::max(span.start, offset) - offset;
                span.end = std::min(span.end, offset + length) - offset;
                if (span.end > span.start)
                    part.spans.push_back(std::move(span));
            }
            auto* cellAttrs = part.attributes();
            pango_layout_set_attributes(measure, cellAttrs);
            pango_attr_list_unref(cellAttrs);
            pango_layout_set_text(measure, cell.c_str(), -1);
            offset += length + 1;
            int w = 0;
            pango_layout_get_size(measure, &w, nullptr);
            width = std::max(width, static_cast<double>(w) / PANGO_SCALE + fontSize);
        }
    g_object_unref(measure);
    auto* tabs = pango_tab_array_new(columns, false);
    for (int c = 0; c < columns; ++c)
        pango_tab_array_set_tab(tabs, c, PANGO_TAB_LEFT, static_cast<int>((c + 1) * width * PANGO_SCALE));
    pango_layout_set_tabs(layout, tabs);
    pango_tab_array_free(tabs);
}

double RichText::tableWidth(PangoLayout* layout) const {
    if (!columns)
        return 0;
    auto* tabs = pango_layout_get_tabs(layout);
    if (!tabs)
        return 0;
    int width = 0;
    pango_tab_array_get_tab(tabs, 0, nullptr, &width);
    pango_tab_array_free(tabs);
    return static_cast<double>(width) * columns / PANGO_SCALE;
}

void RichText::drawTable(cairo_t* cr, PangoLayout* layout) const {
    if (!columns)
        return;
    auto* tabs = pango_layout_get_tabs(layout);
    if (!tabs)
        return;
    int tabWidth = 0;
    pango_tab_array_get_tab(tabs, 0, nullptr, &tabWidth);
    pango_tab_array_free(tabs);
    double width = static_cast<double>(tabWidth) / PANGO_SCALE;
    int h = 0;
    pango_layout_get_size(layout, nullptr, &h);
    cairo_save(cr);
    cairo_set_source_rgb(cr, .45, .45, .45);
    cairo_set_line_width(cr, .5);
    for (int c = 0; c <= columns; ++c) {
        cairo_move_to(cr, c * width - 2, -1);
        cairo_line_to(cr, c * width - 2, static_cast<double>(h) / PANGO_SCALE + 1);
    }
    auto* it = pango_layout_get_iter(layout);
    do {
        int y = 0;
        pango_layout_iter_get_line_yrange(it, &y, nullptr);
        cairo_move_to(cr, -2, static_cast<double>(y) / PANGO_SCALE - 1);
        cairo_line_to(cr, columns * width - 2, static_cast<double>(y) / PANGO_SCALE - 1);
    } while (pango_layout_iter_next_line(it));
    pango_layout_iter_free(it);
    cairo_move_to(cr, -2, static_cast<double>(h) / PANGO_SCALE + 1);
    cairo_line_to(cr, columns * width - 2, static_cast<double>(h) / PANGO_SCALE + 1);
    cairo_stroke(cr);
    cairo_restore(cr);
}
