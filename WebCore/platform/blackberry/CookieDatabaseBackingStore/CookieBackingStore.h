/*
 * Copyright (C) 2009 Julien Chaffraix <jchaffraix@pleyo.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CookieBackingStore_h
#define CookieBackingStore_h

#include "SQLiteDatabase.h"

namespace WebCore {

    class ParsedCookie;

    class CookieBackingStore {
    public:
        void open(const String& cookieJar);
        void close();

        void insert(const ParsedCookie* cookie);
        void update(const ParsedCookie* cookie);
        void remove(const ParsedCookie* cookie);

        void removeAll();

        Vector<ParsedCookie*> getAllCookies();

    private:
        friend CookieBackingStore& cookieBackingStore();

        CookieBackingStore();
        ~CookieBackingStore();

        String m_tableName;
        SQLiteDatabase m_db;
        SQLiteStatement *m_insertStatement;
        SQLiteStatement *m_updateStatement;
        SQLiteStatement *m_deleteStatement;
    };

CookieBackingStore& cookieBackingStore();

} // namespace WebCore

#endif // CookieBackingStore_h
