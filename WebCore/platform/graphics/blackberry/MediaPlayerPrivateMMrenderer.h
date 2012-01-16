/*
 * Based on source with the following notice:
 */

/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
 * with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef MediaPlayerPrivateMMrenderer_h
#define MediaPlayerPrivateMMrenderer_h

#include <BlackBerryPlatformWindow.h>
#include <BlackBerryPlatformGraphicsImpl.h>
#include <WebPageClient.h>
#include <screen/screen.h>

#if USE(ACCELERATED_COMPOSITING)
#include "LayerProxyGLES2.h"
#endif

namespace WebCore {

/**
 * A callback function to get the application to do something for this object.
 */
typedef int (*mpCallbackMsgSend_t)(void *obj, uint32_t flags, void *userdata);

/**
 * A function to get the application's window information.
 */
typedef screen_window_t (*mpScreenWindowFind_t)(const char *group, const char *id, void *userdata);

/**
 * The next two functions must always be available to the main application.
 */
#pragma GCC visibility push(default)

// This is the callback that the application should use once it gets the message.
void mpCallbackHandler(void *data);

// In the event that the source window needs to be resized from the metadata thread, it must be called from the main thread, which ends up calling here.
void mpResizeSourceDimensions(void* data);

// A structure to use in order to call callOnMainThread() to callback to mpCallbackHandler.
class MediaPlayerCallbackData
{
public:
    MediaPlayerCallbackData(void *mp, uint32_t flags) : m_mp(mp), m_flags(flags) {}
    void *m_mp;
    uint32_t m_flags;
    float m_floatval;
};

/**
 * Function to set the callback so this object can get the main thread
 * to do things for it.
 *
 * Returns pointer to previous.
 */
mpCallbackMsgSend_t mpCallbackMsgSendSet(mpCallbackMsgSend_t func, void *userdata);

/**
 * Function to set the callback so this object can get window information from
 * the application.
 *
 * Returns pointer to previous.
 */
mpScreenWindowFind_t mpScreenWindowFindSet(mpScreenWindowFind_t func, void *userdata);

/**
 * Return to normal visibility based on the build type.
 */
#pragma GCC visibility pop

}

#if ENABLE(VIDEO)

#include "MediaPlayerPrivate.h"
#include "NotImplemented.h"
#include "SharedPointer.h"

#include <pthread.h>
#include <mm/renderer.h>
#include <mm/md.h>

#include "MMrenderer.h"
#include "BlackBerryPlatformMediaConn.h"
#include "BlackBerryPlatformMediaConnListener.h"

#define PLAYBOOK_MIN_AUDIO_ELEMENT_WIDTH 300
#define PLAYBOOK_MIN_AUDIO_ELEMENT_HEIGHT 32

namespace WTF {
class String;
}

namespace WebCore {

class GraphicsContext;
class IntSize;
class IntRect;

class MediaPlayerPrivate : public MediaPlayerPrivateInterface, Olympia::Platform::MediaConnListener {

    friend int ppsHandler(const char *name, pps_obj_event_t event, const strm_dict_t *dict, void *mp);
    friend void *workerThread(void *mp);
    friend void mpOutputUpdate(const Olympia::Platform::Graphics::Window *window);
    friend void mpCallbackHandler(void *data);
    friend void mpResizeSourceDimensions(void* data);

    public:

        static void registerMediaEngine(MediaEngineRegistrar);
        ~MediaPlayerPrivate();

        IntSize naturalSize() const;

        bool hasVideo() const;
        bool hasAudio() const { return m_hasAudio; }

        void load(const String &url);
        void cancelLoad();

        void prepareToPlay();
        void play();
        void pause();

        // This function causes playback to stop, the state to be saved,
        // and use of the renderer context to stop.
        void stop();

        bool paused() const;
        bool seeking() const;

        float duration() const;
        float currentTime() const;
        void seek(float);

