/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef GeolocationControllerClientOlympia_h
#define GeolocationControllerClientOlympia_h

#include "WebPage.h"
#include <GeolocationControllerClient.h>
#include <GeolocationPosition.h>
#include <BlackBerryPlatformGeoTracker.h>
#include <BlackBerryPlatformGeoTrackerListener.h>

namespace WebCore {

class GeolocationControllerClientBlackBerry : public WebCore::GeolocationControllerClient, public Olympia::Platform::GeoTrackerListener {
public:
    GeolocationControllerClientBlackBerry(Olympia::WebKit::WebPage*);

    virtual void geolocationDestroyed();
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual WebCore::GeolocationPosition* lastPosition();
    virtual void setEnableHighAccuracy(bool);

    virtual void onLocationUpdate(double timestamp, double latitude, double longitude, double accuracy, double altitude, bool altitudeValid, double altitudeAccuracy, 
                                  bool altitudeAccuracyValid, double speed, bool speedValid, double heading, bool headingValid);
    virtual void onLocationError(const char* error);
    virtual void onPermission(void* context, bool isAllowed);
    Olympia::Platform::GeoTracker* tracker() const { return m_tracker; }

private:
    Olympia::WebKit::WebPage* m_webPage;
    Olympia::Platform::GeoTracker* m_tracker;
    RefPtr<GeolocationPosition> m_lastPosition;
    bool m_accuracy;
};
}
#endif // GeolocationControllerClientOlympia_h
