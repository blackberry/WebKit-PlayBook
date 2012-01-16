/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */

#include "config.h"
#include "InspectorClientBlackBerry.h"

#include "BackingStore.h"
#include "Frame.h"
#include "FrameView.h"
#include "IntRect.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include "WebPage.h"
#include "WebPageClient.h"
#include "WebPage_p.h"

namespace WebCore {

InspectorClientBlackBerry::InspectorClientBlackBerry(Olympia::WebKit::WebPage* page)
    : m_page(page)
{
    m_inspectorSettingsMap.set(new SettingsMap);
}

void InspectorClientBlackBerry::inspectorDestroyed()
{
    delete this;
}

Page* InspectorClientBlackBerry::createPage()
{
    notImplemented();
    return 0;
}

String InspectorClientBlackBerry::localizedStringsURL()
{
    notImplemented();
    return String();
}

String InspectorClientBlackBerry::hiddenPanels()
{
    notImplemented();
    return String();
}

void InspectorClientBlackBerry::showWindow()
{
    notImplemented();
}

void InspectorClientBlackBerry::closeWindow()
{
    notImplemented();
}

void InspectorClientBlackBerry::attachWindow()
{
    notImplemented();
}

void InspectorClientBlackBerry::detachWindow()
{
    notImplemented();
}

void InspectorClientBlackBerry::setAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void InspectorClientBlackBerry::highlight(Node* node)
{
    if (node) {
        m_lastHighlightRect = m_highlightRect;
        m_highlightRect = m_page->d->mapFromContentsToViewport(node->getRect());
        hideHighlight();
    }
}

void InspectorClientBlackBerry::hideHighlight()
{
    if (m_page && m_page->d->m_mainFrame->view()) {
        IntRect repaintRect = unionRect(m_lastHighlightRect, m_highlightRect);
        if (repaintRect.isEmpty())
            return;
        if (m_page->backingStore())
            m_page->backingStore()->repaint(repaintRect.x(), repaintRect.y(), repaintRect.width(), repaintRect.height(), true, true);
    }
}

void InspectorClientBlackBerry::inspectedURLChanged(const String&)
{
    notImplemented();
}

void InspectorClientBlackBerry::populateSetting(const String& key, String* value)
{
    if (m_inspectorSettingsMap->contains(key))
        *value = m_inspectorSettingsMap->get(key);
}

void InspectorClientBlackBerry::storeSetting(const String& key, const String& value)
{
    m_inspectorSettingsMap->set(key, value);
}

void InspectorClientBlackBerry::inspectorWindowObjectCleared()
{
    notImplemented();
}

void InspectorClientBlackBerry::openInspectorFrontend(InspectorController*)
{
    notImplemented();
}

bool InspectorClientBlackBerry::sendMessageToFrontend(const WTF::String& message)
{
    CString utf8Message =  message.utf8();
    m_page->client()->handleWebInspectorMessageToFrontend(0, utf8Message.data(), utf8Message.length());
    return true;
}

} // namespace WebCore
