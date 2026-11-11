#include "TextEditor.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>  // for strcmp, size_t
#include <memory>   // for allocator, make_unique, __shared_p...
#include <regex>
#include <utility>  // for move

#include <gdk/gdkkeysyms.h>  // for GDK_KEY_B, GDK_KEY_ISO_Enter, GDK_...
#include <glib-object.h>     // for g_object_get, g_object_unref, G_CA...

#include "control/AudioController.h"
#include "control/Control.h"  // for Control
#include "control/settings/Settings.h"
#include "gui/XournalppCursor.h"  // for XournalppCursor
#include "model/Document.h"       // for Document
#include "model/Font.h"           // for XojFont
#include "model/Text.h"           // for Text
#include "model/XojPage.h"        // for XojPage
#include "undo/DeleteUndoAction.h"
#include "undo/InsertUndoAction.h"
#include "undo/TextBoxUndoAction.h"
#include "undo/UndoRedoHandler.h"  // for UndoRedoHandler
#include "util/DispatchPool.h"
#include "util/Range.h"
#include "util/glib_casts.h"  // for wrap_for_once_v
#include "util/raii/CStringWrapper.h"
#include "view/overlays/TextEditionView.h"

#include "TextEditorWidget.h"  // for gtk_xoj_int_txt_new

class UndoAction;

/** GtkTextBuffer helper functions **/
static auto getIteratorAtCursor(GtkTextBuffer* buffer) -> GtkTextIter {
    GtkTextIter cursorIter = {nullptr};
    gtk_text_buffer_get_iter_at_mark(buffer, &cursorIter, gtk_text_buffer_get_insert(buffer));
    return cursorIter;
}

/**
 * @brief Compute the byte offset of an iterator in the GtkTextBuffer
 *
 * NB: This is much faster than relying on g_utf8_offset_to_pointer
 */
static auto getByteOffsetOfIterator(GtkTextIter it) -> int {
    // Bytes from beginning of line to iterator
    int pos = gtk_text_iter_get_line_index(&it);
    gtk_text_iter_set_line_index(&it, 0);
    // Count bytes of previous lines
    while (gtk_text_iter_backward_line(&it)) {
        pos += gtk_text_iter_get_bytes_in_line(&it);
    }
    return pos;
}

static auto getByteOffsetOfCursor(GtkTextBuffer* buffer) -> int {
    return getByteOffsetOfIterator(getIteratorAtCursor(buffer));
}

/**
 * @brief Get an iterator at the prescribed byte index.
 *
 * NB: This is much faster than relying on g_utf8_pointer_to_offset for long texts
 */
static auto getIteratorAtByteOffset(GtkTextBuffer* buf, int byteIndex) {
    assert(byteIndex >= 0);
    GtkTextIter it = {nullptr};
    gtk_text_buffer_get_start_iter(buf, &it);

    // Fast forward to the beginning of the line containing our target destination
    for (int linelength = gtk_text_iter_get_bytes_in_line(&it);
         linelength <= byteIndex && gtk_text_iter_forward_line(&it);
         byteIndex -= std::exchange(linelength, gtk_text_iter_get_bytes_in_line(&it))) {}

    if (!gtk_text_iter_is_end(&it)) {
        gtk_text_iter_set_line_index(&it, byteIndex);
    }
    // else { // byteIndex was either past-the-end or pointed to the end }

    return it;
}

static auto cloneToCString(GtkTextBuffer* buf) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    return xoj::util::OwnedCString::assumeOwnership(gtk_text_iter_get_text(&start, &end));
}

/**
 * @brief Clone the buffer's content and insert a string into the clone
 * This makes one less copy operation over using cloneToStdString followed by insert()
 * (The text is copied "only" twice, and not three times)
 */
static auto cloneWithInsertToStdString(GtkTextBuffer* buf, std::string_view insertedStr) -> std::string {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    GtkTextIter insertionPoint = getIteratorAtCursor(buf);

    auto firstHalf = xoj::util::OwnedCString::assumeOwnership(gtk_text_iter_get_text(&start, &insertionPoint));
    auto secondHalf = xoj::util::OwnedCString::assumeOwnership(gtk_text_iter_get_text(&insertionPoint, &end));

    std::string_view str1(firstHalf);
    std::string_view str2(secondHalf);

    std::string res;
    res.reserve(str1.length() + str2.length() + insertedStr.length());
    res += str1;
    res += insertedStr;
    res += str2;

    return res;
}

