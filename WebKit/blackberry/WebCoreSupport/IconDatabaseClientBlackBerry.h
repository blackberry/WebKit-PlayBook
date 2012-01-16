/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 */

#ifndef IconDatabaseClientBlackBerry_h
#define IconDatabaseClientBlackBerry_h

#include "IconDatabaseClient.h"

namespace Olympia {
    namespace WebKit {
        class WebSettings;
    }
}

namespace WebCore {

class IconDatabaseClientBlackBerry : public IconDatabaseClient {
private:
    IconDatabaseClientBlackBerry() : m_initState(NotInitialized) {}
    enum { NotInitialized, InitializeSucceeded, InitializeFailed } m_initState;
public:
    static IconDatabaseClientBlackBerry* getInstance();
    bool initIconDatabase(const Olympia::WebKit::WebSettings*);
};

}

#endif
