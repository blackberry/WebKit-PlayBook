/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#ifndef WebKit_TextCodec_h
#define WebKit_TextCodec_h

#include "BlackBerryGlobal.h"

#include <string>
#include <vector>

namespace Olympia {
namespace WebKit {

    enum TranscodeResult {
        Success,
        SourceEncodingUnsupported,
        SourceBroken,
        TargetEncodingUnsupported,
        TargetBufferInsufficient
    };

    enum Base64DecodePolicy {
        Base64FailOnInvalidCharacter,
        Base64IgnoreWhitespace,
        Base64IgnoreInvalidCharacters
    };

    enum Base64EncodePolicy {
        Base64DoNotInsertCRLF,
        Base64InsertCRLF
    };

    OLYMPIA_EXPORT bool isSameEncoding(const char* encoding1, const char* encoding2);
    OLYMPIA_EXPORT TranscodeResult transcode(const char* sourceEncoding, const char* targetEncoding, const char*& sourceStart, int sourceLength, char*& targetStart, unsigned int targetLength);

    OLYMPIA_EXPORT bool base64Decode(const std::string& base64, std::vector<char>& binary, Base64DecodePolicy policy);
    OLYMPIA_EXPORT bool base64Encode(const std::vector<char>& binary, std::string& base64, Base64EncodePolicy policy);

    OLYMPIA_EXPORT void unescapeURL(const std::string& escaped, std::string& url);
    OLYMPIA_EXPORT void escapeURL(const std::string& url, std::string& escaped);

    OLYMPIA_EXPORT bool getExtensionForMimeType(const std::string& mime, std::string& extension);

} // namespace WebKit
} // namespace Olympia

#endif // WebKit_TextCodec_h
