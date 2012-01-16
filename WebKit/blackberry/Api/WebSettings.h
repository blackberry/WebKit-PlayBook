/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 */

#ifndef WebSettings_h
#define WebSettings_h

#include "BlackBerryGlobal.h"

// FIXME: We shouldn't be using WTF types in our public API. For some reason isSupportedObjectMIMEType() uses a WebCore string.
namespace WTF {
    class String;
}

namespace Olympia {
namespace WebKit {

class WebSettings;
class WebSettingsPrivate;

/*!
    @struct WebSettingsDelegate
    Defines the methods that must be implemented by a delegate of WebSettings.
*/
struct OLYMPIA_EXPORT WebSettingsDelegate {
    virtual ~WebSettingsDelegate() { }

    /*!
        Sent when the value of a setting changed as well as on instantiation of a WebSettings object.
        @param settings The WebSettings object that sent the message.
    */
    virtual void didChangeSettings(WebSettings*) = 0;
};


/*!
    @class WebSettings
*/
class OLYMPIA_EXPORT WebSettings {
private:
    WebSettingsPrivate* m_private;
    WebSettings();
    WebSettings(const WebSettings&);
public:
    static WebSettings* createFromStandardSettings(WebSettingsDelegate* = 0);
    ~WebSettings();

    static WebSettings* standardSettings();

    void setDelegate(WebSettingsDelegate*);
    WebSettingsDelegate* delegate();

    static void addSupportedObjectPluginMIMEType(const char*);
    static bool isSupportedObjectMIMEType(const WTF::String& mimeType);
    static WebString getNormalizedMIMEType(const WebString&);

    bool xssAuditorEnabled() const;
    void setXSSAuditorEnabled(bool);

    bool loadsImagesAutomatically() const;
    void setLoadsImagesAutomatically(bool);

    bool shouldDrawBorderWhileLoadingImages() const;
    void setShouldDrawBorderWhileLoadingImages(bool);

    bool isJavaScriptEnabled() const;
    void setJavaScriptEnabled(bool);

    bool isPrivateBrowsingEnabled() const;
    void setPrivateBrowsingEnabled(bool);

    int defaultFixedFontSize() const;
    void setDefaultFixedFontSize(int);

    int defaultFontSize() const;
    void setDefaultFontSize(int);

    int minimumFontSize() const;
    void setMinimumFontSize(int);

    WebString serifFontFamily() const;
    void setSerifFontFamily(const char*);
    WebString fixedFontFamily() const;
    void setFixedFontFamily(const char*);
    WebString sansSerifFontFamily() const;
    void setSansSerifFontFamily(const char*);
    WebString standardFontFamily() const;
    void setStandardFontFamily(const char*);

    WebString userAgentString() const;
    void setUserAgentString(const char*);

    WebString defaultTextEncodingName() const;
    void setDefaultTextEncodingName(const char*);

    bool isZoomToFitOnLoad() const;
    void setZoomToFitOnLoad(bool);

    enum TextReflowMode { TextReflowDisabled, TextReflowEnabled, TextReflowEnabledOnlyForBlockZoom };
    TextReflowMode textReflowMode() const;
    void setTextReflowMode(TextReflowMode);

    bool isScrollbarsEnabled() const;
    void setScrollbarsEnabled(bool);

    // FIXME: Consider renaming this method upstream, where it is called javaScriptCanOpenWindowsAutomatically
    bool canJavaScriptOpenWindowsAutomatically() const;
    void setJavaScriptOpenWindowsAutomatically(bool);

    bool arePluginsEnabled() const;
    void setPluginsEnabled(bool);

    bool isGeolocationEnabled() const;
    void setGeolocationEnabled(bool);

    // Context Info
    bool doesGetFocusNodeContext() const;
    void setGetFocusNodeContext(bool);

    WebString userStyleSheetString() const;
    void setUserStyleSheetString(const char*);

    WebString userStyleSheetLocation();
    void setUserStyleSheetLocation(const char*);

    // External link handlers
    bool areLinksHandledExternally() const;
    void setAreLinksHandledExternally(bool);

    // BrowserField2 settings
    void setAllowCrossSiteRequests(bool);
    bool allowCrossSiteRequests() const;
    bool isUserScalable() const;
    void setUserScalable(bool);
    int viewportWidth() const;
    void setViewportWidth(int);
    double initialScale() const;
    void setInitialScale(double);

    int firstScheduledLayoutDelay() const;
    void setFirstScheduledLayoutDelay(int);

    // Whether to include pattern: in the list of string patterns
    bool shouldHandlePatternUrls() const;
    void setShouldHandlePatternUrls(bool);

    bool isCookieCacheEnabled() const;
    void setIsCookieCacheEnabled(bool);

    bool areCookiesEnabled() const;
    void setAreCookiesEnabled(bool);

    // Web storage settings
    bool isLocalStorageEnabled() const;
    void setIsLocalStorageEnabled(bool enable);

    bool isDatabasesEnabled() const;
    void setIsDatabasesEnabled(bool enable);

    bool isAppCacheEnabled() const;
    void setIsAppCacheEnabled(bool enable);

    unsigned long long localStorageQuota() const;
    void setLocalStorageQuota(unsigned long long quota);

    // Page cache
    void setMaximumPagesInCache(int pages);
    int maximumPagesInCache() const;

    WebString localStoragePath() const;
    void setLocalStoragePath(const WebString& path);

    WebString databasePath() const;
    void setDatabasePath(const WebString& path);

    WebString appCachePath() const;
    void setAppCachePath(const WebString& path);

    WebString pageGroupName() const;
    void setPageGroupName(const WebString&);

    // FIXME: We shouldn't have an email mode. Instead, we should expose all email-related settings
    // so that the email client can toggle them directly.
    bool isEmailMode() const;
    void setEmailMode(bool enable);

    bool shouldRenderAnimationsOnScrollOrZoom() const;
    void setShouldRenderAnimationsOnScrollOrZoom(bool enable);

    int overZoomColor() const;
    void setOverZoomColor(int);

    bool isWritingDirectionRTL() const;
    void setWritingDirectionRTL(bool);

    bool useWebKitCache() const;
    void setUseWebKitCache(bool use);

    bool isFrameFlatteningEnabled() const;
    void setFrameFlatteningEnabled(bool enable);

    bool isDirectRenderingToWindowEnabled() const;
    void setDirectRenderingToWindowEnabled(bool enable);

    unsigned maxPluginInstances() const;
    void setMaxPluginInstances(unsigned num);

    bool areWebSocketsEnabled() const;
    void setWebSocketsEnabled(bool);
};

} // namespace WebKit
} // namespace Olympia

#endif // WebSettings_h
