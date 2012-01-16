/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */

#ifndef AuthenticationChallenge_h
#define AuthenticationChallenge_h

#include "AuthenticationChallengeBase.h"

namespace WebCore {

class AuthenticationChallenge : public AuthenticationChallengeBase {
public:
    AuthenticationChallenge()
        : m_isStored(false)
    {
    }

    AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error)
        : AuthenticationChallengeBase(protectionSpace, proposedCredential, previousFailureCount, response, error)
        , m_isStored(false)
    {
    }

    void setStored(bool isStored) { m_isStored = isStored; }
    bool isStored() const { return m_isStored; }

private:
    bool m_isStored; // Attached credentials stored in CredentialStorage?
};

}

#endif
