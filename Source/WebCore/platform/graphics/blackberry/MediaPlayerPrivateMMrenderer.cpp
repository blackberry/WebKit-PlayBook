/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(VIDEO)
#include "MediaPlayerPrivateMMrenderer.h"

#include "CookieManager.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "KURL.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "RenderBox.h"
#include "ScrollView.h"
#include "TimeRanges.h"
#include "WebPageClient.h"
#include "Widget.h"

#if USE(ACCELERATED_COMPOSITING)
#include "NativeImageSkia.h"
#include <GLES2/gl2.h>
#endif

#include <BlackBerryPlatformClient.h>
#include <SkDevice.h>
#include <SkImageDecoder.h>
#include <SkScalar.h>
#include <SkXfermode.h>
#include <aoi/aoi_dataformat.h>
#include <aoi/aoi_errlog.h>
#include <errno.h>
#include <limits>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/neutrino.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

// This turns on detailed logging by ppscache, libmd and other libraries
// that use logging via aoi.
#ifndef NDEBUG
#define    ENABLE_AOI_LOGGING
#endif

#ifdef ENABLE_AOI_LOGGING

static AOErrorLevel_t minlvl = static_cast<AOErrorLevel_t>(7);

static void aoiLogger(AOErrorLevel_t level, const char* fmt, va_list ap)
{
    if (level <= minlvl) {
        flockfile(stderr);
        fprintf(stderr, "[%d] ", level);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
        funlockfile(stderr);
    }
}

#endif

// Default output devices
static const char defaultVideoOutput[] = "screen:";
static const char defaultAudioOutput[] = "audio:default";

// The ID and z-order of the window assigned for MMF to use.
#define HTML5_MMR_VIDEO_WINDOW_ID       "HTML5_video_window_MMR"
#define HTML5_MMR_VIDEO_WINDOW_ZORDER   -2

// The number of seconds that we are willing to wait for the condvar signal
// in hasVideo(). This prevents the main WebKit thread from blocking indefinitely
// in the event that the metadata thread becomes blocked indefinitely.
#define METADATA_CONDVAR_WAIT_TIME 10

// These macro converts time values between those used by mm-renderer/libmd and
// those used by webkit. mm-renderer and libmd use milliseconds (unsigned) while
// webkit uses seconds (float).
#define MMR_TIME_TO_WK_TIME(ms)     (static_cast<float>((ms)) / 1000.0)
#define WK_TIME_TO_MMR_TIME(sec)    (static_cast<uint32_t>((sec) * 1000.0))

// Use this environment variable for debugging or demos, to allow VIDEO to be
// paused/played using the h/w button or system tray while the browser is
// not in the foreground or the device is suspended. This can in some ways
// emulate a page doing this by itself using Javascript.
// NOTE THAT ANY GETENV() CALLS IN GLOBAL INITIALIZERS WILL ONLY BE
// EVALUATED ONCE PER AIRFORK RUN, WHICH IS NORMALLY ONCE PER BOOT.
static bool s_allowDeactivatedVideoPlay = (getenv("WK_ALLOW_DEACTIVATED_VIDEO_PLAY"));

// The callbacks.
static mpCallbackMsgSend_t mpCallbackMsgSend;
static void* mpCallbackData;

static mpScreenWindowFind_t mpScreenWindowFind;
static void* mpScreenWindowFindData;

// Root of the renderer context name.m_seeking
static const char mmrNameRoot[] = "wkvideo";

// An instance number. Yes, this and the previous stuff could be static members of the class.
static unsigned s_instanceId = 0;

// The set of currently active HTML5 media elements in the process. This can
// include at most one video element and any number of audio elements.
ConcurrentMediaSet MediaPlayerPrivate::s_activeElements;

// Set of pointers to all existing MediaPlayerPrivate objects, and assoc mutex.
HashSet<MediaPlayerPrivate*> MediaPlayerPrivate::s_elements;
pthread_mutex_t MediaPlayerPrivate::s_elementsMutex = PTHREAD_MUTEX_INITIALIZER;

// A comp manager context for getting frames from videos.
screen_context_t MediaPlayerPrivate::s_compManCtx = 0;

// A flag indicating whether the browser is active. If not the browser could
// be in the background (eg. minimized) or the device could be suspended.
bool MediaPlayerPrivate::s_activated = true;

// SSL certificates path to be passed to MM Framework.
String MediaPlayerPrivate::s_caPath("");

//
// Function add an integer-valued entry to a dictionary object.
// This will create the dictionary if needed.
//
static int addDictAttrIntValue(strm_dict_t** dict, const char* name, int value)
{
    if (!*dict)
        *dict = strm_dict_new();
    if (*dict) {
        char str[16];

        sprintf(str, "%d", value);
        *dict = strm_dict_set(*dict, name, str);
    }
    if (!*dict) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to allocate dictionary object for '%s'.", name);
        return -1;
    }
    return 0;
}

mpCallbackMsgSend_t mpCallbackMsgSendSet(mpCallbackMsgSend_t func, void* userdata)
{
    mpCallbackMsgSend_t old = mpCallbackMsgSend;

    mpCallbackMsgSend = func;
    mpCallbackData = userdata;

    return old;
}

mpScreenWindowFind_t mpScreenWindowFindSet(mpScreenWindowFind_t func, void* userdata)
{
    mpScreenWindowFind_t old = mpScreenWindowFind;

    mpScreenWindowFind = func;
    mpScreenWindowFindData = userdata;

    return old;
}

//
// The friend callback function for handling PPS object changes.
//
int ppsHandler(const char* name, pps_obj_event_t event, const strm_dict_t* dict, void* mp)
{
    MediaPlayerPrivate* mediaPlayer = static_cast<MediaPlayerPrivate*>(mp);

    if (!strcmp(name, "state"))
        return mediaPlayer->playStateHandle(event, dict);
    if (!strcmp(name, "status"))
        return mediaPlayer->playStatusHandle(event, dict);
    if (!strcmp(name, "metadata"))
        return mediaPlayer->metadataHandle(event, dict);
    if (!strcmp(name, "output"))
        return mediaPlayer->playOutputHandle(event, dict);
    if (!strcmp(name, "audio_status"))
        return mediaPlayer->audioStatusHandle(event, dict);

    return 0;
}

//
// Could there really be more than this number of media elements per page?
//
static const unsigned maxMsgReceivers = 64;
static MediaPlayerPrivate* msgReceivers[maxMsgReceivers];
pthread_mutex_t MediaPlayerPrivate::s_rcvrLock = PTHREAD_MUTEX_INITIALIZER;

int MediaPlayerPrivate::msgReceiverAdd()
{
    int rc = -1;
    if ((rc = pthread_mutex_lock(&s_rcvrLock)) != EOK) {
        errno = rc;
        return -1;
    }
    for (unsigned i = 0; i < maxMsgReceivers; ++i) {
        if (!msgReceivers[i]) {
            msgReceivers[i] = this;
            rc = 0;
            break;
        }
    }
    pthread_mutex_unlock(&MediaPlayerPrivate::s_rcvrLock);
    return rc;
}

int MediaPlayerPrivate::msgReceiverRemove()
{
    int rc = -1;
    if ((rc = pthread_mutex_lock(&s_rcvrLock)) != EOK) {
        errno = rc;
        return -1;
    }
    for (unsigned i = 0; i < maxMsgReceivers; ++i) {
        if (msgReceivers[i] == this) {
            msgReceivers[i] = 0;
            rc = 0;
            break;
        }
    }
    pthread_mutex_unlock(&s_rcvrLock);
    return rc;
}

//
// This function assumes the message receivers list is locked.
//
static bool msgReceiverCheck(MediaPlayerPrivate* mpp)
{
    for (unsigned i = 0; i < maxMsgReceivers; ++i) {
        if (msgReceivers[i] == mpp)
            return true;
    }
    return false;
}

void mpCallbackHandler(void* data)
{
    MediaPlayerCallbackData* cd = static_cast<MediaPlayerCallbackData*>(data);
    MediaPlayerPrivate* mediaPlayer = static_cast<MediaPlayerPrivate*>(cd->m_mp);
    char* flag = cd->m_flag;
    char* value = cd->m_value;
    float floatval = 0;
    if (!strcmp(flag, "output.available.volume"))
        floatval = cd->m_floatval;

    // Lock the list of receivers while the pointer is validated.
    pthread_mutex_lock(&MediaPlayerPrivate::s_rcvrLock);
    if (msgReceiverCheck(mediaPlayer)) {
        // Lock the object so it can't be deleted.
        mediaPlayer->callbackStart();
        pthread_mutex_unlock(&MediaPlayerPrivate::s_rcvrLock);

        if (mediaPlayer->m_allowPPSVolumeUpdates && !strcmp(flag, "output.available.volume"))
            mediaPlayer->volumeCallbackHandler(flag, floatval);
        else if (!strncmp(flag, "md_", 3))
            mediaPlayer->metadataCallbackHandler(flag, value);
        else
            mediaPlayer->mediaStateCallbackHandler(flag, value);

        mediaPlayer->callbackEnd();
    } else
        pthread_mutex_unlock(&MediaPlayerPrivate::s_rcvrLock);

    delete cd;
}

void mpResizeSourceDimensions(void* data)
{
    MediaPlayerPrivate* mediaPlayer = static_cast<MediaPlayerPrivate*>(data);
    if (mediaPlayer)
        mediaPlayer->resizeSourceDimensions();
}

const char* MediaPlayerPrivate::mmrContextNameGet()
{
    if (m_rendererContext)
        return m_rendererContext->contextNameGet();
    return 0;
}

//
// static
//
void MediaPlayerPrivate::notifyAppActivatedEvent(bool activated)
{
    if (s_activated == activated)
        return;
    s_activated = activated;

    // Don't pause or resume play if the active media element set does not have
    // video. We keep audio playing until another media source takes over,
    // which will be managed separately by the MediaServiceConnection that is
    // managed by the system tray.
    if (!s_activeElements.videoCount())
        return;

    HTMLMediaElement* element = static_cast<HTMLMediaElement*>(s_activeElements.videoElement()->m_player->mediaPlayerClient());

    if (activated) {
        // We came back into the foreground, or the device resumed from sleep
        if (s_activeElements.videoElement()->m_playOnAppActivate)
            element->play();
        else if (s_activeElements.videoElement()->m_mediaConn && !s_allowDeactivatedVideoPlay) {
            // Upon resuming from deactivation, we need to get the system tray
            // to show the media icon so the user can bring up the media
            // bubble. Tell it we are no longer "stopped". In almost all cases
            // we will be paused at this point. See the note about "stopped"
            // below. Media is always video at this point.
            if (s_activeElements.videoElement()->mmrPaused())
                s_activeElements.videoElement()->m_mediaConn->setMState(BlackBerry::Platform::MediaConnListener::Paused);
            else
                s_activeElements.videoElement()->m_mediaConn->setMState(BlackBerry::Platform::MediaConnListener::Playing);
        }
    } else {
        // Deactivating due to another app in foreground, or device sleep
        if (s_activeElements.videoElement()->mmrPaused()) {
            // Media is not playing. Don't play upon reactivation.
            s_activeElements.videoElement()->m_playOnAppActivate = false;
        } else {
            element->pause();
            // Because we initiated the pause() from here, we want to ensure
            // that we resume play upon reactivation.
            s_activeElements.videoElement()->m_playOnAppActivate = true;
        }
        // Video is not allowed to play so tell the system tray we are
        // "stopped". Otherwise the user can try to press pause/play in the
        // systray's media bubble and the state will look like it's
        // changing between pause and play, with nothing really happening.
        // The video player does the same thing, i.e. takes away the system
        // tray's media icon. I put quotes around "stopped" because this is
        // not a media play status, it's just to get the system tray to not
        // show the media icon or bubble. Media is always video at this point.
        if (s_activeElements.videoElement()->m_mediaConn && !s_allowDeactivatedVideoPlay)
            s_activeElements.videoElement()->m_mediaConn->setMState(BlackBerry::Platform::MediaConnListener::Stopped);
    }
}

//
// This function returns the user agent string associated with the
// frame loader client of our HTMLMediaElement. The call below will
// end up in FrameLoaderClientBlackBerry::userAgent().
//
const String MediaPlayerPrivate::userAgent(const String& url)
{
    HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
    Document* topdoc = element->document()->topDocument();
    ASSERT(topdoc->frame());
    ASSERT(topdoc->frame()->loader());
    if (topdoc->frame())
        return topdoc->frame()->loader()->userAgent(KURL(KURL(), url));
    return String();
}

int MediaPlayerPrivate::showErrorDialog(BlackBerry::WebKit::WebPageClient::AlertType atype) const
{
    int rc = 0;
    static bool disabled = getenv("WK_DISABLE_HTML5_ERROR_DIALOGS"); // Also checked in HTMLMediaElement.cpp
    if (!disabled) {
        HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
        Document* topdoc = element->document()->topDocument();
        if (topdoc->view() && topdoc->view()->hostWindow())
            rc = topdoc->view()->hostWindow()->platformPageClient()->showAlertDialog(atype);
    }
    return rc;
}