TextEditor::TextEditor(Control* control, const PageRef& page, GtkWidget* xournalWidget, double x, double y):
        control(control),
        page(page),
        xournalWidget(xournalWidget),
        textWidget(gtk_xoj_int_txt_new(this), xoj::util::adopt),
        imContext(gtk_im_multicontext_new(), xoj::util::adopt),
        buffer(gtk_text_buffer_new(nullptr), xoj::util::adopt),
        viewPool(std::make_shared<xoj::util::DispatchPool<xoj::view::TextEditionView>>()) {
    // Informs the windowing system of the selection -- i.e. for accessibility purposes
    gtk_text_buffer_add_selection_clipboard(buffer.get(), gtk_clipboard_get(GDK_SELECTION_PRIMARY));

    this->initializeEditionAt(x, y);
    tableColumns = textElement->getRichText().columns;
    textElement->getRichText().toBuffer(buffer.get());
    layoutStatus = LayoutStatus::NEEDS_COMPLETE_UPDATE;
    gtk_text_buffer_register_serialize_tagset(buffer.get(), "foliq-v1");
    auto richFormat = gtk_text_buffer_register_deserialize_tagset(buffer.get(), "foliq-v1");
    gtk_text_buffer_deserialize_set_can_create_tags(buffer.get(), richFormat, true);
    recordHistory();
    g_signal_connect_after(buffer.get(), "insert-text",
                           G_CALLBACK(+[](GtkTextBuffer* b, GtkTextIter* end, gchar* text, gint length, gpointer p) {
                               auto* self = static_cast<TextEditor*>(p);
                               if (self->restoringHistory)
                                   return;
                               GtkTextIter begin = *end;
                               gtk_text_iter_backward_chars(&begin, static_cast<int>(g_utf8_strlen(text, length)));
                               for (const auto& style: self->pendingStyles) {
                                   if (auto* tag = RichText::tag(b, style.first, style.second))
                                       gtk_text_buffer_apply_tag(b, tag, &begin, end);
                               }
                           }),
                           this);

    g_signal_connect(this->buffer.get(), "paste-done", G_CALLBACK(bufferPasteDoneCallback), this);

    {  // Get cursor blinking settings
        GtkSettings* settings = gtk_widget_get_settings(this->xournalWidget);
        g_object_get(settings, "gtk-cursor-blink", &this->cursorBlink, nullptr);
        if (this->cursorBlink) {
            int tmp = 0;
            g_object_get(settings, "gtk-cursor-blink-time", &tmp, nullptr);
            assert(tmp >= 0);
            auto cursorBlinkingPeriod = static_cast<unsigned int>(tmp);
            this->cursorBlinkingTimeOn = cursorBlinkingPeriod * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER;
            this->cursorBlinkingTimeOff = cursorBlinkingPeriod - this->cursorBlinkingTimeOn;
        }
    }

    gtk_im_context_set_client_window(this->imContext.get(), gtk_widget_get_parent_window(this->xournalWidget));
    gtk_im_context_focus_in(this->imContext.get());

    g_signal_connect(this->imContext.get(), "commit", G_CALLBACK(iMCommitCallback), this);
    g_signal_connect(this->imContext.get(), "preedit-changed", G_CALLBACK(iMPreeditChangedCallback), this);
    g_signal_connect(this->imContext.get(), "retrieve-surrounding", G_CALLBACK(iMRetrieveSurroundingCallback), this);
    g_signal_connect(this->imContext.get(), "delete-surrounding", G_CALLBACK(imDeleteSurroundingCallback), this);

    if (this->originalTextElement) {
        // If editing a preexisting text, put the cursor at the right location
        this->mousePressed(x - textElement->getX(), y - textElement->getY());
    } else if (this->cursorBlink) {
        blinkCallback(this);
    } else {
        this->cursorVisible = true;
    }
}

TextEditor::~TextEditor() {
    gtk_im_context_focus_out(this->imContext.get());

    this->xournalWidget = nullptr;
    control->setCopyCutEnabled(false);

    this->contentsChanged(true);

    finalizeEdition();
}

auto TextEditor::getViewPool() const -> const std::shared_ptr<xoj::util::DispatchPool<xoj::view::TextEditionView>>& {
    return viewPool;
}

auto TextEditor::getTextElement() const -> Text* { return this->textElement.get(); }

Text* TextEditor::getElementAfterEdition() {
    updateTextElementContent();
    if (originalTextElement && originalTextElement->getText() == textElement->getText() &&
        originalTextElement->getRichText().encode() == textElement->getRichText().encode() &&
        originalTextElement->getFontName() == textElement->getFontName() &&
        originalTextElement->getFontSize() == textElement->getFontSize() &&
        originalTextElement->getColor() == textElement->getColor())
        return originalTextElement;
    return textElement.get();
}

bool TextEditor::bufferEmpty() const { return gtk_text_buffer_get_char_count(this->buffer.get()) == 0; }

void TextEditor::replaceBufferContent(const std::string& text) {
    gtk_text_buffer_set_text(this->buffer.get(), text.c_str(), -1);

    GtkTextIter first = {nullptr};
    gtk_text_buffer_get_iter_at_offset(this->buffer.get(), &first, 0);
    gtk_text_buffer_place_cursor(this->buffer.get(), &first);
    this->layoutStatus = LayoutStatus::NEEDS_COMPLETE_UPDATE;
    this->cursorBox = computeCursorBox();
}

void TextEditor::setColor(Color color) {
    if (gtk_text_buffer_get_char_count(buffer.get()))
        applyStyle("foreground", Util::rgb_to_hex_string(color));
    else {
        this->textElement->setColor(color);
        contentsChanged(true);
        repaintEditor(false);
    }
}

void TextEditor::setFont(XojFont font) {
    if (gtk_text_buffer_get_char_count(buffer.get())) {
        applyStyles({{"font", font.getName()}, {"size", std::to_string(font.getSize())}});
    } else {
        this->textElement->setFont(font);
        afterFontChange();
        contentsChanged(true);
    }
}

void TextEditor::afterFontChange() {
    this->textElement->updatePangoFont(this->layout.get());
    this->computeVirtualCursorPosition();
    this->repaintEditor();
}

void TextEditor::iMCommitCallback(GtkIMContext* context, const gchar* str, TextEditor* te) {
    gtk_text_buffer_begin_user_action(te->buffer.get());

    bool hadSelection = gtk_text_buffer_get_has_selection(te->buffer.get());
    gtk_text_buffer_delete_selection(te->buffer.get(), true, true);

    if (!strcmp(str, "\n")) {
        std::string inserted = "\n";
        auto cursor = getIteratorAtCursor(te->buffer.get());
        auto start = cursor;
        gtk_text_iter_set_line_offset(&start, 0);
        char* raw = gtk_text_buffer_get_text(te->buffer.get(), &start, &cursor, true);
        std::string line(raw);
        g_free(raw);
        std::smatch match;
        static const std::regex list(R"(^([ \t]*)(• |☐ |☑ |([0-9]{1,6})\. )(.*)$)");
        if (std::regex_match(line, match, list)) {
            if (match[4].str().empty()) {
                gtk_text_buffer_delete(te->buffer.get(), &start, &cursor);
                inserted.clear();
            } else {
                std::string marker = match[2];
                if (match[3].matched)
                    marker = std::to_string(std::stoi(match[3]) + 1) + ". ";
                if (marker == "☑ ")
                    marker = "☐ ";
                inserted += match[1].str() + marker;
            }
        }
        if (!gtk_text_buffer_insert_interactive_at_cursor(te->buffer.get(), inserted.c_str(),
                                                          static_cast<int>(inserted.size()), true)) {
            gtk_widget_error_bell(te->xournalWidget);
        } else {
            te->contentsChanged(true);
        }
    } else {
        if (!hadSelection && te->cursorOverwrite) {
            auto insert = getIteratorAtCursor(te->buffer.get());
            if (!gtk_text_iter_ends_line(&insert)) {
                te->deleteFromCursor(GTK_DELETE_CHARS, 1);
            }
        }

        if (!gtk_text_buffer_insert_interactive_at_cursor(te->buffer.get(), str, -1, true)) {
            gtk_widget_error_bell(te->xournalWidget);
        }
    }

    gtk_text_buffer_end_user_action(te->buffer.get());
    te->contentsChanged();
    te->repaintEditor();
}

