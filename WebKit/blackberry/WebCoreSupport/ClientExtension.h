/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#ifndef ClientExtension_h
#define ClientExtension_h

namespace WebCore
{
    class Frame;
}

namespace Olympia
{
    namespace WebKit
    {
        class WebPageClient;
    }
}

void attachExtensionObjectToFrame(WebCore::Frame*, Olympia::WebKit::WebPageClient*);

#endif // ClientExtension_h
