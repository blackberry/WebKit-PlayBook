/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WorkQueueItem.h"

#include "CString.h"
#include "DumpRenderTreeBlackBerry.h"
#include "Frame.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "OwnArrayPtr.h"
#include "Page.h"
#include "PlatformString.h"
#include "WebPage.h"


bool LoadItem::invoke() const
{
    size_t targetArrSize = JSStringGetMaximumUTF8CStringSize(m_target.get());
    size_t urlArrSize = JSStringGetMaximumUTF8CStringSize(m_url.get());
    OwnArrayPtr<char> target = adoptArrayPtr(new char[targetArrSize]);
    OwnArrayPtr<char> url = adoptArrayPtr(new char[urlArrSize]);
    size_t targetLen = JSStringGetUTF8CString(m_target.get(), target.get(), targetArrSize) - 1;
    JSStringGetUTF8CString(m_url.get(), url.get(), urlArrSize);

    WebCore::Frame* frame;
    if (target && targetLen)
        frame = mainFrame->tree()->find(target.get());
    else
        frame = mainFrame;

    if (!frame)
        return false;

    WebCore::KURL kurl = WebCore::KURL(WebCore::KURL(), url.get());
    frame->loader()->load(kurl, false);
    return true;
}

bool LoadHTMLStringItem::invoke() const
{
    // FIXME: NOT IMPLEMENTED
    return false;
}

bool ReloadItem::invoke() const
{
    mainFrame->loader()->reload(true);
    return true;
}

bool ScriptItem::invoke() const
{
    BlackBerry::WebKit::JavaScriptDataType type;
    BlackBerry::WebKit::WebString result;
    size_t scriptArrSize = JSStringGetMaximumUTF8CStringSize(m_script.get());
    OwnArrayPtr<char> script = adoptArrayPtr(new char[scriptArrSize]);
    JSStringGetUTF8CString(m_script.get(), script.get(), scriptArrSize);
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->executeJavaScript(script.get(), type, result);
    return true;
}

bool BackForwardItem::invoke() const
{
    return BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->goBackOrForward(m_howFar);
}