void TextEditor::iMPreeditChangedCallback(GtkIMContext* context, TextEditor* te) {
    xoj::util::OwnedCString str;
    gint cursor_pos = 0;
    GtkTextIter iter = getIteratorAtCursor(te->buffer.get());

    {
        PangoAttrList* attrs = nullptr;
        gtk_im_context_get_preedit_string(context, str.contentReplacer(), &attrs, &cursor_pos);
        if (attrs == nullptr) {
            attrs = pango_attr_list_new();
        }
        te->preeditAttrList.reset(attrs, xoj::util::adopt);
    }

    if (str && str[0] && !gtk_text_iter_can_insert(&iter, true)) {
        /*
         * Keypress events are passed to input method even if cursor position is
         * not editable; so beep here if it's multi-key input sequence, input
         * method will be reset in key-press-event handler.
         */
        gtk_widget_error_bell(te->xournalWidget);
        return;
    }

    te->preeditString = std::move(str);
    te->preeditCursor = cursor_pos;
    te->contentsChanged();
    te->repaintEditor();
}

auto TextEditor::iMRetrieveSurroundingCallback(GtkIMContext* context, TextEditor* te) -> bool {
    GtkTextIter start = getIteratorAtCursor(te->buffer.get());
    GtkTextIter end = start;

    gint pos = gtk_text_iter_get_line_index(&start);
    gtk_text_iter_set_line_offset(&start, 0);
    gtk_text_iter_forward_to_line_end(&end);

    auto text = xoj::util::OwnedCString::assumeOwnership(gtk_text_iter_get_slice(&start, &end));
    gtk_im_context_set_surrounding(context, text.get(), -1, pos);

    return true;
}

auto TextEditor::imDeleteSurroundingCallback(GtkIMContext* context, gint offset, gint n_chars, TextEditor* te) -> bool {
    GtkTextIter start = getIteratorAtCursor(te->buffer.get());
    GtkTextIter end = start;

    gtk_text_iter_forward_chars(&start, offset);
    gtk_text_iter_forward_chars(&end, offset + n_chars);

    gtk_text_buffer_delete_interactive(te->buffer.get(), &start, &end, true);

    te->contentsChanged();
    te->repaintEditor();

    return true;
}

auto TextEditor::onKeyPressEvent(GdkEventKey* event) -> bool {
    if (event->keyval == GDK_KEY_F4) {
        showFormattingDialog();
        return true;
    }
    if ((event->state & GDK_CONTROL_MASK) && !(event->state & GDK_MOD1_MASK)) {
        auto key = gdk_keyval_to_lower(event->keyval);
        if (key == GDK_KEY_z) {
            undoEdition(event->state & GDK_SHIFT_MASK);
            return true;
        }
        if (key == GDK_KEY_y) {
            undoEdition(true);
            return true;
        }
        if (key == GDK_KEY_i && !(event->state & GDK_SHIFT_MASK)) {
            applyStyle("italic", "1", true);
            return true;
        }
        if (key == GDK_KEY_u && !(event->state & GDK_SHIFT_MASK)) {
            applyStyle("underline", "1", true);
            return true;
        }
        if (key == GDK_KEY_v && (event->state & GDK_SHIFT_MASK)) {
            pasteFromClipboard(true);
            return true;
        }
        if (key == GDK_KEY_k) {
            editLink();
            return true;
        }
        if (key == GDK_KEY_e && (event->state & GDK_SHIFT_MASK)) {
            showFormattingDialog();
            return true;
        }
        if (key == GDK_KEY_t && (event->state & GDK_SHIFT_MASK)) {
            editTable();
            return true;
        }
        if (event->keyval == GDK_KEY_Return) {
            followLink();
            return true;
        }
    }
    if (tableColumns && !(event->state & GDK_CONTROL_MASK) &&
        (gdk_keyval_to_unicode(event->keyval) || event->keyval == GDK_KEY_BackSpace ||
         event->keyval == GDK_KEY_Delete)) {
        editTable();
        return true;
    }
    bool retval = false;
    bool obscure = false;

    GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();
    auto keymap = gdk_keymap_get_for_display(gdk_display_get_default());
    GdkModifierType consumed;
    /*
    According to https://docs.gtk.org/gdk3/method.Keymap.translate_keyboard_state.html
    consumed modifiers should be masked out. For instance, on a US keyboard, the plus symbol is shifted, so when
    comparing a key press to a <Control>plus accelerator <Shift> should be masked out.
    */
    gdk_keymap_translate_keyboard_state(keymap, event->hardware_keycode, static_cast<GdkModifierType>(event->state),
                                        event->group, nullptr, nullptr, nullptr, &consumed);

    GtkTextIter iter = getIteratorAtCursor(this->buffer.get());
    bool canInsert = gtk_text_iter_can_insert(&iter, true);

    // IME needs to handle the input first so the candidate window works correctly
    if (gtk_im_context_filter_keypress(this->imContext.get(), event)) {
        this->needImReset = true;
        if (!canInsert) {
            this->resetImContext();
        }
        obscure = canInsert;
        retval = true;
    } else if (gtk_bindings_activate_event(G_OBJECT(this->textWidget.get()), event)) {
        return true;
    } else if ((event->state & ~consumed & modifiers) == GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_b || event->keyval == GDK_KEY_B) {
            toggleBoldFace();
            return true;
        }
        if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add) {
            increaseFontSize();
            return true;
        }
        if (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) {
            decreaseFontSize();
            return true;
        }
    } else if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_ISO_Enter ||
               event->keyval == GDK_KEY_KP_Enter) {
        this->resetImContext();
        iMCommitCallback(nullptr, "\n", this);

        obscure = true;
        retval = true;
    }
    // Pass through Tab as literal tab, unless Control is held down
    else if ((event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_KP_Tab ||
              event->keyval == GDK_KEY_ISO_Left_Tab) &&
             !(event->state & GDK_CONTROL_MASK)) {
        resetImContext();
        iMCommitCallback(nullptr, "\t", this);
        obscure = true;
        retval = true;
    } else {
        retval = false;
    }

    if (obscure) {
        control->getCursor()->setInvisible(true);
    }

    return retval;
}

