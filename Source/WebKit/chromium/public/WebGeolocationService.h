/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef WebGeolocationService_h
#define WebGeolocationService_h

namespace WebKit {

class WebGeolocationServiceBridge;
class WebString;
class WebURL;

// Provides an embedder API called by WebKit.
class WebGeolocationService {
public:
    virtual void requestPermissionForFrame(int bridgeId, const WebURL& url) { }
    virtual void cancelPermissionRequestForFrame(int bridgeId, const WebURL&) { }
    virtual void startUpdating(int bridgeId, const WebURL& url, bool enableHighAccuracy) { }
    virtual void stopUpdating(int bridgeId) { }
    virtual void suspend(int bridgeId) { }
    virtual void resume(int bridgeId) { }

    // Attaches the WebGeolocationServiceBridge to the embedder and returns its
    // id, which should be used on subsequent calls for the methods above.
    // An ID of zero indicates the attach failed.
    virtual int attachBridge(WebGeolocationServiceBridge*) { return 0; }

    // Detaches the WebGeolocationServiceBridge from the embedder.
    virtual void detachBridge(int bridgeId) { }

protected:
    virtual ~WebGeolocationService() {}
};

} // namespace WebKit

#endif // WebGeolocationService_h
