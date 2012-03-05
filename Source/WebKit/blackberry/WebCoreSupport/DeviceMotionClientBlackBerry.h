/*
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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

#ifndef DeviceMotionClientBlackBerry_h
#define DeviceMotionClientBlackBerry_h

#include "DeviceMotionClient.h"
#include "DeviceMotionData.h"
#include "WebPage.h"

#include <BlackBerryPlatformDeviceMotionTracker.h>
#include <BlackBerryPlatformDeviceMotionTrackerListener.h>

namespace WebCore {

class DeviceMotionClientBlackBerry : public WebCore::DeviceMotionClient, public BlackBerry::Platform::DeviceMotionTrackerListener {
public:
    DeviceMotionClientBlackBerry(BlackBerry::WebKit::WebPage*);
    ~DeviceMotionClientBlackBerry();

    virtual void setController(DeviceMotionController*);
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual DeviceMotionData* currentDeviceMotion() const;
    virtual void deviceMotionControllerDestroyed();
    virtual void onMotion(const BlackBerry::Platform::DeviceMotionEvent*);

private:
    BlackBerry::WebKit::WebPage* m_webPage;
    BlackBerry::Platform::DeviceMotionTracker* m_tracker;
    DeviceMotionController* m_controller;
    RefPtr<DeviceMotionData> m_currentMotion;
    double m_lastEventTime;
};
}
#endif // DeviceMotionClientBlackBerry_h