        // Rate is <http://www.w3.org/TR/2009/WD-html5-20090212/video.html#dom-media-playbackrate>.
        /*
         * "The playbackRate  attribute gives the speed at which the media resource
         * plays, as a multiple of its intrinsic speed. If it is not equal to the
         * defaultPlaybackRate, then the implication is that the user is using a
         * feature such as fast forward or slow motion playback."
         */
        void setRate(float);
        float rate() const { return static_cast<float>(m_curSpeed); }

        // Volume goes from 0.0 (quiestest) to 1.0 (loudest).
        void setVolume(float);

        MediaPlayer::NetworkState networkState() const;
        MediaPlayer::ReadyState readyState() const;

        // Indicated the times that seek() may be used for. Might not
        // be to the beginning if the first buffers are irretrievably discarded.
        PassRefPtr<TimeRanges> buffered() const;
        float maxTimeSeekable() const;
        unsigned bytesLoaded() const;

        // FIXME: see comment in .cpp file
        float percentLoaded();

        void setVisible(bool);
        void setSize(const IntSize&);

        void loadStateChanged();
        void sizeChanged();
        void timeChanged();
        void durationChanged();

        void repaint();
        void paint(GraphicsContext*, const IntRect&);
#if USE(ACCELERATED_COMPOSITING)
        PlatformMedia platformMedia() const;
        void drawBufferingAnimation(const TransformationMatrix& matrix, int positionLocation, int texCoordLocation);
#endif

        bool hasSingleSecurityOrigin() const;

        bool supportsFullscreen() const;

        // TODO: Determine what to do about these. Muting should probably be supported but handled
        //       separately from volume adjustments since volume adjustments are applied globally.
        virtual bool supportsMuting() const { return false; }
        virtual void setMuted(bool) { }

        virtual bool hasAvailableVideoFrame() const;

        bool isFullscreen() const;
        void setFullscreenWebPageClient(Olympia::WebKit::WebPageClient* client);

        Olympia::Platform::Graphics::Window* windowGet();
        int windowPositionGet(unsigned &x, unsigned &y, unsigned &width, unsigned &height) const;
        const char *mmrContextNameGet();
        void showErrorDialog(Olympia::WebKit::WebPageClient::AlertType atype) const;

        static void notifyAppActivatedEvent(bool activated);
        static void setCertificatePath(const String& caPath) { s_caPath = caPath; }

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        void deliverNotification(MediaPlayerProxyNotificationType notificationType);
        void setMediaPlayerProxy(WebMediaPlayerProxy* proxy);
#endif

        unsigned sourceWidth() { return m_source_width; }

        void setAllowPPSVolumeUpdates(bool allow) { m_allowPPSVolumeUpdates = allow; }

    private:

        MediaPlayerPrivate(MediaPlayer*);

        // Callback flags: these define the callbacks the main thread
        // should handle.
        enum {
            MP_INVALID_CALLBACK = 0,
            ////////////////
            MP_REPAINT,
            MP_NET_STATE_CHANGED,
            MP_RDY_STATE_CHANGED,
            MP_VOLUME_CHANGED,
            MP_TIME_CHANGED,
            MP_SIZE_CHANGED,
            MP_RATE_CHANGED,
            MP_DURATION_CHANGED,
            MP_METADATA_SET,
            MP_READ_ERROR,
            MP_MEDIA_ERROR,
            MP_SHOW_ERROR_DIALOG,
#if USE(ACCELERATED_COMPOSITING)
            MP_BUFFERING_STARTED,
            MP_BUFFERING_ENDED,
#endif
            ////////////////
            MP_NUM_CALLBACKS
        } MpCallBackFlags;

        const char *cbFlagStr(uint32_t flag) const;

        typedef enum {
            MP_STATE_INITIAL = 0,
            MP_STATE_IDLE,
            MP_STATE_ACTIVE,
            MP_STATE_UNSUPPORTED
        } MpState;

        const char *stateStr(MpState state) const;

        int volumeCallbackMsgSend(uint32_t flags, float newVolume);
        int mediaStateCallbackMsgSend(uint32_t flags);

