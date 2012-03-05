/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
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

#include "config.h"
#include "OutOfMemoryHandler.h"

#include "CacheClientBlackBerry.h"
#include "MemoryCache.h"
#include "bindings/js/GCController.h"
#include <BlackBerryPlatformSettings.h>

namespace BlackBerry {
namespace WebKit {

unsigned OutOfMemoryHandler::handleOutOfMemory(unsigned)
{
    // FIXME: Add clean-up actions here

    return 0;
}

void OutOfMemoryHandler::shrinkMemoryUsage()
{
    WebCore::gcController().garbageCollectNow();

    // Clean cache after JS garbage collection because JS GC can
    // generate more dead resources.
    if (!WebCore::memoryCache()->disabled()) {
        // setDisabled(true) will try to evict all cached resources
        WebCore::memoryCache()->setDisabled(true);
        // FIXME: should we permanently turn it off?
        WebCore::memoryCache()->setDisabled(false);

        // Probably we want to shrink cache capacity at this point
        WebCore::CacheClientBlackBerry::get()->updateCacheCapacity();
    }
}

void initializeOutOfMemoryHandler()
{
    static OutOfMemoryHandler* handler = new OutOfMemoryHandler;

    BlackBerry::Platform::setOOMHandler(handler);
}

}
}