//
// This function is only called by play()
//
int MediaPlayerPrivate::mmrConnect()
{
    if (m_state == MP_STATE_UNSUPPORTED)
        return -1;

    if (m_rendererContext)
        return 0;

    // Make sure that the metadata thread has completed. If not, then
    // mmr_input_attach() will block until it does, which is bad for the
    // WebKit main thread.

    if (!m_metadataStarted) {
        // Hasn't even started yet. Something is very wrong
        return -1;
    }

    pthread_mutex_lock(&m_metadataFinishedMutex);
    if (!m_metadataFinished) {
        // Metadata thread is still running. Wait for it to finish, with
        // a timeout.
        int wait;
        time_t warningDelta = time(0) - m_metadataStartedTime;
        do {
            timespec ts;
            ts.tv_sec = time(0) + METADATA_CONDVAR_WAIT_TIME - warningDelta;
            ts.tv_nsec = 0;
            wait = -1;

            int rc = pthread_cond_timedwait(&m_metadataFinishedCond, &m_metadataFinishedMutex, &ts);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&m_metadataFinishedMutex);
                wait = showErrorDialog(BlackBerry::WebKit::WebPageClient::MediaMetaDataTimeoutError);
                pthread_mutex_lock(&m_metadataFinishedMutex);
                if (!wait) {
                    // User has cancelled because they don't want to wait. Make
                    // sure the media will not start playing when the metadata
                    // does finally finish (it may already be finished).
                    HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
                    element->pause();
                }
            }
            warningDelta = 0;
        } while (!m_metadataFinished && wait == 1);

        if (!m_metadataFinished || !wait) {
            pthread_mutex_unlock(&m_metadataFinishedMutex);
            return -1;
        }
    }

    pthread_mutex_unlock(&m_metadataFinishedMutex);

    // For video, we can't play if we don't have a frameView yet in order to
    // get the parent window (needed by MediaPlayerPrivate::outputsAttach).
    // Once the page loads more the user and/or web page can try to play again.
    // This fixes youtube html5 video.
    if (hasVideo() && !m_player->frameView()) {
#if (!ERROR_DISABLED)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to play %s%d: There is no frameView (yet).", mmrNameRoot, m_instanceId);
#endif
        return -1;
    }

    static bool allowConcurrentAudio = getenv("WK_ALLOW_CONCURRENT_HTML5_AUDIO");
    if (!allowConcurrentAudio && s_activeElements.mediaCount())
        s_activeElements.stop(this); // Stop everything except "this"

    if (hasVideo()) {
        // If concurrent audio is allowed, we must prevent concurrent video.
        if (allowConcurrentAudio && s_activeElements.videoElement()
            && s_activeElements.videoElement() != this)
            s_activeElements.videoElement()->stop();

        windowCleanup();
    }

    WTF::String cookiePairs;
    if (!m_url.isEmpty())
        cookiePairs = WebCore::cookieManager().getCookie(KURL(WebCore::ParsedURLString, m_url.utf8().data()), WebCore::WithHttpOnlyCookies);
    int rc = -1;
    m_mutex.lock();
    if (!cookiePairs.isEmpty() && cookiePairs.utf8().data())
        m_rendererContext = MMrendererContext::take(this, mmrNameRoot, ppsHandler, m_url.utf8().data(), m_userAgent.utf8().data(), s_caPath.utf8().data(), cookiePairs.utf8().data());
    else
        m_rendererContext = MMrendererContext::take(this, mmrNameRoot, ppsHandler, m_url.utf8().data(), m_userAgent.utf8().data(), s_caPath.utf8().data(), 0);
    m_mutex.unlock();
    if (m_rendererContext) {
        ASSERT(!s_activeElements.videoCount());
        s_activeElements.add(this);
#if USE(ACCELERATED_COMPOSITING)
        m_showBufferingImage = false;
        m_mediaIsBuffering = false;
#endif
        m_shouldBePlaying = false;
        m_lastSeekPosition = 0;
        m_skipPPSPositionUpdates = 0;
        m_lastPausedTime = 0;
        m_reseekArmedTime = 0;

        m_mediaWarningPos = -1;
        m_previousMediaWarningPos = -1;
        free(m_previousMediaWarning);
        m_previousMediaWarning = 0;
        m_mediaErrorPos = -1;
        m_previousMediaErrorPos = -1;
        m_previousMediaError = -1;

        // Send the cookies for this url to mm-renderer
        strm_dict_t* dict(0);
        m_mutex.lock();
        if (!cookiePairs.isEmpty() && cookiePairs.utf8().data())
            dict = contextDictMake(m_url.utf8().data(), m_userAgent.utf8().data(), s_caPath.utf8().data(), cookiePairs.utf8().data());
        else
            dict = contextDictMake(m_url.utf8().data(), m_userAgent.utf8().data(), s_caPath.utf8().data(), 0);
        m_mutex.unlock();
        if (dict) {
            if (m_rendererContext->parametersSet(dict)) {
#if (!ERROR_DISABLED)
                const mmr_error_info_t* ei = m_rendererContext->errorInfo();
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to set cookies on %s%d: %s.",
                      mmrNameRoot, m_instanceId, strerror(ei->error_code));
#endif
            }
        }

        // Attach the outputs and input to the context.
        if (outputsAttach() && inputAttach()) {
            m_state = MP_STATE_ACTIVE;
            rc = 0;
        } else {
            showErrorDialog(BlackBerry::WebKit::WebPageClient::MediaDecodeError);
            m_networkState = MediaPlayer::DecodeError;
            m_player->networkStateChanged();
            mmrDisconnect();
        }

        static bool disableMediaConn = getenv("WK_DISABLE_MEDIACONN");
        // Start listening to MediaConnectionHandler (i.e. system tray)
        if (!m_mediaConn && !disableMediaConn) {
            static bool noBubble = getenv("WK_SUPPRESS_HTML5_MEDIA_BUBBLE");
            if (noBubble)
                m_mediaConn = BlackBerry::Platform::MediaConn::create(this, BlackBerry::Platform::MediaConn::NoMediaBubble);
            else
                m_mediaConn = BlackBerry::Platform::MediaConn::create(this, BlackBerry::Platform::MediaConn::Normal);
        }
    }

    return rc;
}

int MediaPlayerPrivate::mmrDisconnect()
{
    if (m_rendererContext) {
        ASSERT(s_activeElements.contains(this));
        s_activeElements.remove(this);
#if USE(ACCELERATED_COMPOSITING)
        setBuffering(false);
        m_mediaIsBuffering = false;
#endif
        m_shouldBePlaying = false;
        m_audioOutputID = -1;
        m_videoOutputID = -1;
        MMrendererContext::release(m_rendererContext);
        m_inputAttached = false;
        m_rendererContext = 0;
        // Set the video rectangle such that it looks like it changed if
        // this element becomes active again.
        m_videoRect.setWidth(0);
        m_windowWidth = 0;
        m_windowHeight = 0;

        if (isFullscreen()) {
            HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
            element->exitFullscreen();
        }

        if (hasVideo()) {
            if (m_window)
                m_parentWindow->removeChildWindow(m_window);
            windowCleanup();
        }
    }

    // Stop listening to MediaConnectionHandler (i.e. system tray)
    if (m_mediaConn)
        m_mediaConn->destroy();
    m_mediaConn = 0;

    m_lastPausedTime = 0;
    m_reseekArmedTime = 0;

    m_mediaWarningPos = -1;
    m_previousMediaWarningPos = -1;
    free(m_previousMediaWarning);
    m_previousMediaWarning = 0;
    m_mediaErrorPos = -1;
    m_previousMediaErrorPos = -1;
    m_previousMediaError = -1;

    m_state = MP_STATE_IDLE;
    return 0;
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_state(MP_STATE_INITIAL)
    , m_isPaused(true)
    , m_shouldBePlaying(false)
    , m_player(player)
    , m_outputId(-1)
    , m_inputAttached(false)
    , m_threadId(-1)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_reqSpeed(1000)
    , m_currentMediaTime(0)
    , m_bufferLevel(0)
    , m_bufferCapacity(0)
    , m_curSpeed(0) // Assume paused to start, since this is done to get the load to happen.
    , m_bytesLoaded(0)
    , m_timeFailureReported(false)
    , m_bufferFailureReported(false)
    , m_title(0)
    , m_artist(0)
    , m_album(0)
    , m_artFile(0)
    , m_mmrPlayState(MMRPlayStateUnknown)
    , m_duration(0)
    , m_hasAudio(false)
    , m_hasVideo(false)
    , m_sourceWidth(0)
    , m_sourceHeight(0)
    , m_pixelWidth(1)
    , m_pixelHeight(1)
    , m_audioOutputID(-1)
    , m_videoOutputID(-1)
    , m_volume(1.0) // default is full volume?
    , m_windowX(0)
    , m_windowY(0)
    , m_windowWidth(0)
    , m_windowHeight(0)
    , m_xOrigin(0)
    , m_yOrigin(0)
    , m_zoomFactor(1.0)
    , m_rendererContext(0)
    , m_window(0)
    , m_parentWindow(0)
    , m_videoFrameContents(eVideoFrameContentsNone)
    , m_metadataStarted(false)
    , m_metadataFinished(false)
    , m_metadataStartedTime(0)
    , m_metadataFinishedCondTimeExpired(false)
    , m_fullscreenWebPageClient(0)
    , m_playOnAppActivate(false)
    , m_mediaConn(0)
    , m_allowPPSVolumeUpdates(true)
    , m_skipPPSPositionUpdates(0)
    , m_lastSystemVolumeChangeTime(0LL)
    , m_lastControlVolumeChangeTime(0LL)
    , m_lastSeekPosition(0)
    , m_lastPausedTime(0)
    , m_reseekArmedTime(0)
    , m_mediaWarningPos(-1)
    , m_previousMediaWarningPos(-1)
    , m_previousMediaWarning(0)
    , m_mediaErrorPos(-1)
    , m_previousMediaErrorPos(-1)
    , m_previousMediaError(-1)
#if USE(ACCELERATED_COMPOSITING)
    , m_bufferingTimer(this, &MediaPlayerPrivate::bufferingTimerFired)
    , m_showBufferingImage(false)
    , m_mediaIsBuffering(false)
#endif
{
    m_instanceId = ++WebCore::s_instanceId;
    pthread_mutex_lock(&s_elementsMutex);
    s_elements.add(this);
    pthread_mutex_unlock(&s_elementsMutex);

    int rc;
    if ((rc = pthread_mutex_init(&m_msgLock, 0)) != EOK)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to initialize mutex: %s (%d).", strerror(rc), rc);
    ASSERT(rc == EOK);

    if ((rc = msgReceiverAdd()) == -1)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to add to list of receivers. Are there too many?.");
    ASSERT(!rc);

    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d object constructed.", mmrNameRoot, m_instanceId);

    pthread_mutexattr_t mutexAttr;
    pthread_condattr_t condAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_condattr_init(&condAttr);
    pthread_mutex_init(&m_metadataFinishedMutex, &mutexAttr);
    pthread_cond_init(&m_metadataFinishedCond, &condAttr);
    pthread_condattr_destroy(&condAttr);
    pthread_mutexattr_destroy(&mutexAttr);

    pps_cache_t* cache = pps_cache_init(0);
    if (!cache)
        return;

    strm_dict_t* dict = pps_cache_object_get_once("/pps/services/audio/status");
    if (!dict)
        return;

    setVolumeFromDict(dict);
    strm_dict_destroy(dict);
    pps_cache_terminate(cache);
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    pthread_mutex_lock(&s_elementsMutex);
    s_elements.remove(this);
    pthread_mutex_unlock(&s_elementsMutex);
#if USE(ACCELERATED_COMPOSITING)
    // remove media player from platform layer
    if (m_platformLayer)
        m_platformLayer->setMediaPlayer(0);
#endif

    callbackStart();
    msgReceiverRemove();

    pthread_mutex_destroy(&m_msgLock);

    pthread_cond_destroy(&m_metadataFinishedCond);
    pthread_mutex_destroy(&m_metadataFinishedMutex);

    mmrDisconnect(); // Removes us from the s_activeElements set.

    m_videoFrame.reset();

    if (m_hasVideo)
        windowCleanup();
    free(m_title);
    free(m_artist);
    free(m_album);
    if (m_artFile) {
        unlink(m_artFile);
        free(m_artFile);
    }
    m_player = 0;
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d object destructed.", mmrNameRoot, m_instanceId);
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* MediaPlayerPrivate::platformLayer() const
{
    if (m_platformLayer)
        return m_platformLayer.get();

    return 0;
}
#endif

int MediaPlayerPrivate::volumeCallbackMsgSend(const char* flag, const char* value, float newVolume)
{
    MediaPlayerCallbackData* data = new MediaPlayerCallbackData(static_cast<void*>(this), flag, value);
    data->m_floatval = newVolume;
    callOnMainThread(mpCallbackHandler, static_cast<void*>(data));
    return 0;
}

int MediaPlayerPrivate::mediaStateCallbackMsgSend(const char* flag, const char* value)
{
    MediaPlayerCallbackData* data = new MediaPlayerCallbackData(static_cast<void*>(this), flag, value);
    callOnMainThread(mpCallbackHandler, static_cast<void*>(data));
    return 0;
}

const char* networkStateStr(MediaPlayer::NetworkState state)
{
    const char* str = "<invalid>";

    switch (state) {
    case MediaPlayer::Empty:
        str = "Empty";
        break;
    case MediaPlayer::Idle:
        str = "Idle";
        break;
    case MediaPlayer::Loading:
        str = "Loading";
        break;
    case MediaPlayer::Loaded:
        str = "Loaded";
        break;
    case MediaPlayer::FormatError:
        str = "FormatError";
        break;
    case MediaPlayer::NetworkError:
        str = "NetworkError";
        break;
    case MediaPlayer::DecodeError:
        str = "DecodeError";
        break;
    }

    return str;
}

const char* readyStateStr(MediaPlayer::ReadyState state)
{
    const char* str = "<invalid>";

    switch (state) {
    case MediaPlayer::HaveNothing:
        str = "HaveNothing";
        break;
    case MediaPlayer::HaveMetadata:
        str = "HaveMetadata";
        break;
    case MediaPlayer::HaveCurrentData:
        str = "HaveCurrentData";
        break;
    case MediaPlayer::HaveFutureData:
        str = "HaveFutureData";
        break;
    case MediaPlayer::HaveEnoughData:
        str = "HaveEnoughData";
        break;
    }

    return str;
}

const char* MediaPlayerPrivate::mmrPlaystateStr(MMRPlayState state) const
{
    const char* str = "<invalid>";

    switch (state) {
    case MMRPlayStateUnknown:
        str = "MMRPlayStateUnknown";
        break;
    case MMRPlayStatePlaying:
        str = "MMRPlayStatePlaying";
        break;
    case MMRPlayStateStopped:
        str = "MMRPlayStateStopped";
        break;
    case MMRPlayStateIdle:
        str = "MMRPlayStateIdle";
        break;
    }

    return str;
}

const char* MediaPlayerPrivate::cbFlagStr(uint32_t flag) const
{
    const char* str = "<invalid>";

    switch (flag) {
    case MP_REPAINT:
        str = "repaint";
        break;
    case MP_NET_STATE_CHANGED:
        str = "network state changed";
        break;
    case MP_RDY_STATE_CHANGED:
        str = "ready state changed";
        break;
    case MP_VOLUME_CHANGED:
        str = "volume changed";
        break;
    case MP_TIME_CHANGED:
        str = "time changed";
        break;
    case MP_SIZE_CHANGED:
        str = "size changed";
        break;
    case MP_RATE_CHANGED:
        str = "rate changed";
        break;
    case MP_DURATION_CHANGED:
        str = "duration changed";
        break;
    case MP_METADATA_SET:
        str = "metadata set";
        break;
    case MP_READ_ERROR:
        str = "read error";
        break;
    case MP_MEDIA_ERROR:
        str = "media error";
        break;
#if USE(ACCELERATED_COMPOSITING)
    case MP_BUFFERING_STARTED:
        str = "buffering started";
        break;
    case MP_BUFFERING_ENDED:
        str = "buffering ended";
        break;
#endif
    }

    return str;
}

const char* MediaPlayerPrivate::stateStr(MpState state) const
{
    const char* str = "<invalid>";

    switch (state) {
    case MP_STATE_INITIAL:
        str = "Initial";
        break;
    case MP_STATE_IDLE:
        str = "Idle";
        break;
    case MP_STATE_ACTIVE:
        str = "Active";
        break;
    case MP_STATE_UNSUPPORTED:
        str = "Unsupported";
        break;
    }

    return str;
}

void MediaPlayerPrivate::volumeCallbackHandler(const char* flag, float newVolume)
{
    if (fabs(newVolume - m_volume) > 0.001) { // Not within 0.1%.
        timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        uint64_t currentTime = timespec2nsec(&time);
        if (currentTime - m_lastControlVolumeChangeTime < 100000000) {
            // It has been less than a tenth of a second since a control volume
            // change happened. To avoid volume setting loops we'll assume that
            // this volume change was triggered based on the control change.
            // Otherwise we might overwrite even more recent volume values in
            // a stream of control volume changes, because the confirmation
            // of the change arrives here delayed.
            return;
        }
        m_player->volumeChanged(newVolume);
        m_lastSystemVolumeChangeTime = currentTime;
        m_volume = newVolume;
    }
}

void MediaPlayerPrivate::mediaStateCallbackHandler(const char* flag, const char* value)
{
    // for media state change.
    if (!strcmp(flag, "state")) {
        MMRPlayState oldState = m_mmrPlayState;
        m_mmrPlayState = mmrStateFromString(value);
        if (oldState != m_mmrPlayState) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d playback state: %s.",  mmrNameRoot, m_instanceId, value);
            // TODO: determine how these states are relevant and mapped to webkit.
            switch (m_mmrPlayState) {
            case MMRPlayStateIdle:
                // TODO: Does this mean the ready state is now 'have nothing'?
                if (m_networkState != MediaPlayer::Idle) {
                    m_networkState = MediaPlayer::Idle;
                    mediaStateChangedProcessing(MP_REPAINT);
                    mediaStateChangedProcessing(MP_NET_STATE_CHANGED);
                }
                break;
            case MMRPlayStatePlaying:
                // Need EOF equivalent to know if loading or loaded. Assume loading for now.
                if (m_networkState != MediaPlayer::Loading) {
                    m_networkState = MediaPlayer::Loading;
                    mediaStateChangedProcessing(MP_REPAINT);
                    mediaStateChangedProcessing(MP_NET_STATE_CHANGED);
                }
                break;
            case MMRPlayStateStopped:
                // TODO: Does this mean the ready state is now 'have nothing'?
                if (m_networkState != MediaPlayer::Idle) {
                    m_networkState = MediaPlayer::Idle;
                    if (oldState == MMRPlayStatePlaying) {
                        // This means stopped.
                        // m_duration = 0;
                        // mediaStateChangedProcessing(MP_DURATION_CHANGED);
                        m_currentMediaTime = m_duration;
                        mediaStateChangedProcessing(MP_TIME_CHANGED);
                    }
                    mediaStateChangedProcessing(MP_REPAINT);
                    mediaStateChangedProcessing(MP_NET_STATE_CHANGED);
                }
                break;
            case MMRPlayStateUnknown:
            default:
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Received invalid play state from mm_renderer of '%s'.", value);
                m_mmrPlayState = oldState;
                break;
            }
        }
    } else if (!strcmp(flag, "speed")) {
        uint32_t newSpeed = strtoul(value, 0, 10);

        if (newSpeed != m_curSpeed) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d playback speed: %s.",  mmrNameRoot, m_instanceId, value);
            m_curSpeed = newSpeed;
            mediaStateChangedProcessing(MP_RATE_CHANGED);

            if (m_mmrPlayState == MMRPlayStatePlaying && !m_curSpeed && m_currentMediaTime == m_duration) {
                m_networkState = MediaPlayer::Idle;
                m_mmrPlayState = MMRPlayStateIdle;
                mediaStateChangedProcessing(MP_REPAINT);
                mediaStateChangedProcessing(MP_NET_STATE_CHANGED);
            }
        }
    } else if (!strcmp(flag, "warning_pos")) {
        if (value)
            m_mediaWarningPos = strtol(value, 0, 10);
        else
            m_mediaWarningPos = -1;
    } else if (!strcmp(flag, "warning")) {
        if (value) {
            if (m_mediaWarningPos != -1 && (m_mediaWarningPos != m_previousMediaWarningPos || !m_previousMediaWarning || strcmp(value, m_previousMediaWarning))) {
                free(m_previousMediaWarning);
                m_previousMediaWarning = strdup(value);
                if (!strncasecmp(value, "Read error", 10))
                    mediaStateChangedProcessing(MP_READ_ERROR);
            }
            m_previousMediaWarningPos = m_mediaWarningPos;
        } else {
            m_previousMediaWarningPos = -1;
            free(m_previousMediaWarning);
            m_previousMediaWarning = 0;
        }
    } else if (!strcmp(flag, "error_pos")) {
        if (value)
            m_mediaErrorPos = strtol(value, 0, 10);
        else
            m_mediaErrorPos = -1;
    } else if (!strcmp(flag, "error")) {
        if (value) {
            int errval = strtol(value, 0, 10);
            if (m_mediaErrorPos != -1 && (m_mediaErrorPos != m_previousMediaErrorPos || errval != m_previousMediaError)) {
                // We assume that a second error will not come from the pps
                // thread before we have time to process the first one. This
                // is a valid assumption because mm-renderer will stop play.
                // So we only save the most recent error code.
                m_previousMediaError = errval;
                mediaStateChangedProcessing(MP_MEDIA_ERROR);
            }
            m_previousMediaErrorPos = m_mediaErrorPos;
        } else {
            m_previousMediaErrorPos = -1;
            m_previousMediaError = -1;
        }
    } else if (!strcmp(flag, "position")) { // for media status change.
        if (m_mmrPlayState != MMRPlayStatePlaying)
            return;
        if (value) {
            m_timeFailureReported = false;
            // MM-renderer was recently changed to also send position updates
            // while media is paused. This requires extra checking here:
            // 1) Only process this position change if our state is active.
            // Otherwise this is likely a late PPS message received for a
            // different media player object, because we share mm-renderer
            // contexts. This fixes media position updates from previous
            // media affecting media we just switched to.
            // 2) Also make sure that we are not within the short window of
            // time between when the user presses play and mm-renderer tells us
            // the speed has gone from 0 to 1000. This fixes play() getting
            // called twice, which was causing problems for m_reseekArmedTime.
            // 3) We already made sure the mmr state is "playing", which fixes
            // the problem where media position updates after the media stops
            // at the end make it replay from the beginning.
            // 4) Make sure we are not within the short window of time between
            // when the user presses pause and mm-renderer tells us the speed
            // has gone from 1000 to 0. Fixes one case of seek slider jitter
            // because seek is typically preceded by pause (see item 6 below).
            // 5) For item 4, the m_shouldBePlaying based check can fail if
            // the pause/play sequence comes too quickly. For that case we
            // have an extra mechanism to disallow position updates. See comment
            // in MediaPlayerPrivate::play().
            // 6) When the user drags or touches the native media control's
            // timeline slider, WebKit pauses the media player while seek
            // requests are sent. This creates the potential for a lot of
            // timeline slider thumb jitter. But we can't just prevent position
            // updates during pause because this would break two features we
            // need: updating of the buffered bar during pause, and having
            // the slider thumb be in the correct position after seeks done
            // while paused in fullscreen mode. Instead we filter out position
            // updates based on the last seek position.
            uint32_t position = strtoul(value, 0, 10);
            if (position > m_duration)
                position = m_duration;
            if (m_state == MP_STATE_ACTIVE && m_shouldBePlaying != mmrPaused() && !m_skipPPSPositionUpdates
                 && (!mmrPaused() || position == m_lastSeekPosition || !m_lastSeekPosition)) {
                m_currentMediaTime = position;

                // BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Position for %s%d set to %u ms.", mmrNameRoot, m_instanceId, m_currentMediaTime);
                // Tell the generic player that time has changed.
                mediaStateChangedProcessing(MP_TIME_CHANGED);
            }
            if (m_skipPPSPositionUpdates > 0)
                --m_skipPPSPositionUpdates;
        } else if (!m_timeFailureReported) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "No position found in status object for %s%d.", mmrNameRoot, m_instanceId);
            m_timeFailureReported = true;
        }
    } else if (!strcmp(flag, "bufferlevel")) {
        if (value) {
            size_t len = strlen(value)+1;
            char copy[len];
            char* capstr;

            memcpy(copy, value, len);
            capstr = strchr(copy, '/');
            if (capstr) {
                *capstr = 0;
                capstr++;

                m_bufferLevel = strtoul(copy, 0, 10);
                m_bufferCapacity = strtoul(capstr, 0, 10);

                m_bufferFailureReported = false;
            } else if (!m_bufferFailureReported) {
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Bufferlevel improperly formatted in status object for %s%d (%s).",
                    mmrNameRoot, m_instanceId, value);
                m_bufferFailureReported = true;
            }
        } else if (!m_bufferFailureReported && m_mmrPlayState == MMRPlayStatePlaying) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "No bufferlevel found in status object for %s%d.", mmrNameRoot, m_instanceId);
            m_bufferFailureReported = true;
        }
    }
