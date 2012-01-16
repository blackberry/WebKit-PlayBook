/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#include "config.h"
#include "WebKitTextCodec.h"

#include "Base64.h"
#include "CString.h"
#include "KURL.h"
#include "MIMETypeRegistry.h"
#include "TextEncoding.h"
#include "TextCodecICU.h"
#include "Vector.h"

using WebCore::TextEncoding;
using WebCore::TextCodecICU;

namespace Olympia {
namespace WebKit {


bool isSameEncoding(const char* encoding1, const char* encoding2)
{
    TextEncoding te1(encoding1);
    TextEncoding te2(encoding2);

    return te1 == te2;
}

TranscodeResult transcode(const char* sourceEncoding, const char* targetEncoding, const char*& sourceStart, int sourceLength, char*& targetStart, unsigned int targetLength)
{
    TextEncoding teSource(sourceEncoding);
    if (!teSource.isValid())
        return SourceEncodingUnsupported;

    TextEncoding teTarget(targetEncoding);
    if (!teTarget.isValid())
        return TargetEncodingUnsupported;

    TextCodecICU codecSource(teSource);

    bool sawError = false;
    WTF::String ucs2 = codecSource.decode(sourceStart, sourceLength, true, true, sawError);

    if (sawError)
        return SourceBroken;

    TextCodecICU codecTarget(teTarget);

    WTF::CString encoded = codecTarget.encode(ucs2.characters(), ucs2.length(), WebCore::EntitiesForUnencodables);

    if (encoded.length() > targetLength)
        return TargetBufferInsufficient;

    strncpy(targetStart, encoded.data(), encoded.length());
    targetStart += encoded.length();

    return Success;
}

WebCore::Base64DecodePolicy base64DecodePolicyForWebCore(Base64DecodePolicy policy)
{
    // Must make sure Base64DecodePolicy is the same in WebKit and WebCore!
    return static_cast<WebCore::Base64DecodePolicy>(policy);
}

bool base64Decode(const std::string& base64, std::vector<char>& binary, Base64DecodePolicy policy)
{
    WTF::Vector<char> vect;
    bool success = WebCore::base64Decode(base64.c_str(), base64.length(), vect, base64DecodePolicyForWebCore(policy));
    if (!success)
        return false;

    binary.insert(binary.begin(), vect.begin(), vect.end());
    return true;
}

bool base64Encode(const std::vector<char>& binary, std::string& base64, Base64EncodePolicy policy)
{
    WTF::Vector<char> vect;
    vect.append(&binary[0], binary.size());

    WebCore::base64Encode(&binary[0], binary.size(), vect, Base64InsertCRLF == policy ? true : false);

    base64.empty();
    base64.append(&vect[0], vect.size());

    return true;
}

void unescapeURL(const std::string& escaped, std::string& url)
{
    WTF::String escapedString(escaped.data(), escaped.length());

    WTF::String urlString = WebCore::decodeURLEscapeSequences(escapedString);
    WTF::CString utf8 = urlString.utf8();

    url.clear();
    url.append(utf8.data(), utf8.length());
}

void escapeURL(const std::string& url, std::string& escaped)
{
    WTF::String urlString(url.data(), url.length());

    WTF::String escapedString = WebCore::encodeWithURLEscapeSequences(urlString);
    WTF::CString utf8 = escapedString.utf8();

    escaped.clear();
    escaped.append(utf8.data(), utf8.length());
}

bool getExtensionForMimeType(const std::string& mime, std::string& extension)
{
    WTF::String mimeString(mime.data(), mime.length());

    WTF::String ext = WebCore::MIMETypeRegistry::getPreferredExtensionForMIMEType(mimeString);
    if (ext.isEmpty())
        return false;

    WTF::CString utf8 = ext.utf8();
    extension.clear();
    extension.append(utf8.data(), utf8.length());
    return true;
}

} // namespace WebKit
} // namespace Olympia
