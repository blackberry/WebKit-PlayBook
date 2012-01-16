/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */

#ifndef InspectorClientBlackBerry_h
#define InspectorClientBlackBerry_h

#include "InspectorClient.h"
#include "IntRect.h"
#include "WebPage.h"

namespace Olympia {
namespace WebKit {
class WebPage;
}
}

namespace WebCore {

class InspectorClientBlackBerry : public WebCore::InspectorClient {
public:
    InspectorClientBlackBerry(Olympia::WebKit::WebPage* page);
    virtual void inspectorDestroyed();
    virtual Page* createPage();
    virtual String localizedStringsURL();
    virtual String hiddenPanels();
    virtual void showWindow();
    virtual void closeWindow();
    virtual void attachWindow();
    virtual void detachWindow();
    virtual void setAttachedWindowHeight(unsigned);
    virtual void highlight(Node*);
    virtual void hideHighlight();
    virtual void inspectedURLChanged(const String&);
    virtual void populateSetting(const String& key, String* value);
    virtual void storeSetting(const String& key, const String& value);
    virtual void inspectorWindowObjectCleared();
    virtual void openInspectorFrontend(InspectorController*);
    virtual bool sendMessageToFrontend(const WTF::String&);
private:
    Olympia::WebKit::WebPage* m_page;
    typedef HashMap<WTF::String, WTF::String> SettingsMap;
    OwnPtr<SettingsMap> m_inspectorSettingsMap;
    IntRect m_highlightRect;
    IntRect m_lastHighlightRect;
};

} // WebCore

#endif // InspectorClientBlackBerry_h
