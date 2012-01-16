/*
 * Copyright (C) 2009 Julien Chaffraix <jchaffraix@pleyo.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "CookieParser.h"

#include "CString.h"
#include "CurrentTime.h"
#include "Logging.h"
#include "ParsedCookie.h"

namespace WebCore {

static inline bool isCookieHeaderSeparator(UChar c)
{
    return (c == '\r' || c =='\n');
}

static inline bool isLightweightSpace(UChar c)
{
   return (c == ' ' || c == '\t');
}

CookieParser::CookieParser(const KURL& defaultCookieURL)
    : m_defaultCookieURL(defaultCookieURL)
{
}

CookieParser::~CookieParser()
{
}

Vector<ParsedCookie*> CookieParser::parse(const String& cookies)
{
    unsigned cookieStart, cookieEnd = 0;
    double curTime = currentTime();
    Vector<ParsedCookie*, 4> parsedCookies;
 
    if (cookies.length() == 0)  // Code below doesn't handle this case
        return parsedCookies;

    // Iterate over the header to parse all the cookies.
    while (cookieEnd <= cookies.length()) {
        cookieStart = cookieEnd;
        
        // Find a cookie separator.
        while (cookieEnd <= cookies.length() && !isCookieHeaderSeparator(cookies[cookieEnd]))
            cookieEnd++;

        // Detect an empty cookie and go to the next one.
        if (cookieStart == cookieEnd) {
            ++cookieEnd;
            continue;
        }

        if (cookieEnd < cookies.length() && isCookieHeaderSeparator(cookies[cookieEnd]))
            ++cookieEnd;

        ParsedCookie* cookie = parseOneCookie(cookies, cookieStart, cookieEnd - 1, curTime);
        if (cookie)
            parsedCookies.append(cookie);
    }
    return parsedCookies;
}

// Parse the string without "Set-Cookie" according to Firefox grammar (loosely RFC 2109 compliant)
// see netwerk/cookie/src/nsCookieService.cpp comment for it
ParsedCookie* CookieParser::parseOneCookie(const String& cookie, unsigned start, unsigned end, double curTime)
{
    ParsedCookie* res = new ParsedCookie(curTime);

    if (!res) {
        LOG_ERROR("Out of memory");
        return 0;
    }

    // Parse [NAME "="] VALUE

    unsigned tokenEnd = start; // Token end contains the position of the '=' or the end of a token
    unsigned pairEnd = start; // Pair end contains always the position of the ';'

    // find the *first* ';' and the '=' (if they exist)
    bool quoteFound = false;
    bool foundEqual = false;
    while (pairEnd < end && (cookie[pairEnd] != ';' || quoteFound)) {
        if (tokenEnd == start && cookie[pairEnd] == '=') {
            tokenEnd = pairEnd;
            foundEqual = true;
        }
        if (cookie[pairEnd] == '"')
            quoteFound = !quoteFound;
        pairEnd++;
    }

    unsigned tokenStart = start;

    bool hasName = false; // This is a hack to avoid changing too much in this
                          // brutally brittle code.
    if (tokenEnd != start) {
        // There is a '=' so parse the NAME
        unsigned nameEnd = tokenEnd;

        // The tokenEnd is the position of the '=' so the nameEnd is one less
        nameEnd--;

        // Remove lightweight spaces.
        while (nameEnd && isLightweightSpace(cookie[nameEnd]))
            nameEnd--;

        while (tokenStart < nameEnd && isLightweightSpace(cookie[tokenStart]))
            tokenStart++;

        if (nameEnd + 1 <= tokenStart) {
            LOG_ERROR("Empty name. Rejecting the cookie");
            delete res;
            return 0;
        }

        String name = cookie.substring(tokenStart, nameEnd + 1 - start);
        res->setName(name);
        hasName = true;
    }

    // Now parse the VALUE
    tokenStart = tokenEnd + 1;
    if (!hasName)
        --tokenStart;

    // Skip lightweight spaces in our token
    while (tokenStart < pairEnd && isLightweightSpace(cookie[tokenStart]))
        tokenStart++;

    tokenEnd = pairEnd;
    while (tokenEnd > tokenStart && isLightweightSpace(cookie[tokenEnd]))
        tokenEnd--;

    String value;
    if (tokenEnd == tokenStart) {
        // Firefox accepts empty value so we will do the same
        value = String();
    } else
        value = cookie.substring(tokenStart, tokenEnd - tokenStart);

    if (hasName)
        res->setValue(value);
    else if (foundEqual) {
        delete res;
        return 0;
    } else
        res->setName(value); // No NAME=VALUE, only NAME

    while (pairEnd < end) {
        // Switch to the next pair as pairEnd is on the ';' and fast-forward any lightweight spaces.
        pairEnd++;
        while (pairEnd < end && isLightweightSpace(cookie[pairEnd]))
            pairEnd++;

        tokenStart = pairEnd;
        tokenEnd = tokenStart; // initialize token end to catch first '='

        while (pairEnd < end && cookie[pairEnd] != ';') {
            if (tokenEnd == tokenStart && cookie[pairEnd] == '=')
                tokenEnd = pairEnd;
            pairEnd++;
        }

        // FIXME : should we skip lightweight spaces here ?

        unsigned length = tokenEnd - tokenStart;
        unsigned tokenStartSvg = tokenStart;

        String parsedValue;
        if (tokenStart != tokenEnd) {
            // There is an equal sign so remove lightweight spaces in VALUE
            tokenStart = tokenEnd + 1;
            while (tokenStart < pairEnd && isLightweightSpace(cookie[tokenStart]))
                tokenStart++;

            tokenEnd = pairEnd;
            while (tokenEnd > tokenStart && isLightweightSpace(cookie[tokenEnd]))
                tokenEnd--;

            parsedValue = cookie.substring(tokenStart, tokenEnd - tokenStart);
        } else {
            // If the parsedValue is empty, initialise it in case we need it
            parsedValue = String();
            // Handle a token without value.
            length = pairEnd - tokenStart;
        }

       // Detect which "cookie-av" is parsed
       // Look at the first char then parse the whole for performance issue
        switch (cookie[tokenStartSvg]) {
            case 'P':
            case 'p' : {
                if (length >= 4 && cookie.find("ath", tokenStartSvg + 1, false))
                    res->setPath(parsedValue);
                else {
                    LOG_ERROR("Invalid cookie %s (path)", cookie.ascii().data());
                    delete res;
                    return 0;
                }
                break;
            }

            case 'D':
            case 'd' : {
                if (length >= 6 && cookie.find("omain", tokenStartSvg + 1, false)) {
                    if (parsedValue.length() > 1 && parsedValue[0] == '"' && parsedValue[parsedValue.length() - 1] == '"') {
                        parsedValue = parsedValue.substring(1, parsedValue.length() - 2);
                    }

                    // If the domain does not start with a dot, add one for security checks
                    String realDomain = parsedValue[0] == '.' ? parsedValue : "." + parsedValue;
                    res->setDomain(realDomain);
                } else {
                    LOG_ERROR("Invalid cookie %s (domain)", cookie.ascii().data());
                    delete res;
                    return 0;
                }
                break;
            }

            case 'E' :
            case 'e' : {
                if (length >= 7 && cookie.find("xpires", tokenStartSvg + 1, false))
                    res->setExpiry(parsedValue);
                else {
                    LOG_ERROR("Invalid cookie %s (expires)", cookie.ascii().data());
                    delete res;
                    return 0;
                }
                break;
            }

            case 'M' :
            case 'm' : {
                if (length >= 7 && cookie.find("ax-age", tokenStartSvg + 1, false))
                    res->setMaxAge(parsedValue);
                else {
                    LOG_ERROR("Invalid cookie %s (max-age)", cookie.ascii().data());
                    delete res;
                    return 0;
                }
                break;
            }

            case 'C' :
            case 'c' : {
                if (length >= 7 && cookie.find("omment", tokenStartSvg + 1, false))
                    // We do not have room for the comment part (and so do Mozilla) so just log the comment.
                    LOG(Network, "Comment %s for ParsedCookie : %s\n", parsedValue.ascii().data(), cookie.ascii().data());
                else {
                    LOG_ERROR("Invalid cookie %s (comment)", cookie.ascii().data());
                    delete res;
                    return 0;
                }
                break;
            }

            case 'V' :
            case 'v' : {
                if (length >= 7 && cookie.find("ersion", tokenStartSvg + 1, false)) {
                    if (parsedValue.toInt() != 1) {
                        LOG_ERROR("ParsedCookie version %d not supported (only support version=1)", parsedValue.toInt());
                        delete res;
                        return 0;
                    }
                } else {
                    LOG_ERROR("Invalid cookie %s (version)", cookie.ascii().data());
                    delete res;
                    return 0;
                }
                break;
            }

            case 'S' :
            case 's' : {
                // Secure is a standalone token ("Secure;")
                if (length >= 6 && cookie.find("ecure", tokenStartSvg + 1, false))
                    res->setSecureFlag(true);
                else {
                    LOG_ERROR("Invalid cookie %s (secure)", cookie.ascii().data());
                    delete res;
                    return 0;
                }
                break;
            }
            case 'H':
            case 'h': {
                // HttpOnly is a standalone token ("HttpOnly;")
                if (length >= 8 && cookie.find("ttpOnly", tokenStartSvg + 1, false))
                    res->setIsHttpOnly(true);
                else {
                    LOG_ERROR("Invalid cookie %s (HttpOnly)", cookie.ascii().data());
                    delete res;
                    return 0;
                }
                break;
            }

            default : {
                // If length == 0, we should be at the end of the cookie (case : ";\r") so ignore it
                if (length) {
                    LOG_ERROR("Invalid token for cookie %s", cookie.ascii().data());
                }
            }
        }
    }

    // Check if the cookie is valid with respect to the size limit/
    if (!res->isUnderSizeLimit()) {
        LOG_ERROR("ParsedCookie %s is above the 4kb in length : REJECTED", cookie.ascii().data());
        delete res;
        return 0;
    }

    // If some pair was not provided, during parsing then apply some default value
    // the rest has been done in the constructor.

    // If no domain was provided, set it to the host
    if (!res->domain() || !res->domain().length())
        res->setDefaultDomain(m_defaultCookieURL);

    // If no path was provided, set it to the host's path
    if (!res->path() || !res->path().length()) {
        String path = m_defaultCookieURL.string().substring(m_defaultCookieURL.pathStart(), m_defaultCookieURL.pathAfterLastSlash() - m_defaultCookieURL.pathStart() - 1);
        if (path.isEmpty())
            path = "/";
        res->setPath(path);
    }
 
    return res;
}

} // namespace WebCore
