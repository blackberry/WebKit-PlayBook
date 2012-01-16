/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NotificationPresenterImpl_h
#define NotificationPresenterImpl_h

#include "HashMap.h"
#include "HashSet.h"
#include "NotificationAckListener.h"
#include "NotificationPresenterBlackBerry.h"
#include "NotificationPresenter.h"
#include "PlatformString.h"
#include "string.h"
#include "StringHash.h"
#include <wtf/OwnPtr.h>

namespace WebKit {

class NotificationPresenterImpl : public WebCore::NotificationPresenter, public Olympia::Platform::NotificationAckListener
{
public:
    static NotificationPresenter* getInstance();
    virtual ~NotificationPresenterImpl();

    // Requests that a notification be shown.
    virtual bool show(WebCore::Notification*);

    // Requests that a notification that has already been shown be canceled.
    virtual void cancel(WebCore::Notification*);

    // Informs the presenter that a WebCore::Notification object has been destroyed
    // (such as by a page transition).  The presenter may continue showing
    // the notification, but must not attempt to call the event handlers.
    virtual void notificationObjectDestroyed(WebCore::Notification*);

    // Requests user permission to show desktop notifications from a particular
    // script context. The callback parameter should be run when the user has
    // made a decision.
    virtual void requestPermission(WebCore::ScriptExecutionContext*, PassRefPtr<WebCore::VoidCallback>);

    // Cancel all outstanding requests for the ScriptExecutionContext
    virtual void cancelRequestsForPermission(WebCore::ScriptExecutionContext*);

    // Checks the current level of permission.
    virtual Permission checkPermission(WebCore::ScriptExecutionContext*);


    //Interfaces inherits from NotificationAckListener:
    virtual void notificationClicked(const std::string& id);
    virtual void onPermission(const std::string& domain, bool isAllowed);

private:
    NotificationPresenterImpl();

private:
    static NotificationPresenterImpl* m_instance;
    OwnPtr<Olympia::Platform::NotificationPresenterBlackBerry> m_platformPresenter;
    typedef HashMap<RefPtr<WebCore::Notification>, WTF::String> NotificationMap;
    NotificationMap m_notifications;
    typedef HashMap<RefPtr<WebCore::ScriptExecutionContext>, RefPtr<WebCore::VoidCallback> > PermissionRequestMap;
    PermissionRequestMap m_permissionRequests;
    HashSet<WTF::String> m_allowedDomains;
};

} // namespace WebKit

#endif
