/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SocketStreamHandle.h"

#include "CString.h"
#include "Chrome.h"
#include "FrameLoaderClientBlackBerry.h"
#include "KURL.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PageClientBlackBerry.h"
#include "PageGroup.h"
#include "SocketStreamError.h"
#include "SocketStreamFactory.h"
#include "SocketStreamHandleClient.h"

namespace WebCore {

SocketStreamHandle::SocketStreamHandle(const String& groupName, const KURL& url, SocketStreamHandleClient* client)
    : SocketStreamHandleBase(url, client)
    , m_failed(false)
{
    LOG(Network, "SocketStreamHandle %p new client %p", this, m_client);

    // Find a playerId to pass to the platform client.  It can be from any page
    // in the PageGroup, since they all share the same network profile and
    // resources.
    PageGroup* pageGroup = PageGroup::pageGroup(groupName);
    ASSERT(pageGroup && !pageGroup->pages().isEmpty());
    Page* page = *(pageGroup->pages().begin());
    ASSERT(page && page->mainFrame());
    int playerId = static_cast<FrameLoaderClientBlackBerry*>(page->mainFrame()->loader()->client())->playerId();

    // Create a platform socket stream
    Olympia::Platform::SocketStreamFactory* factory = page->chrome()->platformPageClient()->socketStreamFactory();
    ASSERT(factory);
    m_socketStream = factory->createSocketStream(playerId);
    ASSERT(m_socketStream);
    m_socketStream->setListener(this);

    // Open the socket
    CString urlString = url.string().latin1();
    m_socketStream->open(urlString.data(), urlString.length());
}

SocketStreamHandle::~SocketStreamHandle()
{
    LOG(Network, "SocketStreamHandle %p delete", this);
    setClient(0);
}

int SocketStreamHandle::platformSend(const char* buf, int length)
{
    LOG(Network, "SocketStreamHandle %p platformSend", this);
    ASSERT(m_socketStream);
    return m_socketStream->sendData(buf, length);
}

void SocketStreamHandle::platformClose()
{
    LOG(Network, "SocketStreamHandle %p platformClose", this);

    // The platform socket will ignore calls to close after the socket has already closed (whether successfully or with an error).
    // But didFail does not mark the socket as closed: it calls close, and doesn't treat it as closed until it gets a close notification.
    // So if platformClose is called after didFail, just call the close callback immediately instead of expecting the platform socket to do it.
    if (m_failed) {
        notifyClose();
        return;
    }

    ASSERT(m_socketStream);
    m_socketStream->close();
}

void SocketStreamHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    notImplemented();
}

void SocketStreamHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCancellation(const AuthenticationChallenge&)
{
    notImplemented();
}

/** ISocketStreamListener interface */

void SocketStreamHandle::notifyOpen()
{
    ASSERT(m_client);

    // The client can close the handle, potentially removing the last reference.
    RefPtr<SocketStreamHandle> protect(this);
    m_state = Open;
    m_client->didOpen(this);
}

void SocketStreamHandle::notifyClose()
{
    ASSERT(m_client);

    // The client can close the handle, potentially removing the last reference.
    RefPtr<SocketStreamHandle> protect(this);
    m_client->didClose(this);
}

void SocketStreamHandle::notifyFail(PlatformStatus status)
{
    ASSERT(m_client);

    m_failed = true;

    // The client can close the handle, potentially removing the last reference.
    RefPtr<SocketStreamHandle> protect(this);
    m_client->didFail(this, SocketStreamError(status));
}

void SocketStreamHandle::notifyDataReceived(const char* buf, size_t len)
{
    ASSERT(m_client);

    // The client can close the handle, potentially removing the last reference.
    RefPtr<SocketStreamHandle> protect(this);
    m_client->didReceiveData(this, buf, len);
}

void SocketStreamHandle::notifyReadyToSendData()
{
    sendPendingData();
}

} // namespace WebCore