#if USE(ACCELERATED_COMPOSITING)
    else if (!strcmp(flag, "bufferstatus")) {
        if (!value)
            return;
        if (!strcmp(value, "buffering")) {
            if (!m_mediaIsBuffering) {
                m_mediaIsBuffering = true;
                mediaStateChangedProcessing(MP_BUFFERING_STARTED);
            }
        } else {
            if (m_mediaIsBuffering) {
                m_mediaIsBuffering = false;
                mediaStateChangedProcessing(MP_BUFFERING_ENDED);
            }
        }
    }
#endif
}

int MediaPlayerPrivate::mediaStateChangedProcessing(uint32_t flags)
{
    int rc = 0;

    pausedStateUpdate();

    switch (flags) {
    case MP_NET_STATE_CHANGED:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Reporting network state change to %s.", networkStateStr(m_networkState));
        m_player->networkStateChanged();
        break;
    case MP_RDY_STATE_CHANGED:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Reporting ready state change to %s.", readyStateStr(m_readyState));
        m_player->readyStateChanged();
#if USE(ACCELERATED_COMPOSITING)
        // create platform layer for video
        if (!m_platformLayer) {
            m_platformLayer = LayerWebKitThread::create(LayerData::Layer, 0);
            m_platformLayer->setMediaPlayer(m_player);
        }
#endif
        break;
    case MP_VOLUME_CHANGED:
        // BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Reporting volume change.");
        // TODO: MMF can handle volume changes, but does not report the volume.
        // TBD what the MMF plugin to mm-renderer will do. So at this point,
        // can't report the real volume, so report the requested volume.
        // Also, these should be reported only if not caused by the controller.
        // More TODO: How to report volume changes made by fullscreen controller
        // if mm-renderer can't report the volume?
        // m_player->volumeChanged(m_volume);
        break;
    case MP_TIME_CHANGED:
        // If we are deactivated this must be audio because video would have
        // been paused. In any case, to save CPU don't keep kicking the player
        // with time changes. The backing store will already have suspended
        // Screen updates, but this check saves another few percent of CPU.
        // Don't filter out time changes when media pauses or reaches the end.
        // Also don't filter out time changes when media is at the very
        // beginning. This allows playlist based media players like Slacker
        // to recognize song transitions. Otherwise automatically switching
        // to the next track does not work while suspended.
        if (s_activated || m_currentMediaTime == m_duration || !m_currentMediaTime || mmrPaused())
            m_player->timeChanged();
        // We always update the system tray because it is global. This takes
        // very little CPU anyways.
        if (m_mediaConn)
            m_mediaConn->setPosition(m_duration, m_currentMediaTime);
        break;
    case MP_SIZE_CHANGED:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Reporting size change.");
        m_player->sizeChanged();
        break;
    case MP_RATE_CHANGED:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Reporting rate change.");
        m_player->rateChanged();
        break;
    case MP_DURATION_CHANGED:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Reporting duration change to %f.", duration());
        m_player->durationChanged();
        // Update the system tray's duration display
        if (m_mediaConn)
            m_mediaConn->setPosition(m_duration, m_currentMediaTime);
        break;
    case MP_REPAINT:
        m_player->repaint();
        break;
    case MP_METADATA_SET:
        if (m_mediaConn) {
            char* url = 0;
            const char* title = m_title;
            if (!title) {
                // There is no title in the metadata. If the url is a fairly
                // straightforward media file with a 3 or 4 character extension
                // such as mp3, use the filename as the title.
                m_mutex.lock();
                String surl = decodeURLEscapeSequences(m_url);
                m_mutex.unlock();
                url = strdup(surl.utf8().data());
                char* dot = strrchr(url, '.');
                if (dot) {
                    int len = strlen(dot) - 1;
                    if (len == 3 || len == 4) {
                        *dot = '\0';
                        title = strrchr(url, '/');
                        if (title)
                            title++;
                        else
                            title = url;
                    }
                }
            }
            // Make metadata available to system tray.
            m_mediaConn->setMetadata(title, m_artist, m_album, m_artFile, m_duration, m_currentMediaTime);
            free(url);
        }
        break;
    case MP_READ_ERROR:
        if (m_reseekArmedTime) {
            // If we have resumed from a long pause, then a
            // read error will be used as an indication to re-seek. Most
            // likely the server has timed out the http streaming connection.
            seekms(m_currentMediaTime);
            m_reseekArmedTime = 0;
        }
        break;
    case MP_MEDIA_ERROR:
        if (m_previousMediaError > MMR_ERROR_NONE) {
            if (hasVideo())
                showErrorDialog(BlackBerry::WebKit::WebPageClient::MediaVideoReceiveError);
            else
                showErrorDialog(BlackBerry::WebKit::WebPageClient::MediaAudioReceiveError);
        }
        break;
    case MP_SHOW_ERROR_DIALOG:
        showErrorDialog(m_pendingErrorDialog);
        break;
#if USE(ACCELERATED_COMPOSITING)
    case MP_BUFFERING_STARTED:
        setBuffering(true);
        break;
    case MP_BUFFERING_ENDED:
        setBuffering(false);
        break;
#endif
    default:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Attempt to use callback with unknown flag value of %u.", flags);
        rc = -1;
        break;
    }
    return rc;
}

void MediaPlayerPrivate::metadataCallbackHandler(const char* flag, const char* value)
{
    if (!strcmp(flag, "md_title_mediatype")) {
        m_hasAudio = (*value & MEDIA_TYPE_AUDIO);
        m_hasVideo = (*value & MEDIA_TYPE_VIDEO);

        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d metadata object 'mediatype' value '%s'.",  mmrNameRoot, m_instanceId, value);
    } else if (!strcmp(flag, "md_video_height")) {
        unsigned newHeight = strtoul(value, 0, 10);
        if (m_sourceHeight != newHeight) {
            m_sourceHeight = newHeight;
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d metadataCallbackHandler(): height set to %d.",  mmrNameRoot, m_instanceId, m_sourceHeight);
        }
    } else if (!strcmp(flag, "md_video_width")) {
        unsigned newWidth = strtoul(value, 0, 10);
        if (m_sourceWidth != newWidth) {
            m_sourceWidth = newWidth;
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d metadataCallbackHandler(): width set to %d.",  mmrNameRoot, m_instanceId, m_sourceWidth);
        }
    } else if (!strcmp(flag, "md_video_pixel_height")) {
        unsigned newHeight = strtoul(value, 0, 10);
        if (m_pixelHeight != newHeight) {
            m_pixelHeight = newHeight;
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d metadataCallbackHandler(): pixel height set to %d.",  mmrNameRoot, m_instanceId, m_pixelHeight);
        }
    } else if (!strcmp(flag, "md_video_pixel_width")) {
        unsigned newWidth = strtoul(value, 0, 10);
        if (m_pixelWidth != newWidth) {
            m_pixelWidth = newWidth;
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d metadataCallbackHandler(): pixel width set to %d.",  mmrNameRoot, m_instanceId, m_pixelWidth);
        }
    } else if (!strcmp(flag, "md_title_duration")) {
        uint32_t newDuration = strtoul(value, 0, 10);
        if (m_duration != newDuration) {
            uint32_t oldDuration = m_duration;
            m_duration = newDuration;
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d playback duration set to %u ms.", mmrNameRoot, m_instanceId, m_duration);
            mediaStateChangedProcessing(MP_DURATION_CHANGED);
            if (!oldDuration && m_readyState != MediaPlayer::HaveEnoughData) {
                m_readyState = MediaPlayer::HaveEnoughData;
                mediaStateChangedProcessing(MP_RDY_STATE_CHANGED);
            }
        }
    } else if (!strcmp(flag, "md_getdata_error")) {
        static bool showMetadataErrors = getenv("WK_SHOW_DETAILED_MEDIA_ERRORS");
        m_networkState = MediaPlayer::NetworkError;
        mediaStateChangedProcessing(MP_NET_STATE_CHANGED);

        // It is not usually a good idea to show an error dialog at this point,
        // so the showMetadataErrors flag is false by default. Metadata is
        // generally loaded when the page is first loaded, before the user
        // presses play on a particular media or even knows if the page
        // contains media. There could also be multiple media. In general we
        // only want to show an error dialog once the user presses play and is
        // in a "media mindset". This is already being done by
        // HTMLMediaElement::playInternal().
        if (showMetadataErrors) {
            m_pendingErrorDialog = BlackBerry::WebKit::WebPageClient::MediaMetaDataError;
            mediaStateChangedProcessing(MP_SHOW_ERROR_DIALOG);
        }
    } else if (!strcmp(flag, "md_data_set"))
        mediaStateChangedProcessing(MP_METADATA_SET);
    else if (!strcmp(flag, "md_duration_changed"))
        mediaStateChangedProcessing(MP_DURATION_CHANGED);
    else if (!strcmp(flag, "md_rdy_state_changed"))
        mediaStateChangedProcessing(MP_RDY_STATE_CHANGED);
    else if (!strcmp(flag, "md_size_changed"))
        mediaStateChangedProcessing(MP_SIZE_CHANGED);
}

