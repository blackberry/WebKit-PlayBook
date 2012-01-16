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

#include "config.h"
#include "NotificationPresenterImpl.h"

#include "Notification.h"
#include "NotificationPresenterBlackBerry.h"
#include "ScriptExecutionContext.h"
#include <string>
#include "StringHash.h"
#include "UUID.h"

#if ENABLE(NOTIFICATIONS)

using namespace WebCore;

namespace WebKit {

NotificationPresenterImpl* NotificationPresenterImpl::m_instance = 0;

NotificationPresenter* NotificationPresenterImpl::getInstance()
{
    if (!m_instance)
        m_instance = new NotificationPresenterImpl();
    return m_instance;
}

NotificationPresenterImpl::NotificationPresenterImpl()
{
    m_platformPresenter = adoptPtr(Olympia::Platform::NotificationPresenterBlackBerry::create(this));
}

NotificationPresenterImpl::~NotificationPresenterImpl()
{
}


bool NotificationPresenterImpl::show(Notification* n)
{
    if (NotificationPresenter::PermissionAllowed != checkPermission(n->scriptExecutionContext()))
        return false;

    RefPtr<Notification> notification = n;
    if (m_notifications.contains(notification))
        return false;

    String uuid = createCanonicalUUIDString();
    m_notifications.add(notification, uuid);
    String message;
    if (notification->isHTML())
        // FIXME: load and display HTML content
        message = notification->url().string();
    else
        message = notification->contents().title() + ": " + notification->contents().body(); // FIXME: strip to one line

    m_platformPresenter->show(std::string(uuid.utf8().data()), std::string(message.utf8().data()));
    return true;
}

void NotificationPresenterImpl::cancel(Notification* n)
{
    RefPtr<Notification> notification = n;
    if (m_notifications.contains(notification)) {
        m_platformPresenter->cancel(std::string(m_notifications.get(notification).utf8().data()));
        m_notifications.remove(notification);
    }
}

void NotificationPresenterImpl::notificationObjectDestroyed(Notification* n)
{
    cancel(n);
}

void NotificationPresenterImpl::requestPermission(ScriptExecutionContext* context, PassRefPtr<VoidCallback> callback)
{
    m_permissionRequests.add(context, callback);
    m_platformPresenter->requestPermission(std::string(context->url().host().utf8().data()));
}

void NotificationPresenterImpl::onPermission(const std::string& domainStr, bool isAllowed)
{
    String domain = String::fromUTF8(domainStr.c_str());
    PermissionRequestMap::iterator iter = m_permissionRequests.begin();
    for (; iter != m_permissionRequests.end(); ++iter) {
        if (iter->first->url().host() == domain) {
            if (isAllowed) {
                m_allowedDomains.add(domain);
                iter->second->handleEvent();
            } else
                m_allowedDomains.remove(domain);

            m_permissionRequests.remove(iter);
            break;
        }
    }
}

void NotificationPresenterImpl::cancelRequestsForPermission(ScriptExecutionContext*)
{
    // Because we are using a modal dialog to request permission, it's probably impossible to cancel it.
}

NotificationPresenter::Permission NotificationPresenterImpl::checkPermission(ScriptExecutionContext* context)
{
    // FIXME: Should store the permission information into file permenently instead of in m_allowedDomains.
    // The suggested place to do this is in m_platformPresenter->checkPermission().

    if (m_allowedDomains.contains(context->url().host()))
        return NotificationPresenter::PermissionAllowed;
    return NotificationPresenter::PermissionNotAllowed;

}

// This function will be called by platform side.
void NotificationPresenterImpl::notificationClicked(const std::string& idStr)
{
    String id = String::fromUTF8(idStr.c_str());
    NotificationMap::iterator iter = m_notifications.begin();
    for (; iter != m_notifications.end(); ++iter)
        if (iter->second == id && iter->first->scriptExecutionContext()) {
            iter->first->dispatchEvent(Event::create(eventNames().clickEvent, false, true));
            m_notifications.remove(iter);
        }
}

} // namespace WebKit

#endif // ENABLE(NOTIFICATIONS)
