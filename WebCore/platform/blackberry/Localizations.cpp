/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#include "config.h"

#include "IntSize.h"
#include "LocalizeResource.h"
#include "LocalizedStrings.h"
#include "PlatformString.h"
#include "NotImplemented.h"

#include <BlackBerryPlatformClient.h>
#include <string>

namespace WebCore {

String fileButtonChooseFileLabel()
{
    Olympia::Platform::LocalizeResource resource;
    const char* string = resource.getString(Olympia::Platform::FILE_CHOOSE_BUTTON_LABEL);
    return string ? String::fromUTF8(string): String();
}

String fileButtonNoFileSelectedLabel()
{
    Olympia::Platform::LocalizeResource resource;
    const char* string = resource.getString(Olympia::Platform::FILE_BUTTON_NO_FILE_SELECTED_LABEL);
    return string ? String::fromUTF8(string): String();
}

String resetButtonDefaultLabel()
{
    Olympia::Platform::LocalizeResource resource;
    const char* string = resource.getString(Olympia::Platform::RESET_BUTTON_LABEL);
    return string ? String::fromUTF8(string): String();
}

String submitButtonDefaultLabel()
{
    Olympia::Platform::LocalizeResource resource;
    const char* string = resource.getString(Olympia::Platform::SUBMIT_BUTTON_LABEL);
    return string ? String::fromUTF8(string): String();
}

String inputElementAltText()
{
    notImplemented();
    return String();
}

String platformDefaultLanguage()
{
    std::string lang = Olympia::Platform::Client::get()->getLocale();
    //getLocale() returns a POSIX lcoale which uses '_' to separate language and country
    //however, WebCore wants a '-' (e.g. en_us should read en-us)
    int underscorePosition = lang.find('_');
    std::string replaceWith = "-";
    if (underscorePosition != -1)
        return lang.replace(underscorePosition, replaceWith.size(), replaceWith).c_str();
    return lang.c_str();
}

String contextMenuItemTagBold()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCheckSpelling()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCopyImageToClipboard()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCopyLinkToClipboard()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCopy()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCut()
{
    notImplemented();
    return String();
}

String contextMenuItemTagDefaultDirection()
{
    notImplemented();
    return String();
}

String contextMenuItemTagDownloadImageToDisk()
{
    notImplemented();
    return String();
}

String contextMenuItemTagDownloadLinkToDisk()
{
    notImplemented();
    return String();
}

String contextMenuItemTagFontMenu()
{
    notImplemented();
    return String();
}

String contextMenuItemTagGoBack()
{
    notImplemented();
    return String();
}

String contextMenuItemTagGoForward()
{
    notImplemented();
    return String();
}

String contextMenuItemTagIgnoreGrammar()
{
    notImplemented();
    return String();
}

String contextMenuItemTagIgnoreSpelling()
{
    notImplemented();
    return String();
}

String contextMenuItemTagInspectElement()
{
    notImplemented();
    return String();
}

String contextMenuItemTagItalic()
{
    notImplemented();
    return String();
}

String contextMenuItemTagLearnSpelling()
{
    notImplemented();
    return String();
}

String contextMenuItemTagLeftToRight()
{
    notImplemented();
    return String();
}

String contextMenuItemTagNoGuessesFound()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenImageInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenLink()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOutline()
{
    notImplemented();
    return String();
}

String contextMenuItemTagPaste()
{
    notImplemented();
    return String();
}

String contextMenuItemTagReload()
{
    notImplemented();
    return String();
}

String contextMenuItemTagRightToLeft()
{
    notImplemented();
    return String();
}

String contextMenuItemTagSearchWeb()
{
    notImplemented();
    return String();
}

String contextMenuItemTagShowSpellingPanel(bool)
{
    notImplemented();
    return String();
}

String contextMenuItemTagSpellingMenu()
{
    notImplemented();
    return String();
}

String contextMenuItemTagStop()
{
    notImplemented();
    return String();
}

String contextMenuItemTagTextDirectionMenu()
{
    notImplemented();
    return String();
}

String contextMenuItemTagUnderline()
{
    notImplemented();
    return String();
}

String contextMenuItemTagWritingDirectionMenu()
{
    notImplemented();
    return String();
}

String searchableIndexIntroduction()
{
    notImplemented();
    return String();
}

String searchMenuClearRecentSearchesText()
{
    notImplemented();
    return String();
}

String searchMenuNoRecentSearchesText()
{
    notImplemented();
    return String();
}

String searchMenuRecentSearchesText()
{
    notImplemented();
    return String();
}

String imageTitle(String const&, IntSize const&)
{
    notImplemented();
    return String();
}

String AXButtonActionVerb()
{
    notImplemented();
    return String();
}

String AXCheckedCheckBoxActionVerb()
{
    notImplemented();
    return String();
}

String AXDefinitionListDefinitionText()
{
    notImplemented();
    return String();
}

String AXDefinitionListTermText()
{
    notImplemented();
    return String();
}

String AXLinkActionVerb()
{
    notImplemented();
    return String();
}

String AXRadioButtonActionVerb()
{
    notImplemented();
    return String();
}

String AXTextFieldActionVerb()
{
    notImplemented();
    return String();
}

String AXUncheckedCheckBoxActionVerb()
{
    notImplemented();
    return String();
}

String AXMenuListPopupActionVerb()
{
    notImplemented();
    return String();
}

String AXMenuListActionVerb()
{
    notImplemented();
    return String();
}

String unknownFileSizeText()
{
    notImplemented();
    return String();
}

String validationMessageStepMismatchText()
{
    notImplemented();
    return String();
}

String validationMessageRangeOverflowText()
{
    notImplemented();
    return String();
}

String validationMessageRangeUnderflowText()
{
    notImplemented();
    return String();
}

String validationMessagePatternMismatchText()
{
    notImplemented();
    return String();
}

String validationMessageTooLongText()
{
    notImplemented();
    return String();
}

String validationMessageTypeMismatchText()
{
    notImplemented();
    return String();
}

String validationMessageValueMissingText()
{
    notImplemented();
    return String();
}

String localizedMediaControlElementString(const String& controlName)
{
    notImplemented();
    return String();
}

String localizedMediaControlElementHelpText(const String& controlName)
{
    notImplemented();
    return String();
}

String localizedMediaTimeDescription(const String& controlName)
{
    notImplemented();
    return String();
}

String localizedMediaTimeDescription(float time)
{
    notImplemented();
    return String();
}
String mediaElementLoadingStateText()
{
    notImplemented();
    return String();
}

String mediaElementLiveBroadcastStateText()
{
    notImplemented();
    return String();
}

String missingPluginText()
{
    notImplemented();
    return String();
}

String crashedPluginText()
{
    notImplemented();
    return String();
}

String contextMenuItemTagMediaPause()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCopyVideoLinkToClipboard()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenVideoInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagMediaPlay()
{
    notImplemented();
    return String();
}

String contextMenuItemTagCopyAudioLinkToClipboard()
{
    notImplemented();
    return String();
}

String contextMenuItemTagOpenAudioInNewWindow()
{
    notImplemented();
    return String();
}

String contextMenuItemTagMediaMute()
{
    notImplemented();
    return String();
}

String contextMenuItemTagToggleMediaControls()
{
    notImplemented();
    return String();
}

String contextMenuItemTagToggleMediaLoop()
{
    notImplemented();
    return String();
}

String contextMenuItemTagEnterVideoFullscreen()
{
    notImplemented();
    return String();
}

} // namespace WebCore
