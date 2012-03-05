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

#endif // ResourceBlackBerry_h
