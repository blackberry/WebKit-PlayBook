/*
 * $QNXLicenseC:
 * Copyright 2010, QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable
 * license fees to QNX Software Systems before you may reproduce,
 * modify or distribute this software, or any work that includes
 * all or part of this software.   Free development licenses are
 * available for evaluation and non-commercial purposes.  For more
 * information visit http://licensing.qnx.com or email
 * licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review
 * this entire file for other proprietary rights or license notices,
 * as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */

#ifndef FULLSCREENDELEGATE_H_
#define FULLSCREENDELEGATE_H_

#if PLATFORM(BLACKBERRY) && OS(QNX) && ENABLE(VIDEO)

#include "SharedObject.h"
#include <screen/screen.h>

class FullscreenDelegate : public SharedObject<FullscreenDelegate>
{
  public:
    FullscreenDelegate() : m_videoElement(0) { }
    virtual ~FullscreenDelegate() { }

    /**
     * Start fullscreen operation.
     */
    virtual int fullscreenStart(const char *contextName,
                                screen_window_t window, unsigned x, unsigned y,
                                unsigned width, unsigned height) = 0;

    /**
     * End fullscreen operation.
     */
    virtual int fullscreenStop() = 0;

    /**
     * Window set size; called when the non-fullscreen window changes.
     */
    virtual int fullscreenWindowSet(unsigned x, unsigned y, unsigned width, unsigned height) = 0;

    /**
     * The HTMLMediaElement in fullscreen mode. Used by WebView object.
     */
    void *m_videoElement;
};

#endif

#endif /* FULLSCREENDELEGATE_H_ */