auto TextEditor::onKeyReleaseEvent(GdkEventKey* event) -> bool {
    GtkTextIter iter = getIteratorAtCursor(this->buffer.get());

    if (gtk_text_iter_can_insert(&iter, true) && gtk_im_context_filter_keypress(this->imContext.get(), event)) {
        this->needImReset = true;
        return true;
    }
    return false;
}

void TextEditor::toggleOverwrite() {
    this->cursorOverwrite = !this->cursorOverwrite;
    repaintCursorAfterChange();
}

/**
 * I know it's a bit rough and duplicated
 * Improve that later on...
 */
void TextEditor::decreaseFontSize() { adjustFontSize(-1); }
void TextEditor::increaseFontSize() { adjustFontSize(1); }
void TextEditor::adjustFontSize(double delta) {
    auto it = getIteratorAtCursor(buffer.get());
    int pos = gtk_text_iter_get_offset(&it);
    double size = textElement->getFontSize();
    for (const auto& span: RichText::fromBuffer(buffer.get()).spans)
        if (span.kind == "size" && span.start <= pos && span.end >= pos)
            size = g_ascii_strtod(span.value.c_str(), nullptr);
    applyStyle("size", std::to_string(std::clamp(size + delta, 1.0, 1000.0)));
}

void TextEditor::toggleBoldFace() { applyStyle("bold", "1", true); }

void TextEditor::selectAtCursor(TextEditor::SelectType ty) {
    GtkTextIter startPos;
    GtkTextIter endPos;
    gtk_text_buffer_get_selection_bounds(this->buffer.get(), &startPos, &endPos);
    const auto searchFlag = GTK_TEXT_SEARCH_TEXT_ONLY;  // To be used to find double newlines

    switch (ty) {
        case TextEditor::SelectType::WORD: {
            auto currentPos = getIteratorAtCursor(this->buffer.get());
            if (!gtk_text_iter_inside_word(&currentPos)) {
                // Do nothing if cursor is over whitespace
                return;
            }

            if (!gtk_text_iter_starts_word(&currentPos)) {
                gtk_text_iter_backward_word_start(&startPos);
            }
            if (!gtk_text_iter_ends_word(&currentPos)) {
                gtk_text_iter_forward_word_end(&endPos);
            }
            break;
        }
        case TextEditor::SelectType::PARAGRAPH:
            // Note that a GTK "paragraph" is a line, so there's no nice one-liner.
            // We define a paragraph as text separated by double newlines.
            while (!gtk_text_iter_is_start(&startPos)) {
                // There's no GTK function to go to line start, so do it manually.
                while (!gtk_text_iter_starts_line(&startPos)) {
                    if (!gtk_text_iter_backward_word_start(&startPos)) {
                        break;
                    }
                }
                // Check for paragraph start
                GtkTextIter searchPos = startPos;
                gtk_text_iter_backward_chars(&searchPos, 2);
                if (gtk_text_iter_backward_search(&startPos, "\n\n", searchFlag, nullptr, nullptr, &searchPos)) {
                    break;
                }
                gtk_text_iter_backward_line(&startPos);
            }
            while (!gtk_text_iter_ends_line(&endPos)) {
                gtk_text_iter_forward_to_line_end(&endPos);
                // Check for paragraph end
                GtkTextIter searchPos = endPos;
                gtk_text_iter_forward_chars(&searchPos, 2);
                if (gtk_text_iter_forward_search(&endPos, "\n\n", searchFlag, nullptr, nullptr, &searchPos)) {
                    break;
                }
                gtk_text_iter_forward_line(&endPos);
            }
            break;
        case TextEditor::SelectType::ALL:
            gtk_text_buffer_get_bounds(this->buffer.get(), &startPos, &endPos);
            break;
    }

    gtk_text_buffer_select_range(this->buffer.get(), &startPos, &endPos);

    // Selection highlighting is handled through Pango attributes
    this->layoutStatus = LayoutStatus::NEEDS_ATTRIBUTES_UPDATE;
    this->repaintEditor(false);
}

