/*
 * Copyright (C) 2009 Julien Chaffraix <jchaffraix@pleyo.com>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#include "config.h"
#include "CookieBackingStore.h"

#include "CookieManager.h"
#include "ParsedCookie.h"
#include "SQLiteStatement.h"

namespace WebCore {


CookieBackingStore& cookieBackingStore()
{
    static CookieBackingStore *backingStore = 0;
    if (!backingStore)
        backingStore = new CookieBackingStore;
    return *backingStore;
}

CookieBackingStore::CookieBackingStore()
    : m_tableName("cookies") // This is chosen to match Mozilla's table name.
    , m_insertStatement(0)
    , m_updateStatement(0)
    , m_deleteStatement(0)
{
}

CookieBackingStore::~CookieBackingStore()
{
    close();
}

void CookieBackingStore::open(const String& cookieJar)
{
    close();

    if (!m_db.open(cookieJar)) {
        LOG_ERROR("Could not open the cookie database. No cookie will be stored!");
        return;
    }

    m_db.executeCommand("PRAGMA locking_mode=EXCLUSIVE;");
    m_db.executeCommand("PRAGMA journal_mode=TRUNCATE;");

    if (!m_db.tableExists(m_tableName)) {
        String createTableQuery("CREATE TABLE ");
        createTableQuery += m_tableName;
        // This table schema is compliant with Mozilla's.
        createTableQuery += " (name TEXT, value TEXT, host TEXT, path TEXT, expiry DOUBLE, lastAccessed DOUBLE, isSecure INTEGER, isHttpOnly INTEGER, PRIMARY KEY (host, path, name));";

        if (!m_db.executeCommand(createTableQuery)) {
            LOG_ERROR("Could not create the table to store the cookies into. No cookie will be stored!");
            close();
            return;
        }
    }

    String insertQuery("INSERT INTO ");
    insertQuery += m_tableName;
    insertQuery += " (name, value, host, path, expiry, lastAccessed, isSecure, isHttpOnly) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);";

    m_insertStatement = new SQLiteStatement(m_db, insertQuery);
    if (m_insertStatement->prepare()) {
        LOG_ERROR("Cannot save cookies");
    }

    String updateQuery("UPDATE ");
    updateQuery += m_tableName;
    // The where statement is chosen to match CookieMap key.
    updateQuery += " SET name = ?1, value = ?2, host = ?3, path = ?4, expiry = ?5, lastAccessed = ?6, isSecure = ?7, isHttpOnly = ?8 where name = ?1 and host = ?3 and path = ?4;";
    m_updateStatement = new SQLiteStatement(m_db, updateQuery);

    if (m_updateStatement->prepare()) {
        LOG_ERROR("Cannot update cookies");
    }

    String deleteQuery("DELETE FROM ");
    deleteQuery += m_tableName;
    // The where statement is chosen to match CookieMap key.
    deleteQuery += " WHERE name=?1 and host=?2 and path=?3;";
    m_deleteStatement = new SQLiteStatement(m_db, deleteQuery);

    if (m_deleteStatement->prepare()) {
        LOG_ERROR("Cannot delete cookies");
    }

}

void CookieBackingStore::close()
{
    delete m_insertStatement;
    m_insertStatement = 0;
    delete m_updateStatement;
    m_updateStatement = 0;
    delete m_deleteStatement;
    m_deleteStatement = 0;

    if (m_db.isOpen())
        m_db.close();
}

void CookieBackingStore::insert(const ParsedCookie* cookie)
{
    // FIXME: This should be an ASSERT.
    if (cookie->isSession())
        return;

    if (!m_db.isOpen())
        return;
 
    // Binds all the values
    if (m_insertStatement->bindText(1, cookie->name()) || m_insertStatement->bindText(2, cookie->value())
        || m_insertStatement->bindText(3, cookie->domain()) || m_insertStatement->bindText(4, cookie->path())
        || m_insertStatement->bindDouble(5, cookie->expiry()) || m_insertStatement->bindDouble(6, cookie->lastAccessed())
        || m_insertStatement->bindInt64(7, cookie->isSecure()) || m_insertStatement->bindInt64(8, cookie->isHttpOnly())) {
        LOG_ERROR("Cannot bind cookie data to save");
        return;
   }

    int rc = m_insertStatement->step();
    m_insertStatement->reset();
    if (rc != SQLResultOk && rc != SQLResultDone) {
        LOG_ERROR("Cannot save cookie");
        return;
    }
}

void CookieBackingStore::update(const ParsedCookie* cookie)
{
    // FIXME: This should be an ASSERT.
    if (cookie->isSession())
        return;

    if (!m_db.isOpen())
        return;

    // Binds all the values
    if (m_updateStatement->bindText(1, cookie->name()) || m_updateStatement->bindText(2, cookie->value())
        || m_updateStatement->bindText(3, cookie->domain()) || m_updateStatement->bindText(4, cookie->path())
        || m_updateStatement->bindDouble(5, cookie->expiry()) || m_updateStatement->bindDouble(6, cookie->lastAccessed())
        || m_updateStatement->bindInt64(7, cookie->isSecure()) || m_updateStatement->bindInt64(8, cookie->isHttpOnly())) {
        LOG_ERROR("Cannot bind cookie data to update");
        return;
    }

    int rc = m_updateStatement->step();
    m_updateStatement->reset();
    if (rc != SQLResultOk && rc != SQLResultDone) {
        LOG_ERROR("Cannot update cookie");
        return;
    }
}

void CookieBackingStore::remove(const ParsedCookie* cookie)
{
    // FIXME: This should be an ASSERT.
    if (cookie->isSession())
        return;

    if (!m_db.isOpen())
        return;

    // Binds all the values
    if (m_deleteStatement->bindText(1, cookie->name()) || m_deleteStatement->bindText(2, cookie->domain())
        || m_deleteStatement->bindText(3, cookie->path())) {
        LOG_ERROR("Cannot bind cookie data to delete");
        return;
    }

    int rc = m_deleteStatement->step();
    m_deleteStatement->reset();
    if (rc != SQLResultOk && rc != SQLResultDone) {
        LOG_ERROR("Cannot delete cookie from database");
        return;
    }
}

void CookieBackingStore::removeAll()
{
    if (!m_db.isOpen())
        return;

    String deleteQuery("DELETE FROM ");
    deleteQuery += m_tableName;
    deleteQuery += ";";

    SQLiteStatement deleteStatement(m_db, deleteQuery);
    if (deleteStatement.prepare()) {
        LOG_ERROR("Could not prepare DELETE * statement");
        return;
    }

    if (!deleteStatement.executeCommand()) {
        LOG_ERROR("Cannot delete cookie from database");
        return;
    }
}

Vector<ParsedCookie*> CookieBackingStore::getAllCookies()
{
    Vector<ParsedCookie*, 8> cookies;

    // Check that the table exists to avoid doing an unnecessary request.
    if (!m_db.isOpen())
        return cookies;

    String selectQuery("SELECT name, value, host, path, expiry, lastAccessed, isSecure, isHttpOnly FROM ");
    selectQuery += m_tableName;
    selectQuery += ";";

    SQLiteStatement selectStatement(m_db, selectQuery);

    if (selectStatement.prepare()) {
        LOG_ERROR("Cannot retrieved cookies from the database");
        return cookies;
    }

    while (selectStatement.step() == SQLResultRow) {
        // There is a row to fetch

        String name = selectStatement.getColumnText(0);
        String value = selectStatement.getColumnText(1);
        String domain = selectStatement.getColumnText(2);
        String path = selectStatement.getColumnText(3);
        double expiry = selectStatement.getColumnDouble(4);
        double lastAccessed = selectStatement.getColumnDouble(5);
        bool isSecure = (selectStatement.getColumnInt(6) != 0);
        bool isHttpOnly = (selectStatement.getColumnInt(7) != 0);

        cookies.append(new ParsedCookie(name, value, domain, path, expiry, lastAccessed, isSecure, isHttpOnly));
    }

    return cookies;
}

} // namespace WebCore