MediaPlayerPrivate::MMRPlayState MediaPlayerPrivate::mmrStateFromString(const char* state)
{
    if (!strcmp(state, "playing"))
        return MMRPlayStatePlaying;
    if (!strcmp(state, "stopped"))
        return MMRPlayStateStopped;
    if (!strcmp(state, "idle"))
        return MMRPlayStateIdle;

    return MMRPlayStateUnknown;
}

int MediaPlayerPrivate::metadataHandle(pps_obj_event_t event, const strm_dict_t* dict)
{
    const char* value;

    // Note: Libppscache does not operate in delta mode, so we get no
    // indication of which attribute changed or got deleted. Where necessary
    // we must do our own delta checks. Right now these are being done
    // in the main thread, in metadataCallbackHandler().
    switch (event) {
    case pps_obj_event_new:
    case pps_obj_event_change:
        break;
    case pps_obj_event_trunc:
    case pps_obj_event_delete:
    case pps_obj_event_unknown:
    default:
        // ignore this.
        return 0;
        break;
    }

    // TODO: Use MMF-specific metadata attribute names until they are generically decided upon.
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Handling metadata object for %s%d.", mmrNameRoot, m_instanceId);

    value = strm_dict_find_value(dict, "md_title_mediatype");
    if (value)
        mediaStateCallbackMsgSend("md_title_mediatype", value);

    // Intrinsic dimensions of video
    value = strm_dict_find_value(dict, "md_video_height");
    if (value)
        mediaStateCallbackMsgSend("md_video_height", value);
    value = strm_dict_find_value(dict, "md_video_width");
    if (value)
        mediaStateCallbackMsgSend("md_video_width", value);

    // Pixel aspect ratio
    value = strm_dict_find_value(dict, "md_video_pixel_height");
    if (value)
        mediaStateCallbackMsgSend("md_video_pixel_height", value);
    value = strm_dict_find_value(dict, "md_video_pixel_width");
    if (value)
        mediaStateCallbackMsgSend("md_video_pixel_width", value);

    // Length or duration of track
    value = strm_dict_find_value(dict, "md_title_duration");
    if (value)
        mediaStateCallbackMsgSend("md_title_duration", value);

    return 0;
}

int MediaPlayerPrivate::playStateHandle(pps_obj_event_t event, const strm_dict_t* dict)
{
    const char* value;

    // Note: Libppscache does not operate in delta mode, so we get no
    // indication of which attribute changed or got deleted. Where necessary
    // we must do our own delta checks. Right now these are being done in
    // the main thread, in mediaStateCallbackHandler().
    // Also note that the code in services/mm-renderer/core/ppswriter.c
    // function write_attr() currently does not re-write an attribute if it
    // was already written with the same value, so pps delta mode would not
    // work in a totally straightforward way anyways. For example, a changed
    // warning_pos without a new warning means a repeat of the existing
    // warning.
    switch (event) {
    case pps_obj_event_new:
    case pps_obj_event_change:
    case pps_obj_event_delete:
        break;
    case pps_obj_event_trunc:
    case pps_obj_event_unknown:
    default:
        // ignore this.
        return 0;
        break;
    }

    value = strm_dict_find_value(dict, "state");
    if (value)
        mediaStateCallbackMsgSend("state", value);

    value = strm_dict_find_value(dict, "speed");
    if (value)
        mediaStateCallbackMsgSend("speed", value);

    // Even though "warning_pos" is written separately and before "warning",
    // this callback does not seem to get called twice. If it did, then it
    // would make sense to have the processing of "warning_pos" come after
    // "warning", but it does not.
    value = strm_dict_find_value(dict, "warning_pos");
    mediaStateCallbackMsgSend("warning_pos", value);

    value = strm_dict_find_value(dict, "warning");
    mediaStateCallbackMsgSend("warning", value);

    // Even though "error_pos" is written separately and before "error",
    // this callback does not seem to get called twice. If it did, then it
    // would make sense to have the processing of "error_pos" come after
    // "error", but it does not.
    value = strm_dict_find_value(dict, "error_pos");
    mediaStateCallbackMsgSend("error_pos", value);

    value = strm_dict_find_value(dict, "error");
    mediaStateCallbackMsgSend("error", value);

    return 0;
}

int MediaPlayerPrivate::playOutputHandle(pps_obj_event_t event, const strm_dict_t* dict)
{
    // Note: Libppscache does not operate in delta mode, so we get no
    // indication of which attribute changed or got deleted. Where necessary
    // we must do our own delta checks.
    switch (event) {
    case pps_obj_event_new:
    case pps_obj_event_change:
        break;
    case pps_obj_event_trunc:
    case pps_obj_event_delete:
        // TODO: Detach outputs? Or is this triggered by them being detached?
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d playback output detached.",  mmrNameRoot, m_instanceId);
        break;
    case pps_obj_event_unknown:
    default:
        // ignore this.
        break;
    }

    return 0;
}

int MediaPlayerPrivate::playStatusHandle(pps_obj_event_t event, const strm_dict_t* dict)
{
    const char* value;

    // Note: Libppscache does not operate in delta mode, so we get no
    // indication of which attribute changed or got deleted. Where necessary
    // we must do our own delta checks. Right now these are being done in
    // the main thread, in mediaStateCallbackHandler().
    switch (event) {
    case pps_obj_event_new:
    case pps_obj_event_change:
        break;
    case pps_obj_event_trunc:
    case pps_obj_event_delete:
    case pps_obj_event_unknown:
    default:
        // ignore this.
        return 0;
        break;
    }

    // Note: At the end of playback, the current buffer level will go to zero.
    // If webkit is then told that the ready state is HaveNothing, then it
    // will not start playback again if the 'loop' attribute is used.
    // This is in part why the buffer levels as reported by mm-renderer are
    // not used in the state transitions.
    // 2011Nov7, MKF: We send buffer level before position because it's the
    // position update which calls m_player->timeChanged() and ends up updating
    // the percentLoaded in the seek slider.
    value = strm_dict_find_value(dict, "bufferlevel");
    mediaStateCallbackMsgSend("bufferlevel", value);

    value = strm_dict_find_value(dict, "position");
    mediaStateCallbackMsgSend("position", value);

#if USE(ACCELERATED_COMPOSITING)
    value = strm_dict_find_value(dict, "bufferstatus");
    mediaStateCallbackMsgSend("bufferstatus", value);
#endif

    return 0;
}

int MediaPlayerPrivate::audioStatusHandle(pps_obj_event_t event, const strm_dict_t* dict)
{
    // Note: Libppscache does not operate in delta mode, so we get no
    // indication of which attribute changed or got deleted. Where necessary
    // we must do our own delta checks.
    switch (event) {
    case pps_obj_event_new:
    case pps_obj_event_change:
        break;
    case pps_obj_event_trunc:
    case pps_obj_event_delete:
    case pps_obj_event_unknown:
    default:
        // ignore this.
        return 0;
        break;
    }

    setVolumeFromDict(dict);

    return 0;
}

void MediaPlayerPrivate::setVolumeFromDict(const strm_dict_t* dict)
{
    // Process the system's audio status object
    const char* availableOutputDevice = strm_dict_find_value(dict, "output.available");
    if (!availableOutputDevice) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to query currently available output device from output.available");
        return;
    }

    // Find the volume of the output device
    char* key = static_cast<char*>(malloc(strlen("output.") + strlen(availableOutputDevice) + max(strlen(".volume"), strlen(".muted")) + 1));
    sprintf(key, "output.%s.volume", availableOutputDevice);
    const char* value = strm_dict_find_value(dict, key);
    if (value) {
        char* stopValue;
        float newVolume = strtof(value, &stopValue) / 100;

        // Check if the output device is muted
        sprintf(key, "output.%s.muted", availableOutputDevice);
        value = strm_dict_find_value(dict, key);
        if (value && !strcmp(value, "true"))
            newVolume = 0.0f;

        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Currently available output device is: %s, volume level: %.6f, muted: %s", availableOutputDevice, newVolume, value);
        volumeCallbackMsgSend("output.available.volume", value, newVolume);
    }
    free(key);
}

void* workerThread(void* mp)
{
    MediaPlayerPrivate* player = static_cast<MediaPlayerPrivate*>(mp);

    // Make copies of all String parameters so they are safe to use on the
    // metadata worker thread (the String class is not thread safe, especially
    // String::utf8())
    player->m_mutex.lock();
    String url = player->m_url.isolatedCopy();
    String userAgent = player->m_userAgent.isolatedCopy();
    String caPath = player->s_caPath.isolatedCopy();
    player->m_mutex.unlock();

    return player->worker(url, userAgent, caPath);
}

void* MediaPlayerPrivate::worker(String& url, String& userAgent, String& caPath)
{
    pthread_mutex_lock(&s_elementsMutex);

    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d started worker thread (tid=%d).", mmrNameRoot, m_instanceId, pthread_self());

    // Initialize metadata library handling.
    mmmd_hdl_t* mdHdl = metadataInit(url, userAgent, caPath);
    if (!mdHdl) {
        m_state = MP_STATE_UNSUPPORTED;
        signalMetadataCondvarFinished();
        pthread_mutex_unlock(&s_elementsMutex);
        return 0;
    }

    // Get the metadata and inform the main thread when have it.
    int rc = metadataGet(url, mdHdl);
    if (rc != -2) {
        if (rc == -1) {
            // A failure to get metadata means this media cannot be played.
            m_state = MP_STATE_UNSUPPORTED;
        }

        // No need to keep the metadata session any longer.
        metadataTerminate(mdHdl);

        m_threadId = -1;

        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d worker thread exiting (tid=%d).", mmrNameRoot, m_instanceId, pthread_self());
    } else {
        // Emergency thread exit. Cannot trust "this" pointer.
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d Metadata worker thread %d assumed corrupt!", mmrNameRoot, m_instanceId, pthread_self());
    }

    pthread_mutex_unlock(&s_elementsMutex);
    return 0;
}

bool MediaPlayerPrivate::workerThreadStart(int priority)
{
    int rc;
    pthread_attr_t attr;
    struct sched_param param;
    int policy;

    pthread_mutex_lock(&m_metadataFinishedMutex);
    bool alreadyDidThis = m_metadataStarted || m_metadataFinished;
    pthread_mutex_unlock(&m_metadataFinishedMutex);
    if (alreadyDidThis)
        return true;

    pthread_attr_init(&attr);
    if (priority) {
        // fill in the param structure from the current thread before
        // setting priority
        rc = pthread_getschedparam(pthread_self(), &policy, &param);
        if (rc != EOK) {
            pthread_attr_destroy(&attr);
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Error getting scheduling;"
                " cannot adjust worker thread priority: %s (%d).", strerror(rc), rc);
            errno = rc;
            return false;
        }

        param.sched_priority = priority;

        rc = pthread_attr_setschedparam(&attr, &param);
        if (rc != EOK) {
            pthread_attr_destroy(&attr);
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Error setting worker thread priority to %d: %s (%d).",
                priority, strerror(rc), rc);
            errno = rc;
            return false;
        }

        rc = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        if (rc != EOK) {
            pthread_attr_destroy(&attr);
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Error setting worker thread attributes"
                " to explicit: %s (%d).", strerror(rc), rc);
            errno = rc;
            return false;
        }
    }

    // Set detached; normally will end after getting metadata.
    rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (rc != EOK) {
        pthread_attr_destroy(&attr);
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Error setting worker thread state"
             " to detached: %s (%d).", strerror(rc), rc);
        errno = rc;
        return false;
    }

    rc = pthread_create(&this->m_threadId, &attr, &workerThread, this);
    if (rc != EOK) {
        pthread_attr_destroy(&attr);
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to start worker thread: %s (%d).",
             strerror(rc), rc);
        errno = rc;
        return false;
    }
    m_metadataStarted = true;
    m_metadataStartedTime = time(0);

    char name[_NTO_THREAD_NAME_MAX];
    snprintf(name, sizeof(name), "%s%d", mmrNameRoot, m_instanceId);
    pthread_setname_np(m_threadId, name);

    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Started thread '%s'.", name);

    return true;
}


//
// This function should allow the newly created object to do nothing that
// could possibly call back into webkit (m_player), since the player doesn't
// have a handle to the implementation yet.
// However, some calls back into the player from the object are necessary.
//
// static
PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivate::create(MediaPlayer* player)
{
    return adoptPtr(new MediaPlayerPrivate(player));
}

// static
void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
}

bool MediaPlayerPrivate::outputsAttach()
{
    if (m_videoOutputID != -1) {
        // Already done.
        return true;
    }

    // Attach a video output in order to perform control of where it is rendered
    const char* outputUrl = getenv("WK_VIDEO_OUTPUT");
    const char* parentGroupID = 0;
    const char* topGroupID = 0;
    char* fullOutputUrl;
    if (!outputUrl)
        outputUrl = defaultVideoOutput;

    if (hasVideo()) {
        if (m_player->frameView() && m_player->frameView()->hostWindow()) {
            // We keep track of the parent window of our embedded window using a
            // member variable. This is necessary because the m_player->frameView()
            // may be set to zero by the destructor in RenderVideo.cpp before our
            // destructor is called, leaving us unable to clean up child windows
            // (in mmrDisconnect).
            m_parentWindow = m_player->frameView()->hostWindow()->platformPageClient()->platformWindow();
        }

        if (m_parentWindow) {
            parentGroupID = m_parentWindow->windowGroup();
            topGroupID = m_parentWindow->parentWindowGroup();
        }

        if (parentGroupID && *parentGroupID && topGroupID && *topGroupID) {
            // Need to add the following to the URL as parameters:
            // - Window ID that mm-renderer should use for its child window
            // - Window group that the mm-renderer window should join
            // - Top level window group mm-renderer can use for navigator queries
            int zorder = HTML5_MMR_VIDEO_WINDOW_ZORDER;
            int embedded = 1;
            const char* urlFormat = "%s%czorder=%d&winembed=%d&winid=%s&wingrp=%s&topgrp=%s";
            bool hasParams = strchr(outputUrl, '?');

            fullOutputUrl = (char*) alloca(strlen(urlFormat) +
                                        strlen(outputUrl) +
                                        strlen(HTML5_MMR_VIDEO_WINDOW_ID) +
                                        strlen(parentGroupID) +
                                        strlen(topGroupID) + 10 /* be safe */);
            if (fullOutputUrl) {
                sprintf(fullOutputUrl, urlFormat,
                    outputUrl, hasParams ? '&' : '?', zorder, embedded,
                    HTML5_MMR_VIDEO_WINDOW_ID,
                    parentGroupID,
                    topGroupID);
                outputUrl = fullOutputUrl;
            } else {
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "no room on stack for building output URL");
                return false;
            }
        } else {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "WebKit Window or App window Group IDs could not be determined");
            return false;
        }
        m_videoOutputID = m_rendererContext->outputAttach(outputUrl, "video");
        if (m_videoOutputID == -1) {
#if (!ERROR_DISABLED)
            const mmr_error_info_t* ei = m_rendererContext->errorInfo();
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to attach video ouput '%s' to %s%d: %s.", outputUrl, mmrNameRoot,
                m_instanceId, strerror(ei->error_code));
