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

#ifndef ResourceHolderImpl_h
#define ResourceHolderImpl_h

#include "ResourceHolder.h"
#include "ScopePointer.h"
#include <string>

namespace WebCore {
class CachedResource;
}

namespace BlackBerry {
namespace WebKit {

class ResourceHolderImpl: public ResourceHolder {
public:
    static ResourceHolderImpl* create(const WebCore::CachedResource&);
    
    virtual void ref();
    virtual void deref();
    virtual const char* url() const;
    virtual const char* mimeType() const;
    virtual const char* suggestedFileName() const;
    virtual unsigned getMoreData(const unsigned char*& data, unsigned position = 0) const;
    virtual unsigned size() const;
private:
    ResourceHolderImpl();

    unsigned m_refCount;
    std::string m_url;
    std::string m_mimeType;
    std::string m_suggestedFileName;
    // FIXME: Ideally we could share the SharedBuffer object in WebKit. Let's try that later.
    ScopeArray<unsigned char> m_buffer;
    unsigned m_bufferSize;
};

}
}

#endif // ResourceHolderImpl_h
