/*
 * Copyright (C) Research In Motion Limited 2009-2011. All rights reserved.
 */

#ifndef InputHandler_h
#define InputHandler_h

#include "CSSMutableStyleDeclaration.h"
#include "HTMLInputElement.h"
#include "BlackBerryPlatformInputEvents.h"
#include "BlackBerryPlatformPrimitives.h"
#if OS(BLACKBERRY)
#include "OlympiaPlatformReplaceText.h"
#endif
#include "PlatformKeyboardEvent.h"
#include "Timer.h"

#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WTF {
class String;
}

namespace WebCore {
class Element;
class Frame;
class HTMLSelectElement;
class Node;
}

namespace Olympia {
namespace WebKit {

class WebPagePrivate;
typedef WTF::HashSet<RefPtr<WebCore::Element> > ElementHash;

class InputHandler {
public:
    InputHandler(WebPagePrivate*);
    ~InputHandler();

    enum FocusElementType { TextEdit, TextPopup /*Date/Time & Color*/, SelectPopup, Plugin };
    enum CaretScrollType { CenterAlways, CenterIfNeeded, EdgeIfNeeded };

    void nodeFocused(WebCore::Node*);
    void nodeTextChanged(const WebCore::Node*);
    void selectionChanged();
    void frameUnloaded(WebCore::Frame*);

#if OS(BLACKBERRY)
    Olympia::Platform::ReplaceTextErrorCode replaceText(const Olympia::Platform::ReplaceArguments& replaceArguments, const Olympia::Platform::AttributedText& attributedText);
#endif

    bool handleKeyboardInput(WebCore::PlatformKeyboardEvent::Type, const unsigned short character, unsigned modifiers = 0);
    void handleNavigationMove(const unsigned short character, bool shiftDown, bool altDown);
    void requestElementText(int requestedFrameId, int requestedElementId, int offset, int length);

    void deleteSelection();
    void insertText(const WTF::String&);
    void clearField();

    void cut();
    void copy();
    void paste();

    void setCaretPosition(int requestedFrameId, int requestedElementId, int caretPosition);
    void cancelSelection();

    void setInputValue(const WTF::String&);

    Olympia::Platform::IntRect rectForCaret(int index);

    bool selectionAtStartOfElement();
    bool selectionAtEndOfElement();
    bool isInputMode() { return isActiveTextEdit(); }

    void setNavigationMode(bool active, bool sendMessage = true);

    void ensureFocusElementVisible(bool centerFieldInDisplay = true);
    void handleInputLocaleChanged(bool isRTL);

    /* PopupMenu Methods */
    bool willOpenPopupForNode(WebCore::Node*);
    bool didNodeOpenPopup(WebCore::Node*);
    bool openLineInputPopup(WebCore::HTMLInputElement*);
    bool openSelectPopup(WebCore::HTMLSelectElement*);
    void setPopupListIndex(int index);
    void setPopupListIndexes(int size, bool* selecteds);

    bool processingChange() { return m_processingChange; }
    void setProcessingChange(bool processingChange) { m_processingChange = processingChange; }

private:
    void setElementFocused(WebCore::Element*);
    void setPluginFocused(WebCore::Element*);
    void setElementUnfocused(bool refocusOccuring = false);

    void ensureFocusTextElementVisible(CaretScrollType scrollType = CenterAlways);
    void ensureFocusPluginElementVisible();

    void clearCurrentFocusElement();

    bool isActiveTextEdit() { return m_currentFocusElement && m_currentFocusElementType == TextEdit; }
    bool isActiveTextPopup() { return m_currentFocusElement && m_currentFocusElementType == TextPopup; }
    bool isActiveSelectPopup() { return m_currentFocusElement && m_currentFocusElementType == SelectPopup; }
    bool isActivePlugin() { return m_currentFocusElement && m_currentFocusElementType == Plugin; }


    bool openDatePopup(WebCore::HTMLInputElement*, Olympia::Platform::OlympiaInputType);
    bool openColorPopup(WebCore::HTMLInputElement*);

    void addElementToFrameSet(WebCore::Element*);
    void cleanFrameSet(WebCore::Frame*);

    bool validElement(WebCore::Frame*, const WebCore::Element*);

    bool executeTextEditCommand(const WTF::String&);

    WTF::String elementText(WebCore::Element*);
    Olympia::Platform::OlympiaInputType elementType(const WebCore::Element*);
    bool isElementTypePlugin(const WebCore::Element*);

    unsigned int selectionStart();
    unsigned int selectionEnd();
    void setSelection(unsigned int selectionStart, unsigned int selectionEnd);
    RefPtr<WebCore::Range> subrangeForFocusElement(unsigned int offsetStart, unsigned int offsetLength);
    RefPtr<WebCore::Range> focusElementRange(unsigned int offsetStart, unsigned int offsetLength);

#if OS(BLACKBERRY)
    Olympia::Platform::ReplaceTextErrorCode processReplaceText(const Olympia::Platform::ReplaceArguments& replaceArguments, const Olympia::Platform::AttributedText& attributedText);

    void addAttributedTextMarker(unsigned int startPosition, unsigned int length, uint64_t attributes);
    void removeAttributedTextMarker();
#endif

#if OS(BLACKBERRY)
    void applyPasswordFieldUnmaskSecure(const WebCore::Element* element, int startUnmask, int lengthUnmask);

    void notifyTextChanged(const WebCore::Element*);
    bool setCursorPositionIfInputTextValidated(const WTF::String&, unsigned int desiredCursorPosition);

    void syncWatchdogFired(WebCore::Timer<InputHandler>*);
    void syncAckReceived();
    void syncMessageSent();
#endif
    bool allowSyncInput();

    WebPagePrivate* m_webPage;

    RefPtr<WebCore::Element> m_currentFocusElement;
    WTF::HashMap<WebCore::Frame*, ElementHash> m_frameElements;

    bool m_processingChange;
    bool m_navigationMode;
    FocusElementType m_currentFocusElementType;

#if OS(BLACKBERRY)
    int m_numberOfOutstandingSyncMessages;
    WebCore::Timer<InputHandler>* m_syncWatchdogTimer;
#endif
};

}
}

#endif // InputHandler_h
