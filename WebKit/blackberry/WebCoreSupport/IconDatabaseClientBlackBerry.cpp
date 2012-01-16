/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 */

#include "config.h"
#include "IconDatabaseClientBlackBerry.h"

#include "IconDatabase.h"
#include "WebSettings.h"
#include "WebString.h"

namespace WebCore {
IconDatabaseClientBlackBerry* IconDatabaseClientBlackBerry::getInstance()
{
    static IconDatabaseClientBlackBerry* instance = 0;
    if (!instance)
        instance = new IconDatabaseClientBlackBerry();
    return instance;
}

bool IconDatabaseClientBlackBerry::initIconDatabase(const Olympia::WebKit::WebSettings* settings)
{
    bool enable = !settings->isPrivateBrowsingEnabled() && settings->isDatabasesEnabled();
    iconDatabase()->setEnabled(enable);
    if (!enable) {
        m_initState = NotInitialized;
        return false;
    }

    if (m_initState == InitializeFailed)
        return false;

    if (m_initState == InitializeSucceeded)
        return true;

    iconDatabase()->setClient(this);

    Olympia::WebKit::WebString path = settings->databasePath();

    if (path.isEmpty())
        path = settings->localStoragePath();

    m_initState = iconDatabase()->open(path) ? InitializeSucceeded : InitializeFailed;

    return (m_initState == InitializeSucceeded);
}
} // namespace WebCore