#endif
            return false;
        }
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Attached video output '%s' to %s%d.", outputUrl, mmrNameRoot, m_instanceId);
    }

    // Attach an audio output in order to perform volume control
    outputUrl = getenv("WK_AUDIO_OUTPUT");
    if (!outputUrl)
        outputUrl = defaultAudioOutput;
    m_audioOutputID = m_rendererContext->outputAttach(outputUrl, "audio");
    if (m_audioOutputID == -1) {
#if (!ERROR_DISABLED)
        const mmr_error_info_t* ei = m_rendererContext->errorInfo();
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to attach audio ouput '%s' to %s%d: %s.", outputUrl, mmrNameRoot,
            m_instanceId, strerror(ei->error_code));
#endif
        return false;
    }

    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Attached audio ouput '%s' to %s%d.", outputUrl, mmrNameRoot, m_instanceId);
    // Set the volume to whatever it's supposed to be
    // setMediaVolume(); // Not used. Global system volume is used instead

    return true;
}

bool MediaPlayerPrivate::inputAttach()
{
    if (m_inputAttached)
        return true;

    m_mutex.lock();
    if (m_rendererContext->inputAttach(m_url.utf8().data(), "track") != -1) {
        m_mutex.unlock();
        m_inputAttached = true;
        // If this is due to a transition back to active after previously
        // playing, must not see a Playing to Stopped transition from
        // mm-renderer, or it will look like end of playback.
        m_mmrPlayState = MMRPlayStateIdle;
    } else {
        m_mutex.unlock();
#if (!ERROR_DISABLED)
        const mmr_error_info_t* ei = m_rendererContext->errorInfo();
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to attach \"%s\": %s.", m_url.utf8().data(),
            strerror(ei->error_code));
#endif
        // TODO: proper error handling
        if (m_networkState != MediaPlayer::NetworkError) {
            m_networkState = MediaPlayer::NetworkError;
            mediaStateChangedProcessing(MP_NET_STATE_CHANGED);
        }
    }

    return m_inputAttached;
}

bool MediaPlayerPrivate::hasVideo() const
{
    if (!m_metadataStarted)
        return m_hasVideo;

    bool hasVideo;
    static bool concurrentMetadata = !getenv("WK_DISABLE_CONCURRENT_METADATA");
    pthread_mutex_lock(&m_metadataFinishedMutex);

    if (m_metadataFinished || m_metadataFinishedCondTimeExpired || concurrentMetadata) {
        hasVideo = m_hasVideo;
        pthread_mutex_unlock(&m_metadataFinishedMutex);
        return hasVideo;
    }

    // The following code is here for debugging only. This function has been
    // changed to never wait for metadata to be ready. The waiting is now done
    // when play starts.
    int wait;
    do {
        timespec ts;
        ts.tv_sec = time(0) + METADATA_CONDVAR_WAIT_TIME;
        ts.tv_nsec = 0;
        int rc = pthread_cond_timedwait(&m_metadataFinishedCond, &m_metadataFinishedMutex, &ts);
        wait = 0;

        // If we timed out waiting on the condvar, future calls to this function should not wait again.
        if (ETIMEDOUT == rc) {
            pthread_mutex_unlock(&m_metadataFinishedMutex);
            wait = showErrorDialog(BlackBerry::WebKit::WebPageClient::MediaMetaDataTimeoutError);
            pthread_mutex_lock(&m_metadataFinishedMutex);

            // The error dialog is modal, so in the additional time the user took
            // to respond the loading of metadata may have finished.
            if (!m_metadataFinished) {
                if (!wait)
                    m_metadataFinishedCondTimeExpired = true;
            } else {
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d hasVideo() TIME OUT processing aborted since metadata thread is now finished.", mmrNameRoot, m_instanceId);
                wait = 0;
            }
        }
    } while (wait);

    hasVideo = m_hasVideo;

    pthread_mutex_unlock(&m_metadataFinishedMutex);

    return hasVideo;
}

void MediaPlayerPrivate::load(const String& url)
{
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d attempting to load %s.", mmrNameRoot,
        m_instanceId, url.utf8().data());

    String modifiedUrl(url);

    if (modifiedUrl.startsWith("local://")) {
        KURL kurl = KURL(KURL(), modifiedUrl);
        kurl.setProtocol("file");
        String tempPath(BlackBerry::Platform::Client::get()->getApplicationAirDirectory().c_str());
        tempPath.append(kurl.path());
        kurl.setPath(tempPath);
        modifiedUrl = kurl.string();
    }
    if (modifiedUrl.startsWith("file://")) {
        // The QNX Multimedia Framework cannot handle filenames containing
        // URL escape sequences.
        modifiedUrl = decodeURLEscapeSequences(modifiedUrl);
    }

    m_mutex.lock();
    m_url = modifiedUrl;
    m_userAgent = userAgent(m_url);
    m_mutex.unlock();


    // Start the thread now to get the metadata there, allowing
    // the main thread to continue to display other elements of the page.
    workerThreadStart();
}

void MediaPlayerPrivate::prepareToPlay()
{
    play();
}

void MediaPlayerPrivate::play()
{
    bool justConnected = false;

    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d play called (state %s).", mmrNameRoot, m_instanceId, stateStr(m_state));

    if (m_state == MP_STATE_UNSUPPORTED)
        return;

    // Make sure there's a connection to the renderer.
    if (!m_rendererContext) {
        if (mmrConnect() == -1)
            return;
        justConnected = true;
    }

    // TODO: Check that not already playing (PPS object) Or not?

    if (m_rendererContext->speedSet(m_reqSpeed) == -1) {
#if (!ERROR_DISABLED)
        const mmr_error_info_t* ei = m_rendererContext->errorInfo();
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to play %s%d: %s.", mmrNameRoot,
            m_instanceId, strerror(ei->error_code));
#endif
    } else {
        // Need to seek if this is effectively a resume after not being the
        // active element.
        if (justConnected && m_currentMediaTime) {
            char ms[32];
            sprintf(ms, "%u", m_currentMediaTime);
            if (m_rendererContext->seek(ms) == -1) {
#if (!ERROR_DISABLED)
                const mmr_error_info_t* ei = m_rendererContext->errorInfo();
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to seek to %s in %s%d: %s.", ms, mmrNameRoot,
                    m_instanceId, strerror(ei->error_code));
#endif
            }
        }
        if (m_rendererContext->play() == -1) {
#if (!ERROR_DISABLED)
             const mmr_error_info_t* ei = m_rendererContext->errorInfo();
             BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to play %s%d: %s.", mmrNameRoot,
                 m_instanceId, strerror(ei->error_code));
#endif
        } else {
            // Check if the pause timeout occurred and process it if necessary.
            processPauseTimeoutIfNecessary();

            // In the event that the browser app deactivates while the video is playing, we will want to resume upon reactivation.
            m_playOnAppActivate = true;

            // Check if we are already in the state where pause has been pressed
            // using the controls on the web page and we are still waiting for
            // mm-renderer to tell us that play has actually been paused. This
            // is one of the conditions used to test for out-of-sequence events
            // or race conditions. If we simply set m_shouldBePlaying to be
            // true, then it will seem like this bad condition has ended. In
            // fact it has gotten worse because the play is now queued as well.
            // To handle this situation, disallow position updates for a short
            // time (until pausedStateUpdate() is next called). As a safety we
            // won't skip more than 3 position updates.
            if (m_state == MP_STATE_ACTIVE && !m_shouldBePlaying && !mmrPaused())
                m_skipPPSPositionUpdates = 3;
            m_shouldBePlaying = true;

            // Trigger a paint() call
            m_player->repaint();
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d started playback.", mmrNameRoot, m_instanceId);
        }
    }
}

void MediaPlayerPrivate::pause()
{
    m_reqSpeed = 0;
    if (m_rendererContext && m_rendererContext->speedSet(0) == -1) {
#if (!ERROR_DISABLED)
        const mmr_error_info_t* ei = m_rendererContext->errorInfo();
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to pause %s%d: %s.", mmrNameRoot,
            m_instanceId, strerror(ei->error_code));
#endif
    } else {
        // We are paused, so we don't want to play if browser app reactivates.
        m_playOnAppActivate = false;
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d paused.", mmrNameRoot, m_instanceId);
        m_shouldBePlaying = false;
    }
}

bool MediaPlayerPrivate::processPauseTimeoutIfNecessary()
{
    static const char* env = getenv("WK_HTML5_PAUSE_TIMEOUT");
    bool retval = false;
    // The amount of time (in seconds) that the player should be allowed to
    // remain in a paused state before we enable "re-seek on read error"
    // processing. This should ensure that mm-renderer's network connection to
    // the host of the media file is created again in case the server has
    // closed it due to a timeout. See PR 92661.
    time_t timeout = 60;

    if (env) {
        int t = atoi(env);
        if (!t)
            return retval; // Timeout disabled
        if (t > 0)
            timeout = t;
    }

    time_t currentTime = time(0);
    if (!m_currentMediaTime || m_currentMediaTime >= m_duration)
        m_reseekArmedTime = 0;
    else if (s_activeElements.contains(this)
        && m_rendererContext
        && m_lastPausedTime
        && currentTime - m_lastPausedTime > timeout) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d pause timeout occurred.", mmrNameRoot, m_instanceId);

        m_reseekArmedTime = currentTime;
        retval = true;
    }
    m_lastPausedTime = 0;

    return retval;
}

void MediaPlayerPrivate::seek(float time)
{
    uint32_t timems = WK_TIME_TO_MMR_TIME(time);
    m_lastSeekPosition = timems;
    seekms(timems);
}

void MediaPlayerPrivate::seekms(uint32_t timems)
{
    char ms[32];

    if (m_state == MP_STATE_UNSUPPORTED)
        return;

    if (timems > m_duration)
        timems = m_duration;

    if (m_state == MP_STATE_ACTIVE && m_shouldBePlaying && mmrPaused()) {
        // Play has been pressed but mm-renderer has not had time to tell
        // us that we are not paused. Don't send redundant seeks during this
        // time window. This avoids confusing mm-renderer into having weird
        // errors of the form: avc_init_input:1078 Error translating AVCC.
        // This fixes one of several problems with videos on bing.com.
        if (timems == m_currentMediaTime)
            return;
    }

    sprintf(ms, "%u", timems);

    if (m_rendererContext && m_rendererContext->seek(ms) == -1) {
#if (!ERROR_DISABLED)
        const mmr_error_info_t* ei = m_rendererContext->errorInfo();
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to seek to %s on %s%d: %s.", ms,
            mmrNameRoot, m_instanceId, strerror(ei->error_code));
#endif
    } else {
        if (!m_curSpeed) {
            // Paused, so current time might not be updated by mm-renderer.
            if (m_currentMediaTime != timems) {
                m_currentMediaTime = timems;
                m_player->timeChanged();
            }
        }
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d seeked to %s.", mmrNameRoot, m_instanceId, ms);
    }
}

void MediaPlayerPrivate::stop()
{
    if (m_rendererContext) {
        // State should already be saved; play() will use that state when 'resumed'.
        HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
        element->pause();

#if 0
        // FIXME: Disabled for now since it does not work, and can take
        // quite a bit of time.
        if (element->hasVideo())
            stillFrameCapture(m_currentMediaTime);
#endif

        mmrDisconnect();
    }
}

float MediaPlayerPrivate::duration() const
{
    // TODO: If streaming media, return numeric_limits<float>::infinity();
    // TODO: If not loaded, return numeric_limits<float>:: Not a Number; but
    // GStreamer returns 0.0 in these cases. This is already covered.
    // Maybe that's because numeric_limits<float>::quiet_NaN(); isn't available?
    return MMR_TIME_TO_WK_TIME(m_duration);
}

float MediaPlayerPrivate::currentTime() const
{
    return MMR_TIME_TO_WK_TIME(m_currentMediaTime);
}

bool MediaPlayerPrivate::paused() const
{
    if (mmrPaused() || !m_shouldBePlaying)
        return true;
    return false;
}

bool MediaPlayerPrivate::mmrPaused() const
{
    // TODO: Is idle always paused?
    if (m_state == MP_STATE_IDLE)
        return true;
    if (m_curSpeed != 0
        && m_mmrPlayState == MMRPlayStatePlaying) {
        return false;
    }
    return true;
}

void MediaPlayerPrivate::pausedStateUpdate()
{
    bool isPaused = mmrPaused();
    m_skipPPSPositionUpdates = 0;
    if (isPaused != m_isPaused) {
        m_isPaused = isPaused;
        m_shouldBePlaying = !isPaused;
        m_lastSeekPosition = 0;

        // Our paused state has changed. Tell the system tray via
        // libwebview's MediaConnectionHandler.
        if (m_mediaConn) {
            if (isPaused) {
                if (s_activated || !hasVideo() || s_allowDeactivatedVideoPlay)
                    m_mediaConn->setMState(BlackBerry::Platform::MediaConnListener::Paused);
            } else {
                mediaStateChangedProcessing(MP_METADATA_SET); // Set metadata
                m_mediaConn->setMState(BlackBerry::Platform::MediaConnListener::Playing);
            }
        }
        if (isFullscreen()) {
            // Paused state change not due to local controller...
            HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
            if (isPaused)
                element->pause();
            else {
                // The HMI fullscreen widget has resumed play. Check if the
                // pause timeout occurred.
                processPauseTimeoutIfNecessary();
                element->play();
            }
        }

        if (isPaused) {
            // Record the time at which we paused, so we can avoid mm-renderer
            // network timeouts causing problems when user wants to play again.
            m_lastPausedTime = time(0);
        }
    }
}

bool MediaPlayerPrivate::seeking() const
{
    return false;
}

// Returns the size of the video
IntSize MediaPlayerPrivate::naturalSize() const
{
    // Cannot return empty size, otherwise paint() will never get called.
    // Also, the values here will affect the aspect ratio of the output rectangle that will
    // be used for renderering the video, so we must take PAR into account.
    // Now, hope that metadata has been provided before this gets called if this is a video.
    double pixelWidth = static_cast<double>(m_pixelWidth);
    double pixelHeight = static_cast<double>(m_pixelHeight);
    if (!m_pixelWidth || !m_pixelHeight)
        pixelWidth = pixelHeight = 1.0;

    // Use floating point arithmetic to eliminate the chance of integer
    // overflow. PAR numbers can be 5 digits or more.
    double adjustedSourceWidth = static_cast<double>(m_sourceWidth) * pixelWidth / pixelHeight;
    return IntSize(static_cast<int>(adjustedSourceWidth + 0.5), m_sourceHeight);
}

// Convert from the [0.0,1.0] linear scale that the volume slider provides, to
// the [0,100] logarithmic scale that mm-renderer uses. Based upon emperical
// testing, this formula sounds about right.
inline int MediaPlayerPrivate::convertVolume(float in)
{
    if (in > 0.95)
        return 100;
    if (in < 0.05)
        return 0;

    return static_cast<int>(21 * logf(in)) + 100;
}

//
// This sets the volume of the current media via mm-renderer, and is not
// used at this time. Instead the global system volume is used.
//
int MediaPlayerPrivate::setMediaVolume()
{
    strm_dict_t* dict;
    int rc = -1;

    dict = strm_dict_new();
    if (dict) {
        char volStr[8];

        // Assume volume input is 0.0 to 1.0 (0% to 100%); output is 0 to 100.
        snprintf(volStr, sizeof(volStr), "%d", convertVolume(m_volume));

        dict = strm_dict_set(dict, "volume", volStr);
        if (dict) {
            if (!m_rendererContext->outputParameters(m_audioOutputID, dict)) {
                rc = 0;
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Set volume to %s on %s%d.", volStr, mmrNameRoot, m_instanceId);
                // It looks like changes are reported only for things that were not caused by
                // client action. For example, cannot report a 0 volume when muted by the controller
                // since that sets the controller's volume to 0. Assume similar logic for other events.
            } else {
#if (!ERROR_DISABLED)
                const mmr_error_info_t* ei = m_rendererContext->errorInfo();
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to set volume to %s on %s%d: %s.", volStr,
                    mmrNameRoot, m_instanceId, strerror(ei->error_code));
#endif
            }
        } else
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to add entry to dictionary to set the volume.");
    } else
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to create dictionary to set the volume.");

    return rc;
}

