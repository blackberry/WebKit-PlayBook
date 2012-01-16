/*
 * Copyright (C) 2010 Research In Motion Limited http://www.rim.com/
 */

#ifndef ResourceBlackBerry_h
#define ResourceBlackBerry_h

#include "PassRefPtr.h"
#include "PlatformString.h"
namespace WebCore {

class Image;

class ResourceBlackBerry {
public:
    static PassRefPtr<Image> loadPlatformImageResource(const char *name);
};

} // namespace WebCore

#endif // ResourceOlympia_h
