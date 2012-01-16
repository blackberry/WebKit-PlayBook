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

#include "config.h"
#include "DeviceOrientationClientBlackBerry.h"

#include "DeviceOrientationController.h"
#include "Page.h"
#include "WebPage.h"
#include "WebPage_p.h"

using namespace WebCore;

DeviceOrientationClientBlackBerry::DeviceOrientationClientBlackBerry(Olympia::WebKit::WebPage* webPage)
    : m_webPage(webPage)
    , m_tracker(0)
    , m_controller(0)
{
}

DeviceOrientationClientBlackBerry::~DeviceOrientationClientBlackBerry()
{
    if (m_tracker)
        m_tracker->destroy();
}

void DeviceOrientationClientBlackBerry::setController(DeviceOrientationController* controller)
{
    m_controller = controller;
}

void DeviceOrientationClientBlackBerry::deviceOrientationControllerDestroyed()
{
    delete this;
}

void DeviceOrientationClientBlackBerry::startUpdating()
{
    if (m_tracker)
        m_tracker->resume();
    else
        m_tracker = Olympia::Platform::DeviceOrientationTracker::create(this);
}

void DeviceOrientationClientBlackBerry::stopUpdating()
{
    if (m_tracker)
        m_tracker->suspend();
}

DeviceOrientation* DeviceOrientationClientBlackBerry::lastOrientation() const
{
    return m_currentOrientation.get();
}

void DeviceOrientationClientBlackBerry::onOrientation(const Olympia::Platform::DeviceOrientationEvent* event)
{
    m_currentOrientation = DeviceOrientation::create(true, event->z, true, event->x, true, event->y);
    if (!m_controller)
        return;

    m_controller->didChangeDeviceOrientation(lastOrientation());
}