void MediaPlayerPrivate::setVolume(float volume)
{
    // TODO: Add muting support so setMuted() gets handled differently from HTMLMediaElement and setVolume()
    // doesn't get called with the old volume before being called with the updated volume a second time.
    // Until then, we need this check so we're not unnecessarily sending meaningless volume changes to PPS.
    if (fabs(volume - m_volume) < 0.001) // Within 0.1%.
        return;

    timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    uint64_t currentTime = timespec2nsec(&time);
    if (currentTime - m_lastSystemVolumeChangeTime < 100000000) {
        // It has been less than a tenth of a second since a system volume
        // change happened. To avoid volume setting loops we will assume that
        // this volume change was triggered based on the system change by
        // over-anxious Javascript (eg. videos on bing.com) and ignore it.
        return;
    }

    // Always save it, since need to really set it to this when starting.
    m_lastControlVolumeChangeTime = currentTime;
    m_volume = volume;

    FILE* ppsAudioFile = fopen("/pps/services/audio/control", "w");
    if (ppsAudioFile) {
        char ppsMessage[128];
        float ppsVolume = volume * 100;
        sprintf(ppsMessage, "msg::set_output_level\nid::1\ndat:json:{\"level\":%.6f}\n", ppsVolume);
        fwrite(ppsMessage, 1, strlen(ppsMessage), ppsAudioFile);
        fclose(ppsAudioFile);
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Global audio volume set from MediaPlayerPrivate through PPS to %.6f.", ppsVolume);
    } else
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to set global audio volume through PPS!");
}

void MediaPlayerPrivate::setRate(float rate)
{
    // TODO: Determine if mm_renderer handles audio changes as described at
    // <http://www.w3.org/TR/html5/video.html>
    m_reqSpeed = static_cast<int>(rate * 1000.0);
    if (m_rendererContext) {
        if (m_rendererContext->speedSet(m_reqSpeed) == -1) {
#if (!ERROR_DISABLED)
            const mmr_error_info_t* ei = m_rendererContext->errorInfo();
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to set rate to %d on %s%d: %s.", m_reqSpeed,
                mmrNameRoot, m_instanceId, strerror(ei->error_code));
#endif
        } else {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d set rate to %d.", mmrNameRoot, m_instanceId, m_reqSpeed);
            // TODO: Ok in this thread?
            m_player->rateChanged();
        }
    }
}

void MediaPlayerPrivate::cancelLoad()
{
    // TODO: Is this sufficent? What state has to change?
    if (m_rendererContext && m_rendererContext->stop() == -1) {
#if (!ERROR_DISABLED)
        const mmr_error_info_t* ei = m_rendererContext->errorInfo();
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to stop/cancel load on %s%d: %s.",
            mmrNameRoot, m_instanceId, strerror(ei->error_code));
#endif
    } else
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d canceled load.", mmrNameRoot, m_instanceId);
    m_state = MP_STATE_IDLE;
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return m_readyState;
}

PassRefPtr<TimeRanges> MediaPlayerPrivate::buffered() const
{
    RefPtr<TimeRanges> timeRanges = TimeRanges::create();
    if (m_bufferLevel) {
        uint32_t loaded = m_currentMediaTime + m_bufferLevel;
        if (loaded > m_duration)
            loaded = m_duration;
        if (loaded > 0)
            timeRanges->add(0.0, MMR_TIME_TO_WK_TIME(loaded));
    }
    return timeRanges.release();
}

float MediaPlayerPrivate::percentLoaded()
{
    if (!s_activeElements.mediaCount())
        return 0;

    if (!m_duration)
        return 0;

    float buffered = 0;
    RefPtr<TimeRanges> timeRanges = this->buffered();
    for (unsigned i = 0; i < timeRanges->length(); ++i) {
        ExceptionCode ignoredException;
        float start = timeRanges->start(i, ignoredException);
        float end = timeRanges->end(i, ignoredException);
        buffered += end - start;
    }

    float loaded = buffered / MMR_TIME_TO_WK_TIME(m_duration);
    return loaded;
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    // NOTE: A valid answer from this is required to get the 'loop' attribute
    // to work.
    // TODO: fix for streaming media
    //
    // This example is from gstreamer. No matter; still need to know from
    // mm-renderer if media is streaming or not.
    //
    // if (m_isStreaming) {
    //     return numeric_limits<float>::infinity();
    // }

    return MMR_TIME_TO_WK_TIME(m_duration);
}

unsigned MediaPlayerPrivate::bytesLoaded() const
{
    notImplemented();
    return m_bytesLoaded;
}

void MediaPlayerPrivate::setSize(const IntSize& size)
{
    // m_size = size; TODO: What is this for? it doesn't appear to be used by GStreamer.
    notImplemented();
}

void MediaPlayerPrivate::setVisible(bool visible)
{
    // This was empty in GStreamer implementation as well.
    // NOTE: Perhaps we need to use this to help with controlling access to
    // the renderer.
    notImplemented();
}

void MediaPlayerPrivate::outputUpdate(const IntRect& rect)
{
    static bool prevFullscreen = false;
    bool exitedFullscreen = (isFullscreen() != prevFullscreen && prevFullscreen);

    prevFullscreen = isFullscreen();
    if (rect == m_videoRect && !exitedFullscreen)
        return;

    m_videoRect = rect;
    outputUpdate(true);
}

void MediaPlayerPrivate::outputUpdate(bool updateClip) const
{
    int originX = m_videoRect.x();
    int originY = m_videoRect.y();
    int width = m_videoRect.width();
    int height = m_videoRect.height();

    if (isFullscreen()) {
        unsigned x, y, w, h;

        windowPositionGet(x, y, w, h);
        m_fullscreenWebPageClient->fullscreenWindowSet(x, y, w, h);
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d updated fullscreen delegate to (%d,%d)(%d,%d).",
            mmrNameRoot, m_instanceId, x, y, w, h);

        return;
    }

    strm_dict_t* dict = 0;

    do {
        if (addDictAttrIntValue(&dict, "video_dest_x", originX) == -1)
            break;
        if (addDictAttrIntValue(&dict, "video_dest_y", originY) == -1)
            break;
        if (addDictAttrIntValue(&dict, "video_dest_w", width) == -1)
            break;
        if (addDictAttrIntValue(&dict, "video_dest_h", height) == -1)
            break;
        // Use only the correct amount of buffers that are borrowed from the renderer's window.
        if (addDictAttrIntValue(&dict, "src_view_w", m_sourceWidth) == -1)
            break;
        if (addDictAttrIntValue(&dict, "src_view_h", m_sourceHeight) == -1)
            break;
    } while (0);

    if (updateClip) {
        do {
            if (addDictAttrIntValue(&dict, "video_clip_w", m_windowWidth) == -1)
                break;
            if (addDictAttrIntValue(&dict, "video_clip_h", m_windowHeight) == -1)
                break;
        } while (0);
    }

    if (dict) {
        if (!m_rendererContext->outputParameters(m_videoOutputID, dict)) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d set video rectangle to (%d,%d)(%d,%d).",
                mmrNameRoot, m_instanceId, originX, originY, width, height);
#if (!ERROR_DISABLED)
        } else {
            const mmr_error_info_t* ei = m_rendererContext->errorInfo();
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to set video rectangle on %s%d: %s.",
                mmrNameRoot, m_instanceId, strerror(ei->error_code));
#endif
        }
    }
}

void MediaPlayerPrivate::paint(GraphicsContext* context, const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING)
    if (!context) {
        if (!m_window) {
           if (!screenWindowSet())
               return; //  This is a serious error. Do nothing.
        }

        if (m_rendererContext)
            outputUpdate(rect);
        return;
    }
#endif

    if (!hasVideo() || context->paintingDisabled() || !m_player->visible())
        return;

    PlatformGraphicsContext* graphics = context->platformContext();
    ASSERT(graphics);

    enum {
        ePaintNone = 0,
        ePaintTransparent,
        ePaintBlack,
        ePaintRed, // use these for diagnostics
        ePaintGreen,
        ePaintBlue,
        ePaintYellow
    };

    int paintColour = ePaintNone;
    SkCanvas* canvas = graphics->canvas();
    SkRect skRect;
    bool notready = false;

    if (!m_rendererContext)
        notready = true;
    if (!m_window) {
       if (!screenWindowSet()) {
           //  This is a serious error. Do nothing
           notready = true;
       }
    }

    // Clip the retangle; use FrameView::windowClipRect to get the clipping rectangle.
    FrameView* frameView = m_player->frameView();
    IntRect clippedRect = rect;
    clippedRect.intersect(frameView->windowClipRect());

    // The rectangle where the video is supposed to appear, according to webkit.
    skRect.set(clippedRect.x(), clippedRect.y(), clippedRect.x() + clippedRect.width(), clippedRect.y() + clippedRect.height());

    canvas->save();
    switch (m_state) {
    case MP_STATE_ACTIVE:
        // Don't show the video unless we are ready to size it properly
        if (!notready)
            paintColour = ePaintTransparent;
        break;
    case MP_STATE_INITIAL:
        if (m_videoFrameContents != eVideoFrameContentsNone)
            canvas->drawBitmapRect(m_videoFrame, 0, skRect);
        else
            paintColour = ePaintBlack;
        break;
    case MP_STATE_IDLE:
        if (m_videoFrameContents == eVideoFrameContentsPauseFrame)
            canvas->drawBitmapRect(m_videoFrame, 0, skRect);
        else
            paintColour = ePaintBlack;
        break;
    case MP_STATE_UNSUPPORTED:
        // Should probably be a user friendly indicator that can't support this...
        paintColour = ePaintBlack;
        break;
    }

    if (paintColour != ePaintNone) {
        // Paint a box where video is with the requested 'colour'
        SkPaint paint;

        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        if (paintColour == ePaintTransparent)
            paint.setARGB(0, 0, 0, 0);
        else if (paintColour == ePaintBlack)
            paint.setARGB(255, 0, 0, 0);
        else if (paintColour == ePaintGreen)
            paint.setARGB(150, 0, 255, 0);
        else if (paintColour == ePaintBlue)
            paint.setARGB(150, 0, 0, 255);
        else if (paintColour == ePaintYellow)
            paint.setARGB(150, 255, 255, 0);
        else {
            // assume red. Use some transparency to see what's behind.
            paint.setARGB(150, 255, 0, 0);
        }

        canvas->drawRect(skRect, paint);
    }
    canvas->restore();

    if (notready)
        return;

#ifdef WINDOW_POSITION_DIAGNOSTICS
    {
        static int contextOriginX = 0;
        static int contextOriginY = 0;
        static int windowX = 0;
        static int windowY = 0;
        static int windowScrollOffsetX = 0;
        static int windowScrollOffsetY = 0;
        static int windowViewportX = 0;
        static int windowViewportY = 0;
        static double zoomFactor = 1.0;

        int newContextOriginX = context->origin().x();
        int newContextOriginY = context->origin().y();
        int newWindowX = window->getX();
        int newWindowY = window->getY();
        int newWindowScrollOffsetX = window->getScrollOffsetX();
        int newWindowScrollOffsetY = window->getScrollOffsetY();
        int newWindowViewportX = window->getViewportPosX();
        int newWindowViewportY = window->getViewportPosY();
        double newZoomFactor = window->getZoomFactor();

        flockfile(stderr);
        if (newContextOriginX != contextOriginX
            || newContextOriginY != contextOriginY
            || newWindowX != windowX
            || newWindowY != windowY
            || newWindowScrollOffsetX != windowScrollOffsetX
            || newWindowScrollOffsetY != windowScrollOffsetY
            || newWindowViewportX != windowViewportX
            || newWindowViewportY != windowViewportY
            || newZoomFactor != zoomFactor) {
            fprintf(stderr, "     %7s %7s %7s %7s %7s\n", "Origin", "Window", "Scroll", "Port", "Zoom");
            fprintf(stderr, "old: %3d,%-3d %3d,%-3d %3d,%-3d %3d,%-3d  %g\n", contextOriginX, contextOriginY, windowX, windowY, windowScrollOffsetX, windowScrollOffsetY, windowViewportX, windowViewportY, zoomFactor);
            fprintf(stderr, "new: %3d,%-3d %3d,%-3d %3d,%-3d %3d,%-3d  %g\n", newContextOriginX, newContextOriginY, newWindowX, newWindowY, newWindowScrollOffsetX, newWindowScrollOffsetY, newWindowViewportX, newWindowViewportY, newZoomFactor);
            contextOriginX = newContextOriginX;
            contextOriginY = newContextOriginY;
            windowX = newWindowX;
            windowY = newWindowY;
            windowScrollOffsetX = newWindowScrollOffsetX;
            windowScrollOffsetY = newWindowScrollOffsetY;
            windowViewportX = newWindowViewportX;
            windowViewportY = newWindowViewportY;
            zoomFactor = newZoomFactor;
        }
        if (rect != m_videoRect) {
            fprintf(stderr, "Video rect changed from (%d,%d)(%d,%d) to (%d,%d)(%d,%d).\n",
                m_videoRect.x(), m_videoRect.y(), m_videoRect.x() + m_videoRect.width(), m_videoRect.y() + m_videoRect.height(),
                rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
            // will be updated below
        }
        funlockfile(stderr);
    }
#endif

    // FIXME: Update output rectangle placement
    outputUpdate(rect);
}

// static
void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    set<string> supported = MMrendererContext::allSupportedMimeTypes();
    set<string>::iterator i = supported.begin();
    for (; i != supported.end(); i++)
        types.add(i->c_str());
}

// static
MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    if (type.isNull() || type.isEmpty()) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "MediaPlayer does not support type; type is null or empty.");
        return MediaPlayer::IsNotSupported;
    }

    // spec says we should not return "probably" if the codecs string is empty
    if (MMrendererContext::mimeTypeSupported(type.ascii().data())) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "MediaPlayer supports type; cache contains type '%s'.", type.ascii().data());
        return codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;
    }
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "MediaPlayer does not support type; cache doesn't contain type '%s'.", type.ascii().data());
    return MediaPlayer::IsNotSupported;
}

bool MediaPlayerPrivate::hasSingleSecurityOrigin() const
{
    return false;
}

// The requested types; frame and album art added dynamically.
static const char mdTitleTypes[] = "md_title::mediatype,duration,name,artist,album";
static const char mdVideoTypes[] = "md_video::width,height,pixel_width,pixel_height";

// The response configurations.
enum {
    ATTR_MEDIATYPE,
    ATTR_DURATION,
    ATTR_NAME,
    ATTR_ARTIST,
    ATTR_ALBUM,
    ATTR_ALBUMART,
    ATTR_WIDTH,
    ATTR_HEIGHT,
    ATTR_PIXELWIDTH,
    ATTR_PIXELHEIGHT,
    ATTR_FRAME,

    LAST_ATTR
};

static const char* const attrs[] = {
    "md_title_mediatype",
    "md_title_duration",
    "md_title_name",
    "md_title_artist",
    "md_title_album",
    "md_title_art",
    "md_video_width",
    "md_video_height",
    "md_video_pixel_width",
    "md_video_pixel_height",
    "md_video_frame",

    0
};

// NOTE: Call metadataInit() with no parameters only from the main WebKit
// thread so it's safe to access m_url, m_userAgent and s_caPath. Use
// metadataInit(String& url, String& userAgent, String& caPath) instead, where
// all String parameters are thread safe copies, if we need to call this
// function from another thread (the metadata worker thread).
mmmd_hdl_t* MediaPlayerPrivate::metadataInit()
{
    return metadataInit(m_url, m_userAgent, s_caPath);
}

