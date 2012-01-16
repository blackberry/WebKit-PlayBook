/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */

#include "config.h"
#include "Pasteboard.h"

#include "DocumentFragment.h"
#include "Frame.h"
#include "KURL.h"
#include "markup.h"
#include "PlatformString.h"
#include "Range.h"

#if OS(QNX)
#include <clipboard/clipboard.h>
#endif

#include "NotImplemented.h"

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

Pasteboard::Pasteboard()
{
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

void Pasteboard::clear()
{
#if OS(QNX)
    empty_clipboard();
#else
    notImplemented();
#endif
}

void Pasteboard::writeImage(Node*, KURL const&, String const&)
{
    notImplemented();
}

void Pasteboard::writeSelection(Range* selectedRange, bool, Frame* frame)
{
#if OS(QNX)
    clear();
    char* text = strdup(frame->editor()->selectedText().utf8().data());
    char* markup = strdup(createMarkup(selectedRange, 0, AnnotateForInterchange).utf8().data());
    set_clipboard_data("text", strlen(text), text);
    set_clipboard_data("html", strlen(markup), markup);
#else
    UNUSED_PARAM(selectedRange);
    UNUSED_PARAM(frame);
    notImplemented();
#endif
}

void Pasteboard::writeURL(KURL const& url, String const&, Frame*)
{
    ASSERT(!url.isEmpty());

#if OS(QNX)
    clear();
    CString utf8 = url.string().utf8();
    set_clipboard_data("url", utf8.length(), utf8.data());
#else
    UNUSED_PARAM(url);
    notImplemented();
#endif
}

void Pasteboard::writePlainText(const String& text)
{
#if OS(QNX)
    clear();
    CString utf8 = text.utf8();
    set_clipboard_data("text", utf8.length(), utf8.data());
#else
    UNUSED_PARAM(text);
    notImplemented();
#endif
}

String Pasteboard::plainText(Frame*)
{
#if OS(QNX)
    char* utf8 = 0;
    get_clipboard_data("text", &utf8);

    if (!utf8)
        return String();

    String text = String::fromUTF8(utf8);
    free(utf8);

    return text;
#else
    notImplemented();
    return String();
#endif
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
#if OS(QNX)
    chosePlainText = false;

    // Note:  We are able to check if the format exists prior to reading but the check & the early return
    // path of get_clipboard_data are the exact same, so just use get_clipboard_data and validate the
    // return value to determine if the data was present.
    char* htmlChar = 0;
    get_clipboard_data("html", &htmlChar);
    String html = String::fromUTF8(htmlChar);
    if (!html.isEmpty()) {
        char* urlChar = 0;
        get_clipboard_data("url", &urlChar);

        RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), html, String::fromUTF8(urlChar), FragmentScriptingNotAllowed);

        if (htmlChar)
            free(htmlChar);

        if (urlChar)
            free(urlChar);

        if (fragment)
            return fragment.release();
    }

    if (!allowPlainText)
        return 0;

    char* textChar = 0;
    get_clipboard_data("text", &textChar);
    String text = String::fromUTF8(textChar);

    RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), text);

    if (textChar)
        free(textChar);

    if (fragment) {
        chosePlainText = true;
        return fragment.release();
    }
#else
    UNUSED_PARAM(frame);
    UNUSED_PARAM(context);
    UNUSED_PARAM(allowPlainText);
    UNUSED_PARAM(chosePlainText);
    notImplemented();
#endif
    return 0;
}

} // namespace WebCore
