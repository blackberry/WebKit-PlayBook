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

#ifndef WebSettings_p_h
#define WebSettings_p_h

#include "WebSettings.h"

#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

#define synthesizeAccessorsForPrimitiveValuePrefixAndType(prefix, type) \
     void set##prefix(const WTF::String& key, type newValue) { \
         ASSERT(impl); \
         if (get##prefix(key) == newValue) \
            return; \
         if (copyOnWrite) { \
            copyOnWrite = false; \
            impl = new WebSettingsPrivateImpl(*impl); \
         } \
         PrimitiveValue primitiveValue; \
         primitiveValue.prefix##Value = newValue; \
         impl->primitiveValues.set(key, primitiveValue); \
         if (delegate) \
            delegate->didChangeSettings(sender); \
     } \
     type get##prefix(const WTF::String& key) const { \
          ASSERT(impl); \
          if (!impl->primitiveValues.contains(key)) \
              return static_cast<type>(false); \
          return impl->primitiveValues.get(key).prefix##Value; \
      }

namespace WTF {
class String;
}

namespace BlackBerry {
namespace WebKit {

struct WebSettingsPrivate {
    union PrimitiveValue {
        WebSettings::TextReflowMode TextReflowModeValue;
        bool BooleanValue;
        double DoubleValue;
        int IntegerValue;
        unsigned UnsignedValue;
        unsigned long long UnsignedLongLongValue;
    };

    struct WebSettingsPrivateImpl {
        WTF::HashMap<WTF::String, PrimitiveValue> primitiveValues;
        WTF::HashMap<WTF::String, WTF::String> stringValues;
    };

    WebSettingsPrivateImpl* impl;
    WebSettingsDelegate* delegate;
    WebSettings* sender;
    bool copyOnWrite;

    WebSettingsPrivate();

    synthesizeAccessorsForPrimitiveValuePrefixAndType(TextReflowMode, WebSettings::TextReflowMode);

    synthesizeAccessorsForPrimitiveValuePrefixAndType(Boolean, bool);

    synthesizeAccessorsForPrimitiveValuePrefixAndType(Double, double);

    synthesizeAccessorsForPrimitiveValuePrefixAndType(Integer, int);

    synthesizeAccessorsForPrimitiveValuePrefixAndType(Unsigned, unsigned);

    synthesizeAccessorsForPrimitiveValuePrefixAndType(UnsignedLongLong, unsigned long long);

    WTF::String getString(const WTF::String& key) const
    {
        ASSERT(impl);
        if (!impl->stringValues.contains(key))
            return WTF::String();
        return impl->stringValues.get(key);
    }

    void setString(const WTF::String& key, const WTF::String& newValue)
    {
        ASSERT(impl);
        if (getString(key) == newValue)
            return;
        if (copyOnWrite) {
            copyOnWrite = false;
            impl = new WebSettingsPrivateImpl(*impl);
        }
        impl->stringValues.set(key, newValue);
        if (delegate)
            delegate->didChangeSettings(sender);
    }
};

} // namespace WebKit
} // namespace BlackBerry

#endif // WebSettings_p_h
