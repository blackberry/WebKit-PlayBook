/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 */

#include "config.h"
#include "SwapGroup.h"

#include "WebPage.h"
#include "WebPage_p.h"
#include "WebPageClient.h"

namespace Olympia {
namespace WebKit {

static SwapGroup* s_currentSwapGroup = 0;
static Mutex s_currentSwapGroupMutex;

SwapGroup::SwapGroup(WebPage* webPage)
    : m_webPage(webPage)
{
    MutexLocker locker(s_currentSwapGroupMutex);
    ASSERT(!s_currentSwapGroup);
    s_currentSwapGroup = this;
}

SwapGroup::~SwapGroup()
{
    commit();

    MutexLocker locker(s_currentSwapGroupMutex);
    s_currentSwapGroup = 0;
}

SwapGroup* SwapGroup::current()
{
    MutexLocker locker(s_currentSwapGroupMutex);
    return s_currentSwapGroup;
}

bool SwapGroup::isCommitting()
{
    MutexLocker currentLocker(s_currentSwapGroupMutex);
    if (!s_currentSwapGroup)
        return false;

    MutexLocker locker(s_currentSwapGroup->m_commitMutex);
    return s_currentSwapGroup->m_committing;
}

void SwapGroup::commit()
{
    if (m_swappables.isEmpty())
        return;

    {
        MutexLocker locker(m_commitMutex);
        m_committing = true;
    }

    for (HashSet<Swappable*>::const_iterator it = m_swappables.begin(); it != m_swappables.end(); ++it)
        (*it)->swapBuffers();

    m_webPage->client()->syncToUserInterfaceThread();

    for (HashSet<Swappable*>::const_iterator it = m_swappables.begin(); it != m_swappables.end(); ++it)
        (*it)->didCommitSwapGroup();

    m_swappables.clear();

    {
        MutexLocker locker(m_commitMutex);
        m_committing = false;
    }
}

}
}
