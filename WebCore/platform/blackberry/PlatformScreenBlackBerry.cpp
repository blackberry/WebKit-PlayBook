/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */

#include "config.h"
#include "PlatformScreen.h"

#include "FloatRect.h"
#include "HostWindow.h"
#include "PageClientBlackBerry.h"
#include "ScrollView.h"
#include "Widget.h"

#include "NotImplemented.h"

#include <BlackBerryPlatformScreen.h>

namespace WebCore {

bool screenIsMonochrome(Widget*)
{
    return false;
}

int screenDepthPerComponent(Widget*)
{
    return 8;
}

int screenDepth(Widget*)
{
    return 24;
}

FloatRect screenAvailableRect(Widget* widget)
{
    return FloatRect(FloatPoint(0, 0), FloatSize(Olympia::Platform::Graphics::Screen::size()));
}

FloatRect screenRect(Widget* widget)
{
    return FloatRect(FloatPoint(0, 0), FloatSize(Olympia::Platform::Graphics::Screen::size()));
}

} // namespace WebCore
