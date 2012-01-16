/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#include "config.h"

#include "MainThread.h"
#include "BlackBerryPlatformClient.h"

namespace WTF {

void initializeMainThreadPlatform()
{
}

void scheduleDispatchFunctionsOnMainThread()
{
#if ENABLE(SINGLE_THREADED)
    dispatchFunctionsFromMainThread();
#elif PLATFORM(BLACKBERRY)
    Olympia::Platform::Client::get()->scheduleCallOnMainThread(dispatchFunctionsFromMainThread);
#else
#error "Please implement MainThreadOlympia for this platform"
#endif
}

} // namespace WTF