        // The three callbacks for the factory
        static MediaPlayerPrivateInterface* create(MediaPlayer* player);
        static void getSupportedTypes(HashSet<String>&);
        static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs);

        bool outputsAttach();
        bool inputAttach();
        int volumeSet();

        // PPS event handling methods
        int playStateHandle(pps_obj_event_t event, const strm_dict_t *dict);
        int playStatusHandle(pps_obj_event_t event, const strm_dict_t *dict);
        int playOutputHandle(pps_obj_event_t event, const strm_dict_t *dict);
        int metadataHandle(pps_obj_event_t event, const strm_dict_t *dict);
        int audioStatusHandle(pps_obj_event_t event, const strm_dict_t *dict);

        // Worker thread for doing work outside webkit's main thread,
        // and to remind webkit that there are frames rendering (if necessary).
        // No longer blocks until worker thread is joined; is detached.
        bool workerThreadStart(int priority = 0);
        void *worker(String& url, String& userAgent, String& caPath);

        int msgReceiverAdd();
        int msgReceiverRemove();

        // Callback handler for messages to main thread.
        // Use should be wrapped with the start and end calls to make sure
        // that it's not deleted during the handling of a callback.
        int callbackStart() const
        {
            if (pthread_mutex_lock(&m_msgLock) != EOK)
                return -1;
            return 0;
        }

        void volumeCallbackHandler(float newVolume);
        int mediaStateCallbackHandler(uint32_t flags);
        static inline void insertCallback(uint32_t callbacks[], size_t num, uint32_t cb);

        int callbackEnd() const
        {
            if (pthread_mutex_unlock(&m_msgLock) != EOK)
                return -1;
            return 0;
        }

        mmmd_hdl_t* metadataInit();
        mmmd_hdl_t* metadataInit(const String& url, const String& userAgent, const String& caPath); // Call this one when we don't want metadataInit() accessing String parameters directly (for thread safety).
        bool metadataTerminate(mmmd_hdl_t* md_hdl);
        int metadataGet(const String &url, mmmd_hdl_t *md_hdl);
        int metadataParse(char *md);

        void signalMetadataCondvarFinished();

        static MediaPlayerPrivate *s_activeElement;
        static HashSet<MediaPlayerPrivate *> s_elements;
        static pthread_mutex_t s_elementsMutex;

        // Updates the output parameters
        void outputUpdate(bool updateClip) const;
        void outputUpdate(const IntRect& rect);

        Olympia::Platform::Graphics::Window* getPeerWindow(const char* uniqueId) const;
        void pausedStateUpdate();
        void seekms(uint32_t);
        const String userAgent(const String &url);

        // MediaConnListener functions
        virtual void onTrackPause();
        virtual void onTrackPlay();
        virtual void onError(const char* error);
        virtual void onMediaStateChange(Olympia::Platform::MediaConnListener::MState state) {}; // Will never be called

#if USE(ACCELERATED_COMPOSITING)
        virtual PlatformLayer* platformLayer() const;

        // whether accelerated rendering is supported by the media engine for the current media.
        virtual bool supportsAcceleratedRendering() const { return true; }

        // called when the rendering system flips the into or out of accelerated rendering mode.
        virtual void acceleratedRenderingStateChanged() { }
#endif

    private:
        MpState m_state;
        bool m_isPaused;
        bool m_shouldBePlaying;
        MediaPlayer *m_player;
        int m_outputId;

        Mutex m_mutex;
        String m_url;
        String m_userAgent;
        bool m_inputAttached;

        pthread_t m_threadId;

        mutable MediaPlayer::NetworkState m_networkState;
        MediaPlayer::ReadyState m_readyState;

        unsigned m_instance_id;

        uint32_t m_reqSpeed;

        uint32_t m_currentMediaTime; // milliseconds
        uint32_t m_bufferLevel;      // milliseconds
        uint32_t m_bufferCapacity;   // milliseconds
        uint32_t m_curSpeed;
        unsigned m_bytesLoaded;

