/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#ifndef DumpRenderTreeClient_h
#define DumpRenderTreeClient_h

#include "BlackBerryGlobal.h"

#include "PlatformString.h"
#include "Timer.h"
#include "wtf/Vector.h"

namespace WebCore {
class Frame;
class DOMWrapperWorld;
class Range;
class SecurityOrigin;
}

namespace Olympia {
namespace WebKit {
class WebPage;

class OLYMPIA_EXPORT DumpRenderTreeClient {
public:
    virtual void runTests() = 0;

    // FrameLoaderClient delegates
    virtual void didStartProvisionalLoadForFrame(WebCore::Frame* frame) = 0;
    virtual void didCommitLoadForFrame(WebCore::Frame* frame) = 0;
    virtual void didFailProvisionalLoadForFrame(WebCore::Frame* frame) = 0;
    virtual void didFailLoadForFrame(WebCore::Frame* frame) = 0;
    virtual void didFinishLoadForFrame(WebCore::Frame* frame) = 0;
    virtual void didFinishDocumentLoadForFrame(WebCore::Frame* frame) = 0;
    virtual void didClearWindowObjectInWorld(WebCore::DOMWrapperWorld* world) = 0;
    virtual void didReceiveTitleForFrame(const WTF::String& title, WebCore::Frame* frame) = 0;

    // ChromeClient delegates
    virtual void addMessageToConsole(const WTF::String& message, unsigned int lineNumber, const WTF::String& sourceID) = 0;
    virtual void runJavaScriptAlert(const WTF::String& message) = 0;
    virtual bool runJavaScriptConfirm(const WTF::String& message) = 0;
    virtual WTF::String runJavaScriptPrompt(const WTF::String& message, const WTF::String& defaultValue) = 0;
    virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message) = 0;
    virtual void setStatusText(const WTF::String& status) = 0;
    virtual void exceededDatabaseQuota(WebCore::SecurityOrigin* origin, const WTF::String& name) = 0;

    // EditorClient delegates
    virtual void setAcceptsEditing(bool acceptsEditing) = 0;
    virtual void didBeginEditing() = 0;
    virtual void didEndEditing() = 0;
    virtual void didChange() = 0;
    virtual void didChangeSelection() = 0;
    virtual bool shouldBeginEditingInDOMRange(WebCore::Range* range) = 0;
    virtual bool shouldEndEditingInDOMRange(WebCore::Range* range) = 0;
    virtual bool shouldDeleteDOMRange(WebCore::Range* range) = 0;
    virtual bool shouldChangeSelectedDOMRangeToDOMRangeAffinityStillSelecting(WebCore::Range* fromRange, WebCore::Range* toRange, int affinity, bool stillSelecting) = 0;

};
}
}

#endif // DumpRenderTreeClient_h