void TextEditor::moveCursor(GtkMovementStep step, int count, bool extendSelection) {
    resetImContext();

    // Not possible, but we have to handle the events, else the page gets scrolled
    //	if (step == GTK_MOVEMENT_PAGES) {
    //		if (!gtk_text_view_scroll_pages(text_view, count, extend_selection))
    //			gtk_widget_error_bell(GTK_WIDGET (text_view));
    //
    //		gtk_text_view_check_cursor_blink(text_view);
    //		gtk_text_view_pend_cursor_blink(text_view);
    //		return;
    //	} else if (step == GTK_MOVEMENT_HORIZONTAL_PAGES) {
    //		if (!gtk_text_view_scroll_hpages(text_view, count, extend_selection))
    //			gtk_widget_error_bell(GTK_WIDGET (text_view));
    //
    //		gtk_text_view_check_cursor_blink(text_view);
    //		gtk_text_view_pend_cursor_blink(text_view);
    //		return;
    //	}

    GtkTextIter insert = getIteratorAtCursor(this->buffer.get());
    GtkTextIter newplace = insert;

    bool updateVirtualCursor = true;

    switch (step) {
        case GTK_MOVEMENT_LOGICAL_POSITIONS:  // not used!?
            gtk_text_iter_forward_visible_cursor_positions(&newplace, count);
            break;
        case GTK_MOVEMENT_VISUAL_POSITIONS:
            if (count < 0) {
                gtk_text_iter_backward_cursor_position(&newplace);
            } else {
                gtk_text_iter_forward_cursor_position(&newplace);
            }
            break;

        case GTK_MOVEMENT_WORDS:
            if (count < 0) {
                gtk_text_iter_backward_visible_word_starts(&newplace, -count);
            } else if (count > 0) {
                if (!gtk_text_iter_forward_visible_word_ends(&newplace, count)) {
                    gtk_text_iter_forward_to_line_end(&newplace);
                }
            }
            break;

        case GTK_MOVEMENT_DISPLAY_LINES:
            updateVirtualCursor = false;
            jumpALine(&newplace, count);
            break;

        case GTK_MOVEMENT_PARAGRAPHS:
            if (count > 0) {
                if (!gtk_text_iter_ends_line(&newplace)) {
                    gtk_text_iter_forward_to_line_end(&newplace);
                    --count;
                }
                gtk_text_iter_forward_visible_lines(&newplace, count);
                gtk_text_iter_forward_to_line_end(&newplace);
            } else if (count < 0) {
                if (gtk_text_iter_get_line_offset(&newplace) > 0) {
                    gtk_text_iter_set_line_offset(&newplace, 0);
                }
                gtk_text_iter_forward_visible_lines(&newplace, count);
                gtk_text_iter_set_line_offset(&newplace, 0);
            }
            break;

        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
            if (count > 0) {
                if (!gtk_text_iter_ends_line(&newplace)) {
                    gtk_text_iter_forward_to_line_end(&newplace);
                }
            } else if (count < 0) {
                gtk_text_iter_set_line_offset(&newplace, 0);
            }
            break;

        case GTK_MOVEMENT_BUFFER_ENDS:
            if (count > 0) {
                gtk_text_buffer_get_end_iter(this->buffer.get(), &newplace);
            } else if (count < 0) {
                gtk_text_buffer_get_iter_at_offset(this->buffer.get(), &newplace, 0);
            }
            break;

        default:
            break;
    }

    // call moveCursor() even if the cursor hasn't moved, since it cancels the selection
    moveCursor(&newplace, extendSelection);

    if (updateVirtualCursor) {
        computeVirtualCursorPosition();
    }

    if (gtk_text_iter_equal(&insert, &newplace)) {
        gtk_widget_error_bell(this->xournalWidget);
    }
}

void TextEditor::findPos(GtkTextIter* iter, double xPos, double yPos) const {
    int index = 0;
    int trailing = 0;
    pango_layout_xy_to_index(this->getUpToDateLayout(), static_cast<int>(std::round(xPos * PANGO_SCALE)),
                             static_cast<int>(std::round(yPos * PANGO_SCALE)), &index, &trailing);
    /*
     * trailing is non-zero iff the abscissa is past the middle of the grapheme.
     * In this case, it contains the length of the grapheme in utf8 char count.
     * This way, we put the cursor after the grapheme when clicking past the middle of the grapheme.
     */
    *iter = getIteratorAtByteOffset(this->buffer.get(), index);
    gtk_text_iter_forward_chars(iter, trailing);
}

void TextEditor::updateTextElementContent() {
    auto rich = RichText::fromBuffer(buffer.get(), tableColumns);
    if (rich.valid())
        textElement->setRichText(std::move(rich));
}

void TextEditor::contentsChanged(bool forceCreateUndoAction) {
    if (!restoringHistory)
        recordHistory();
    this->layoutStatus = LayoutStatus::NEEDS_COMPLETE_UPDATE;
    this->computeVirtualCursorPosition();
}

void TextEditor::markPos(double x, double y, bool extendSelection) {
    GtkTextIter newplace = getIteratorAtCursor(this->buffer.get());

    findPos(&newplace, x, y);

    // call moveCursor() even if the cursor hasn't moved, since it cancels the selection
    moveCursor(&newplace, extendSelection);
    computeVirtualCursorPosition();
}

void TextEditor::mousePressed(double x, double y) {
    this->mouseDown = true;
    // Todo select if SHIFT is pressed
    markPos(x, y, false);
}

void TextEditor::mouseMoved(double x, double y) {
    if (this->mouseDown) {
        markPos(x, y, true);
    }
}

void TextEditor::mouseReleased() { this->mouseDown = false; }

void TextEditor::jumpALine(GtkTextIter* textIter, int count) {
    int cursorLine = gtk_text_iter_get_line(textIter);

    if (cursorLine + count < 0) {
        return;
    }

    PangoLayoutLine* line = pango_layout_get_line_readonly(this->layout.get(), cursorLine + count);
    if (line == nullptr) {
        return;
    }

    int index = 0;
    int trailing = 0;
    pango_layout_line_x_to_index(line, this->virtualCursorAbscissa, &index, &trailing);
    /*
     * trailing is non-zero iff the abscissa is past the middle of the grapheme.
     * In this case, it contains the length of the grapheme in utf8 char count.
     */
    *textIter = getIteratorAtByteOffset(this->buffer.get(), index);
    gtk_text_iter_forward_chars(textIter, trailing);
}

void TextEditor::computeVirtualCursorPosition() {
    int offset = getByteOffsetOfCursor(this->buffer.get());

    PangoRectangle rect = {0};
    pango_layout_index_to_pos(this->getUpToDateLayout(), offset, &rect);
    this->virtualCursorAbscissa = rect.x;
}

void TextEditor::moveCursor(const GtkTextIter* newLocation, gboolean extendSelection) {
    bool selectionChanged = true;
    if (extendSelection) {
        if (auto oldLoc = getIteratorAtCursor(this->buffer.get()); gtk_text_iter_equal(newLocation, &oldLoc)) {
            // Nothing changed
            return;
        }
        gtk_text_buffer_move_mark_by_name(this->buffer.get(), "insert", newLocation);
        control->setCopyCutEnabled(true);
    } else {
        // if !extendSelection, we clear the selection even if the cursor does not move
        selectionChanged = gtk_text_buffer_get_has_selection(this->buffer.get());
        gtk_text_buffer_place_cursor(this->buffer.get(), newLocation);
        control->setCopyCutEnabled(false);
    }

    if (this->cursorBlink) {
        // Whenever the cursor moves, the blinking cycle restarts from the start (i.e. the cursor is first shown).
        this->cursorVisible = false;  // Will be toggled to true by BlinkTimer::callback before the repaint
        blinkCallback(this);
    }

    if (selectionChanged) {
        // The selection background color is set through Pango attributes
        this->layoutStatus = LayoutStatus::NEEDS_ATTRIBUTES_UPDATE;
        // Repaint the entire box. Computing the exact area that was (un)selected would be better but complicated
        this->repaintEditor(false);
    } else {
        repaintCursorAfterChange();
    }
}