mmmd_hdl_t* MediaPlayerPrivate::metadataInit(const String& url, const String& userAgent, const String& caPath)
{
    strm_dict_t* mdParams = 0;
    mmmd_hdl_t* mdHdl = 0;

    // Initialize the library only once. For now, never uninitialize it.
    if (!mdParams) {
#ifdef    ENABLE_AOI_LOGGING
        // Need to do this once only
        AoSetLogger(aoiLogger);
#endif
        if (mmmd_init(0) == -1) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to initialize metadata library: %s (%d).", strerror(errno), errno);
            return 0;
        }
    }

    char contextName[32];
    sprintf(contextName, "%s%d", mmrNameRoot, m_instanceId);

    mdHdl = mmmd_session_open(contextName, 0);
    if (!mdHdl) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to open metadata session: %s (%d).", strerror(errno), errno);
        mmmd_terminate();
        return 0;
    }

    WTF::String cookiePairs;
    if (!url.isEmpty())
        cookiePairs = WebCore::cookieManager().getCookie(KURL(WebCore::ParsedURLString, url.utf8().data()), WebCore::WithHttpOnlyCookies);
    if (!cookiePairs.isEmpty() && cookiePairs.utf8().data())
        mdParams = contextDictMake(url.utf8().data(), userAgent.utf8().data(), caPath.utf8().data(), cookiePairs.utf8().data());
    else
        mdParams = contextDictMake(url.utf8().data(), userAgent.utf8().data(), caPath.utf8().data(), 0);

    if (!mdParams) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to create metadata parameters dictionary.");
        mmmd_session_close(mdHdl);
        mmmd_terminate();
        return 0;
    }

    if (mmmd_session_params_set(mdHdl, mdParams) == -1) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to open metadata session: %s (%d).", strerror(errno), errno);
        strm_dict_destroy(mdParams);
        mmmd_session_close(mdHdl);
        mmmd_terminate();
        return 0;
    }

    return mdHdl;
}

bool MediaPlayerPrivate::metadataTerminate(mmmd_hdl_t* mdHdl)
{
    if (mdHdl)
        mmmd_session_close(mdHdl);

    // If the media file is a video, but we were unable to retrieve valid metadata regarding its dimensions,
    // then we want to ensure that the dimensions are set to something sane (otherwise the controls will get
    // "squished" into a 0x0 rect).
    if (m_player) {
        HTMLMediaElement* client = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
        if (client && !(m_sourceWidth && m_sourceHeight)) {
            // We only need to call this if our source width or source height is 0 and if we have a client that exists.
            callOnMainThread(mpResizeSourceDimensions, static_cast<void*>(this));
        }
    }

    // TODO: Also signal the condvar in another place in the metadata thread once we have the
    // video dimensions (m_sourceWidth and m_sourceHeight) and once we know if the media type is video (m_hasVideo).
    // In the future, we may want to investigate a better method to parallelize the getting of metadata.
    signalMetadataCondvarFinished();

    return true;
}

int MediaPlayerPrivate::metadataGet(const String &url, mmmd_hdl_t* mdHdl)
{
    int rc;
    char* md = 0;
    char artFile[64];
    char frameFile[64];
    static bool noFirstFrame = getenv("WK_NO_FIRSTFRAME");

    free(m_title);
    m_title = 0;
    free(m_artist);
    m_artist = 0;
    free(m_album);
    m_album = 0;
    if (m_artFile) {
        unlink(m_artFile);
        free(m_artFile);
        m_artFile = 0;
    }

    // Build the context specific type string: each player needs its own
    // file for the first frame of video.
    char* typeStr;
    const size_t typeStrSize = 512;
    typeStr = (char*) alloca(typeStrSize);

    snprintf(artFile, sizeof(artFile), "/tmp/art%s%d.jpg", mmrNameRoot, m_instanceId);
    unlink(artFile);
    snprintf(frameFile, sizeof(frameFile), "/tmp/%s%d.bmp", mmrNameRoot, m_instanceId);

    if (noFirstFrame) {
        rc = snprintf(typeStr, typeStrSize, "%s,art?file=%s\n%s",
                 mdTitleTypes, artFile, mdVideoTypes);
    } else {
        rc = snprintf(typeStr, typeStrSize, "%s,art?file=%s\n%s,frame?file=%s",
                 mdTitleTypes, artFile, mdVideoTypes, frameFile);
    }
    if (rc < 0 || (size_t) rc > typeStrSize) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to build metadata request string: %s.",
            rc < 0 ? strerror(errno) : "too long");
        return -1;
    }

    pthread_mutex_unlock(&s_elementsMutex);
    rc = mmmd_get(mdHdl, url.utf8().data(), typeStr, 0, 0, &md);
    pthread_mutex_lock(&s_elementsMutex);

    // To prevent crashes, we need to check if the metadata timeout tripped
    // and the user navigated to a new page. In other words, if the current
    // object is now freed.
    if (!s_elements.contains(this)) {
        // Assume "this" is now a bad pointer
        mmmd_session_close(mdHdl);
        return -2;
    }

    m_pendingErrorDialog = BlackBerry::WebKit::WebPageClient::MediaOK;

    if (rc > 0) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Metadata for \"%s\": \n%s\n", url.utf8().data(), md);
        rc = metadataParse(md);
        free(md);

        m_artFile = strdup(artFile);
        if (!noFirstFrame)
            unlink(frameFile);
    } else if (!rc) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "No metadata found for \"%s\". Continuing...", url.utf8().data());
        // This is not necessarily an error, eg. playlists. We will let things
        // continue normally and there will be a decoding error if this error
        // is real.
    } else {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Error getting metadata for \"%s\".", url.utf8().data());
        mediaStateCallbackMsgSend("md_getdata_error", 0);
    }

    // Set metadata for system tray even if all null. This will generally not
    // do anything at this point because m_mediaConn will usually not have
    // been created yet, so we also update the metadata each time the media
    // starts playing.
    mediaStateCallbackMsgSend("md_data_set", 0);

    // Don't assume the media can't be played if there's no metadata
    return rc >= 0 ? 0 : -1;
}

//
// This function parses the metadata about the media to be handled.
// It will modify the string.
//
int MediaPlayerPrivate::metadataParse(char* md)
{
    char* attr = md;
    char* value;
    char* valueEnd;
    int numItems = 0;

    // While there's an attribute
    while (attr) {
        // Look for the end of the attr.
        value = attr + 1;
        while (*value && *value != ':')
            ++value;

        // null terminate the end of the attribute and advance to the value.
        if (*value) {
            *value = 0;
            value += 2;
        } else {
            // malformed metadata?
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Attribute \"%s\" is badly formed.", attr);
            return -1;
        }
        // Null the end of the value
        valueEnd = value + 1;
        while (*valueEnd && *valueEnd != '\n')
            ++valueEnd;

        if (*valueEnd == '\n')
            *valueEnd++ = 0;

        // Find the attribute in the list and deal with it.
        for (unsigned i = 0; attrs[i]; ++i) {
            if (!strcmp(attr, attrs[i])) {
                switch (i) {
                case ATTR_MEDIATYPE:
                    {
                        uint32_t mediatype = atoi(value);
                        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Found mediatype of 0x%08X.", mediatype);

                        if (mediatype & MEDIA_TYPE_VIDEO)
                            m_hasVideo = true;

                        if (mediatype & MEDIA_TYPE_AUDIO)
                            m_hasAudio = true;

                        numItems++;
                    }
                    break;
                case ATTR_DURATION:
                    {
                        uint32_t newDuration = strtoul(value, 0, 10);

                        if (m_duration != newDuration) {
                            uint32_t oldDuration = m_duration;
                            m_duration = newDuration;
                            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d playback duration set to %u ms.",  mmrNameRoot, m_instanceId, m_duration);
                            mediaStateCallbackMsgSend("md_duration_changed", 0);
                            if (!oldDuration && m_readyState != MediaPlayer::HaveEnoughData) {
                                m_readyState = MediaPlayer::HaveEnoughData;
                                mediaStateCallbackMsgSend("md_rdy_state_changed", 0);
                            }
                        }
                        numItems++;
                    }
                    break;
                case ATTR_NAME:
                    {
                        m_title = strdup(value);
                        numItems++;
                    }
                    break;
                case ATTR_ARTIST:
                    {
                        m_artist = strdup(value);
                        numItems++;
                    }
                    break;
                case ATTR_ALBUM:
                    {
                        m_album = strdup(value);
                        numItems++;
                    }
                    break;
                case ATTR_ALBUMART:
                    {
                        // Note that libmd currently never returns a response
                        // for album art. However it does create the image file,
                        // so we will simply use the image if present.
                        numItems++;
                    }
                    break;
                case ATTR_HEIGHT:
                    {
                        unsigned newHeight = strtoul(value, 0, 10);

                        if (m_sourceHeight != newHeight) {
                            m_sourceHeight = newHeight;
                            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d height set to %d.",  mmrNameRoot, m_instanceId, m_sourceHeight);
                        }
                        numItems++;
                    }
                    break;
                case ATTR_WIDTH:
                    {
                        unsigned newWidth = strtoul(value, 0, 10);

                        if (m_sourceWidth != newWidth) {
                            m_sourceWidth = newWidth;
                            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d width set to %d.",  mmrNameRoot, m_instanceId, m_sourceWidth);
                        }
                        numItems++;
                        mediaStateCallbackMsgSend("md_size_changed", 0);
                    }
                    break;
                case ATTR_PIXELHEIGHT:
                    {
                        unsigned newHeight = strtoul(value, 0, 10);
                        if (m_pixelHeight != newHeight) {
                            m_pixelHeight = newHeight;
                            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d metadataParse(): pixel height set to %d.",  mmrNameRoot, m_instanceId, m_pixelHeight);
                        }
                        numItems++;
                    }
                    break;
                case ATTR_PIXELWIDTH:
                    {
                        unsigned newWidth = strtoul(value, 0, 10);
                        if (m_pixelWidth != newWidth) {
                            m_pixelWidth = newWidth;
                            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d metadataParse(): pixel width set to %d.",  mmrNameRoot, m_instanceId, m_pixelWidth);
                        }
                        numItems++;
                    }
                    break;
                case ATTR_FRAME:
                    // If in the response, returns where the frame was saved.
                    m_videoFrame.reset();
                    if (SkImageDecoder::DecodeFile(value, &m_videoFrame)) {
                        if (!m_currentMediaTime)
                            m_videoFrameContents = eVideoFrameContentsFirstFrame;
                        else
                            m_videoFrameContents = eVideoFrameContentsPauseFrame;
                        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d: Saved video frame type %d.", mmrNameRoot, m_instanceId, m_videoFrameContents);
                        // We used to unlink frame files here, but libmd
                        // may create the file even if it does not respond,
                        // so the unlink has been moved. This fixes zero length
                        // frame files left behind in the case of audio.
                    }
                }
                break;
            }
        }

        // Find the next attribute, if there is one
        if (*valueEnd)
            attr = valueEnd;
        else
            attr = 0;
    }

    return numItems;
}

void MediaPlayerPrivate::signalMetadataCondvarFinished()
{
    // If nothing has signaled the condvar already, signal it now in case anyone is waiting.
    if (!m_metadataFinished) {
        pthread_mutex_lock(&m_metadataFinishedMutex);
        m_metadataFinished = true;
        pthread_mutex_unlock(&m_metadataFinishedMutex);
        pthread_cond_signal(&m_metadataFinishedCond);
    }
}

//
// This function is called the first time that paint() gets called. It will
// only get called in subsequent paints if we don't manage to set up m_window
// the first time. This mainly happens when the media window creation
// notification from mm-renderer is delayed.
//
BlackBerry::Platform::Graphics::Window* MediaPlayerPrivate::screenWindowSet()
{
    m_window = 0;
    if (!m_parentWindow)
        return 0;

    m_window = getPeerWindow(HTML5_MMR_VIDEO_WINDOW_ID);
    if (!m_window) {
        // There was a delay and the mm-renderer window has not been created
        // in our window's group yet, or we have not processed the
        // notification from Screen yet.
        return 0;
    }

    return m_window;
}

void MediaPlayerPrivate::windowCleanup()
{
    if (m_window) {
        m_window = 0;
        m_parentWindow = 0;
    }
}

BlackBerry::Platform::Graphics::Window* MediaPlayerPrivate::windowGet()
{
    if (s_activeElements.videoElement() == this)
        return getPeerWindow(HTML5_MMR_VIDEO_WINDOW_ID);
    return 0;
}

bool MediaPlayerPrivate::supportsFullscreen() const
{
    return true;
}

bool MediaPlayerPrivate::hasAvailableVideoFrame() const
{
    // The default behaviour for this is to look at the reported state, and
    // if it's greater than has metadata, then this returns true. In this case,
    // this isn't true until mm-renderer's state is changed.
    // However, if idle, that means are likely paused elsewhere.
    return m_state != MP_STATE_INITIAL || m_mmrPlayState == MMRPlayStatePlaying
        || m_mmrPlayState == MMRPlayStateStopped || m_currentMediaTime;
}

