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

#include "config.h"
#include "GeolocationControllerClientBlackBerry.h"

#include "Geolocation.h"
#include "GeolocationController.h"
#include "GeolocationError.h"
#include "GeolocationPosition.h"
#include "Page.h"
#include "WebPage.h"
#include "WebPage_p.h"

using namespace WebCore;

GeolocationControllerClientBlackBerry::GeolocationControllerClientBlackBerry(Olympia::WebKit::WebPage* webPage)
    : m_webPage(webPage)
    , m_tracker(0)
    , m_accuracy(false)
{
}

void GeolocationControllerClientBlackBerry::geolocationDestroyed()
{
    if (m_tracker)
        m_tracker->destroy();
    delete this;
}

void GeolocationControllerClientBlackBerry::startUpdating()
{
    if (m_tracker)
        m_tracker->resume();
    else
        m_tracker = Olympia::Platform::GeoTracker::create(this, 0, m_accuracy, -1, -1);
}

void GeolocationControllerClientBlackBerry::stopUpdating()
{
    if (m_tracker)
        m_tracker->suspend();
}

GeolocationPosition* GeolocationControllerClientBlackBerry::lastPosition()
{
    return m_lastPosition.get();
}

void GeolocationControllerClientBlackBerry::onLocationUpdate(double timestamp, double latitude, double longitude, double accuracy, double altitude, bool altitudeValid, 
                                                             double altitudeAccuracy, bool altitudeAccuracyValid, double speed, bool speedValid, double heading, bool headingValid)
{
    m_lastPosition = GeolocationPosition::create(timestamp, latitude, longitude, accuracy, altitudeValid, altitude, altitudeAccuracyValid, 
                                                 altitudeAccuracy, headingValid, heading, speedValid, speed);
    m_webPage->d->m_page->geolocationController()->positionChanged(m_lastPosition.get());
}

void GeolocationControllerClientBlackBerry::onLocationError(const char* errorStr)
{
    RefPtr<GeolocationError> error = GeolocationError::create(GeolocationError::PositionUnavailable, String::fromUTF8(errorStr));
    m_webPage->d->m_page->geolocationController()->errorOccurred(error.get());
}

void GeolocationControllerClientBlackBerry::onPermission(void* context, bool isAllowed)
{
    Geolocation* position = static_cast<Geolocation*>(context);
    position->setIsAllowed(isAllowed);
}

void GeolocationControllerClientBlackBerry::setEnableHighAccuracy(bool newAccuracy)
{
    if (m_accuracy == newAccuracy)
        return;
    if (m_tracker) {
        m_tracker->destroy();
        m_tracker = Olympia::Platform::GeoTracker::create(this, 0, newAccuracy, -1, -1);
    }
}