void TextEditor::updateCursorBox() {
    this->cursorBox = computeCursorBox();

    if (!viewPool->empty()) {
        // Inform the IM of the cursor location (for word selection popup's location)
        // We use the first view as the main view, as far as the IM is concerned
        auto box = viewPool->front().toWindowCoordinates(xoj::util::Rectangle<double>(this->cursorBox));

        GdkRectangle cursorRect;  // cursor position in window coordinates
        cursorRect.x = static_cast<int>(box.x);
        cursorRect.y = static_cast<int>(box.y);
        cursorRect.height = static_cast<int>(box.height);
        cursorRect.width = static_cast<int>(box.width);
        gtk_im_context_set_cursor_location(this->imContext.get(), &cursorRect);
    }
}

static auto whitespace(gunichar ch, gpointer user_data) -> gboolean { return (ch == ' ' || ch == '\t'); }

static auto not_whitespace(gunichar ch, gpointer user_data) -> gboolean { return !whitespace(ch, user_data); }

static auto find_whitepace_region(const GtkTextIter* center, GtkTextIter* start, GtkTextIter* end) -> gboolean {
    *start = *center;
    *end = *center;

    if (gtk_text_iter_backward_find_char(start, not_whitespace, nullptr, nullptr)) {
        gtk_text_iter_forward_char(start); /* we want the first whitespace... */
    }
    if (whitespace(gtk_text_iter_get_char(end), nullptr)) {
        gtk_text_iter_forward_find_char(end, not_whitespace, nullptr, nullptr);
    }

    return !gtk_text_iter_equal(start, end);
}

void TextEditor::deleteFromCursor(GtkDeleteType type, int count) {

    this->resetImContext();

    if (type == GTK_DELETE_CHARS) {
        // Char delete deletes the selection, if one exists
        if (gtk_text_buffer_delete_selection(this->buffer.get(), true, true)) {
            this->contentsChanged(true);
            this->repaintEditor();
            return;
        }
    }

    GtkTextIter insert = getIteratorAtCursor(this->buffer.get());

    GtkTextIter start = insert;
    GtkTextIter end = insert;

    switch (type) {
        case GTK_DELETE_CHARS:
            gtk_text_iter_forward_cursor_positions(&end, count);
            break;

        case GTK_DELETE_WORD_ENDS:
            if (count > 0) {
                gtk_text_iter_forward_word_ends(&end, count);
            } else if (count < 0) {
                gtk_text_iter_backward_word_starts(&start, 0 - count);
            }
            break;

        case GTK_DELETE_WORDS:
            break;

        case GTK_DELETE_DISPLAY_LINE_ENDS:
            break;

        case GTK_DELETE_DISPLAY_LINES:
            break;

        case GTK_DELETE_PARAGRAPH_ENDS:
            if (count > 0) {
                /* If we're already at a newline, we need to
                 * simply delete that newline, instead of
                 * moving to the next one.
                 */
                if (gtk_text_iter_ends_line(&end)) {
                    gtk_text_iter_forward_line(&end);
                    --count;
                }

                while (count > 0) {
                    if (!gtk_text_iter_forward_to_line_end(&end)) {
                        break;
                    }

                    --count;
                }
            } else if (count < 0) {
                if (gtk_text_iter_starts_line(&start)) {
                    gtk_text_iter_backward_line(&start);
                    if (!gtk_text_iter_ends_line(&end)) {
                        gtk_text_iter_forward_to_line_end(&start);
                    }
                } else {
                    gtk_text_iter_set_line_offset(&start, 0);
                }
                ++count;

                gtk_text_iter_backward_lines(&start, -count);
            }
            break;

        case GTK_DELETE_PARAGRAPHS:
            if (count > 0) {
                gtk_text_iter_set_line_offset(&start, 0);
                gtk_text_iter_forward_to_line_end(&end);

                /* Do the lines beyond the first. */
                while (count > 1) {
                    gtk_text_iter_forward_to_line_end(&end);
                    --count;
                }
            }

            break;

        case GTK_DELETE_WHITESPACE: {
            find_whitepace_region(&insert, &start, &end);
        } break;

        default:
            break;
    }

    if (!gtk_text_iter_equal(&start, &end)) {
        gtk_text_buffer_begin_user_action(this->buffer.get());

        if (!gtk_text_buffer_delete_interactive(this->buffer.get(), &start, &end, true)) {
            gtk_widget_error_bell(this->xournalWidget);
        }

        gtk_text_buffer_end_user_action(this->buffer.get());
    } else {
        gtk_widget_error_bell(this->xournalWidget);
    }

    this->contentsChanged();
    this->repaintEditor();
}

void TextEditor::backspace() {

    resetImContext();

    // Backspace deletes the selection, if one exists
    if (gtk_text_buffer_delete_selection(this->buffer.get(), true, true)) {
        this->contentsChanged();
        this->repaintEditor();
        return;
    }

    GtkTextIter insert = getIteratorAtCursor(this->buffer.get());

    if (gtk_text_buffer_backspace(this->buffer.get(), &insert, true, true)) {
        this->contentsChanged();
        this->repaintEditor();
    } else {
        gtk_widget_error_bell(this->xournalWidget);
    }
}

