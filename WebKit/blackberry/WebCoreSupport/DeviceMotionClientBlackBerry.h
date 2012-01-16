/*
 * Copyright 2010 Apple Inc. All rights reserved.
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

#ifndef DeviceMotionClientBlackBerry_h
#define DeviceMotionClientBlackBerry_h

#include "DeviceMotionClient.h"
#include "DeviceMotionData.h"
#include "WebPage.h"

#include <BlackBerryPlatformDeviceMotionTracker.h>
#include <BlackBerryPlatformDeviceMotionTrackerListener.h>

namespace WebCore {

class DeviceMotionClientBlackBerry : public WebCore::DeviceMotionClient, public Olympia::Platform::DeviceMotionTrackerListener {
public:
    DeviceMotionClientBlackBerry(Olympia::WebKit::WebPage*);
    ~DeviceMotionClientBlackBerry();

    virtual void setController(DeviceMotionController*);
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual DeviceMotionData* currentDeviceMotion() const;
    virtual void deviceMotionControllerDestroyed();
    virtual void onMotion(const Olympia::Platform::DeviceMotionEvent* event);

private:
    Olympia::WebKit::WebPage* m_webPage;
    Olympia::Platform::DeviceMotionTracker* m_tracker;
    DeviceMotionController* m_controller;
    RefPtr<DeviceMotionData> m_currentMotion;
    double m_lastEventTime;
};
}
#endif // DeviceMotionClientBlackBerry_h
