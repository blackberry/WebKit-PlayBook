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

#ifndef WebGeolocationServiceBridge_h
#define WebGeolocationServiceBridge_h

namespace WebCore {
class GeolocationServiceBridge;
class GeolocationServiceChromium;
}

namespace WebKit {

class WebString;
class WebURL;

// Provides a WebKit API called by the embedder.
class WebGeolocationServiceBridge {
public:
    virtual void setIsAllowed(bool allowed) = 0;
    virtual void setLastPosition(double latitude, double longitude, bool providesAltitude, double altitude, double accuracy, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, long long timestamp) = 0;
    virtual void setLastError(int errorCode, const WebString& message) = 0;
    // Called when the WebGeolocationService to which this bridge is attached is going out of scope. On receiving
    // this call the bridge implementation must not make any further access to the service.
    virtual void didDestroyGeolocationService() = 0;
    // FIXME: Remove this badly named method when all callers are using didDestroyGeolocationService directly.
    void onWebGeolocationServiceDestroyed() { didDestroyGeolocationService(); }

protected:
    virtual ~WebGeolocationServiceBridge() {}
};

} // namespace WebKit

#endif // WebGeolocationServiceBridge_h