void TextEditor::copyToClipboard() const {
    GtkClipboard* clipboard = gtk_widget_get_clipboard(this->xournalWidget, GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_copy_clipboard(this->buffer.get(), clipboard);
}

void TextEditor::cutToClipboard() {
    GtkClipboard* clipboard = gtk_widget_get_clipboard(this->xournalWidget, GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_cut_clipboard(this->buffer.get(), clipboard, true);

    this->contentsChanged(true);
    this->repaintEditor();
}

void TextEditor::pasteFromClipboard(bool plain) {
    GtkClipboard* clipboard = gtk_widget_get_clipboard(this->xournalWidget, GDK_SELECTION_CLIPBOARD);
    if (plain) {
        char* content = gtk_clipboard_wait_for_text(clipboard);
        if (content) {
            gtk_text_buffer_delete_selection(buffer.get(), true, true);
            auto a = getIteratorAtCursor(buffer.get());
            int start = gtk_text_iter_get_offset(&a);
            std::string normalized(content);
            for (size_t pos = 0; (pos = normalized.find("\r\n", pos)) != std::string::npos;)
                normalized.replace(pos, 2, "\n");
            std::replace(normalized.begin(), normalized.end(), '\r', '\n');
            gtk_text_buffer_insert(buffer.get(), &a, normalized.c_str(), -1);
            auto b = a;
            gtk_text_buffer_get_iter_at_offset(buffer.get(), &a, start);
            gtk_text_buffer_remove_all_tags(buffer.get(), &a, &b);
            g_free(content);
            contentsChanged(true);
            repaintEditor();
        }
        return;
    }
    gtk_text_buffer_paste_clipboard(this->buffer.get(), clipboard, nullptr, true);
}

void TextEditor::bufferPasteDoneCallback(GtkTextBuffer* buffer, GtkClipboard* clipboard, TextEditor* te) {
    te->contentsChanged(true);
    te->repaintEditor();
}

void TextEditor::resetImContext() {
    if (this->needImReset) {
        this->needImReset = false;
        gtk_im_context_reset(this->imContext.get());
    }
}

/*
 * Blink!
 */
auto TextEditor::blinkCallback(TextEditor* te) -> bool {
    te->cursorVisible = !te->cursorVisible;
    auto time = te->cursorVisible ? te->cursorBlinkingTimeOn : te->cursorBlinkingTimeOff;
    te->blinkTimer = gdk_threads_add_timeout(time, xoj::util::wrap_for_once_v<blinkCallback>, te);

    Range dirtyRange = te->cursorBox;
    dirtyRange.translate(te->textElement->getX(), te->textElement->getY());
    te->viewPool->dispatch(xoj::view::TextEditionView::FLAG_DIRTY_REGION, dirtyRange);

    // Remove ourselves
    return false;
}

void TextEditor::setTextToPangoLayout(PangoLayout* pl) const {
    auto rich = RichText::fromBuffer(buffer.get(), tableColumns);
    rich.configureLayout(pl, textElement->getFontSize());
    std::string_view preed(preeditString);

    if (!preed.empty()) {
        // When using an Input Method, we need to insert the preeditString into the text at the cursor location
        std::string txt = cloneWithInsertToStdString(this->buffer.get(), preed);

        int pos = getByteOffsetOfCursor(this->buffer.get());
        xoj::util::PangoAttrListSPtr attrlist(RichText::fromBuffer(buffer.get()).attributes(), xoj::util::adopt);
        pango_attr_list_splice(attrlist.get(), this->preeditAttrList.get(), pos, static_cast<int>(preed.length()));

        pango_layout_set_attributes(pl, attrlist.get());

        pango_layout_set_text(pl, txt.c_str(), static_cast<int>(txt.length()));
    } else {
        setSelectionAttributesToPangoLayout(pl);
        pango_layout_set_text(pl, cloneToCString(this->buffer.get()).get(), -1);
    }
}

Color TextEditor::getSelectionColor() const { return this->control->getSettings()->getSelectionColor(); }

void TextEditor::setSelectionAttributesToPangoLayout(PangoLayout* pl) const {
    xoj::util::PangoAttrListSPtr attrlist(RichText::fromBuffer(buffer.get()).attributes(), xoj::util::adopt);

    GtkTextIter start;
    GtkTextIter end;
    bool hasSelection = gtk_text_buffer_get_selection_bounds(this->buffer.get(), &start, &end);

    if (hasSelection) {
        auto selectionColorU16 = Util::argb_to_ColorU16(this->getSelectionColor());
        PangoAttribute* attrib =
                pango_attr_background_new(selectionColorU16.red, selectionColorU16.green, selectionColorU16.blue);
        attrib->start_index = static_cast<unsigned int>(getByteOffsetOfIterator(start));
        attrib->end_index = static_cast<unsigned int>(getByteOffsetOfIterator(end));

        pango_attr_list_insert(attrlist.get(), attrib);  // attrlist takes ownership of attrib
    }

    pango_layout_set_attributes(pl, attrlist.get());
}

auto TextEditor::computeBoundingBox() const -> Range {
    /*
     * NB: we cannot rely on Text::calcSize directly, since it would not take the size changes due to the IM
     * preeditString into account.
     */
    int w = 0;
    int h = 0;
    pango_layout_get_size(getUpToDateLayout(), &w, &h);
    double width = std::max((static_cast<double>(w)) / PANGO_SCALE,
                            textElement->getRichText().tableWidth(getUpToDateLayout()));
    double height = (static_cast<double>(h)) / PANGO_SCALE;
    double x = textElement->getX();
    double y = textElement->getY();

    // Warning: width can be negative (e.g. for languages written from right to left)
    Range res(x, y);
    res.addPoint(x + width, y + height);
    return res;
}

auto TextEditor::getUpToDateLayout() const -> PangoLayout* {
    switch (layoutStatus) {
        case LayoutStatus::NEEDS_COMPLETE_UPDATE:
            setTextToPangoLayout(this->layout.get());
            break;
        case LayoutStatus::NEEDS_ATTRIBUTES_UPDATE:
            setSelectionAttributesToPangoLayout(this->layout.get());
            break;
        case LayoutStatus::UP_TO_DATE:
            break;
    }
    layoutStatus = LayoutStatus::UP_TO_DATE;
    return this->layout.get();
}

auto TextEditor::getCursorBox() const -> const Range& { return this->cursorBox; }

auto TextEditor::getContentBoundingBox() const -> const Range& { return this->previousBoundingBox; }

bool TextEditor::isCursorVisible() const { return cursorVisible; }

auto TextEditor::computeCursorBox() const -> Range {
    // Compute the bounding box of the active grapheme (i.e. the one just after the cursor)
    int offset = getByteOffsetOfCursor(this->buffer.get());
    if (this->preeditString && this->preeditCursor != 0) {
        const gchar* preeditText = this->preeditString.get();
        offset += static_cast<int>(g_utf8_offset_to_pointer(preeditText, preeditCursor) - preeditText);
    }
    PangoRectangle rect = {0};
    pango_layout_index_to_pos(getUpToDateLayout(), offset, &rect);
    const double ratio = 1.0 / PANGO_SCALE;

    // Warning: rect.width could be negative (e.g. for languages written from right to left).
    Range res(rect.x * ratio, rect.y * ratio);
    res.addPoint((rect.x + (cursorOverwrite ? rect.width : 0.0)) * ratio, (rect.y + rect.height) * ratio);
    return res;
}

void TextEditor::repaintEditor(bool sizeChanged) {
    Range dirtyRange(this->previousBoundingBox);
    if (sizeChanged) {
        this->previousBoundingBox = this->computeBoundingBox();
        dirtyRange = dirtyRange.unite(this->previousBoundingBox);
    }
    this->updateCursorBox();
    this->viewPool->dispatch(xoj::view::TextEditionView::FLAG_DIRTY_REGION, dirtyRange);
}

void TextEditor::repaintCursorAfterChange() {
    Range dirtyRange = this->cursorBox;
    this->updateCursorBox();
    dirtyRange = dirtyRange.unite(this->cursorBox);
    dirtyRange.translate(this->textElement->getX(), this->textElement->getY());
    this->viewPool->dispatch(xoj::view::TextEditionView::FLAG_DIRTY_REGION, dirtyRange);
}

void TextEditor::finalizeEdition() {

    auto* doc = this->control->getDocument();
    UndoRedoHandler* undo = this->control->getUndoRedoHandler();

    this->control->setFontSelected(this->control->getSettings()->getFont());

    if (this->bufferEmpty()) {
        // Delete the edited element from layer
        if (originalTextElement) {
            auto eraseDeleteUndoAction = std::make_unique<DeleteUndoAction>(page, true);
            doc->lock();
            Layer* layer = this->page->getSelectedLayer();
            auto elementIndex = layer->removeElement(originalTextElement, false);
            doc->unlock();
            if (elementIndex != Element::InvalidIndex) [[likely]] {
                eraseDeleteUndoAction->addElement(layer, originalTextElement, elementIndex);
                undo->addUndoAction(std::move(eraseDeleteUndoAction));
            }  // A warning has already been issued otherwise
            originalTextElement = nullptr;
        }
        this->viewPool->dispatchAndClear(xoj::view::TextEditionView::FINALIZATION_REQUEST, this->previousBoundingBox);
        return;
    }

    this->updateTextElementContent();
    if (originalTextElement && getElementAfterEdition() == originalTextElement) {
        originalTextElement->setInEditing(false);
        viewPool->dispatchAndClear(xoj::view::TextEditionView::FINALIZATION_REQUEST, previousBoundingBox);
        page->fireElementChanged(originalTextElement);
        return;
    }
    if (originalTextElement) {
        // Modifying a preexisting element
        this->viewPool->dispatchAndClear(xoj::view::TextEditionView::FINALIZATION_REQUEST, this->previousBoundingBox);

        doc->lock();
        Layer* layer = this->page->getSelectedLayer();
        auto index = layer->removeElement(this->originalTextElement, false);
        layer->insertElement(this->textElement.get(), index);
        doc->unlock();

        this->page->fireElementChanged(this->textElement.get());

        if (index != Element::InvalidIndex) [[likely]] {
            this->originalTextElement->setInEditing(false);
            undo->addUndoAction(std::make_unique<TextBoxUndoAction>(this->page, layer, this->textElement.release(),
                                                                    this->originalTextElement));
        } else {
            // A warning has already been issued
            undo->addUndoAction(std::make_unique<InsertUndoAction>(this->page, layer, this->textElement.release()));
        }
        originalTextElement = nullptr;
    } else {
        // Creating a new element
        doc->lock();
        Layer* layer = this->page->getSelectedLayer();
        layer->addElement(textElement.get());
        doc->unlock();
        this->viewPool->dispatchAndClear(xoj::view::TextEditionView::FINALIZATION_REQUEST, this->previousBoundingBox);
        this->page->fireElementChanged(textElement.get());
        undo->addUndoAction(std::make_unique<InsertUndoAction>(page, layer, textElement.release()));
    }
}

void TextEditor::initializeEditionAt(double x, double y) {
    // Is there already a textfield?
    Text* text = nullptr;

    // Should we reverse this loop to select the most recent text rather than the oldest?
    const auto& elements = this->page->getSelectedLayer()->getElements();
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        Element* e = *it;
        if (e->getType() == ELEMENT_TEXT) {
            GdkRectangle matchRect = {gint(x), gint(y), 1, 1};
            if (e->intersectsArea(&matchRect)) {
                text = dynamic_cast<Text*>(e);
                break;
            }
        }
    }

    if (text == nullptr) {
        ToolHandler* h = this->control->getToolHandler();
        this->textElement = std::make_unique<Text>();
        this->textElement->setColor(h->getColor());
        this->textElement->setFont(control->getSettings()->getFont());
        this->textElement->setX(x);
        this->textElement->setY(y - this->textElement->getElementHeight() / 2);

        if (auto audioController = control->getAudioController(); audioController && audioController->isRecording()) {
            fs::path audioFilename = audioController->getAudioFilename();
            size_t sttime = audioController->getStartTime();
            size_t milliseconds = ((g_get_monotonic_time() / 1000) - sttime);
            this->textElement->setTimestamp(milliseconds);
            this->textElement->setAudioFilename(audioFilename);
        }
        this->originalTextElement = nullptr;
    } else {
        this->control->setFontSelected(text->getFont());
        this->originalTextElement = text;

        // The original code did not use clone here (but did the cloning by hand). Why?
        this->textElement.reset(text->clone());

        /*
         * Original code
         *
        this->textElement = std::make_unique<Text>();
        this->textElement->setX(text->getX());
        this->textElement->setY(text->getY());
        this->textElement->setColor(text->getColor());
        this->textElement->setFont(text->getFont());
        this->textElement->setText(text->getText());
        this->textElement->setTimestamp(text->getTimestamp());
        this->textElement->setAudioFilename(text->getAudioFilename());
        **/

        text->setInEditing(true);
        this->page->fireElementChanged(text);
    }
    this->layout = this->textElement->createPangoLayout();
    this->previousBoundingBox = Range(this->textElement->boundingRect());
    this->replaceBufferContent(this->textElement->getText());
}