int MediaPlayerPrivate::videoFrameCapture()
{
    int size[2];
    int position[2];
    int rc;
    int stride;
    int format;
    int usage;
    static bool noPauseFrame = getenv("WK_NO_PAUSEFRAME");

    if (noPauseFrame) {
        m_videoFrame.reset();
        m_videoFrameContents = eVideoFrameContentsNone;
        return -1;
    }

    screen_pixmap_t screenPixMap = 0;
    screen_buffer_t screenFrameBuffer = 0;
    void* frameBuffer = 0;

    do { // TRY handler
        // TODO: Move the platform window calls to BlackBerry::Platform::Graphics::Window and call those from here instead.
        screen_window_t screenWindow = static_cast<screen_window_t>(m_window->platformHandle());

        // The output buffer size is set to match the source buffer.
        rc = screen_get_window_property_iv(screenWindow, SCREEN_PROPERTY_SOURCE_POSITION, position);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to get source window position: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }
        rc = screen_get_window_property_iv(screenWindow, SCREEN_PROPERTY_SOURCE_SIZE, size);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to get source window size: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }

        if (m_videoFrameContents != eVideoFrameContentsNone) {
            // See if need to change the buffer size.
            if (size[0] != m_videoFrame.width() || size[1] != m_videoFrame.height()) {
                m_videoFrame.reset();
                m_videoFrameContents = eVideoFrameContentsNone;
            }
        }

        // Need to provide the buffer to comp manager, since can't assign pixels to
        // skia bitmaps with the way the current drawing model works.
        if (m_videoFrameContents == eVideoFrameContentsNone) {
            m_videoFrame.setConfig(SkBitmap::kARGB_8888_Config, size[0], size[1]);
            m_videoFrame.setIsOpaque(true);
            if (!m_videoFrame.allocPixels()) {
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to allocate bitmap pixels.", mmrNameRoot, m_instanceId);
                break;
            }
        }
        // might have been first frame...
        m_videoFrameContents = eVideoFrameContentsPauseFrame;

        frameBuffer = m_videoFrame.getPixels();
        stride = m_videoFrame.rowBytes();

        if (!s_compManCtx) {
             rc = screen_create_context(&s_compManCtx, 0);
             if (rc) {
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable create screen context: %s (%d).",
                    mmrNameRoot, m_instanceId, strerror(errno), errno);
                break;
             }
        }

        rc = screen_create_pixmap(&screenPixMap, s_compManCtx);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable create pixmap: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }

        // Intent is that this comp manager format matches Skia kARGB_8888_Config
        format = SCREEN_FORMAT_RGBX8888;
        rc = screen_set_pixmap_property_iv(screenPixMap, SCREEN_PROPERTY_FORMAT, &format);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to set pixmap format: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }

        rc = screen_set_pixmap_property_iv(screenPixMap, SCREEN_PROPERTY_BUFFER_SIZE, size);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to set pixmap size to (%d,%d): %s (%d).",
                mmrNameRoot, size[0], size[1], m_instanceId, strerror(errno), errno);
            break;
        }

        usage = SCREEN_USAGE_NATIVE | SCREEN_USAGE_READ;
        rc = screen_set_pixmap_property_iv(screenPixMap, SCREEN_PROPERTY_USAGE, &usage);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to set pixmap usage: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }

        // Requires a copy, since both comp manager and Skia are using their
        // own buffers.
        // It might be worth effort to find out why SkBitmap requires to own
        // its buffer; providing the buffer comp manager cannot be guaranteed
        // to work on all platforms, since the blit might be hardware
        // accelerated and require specific buffer properties.
        rc = screen_create_pixmap_buffer(screenPixMap);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to create pixmap buffer: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }

        rc = screen_get_pixmap_property_pv(screenPixMap, SCREEN_PROPERTY_RENDER_BUFFERS, (void**) &screenFrameBuffer);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to get pixmap buffer: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }

        struct {
            int srcX[2];
            int srcY[2];
            int srcWidth[2];
            int srcHeight[2];
            int dstX[2];
            int dstY[2];
            int dstWidth[2];
            int dstHeight[2];
            int none;
        } screen_blit_attr = {
            { SCREEN_BLIT_SOURCE_X, position[0] },
            { SCREEN_BLIT_SOURCE_Y, position[1] },
            { SCREEN_BLIT_SOURCE_WIDTH, size[0] },
            { SCREEN_BLIT_SOURCE_HEIGHT, size[1] },
            { SCREEN_BLIT_DESTINATION_X, 0 },
            { SCREEN_BLIT_DESTINATION_Y, 0 },
            { SCREEN_BLIT_DESTINATION_WIDTH, size[0] },
            { SCREEN_BLIT_DESTINATION_HEIGHT, size[1] },
            SCREEN_BLIT_END
        };

        // Use the window as the source and let screen figure out the source
        // buffer to use.
        rc = screen_blit(s_compManCtx, screenFrameBuffer, (screen_buffer_t) screenWindow, (int*) &screen_blit_attr);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to copy from window: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }

        // Force the blit now and wait until it completes.
        rc = screen_flush_context(s_compManCtx, SCREEN_WAIT_IDLE);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to finish blits: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }

        // Need to copy from the comp manager buffer to the SkBitmap pixels
        void* srcFrameBuffer;
        rc = screen_get_buffer_property_pv(screenFrameBuffer, SCREEN_PROPERTY_POINTER, &srcFrameBuffer);
        if (rc) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Unable to get buffer pointer: %s (%d).",
                mmrNameRoot, m_instanceId, strerror(errno), errno);
            break;
        }
        // TODO: change to row-by-row copies of the correct length?
        memcpy(frameBuffer, srcFrameBuffer, stride * size[1]);

        if (!m_videoFrame.readyToDraw()) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "%s%d: Video frame bitmap not ready to draw.", mmrNameRoot, m_instanceId);
            rc = -1;
        }

    } while (0); // So we can break out if something goes wrong.

    if (screenPixMap)
        screen_destroy_pixmap(screenPixMap);

    if (rc) {
        // Something went wrong; clean up
        m_videoFrame.reset();
        m_videoFrameContents = eVideoFrameContentsNone;
        return -1;
    }
    // Make sure a paint() happens, particularly if paused.
    m_player->repaint();

    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "%s%d: Saved video frame, size (%d,%d) (thread %d).",
        mmrNameRoot, m_instanceId, size[0], size[1], pthread_self());
    return 0;
}

int MediaPlayerPrivate::stillFrameCapture(uint32_t mmrTime)
{
    // Initialize metadata library handling.
    mmmd_hdl_t* mdHdl = metadataInit();
    if (!mdHdl)
        return -1;

    int rc = -1;
    char* md = 0;
    const size_t typeStrSize = 256;
    char* typeStr;
    char frameFile[64];

    typeStr = (char*) alloca(typeStrSize);
    snprintf(frameFile, sizeof(frameFile), "/tmp/%s%d.bmp", mmrNameRoot, m_instanceId);
    rc = snprintf(typeStr, typeStrSize, "md_video::frame?file=%s&time=%u",
                  frameFile, mmrTime);
    if (rc < 0 || (size_t) rc > typeStrSize) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to build metadata request string: %s.",
                  rc < 0 ? strerror(errno) : "too long");
        rc = -1;
    } else {
        MutexLocker locker(m_mutex);
        rc = mmmd_get(mdHdl, m_url.utf8().data(), typeStr, 0, 0, &md);
        if (rc > 0) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Metadata for \"%s\": \n%s\n", m_url.utf8().data(), md);
            rc = metadataParse(md);
            free(md);
            unlink(frameFile);
        } else {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Error getting still frame for \"%s\".", m_url.utf8().data());
            rc = -1;
        }
    }

    // No need to keep the metadata session any longer.
    metadataTerminate(mdHdl);

    return rc;
}

int MediaPlayerPrivate::windowPositionGet(unsigned& x, unsigned& y, unsigned& width, unsigned& height) const
{
    x = static_cast<unsigned>(m_xOrigin + m_videoRect.x());
    y = static_cast<unsigned>(m_yOrigin + m_videoRect.y());
    width = static_cast<unsigned>(m_videoRect.width());
    height = static_cast<unsigned>(m_videoRect.height());

    return 0;
}

BlackBerry::Platform::Graphics::Window* MediaPlayerPrivate::getPeerWindow(const char* uniqueId) const
{
    BlackBerry::Platform::Graphics::Window* peerWindow = 0;

    ASSERT(m_player);

    if (m_parentWindow) {
        vector<BlackBerry::Platform::Graphics::Window*> childWindows = m_parentWindow->childWindows();
        int numWindows = childWindows.size();
        for (int i = 0; i < numWindows; i++) {
            BlackBerry::Platform::Graphics::Window* win = childWindows[i];
            if (win) {
                if (win->uniqueId()) {
                    if (!strcmp(win->uniqueId(), uniqueId)) {
                        peerWindow = win;
                        break;
                    }
                }
            }
        }
    }

    return peerWindow;
}

void MediaPlayerPrivate::resizeSourceDimensions()
{
    if (!s_elements.contains(this) || !m_player)
        return;

    HTMLMediaElement* client = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());

    // If we have an HTMLVideoElement but the source has no video, then we need to resize the media element.
    if (client && client->isVideo() && !hasVideo()) {
        RenderObject* ro = client->renderer();
        if (ro) {
            IntRect rect = ro->enclosingBox()->contentBoxRect();

            // If the rect dimensions are less than the allowed minimum, use the minimum instead.
            int newWidth = (rect.width() > PLAYBOOK_MIN_AUDIO_ELEMENT_WIDTH) ? rect.width() : PLAYBOOK_MIN_AUDIO_ELEMENT_WIDTH;
            int newHeight = (rect.height() > PLAYBOOK_MIN_AUDIO_ELEMENT_HEIGHT) ? rect.height() : PLAYBOOK_MIN_AUDIO_ELEMENT_HEIGHT;

            char attrString[12];

            sprintf(attrString, "%d", newWidth);
            client->setAttribute(HTMLNames::widthAttr, attrString);

            sprintf(attrString, "%d", newHeight);
            client->setAttribute(HTMLNames::heightAttr, attrString);
        }
    }

    // If we don't know what the width and height of the video source is, then we need to set it to something sane.
    if (client && client->isVideo() && !(m_sourceWidth && m_sourceHeight)) {
        RenderObject* ro = client->renderer();
        if (ro) {
            IntRect rect = ro->enclosingBox()->contentBoxRect();
            m_sourceWidth = rect.width();
            m_sourceHeight = rect.height();
        }
    }
}

bool MediaPlayerPrivate::isFullscreen() const
{
    return m_fullscreenWebPageClient;
}

void MediaPlayerPrivate::setFullscreenWebPageClient(BlackBerry::WebKit::WebPageClient* client)
{
    if (m_fullscreenWebPageClient == client)
        return;

    int screenRet;
    int floating, zorder;
    static bool debugFullscreen = getenv("WK_FULLSCREEN_HTML5_VIDEO_DEBUG");
    if (client) {
        m_fullscreenWebPageClient = client;
        // Entering fullscreen
        floating = 1;
        if (debugFullscreen)
            zorder = HTML5_MMR_VIDEO_WINDOW_ZORDER;
        else {
            zorder = -HTML5_MMR_VIDEO_WINDOW_ZORDER;
            // While we are in fullscreen, we want transparency "discard" on
            // everything except the video window.
            BlackBerry::Platform::Graphics::Window::setTransparencyDiscardFilter(HTML5_MMR_VIDEO_WINDOW_ID);
        }
        // Make sure the native controls timeline slider thumb gets updated when
        // seeking is done while paused, in fullscreen mode.
        m_lastSeekPosition = 0;
    } else {
        // Exiting fullscreen
        m_fullscreenWebPageClient = client;
        floating = 0;
        zorder = HTML5_MMR_VIDEO_WINDOW_ZORDER;
        // This sets the transparency "discard" to what it was before.
        BlackBerry::Platform::Graphics::Window::setTransparencyDiscardFilter(0);

        // The following repaint is needed especially if video is paused and
        // fullscreen is exiting, so that a MediaPlayerPrivate::paint() is
        // triggered and the code in outputUpdate() sets the correct window
        // rectangle.
        m_player->repaint();

        // If video is paused, also do something that ends up calling
        // HTMLMediaElement::updatePlayState(), so that the controls show up.
        // They were hidden while we were in fullscreen mode to save CPU.
        // See RenderMedia::updateControlVisibility().
        if (mmrPaused())
            m_player->timeChanged();
    }
    if (m_window) {
        screen_window_t screenWindow = static_cast<screen_window_t>(m_window->platformHandle());
        screenRet = screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_ZORDER, &zorder);
        if (screenRet)
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "screen_set_window_property_iv SCREEN_PROPERTY_ZORDER failed: %s (%d).", strerror(errno), errno);
        screenRet = screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_FLOATING, &floating);
        if (screenRet)
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "screen_set_window_property_iv SCREEN_PROPERTY_FLOATING failed: %s (%d).", strerror(errno), errno);
        //
        // Flush the screen context so the zorder change takes effect right
        // away - needed especially if video is paused and fullscreen is exiting
        //
        screen_context_t ctx = BlackBerry::Platform::Graphics::screenContext();
        screen_flush_context(ctx, 0);
    }
}

//
// This will get called when the system tray wants WebKit to pause its media.
// For example if the user has started playing media using a different
// application. This will be most noticeable when playing html5 audio because
// video will have already paused when the other app went into the foreground.
// This function is also called when the user presses the h/w pause button, or
// the system tray media bubble's pause button.
//
void MediaPlayerPrivate::onTrackPause()
{
    if (m_player) {
        HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
        if (element && !mmrPaused())
            element->pause();
    }
}

// This function is called when the user presses the h/w play button, or
// the system tray media bubble's play button.
void MediaPlayerPrivate::onTrackPlay()
{
    if (m_player) {
        HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());
        if (element && mmrPaused()) {
            // The media element is not playing. Try to play it. Only
            // audio is allowed to play if we are deactivated.
            if (s_activated || !hasVideo() || s_allowDeactivatedVideoPlay)
                element->play();
        }
    }
}

void MediaPlayerPrivate::onError(const char* error)
{
    notImplemented();
}

#if USE(ACCELERATED_COMPOSITING)
static const double BufferingAnimationDelay = 1.0 / 24;

static char* s_bufferingImageData = 0;
static int s_bufferingImageWidth = 0;
static int s_bufferingImageHeight = 0;

static void loadBufferingImageData()
{
    static bool loaded = false;
    if (!loaded) {
        static Image* bufferingIcon = Image::loadPlatformResource("vidbuffer").leakRef();
        NativeImageSkia* nativeImage = bufferingIcon->nativeImageForCurrentFrame();
        if (!nativeImage)
            return;

        if (!nativeImage->isDataComplete())
            return;

        loaded = true;
        nativeImage->lockPixels();

        int bufSize = nativeImage->width() * nativeImage->height() * 4;
        s_bufferingImageWidth = nativeImage->width();
        s_bufferingImageHeight = nativeImage->height();
        s_bufferingImageData = static_cast<char*>(malloc(bufSize));
        memcpy(s_bufferingImageData, nativeImage->getPixels(), bufSize);

        nativeImage->unlockPixels();
        bufferingIcon->deref();
    }
}

void MediaPlayerPrivate::bufferingTimerFired(Timer<MediaPlayerPrivate>*)
{
    if (m_showBufferingImage) {
        if (!isFullscreen() && m_platformLayer)
            m_platformLayer->setNeedsDisplay();
        m_bufferingTimer.startOneShot(BufferingAnimationDelay);
    }
}

void MediaPlayerPrivate::setBuffering(bool buffering)
{
    if (!hasVideo())
        buffering = false; // Buffering animation not visible for audio
    if (buffering != m_showBufferingImage) {
        m_showBufferingImage = buffering;
        if (buffering) {
            loadBufferingImageData();
            m_bufferingTimer.startOneShot(BufferingAnimationDelay);
        } else
            m_bufferingTimer.stop();

        if (m_platformLayer)
            m_platformLayer->setNeedsDisplay();
    }
}

PlatformMedia MediaPlayerPrivate::platformMedia() const
{
    PlatformMedia pm;
    pm.type = PlatformMedia::QNXMediaPlayerType;
    pm.media.qnxMediaPlayer = const_cast<MediaPlayerPrivate*>(this);
    return pm;
}

static unsigned int allocateTextureId()
{
    unsigned int texid;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    // Do basic linear filtering on resize.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // NPOT textures in GL ES only work when the wrap mode is set to GL_CLAMP_TO_EDGE.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texid;
}

void MediaPlayerPrivate::drawBufferingAnimation(const TransformationMatrix& matrix, int positionLocation, int texCoordLocation)
{
    if (m_showBufferingImage && s_bufferingImageData && !isFullscreen()) {
        TransformationMatrix renderMatrix = matrix;

        // rotate the buffering indicator so that it takes 1 second to do 1 revolution
        timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        renderMatrix.rotate(time.tv_nsec / 1000000000.0 * 360.0);

        static bool initialized = false;
        static unsigned int texId = allocateTextureId();
        glBindTexture(GL_TEXTURE_2D, texId);
        if (!initialized) {
            initialized = true;
            glTexImage2D(GL_TEXTURE_2D, 0 /*level*/, GL_RGBA /*internal*/,
                s_bufferingImageWidth, s_bufferingImageHeight,
                                 0 /*border*/, GL_RGBA, GL_UNSIGNED_BYTE, s_bufferingImageData);
            free(s_bufferingImageData);
        }

        float texcoords[] = { 0, 0,  0, 1,  1, 1,  1, 0 };
        FloatPoint vertices[4];
        float bx = s_bufferingImageWidth / 2.0;
        float by = s_bufferingImageHeight / 2.0;
        vertices[0] = renderMatrix.mapPoint(FloatPoint(-bx, -by));
        vertices[1] = renderMatrix.mapPoint(FloatPoint(-bx, by));
        vertices[2] = renderMatrix.mapPoint(FloatPoint(bx, by));
        vertices[3] = renderMatrix.mapPoint(FloatPoint(bx, -by));

        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}
#endif

} // namespace WebCore

#endif