        // FIXME: see comment in percentLoaded() method in .cpp file
        float m_percentLoaded;

        bool m_timeFailureReported;
        bool m_bufferFailureReported;

        char *m_title;
        char *m_artist;
        char *m_album;
        char *m_artFile;

        // This mutex locks the list of message receivers from the main thread.
        static pthread_mutex_t s_rcvrLock;
        // This mutex is taken the the object is handling message from the main thread.
        mutable pthread_mutex_t m_msgLock;

        /**
         * Play states that come from mm_renderer.
         */
        enum MMRPlayState {
            MMRPlayStateUnknown = 0,
            MMRPlayStatePlaying, // m_curSpeed determines whether we are paused
            MMRPlayStateStopped,
            MMRPlayStateIdle
        };

        MMRPlayState mmrStateFromString(const char *state);
        const char *mmrPlaystateStr(MMRPlayState state) const;

        MMRPlayState m_mmrPlayState;

        // Characteristics of the item. Should probably be in its own object.
        uint32_t m_duration; // milliseconds
        bool m_hasAudio;
        bool m_hasVideo;
        unsigned m_source_width;
        unsigned m_source_height;

        int m_audioOutputID;
        int m_videoOutputID;

        float m_volume;

        int m_windowX;
        int m_windowY;
        int m_windowWidth;
        int m_windowHeight;

        IntRect m_videoRect;
        int m_x_origin;
        int m_y_origin;
        double m_zoomFactor;

        MMrendererContext *m_rendererContext;

        Olympia::Platform::Graphics::Window *m_window;
        Olympia::Platform::Graphics::Window *m_parentWindow;
        static int m_windowRefCount;

        /* Media players use their own context so that when the blit is
         * executed it doesn't interfere with any of the page rendering blits.
         */
        static screen_context_t s_compManCtx;

        enum {
            eVideoFrameContentsNone = 0,
            eVideoFrameContentsFirstFrame,
            eVideoFrameContentsPauseFrame
        };

        int m_videoFrameContents;
        SkBitmap m_videoFrame;

        int mmrConnect();
        int mmrDisconnect();

        // int screenWindowGroup(bool add);
        Olympia::Platform::Graphics::Window *screenWindowSet();
        void windowCleanup();

        int videoFrameCapture();
        int stillFrameCapture(uint32_t timems);

        bool m_metadataStarted;
        bool m_metadataFinished;
        mutable pthread_mutex_t m_metadataFinishedMutex;
        mutable pthread_cond_t m_metadataFinishedCond;
        mutable bool m_metadataFinishedCondTimeExpired;

        void resizeSourceDimensions();

        void setVolumeFromDict(const strm_dict_t *dict);

        int convertVolume(float in);

        Olympia::WebKit::WebPageClient* m_fullscreenWebPageClient;

        bool m_playOnAppActivate;
        static bool s_activated;
        static String s_caPath;
        Olympia::Platform::MediaConn* m_mediaConn;

        bool m_allowPPSVolumeUpdates;
        time_t m_lastPausedTime;
        time_t m_reseekArmedTime;
        int32_t m_mediaWarningPos;
        int32_t m_previousMediaWarningPos;
        char *m_previousMediaWarning;
        int32_t m_mediaErrorPos;
        int32_t m_previousMediaErrorPos;
        int m_previousMediaError;
        Olympia::WebKit::WebPageClient::AlertType m_pendingErrorDialog;

        // Returns true if the timeout has occurred and was processed, false otherwise.
        bool processPauseTimeoutIfNecessary();

#if USE(ACCELERATED_COMPOSITING)
        void bufferingTimerFired(Timer<MediaPlayerPrivate>*);
        Timer<MediaPlayerPrivate> m_bufferingTimer;

        RefPtr<PlatformLayer> m_platformLayer;
        bool m_showBufferingImage;
        bool m_mediaIsBuffering;
        void setBuffering(bool buffering);
#endif
};

}

#endif  /* ENABLE(VIDEO) */

#endif  /* MediaPlayerPrivateMMrenderer_h */
