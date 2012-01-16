/*
 * Copyright (C) 2010 QNX Software Systems
 *
 * TODO: proper copyright notice for this.
 */
#include "config.h"

#if ENABLE(VIDEO)

#include <BlackBerryPlatformClient.h>
#include <BlackBerryPlatformLog.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "MMrenderer.h"
#include "MediaPlayerPrivateMMrenderer.h"
#include "CookieManager.h"

namespace WebCore {

/**
 * TODO: Determine if this is variable, and if so, what it is each time and
 *       build it up.
 */
#define PPS_ROOT_PATH "/pps/services/multimedia/renderer/"

/**
 * Location of audio status directory in PPS.
 */
#define PPS_AUDIO_PATH "/pps/services/audio/"

/**
 * Location of renderer context directories in PPS.
 */
static const char pps_ps_path[] = PPS_ROOT_PATH"context";

/**
 * The mime attribute of this object has a comma separated list of the MIME
 * types supported by mm-renderer.
 */
// TODO: Update the way we read PPS objects here. We should not need to specify the exact filesystem location
//       of the file we want to read that contains the "mime" object, which contains the supported MIME types.
//       See PR 92803.
static const char pps_mmf_route_path[] = PPS_ROOT_PATH"component/mmr-mmf-routing";

static const char pps_mime_obj_name[] = "mime";

MMrendererContext::MMrendererContext *MMrendererContext::s_instance = 0;

/**
 *  This is null until someone calls initializeExtraSupportedMimeTypes().
 */
HashSet<String>* MMrendererContext::s_extraSupportedMimeTypes = 0;

/**
 * This function is used to make a dictionary for the context.
 */
strm_dict_t *contextDictMake(const char *url, const char *userAgent, const char *caPath)
{
    strm_dict_t *dict = strm_dict_new();
    strm_dict_t *tmp;

    if (!dict)
        return dict;

    if (url) {
        WTF::String cookiePairs = WebCore::cookieManager().getCookie(KURL(WebCore::ParsedURLString, url), WebCore::WithHttpOnlyCookies);
        if (!cookiePairs.isEmpty()) {
            if (cookiePairs.utf8().data()) {
                tmp = strm_dict_set(dict, "OPT_COOKIE", cookiePairs.utf8().data());
                if (tmp) {
                    dict = tmp;
                }
            }
        }
    }

    // Note that if a user agent is not specified here, http_streamer.so
    // will use the contents of env var MM_HTTP_USERAGENT, and if that is
    // also not set it simply uses the hard coded "io-media". In other words
    // this is mandatory.
    if (userAgent && *userAgent) {
        Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Passing OPT_USERAGENT \"%s\".", userAgent);
        tmp = strm_dict_set(dict, "OPT_USERAGENT", userAgent);
        if (tmp) {
            dict = tmp;
        }
    }

    if (caPath && *caPath) {
        Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Using CAPATH directory \"%s\".", caPath);
        tmp = strm_dict_set(dict, "OPT_CAPATH", caPath);
        if (tmp) {
            dict = tmp;
        }
    }
    if (strncasecmp(url, "https:", 6) == 0) {
        // The http_streamer.so defaults these both to zero. The libcurl
        // defaults are 1 and 2 respectively. I am using values that are
        // a simplification of the code in CURLHandle::setRequest().
        tmp = strm_dict_set(dict, "OPT_SSL_VERIFYPEER", "1");
        if (tmp) {
            dict = tmp;
        }
        tmp = strm_dict_set(dict, "OPT_SSL_VERIFYHOST", "1");
        if (tmp) {
            dict = tmp;
        }
    }

    if (getenv("WK_CURL_DEBUG")) {
        tmp = strm_dict_set(dict, "OPT_VERBOSE", "1");
        if (tmp) {
            dict = tmp;
        }
    }

    string proxyAddress = Olympia::Platform::Client::get()->getProxyAddress();
    string proxyPort = Olympia::Platform::Client::get()->getProxyPort();
    if(proxyAddress.size() && proxyPort.size()) {
        tmp = strm_dict_set(dict, "OPT_PROXY", proxyAddress.c_str());
        if (tmp) {
            dict = tmp;
        }

        tmp = strm_dict_set(dict, "OPT_PROXYPORT", proxyPort.c_str());
        if (tmp) {
            dict = tmp;
        }

        tmp = strm_dict_set(dict, "OPT_PROXYTYPE", "0");
        if (tmp) {
            dict = tmp;
        }

        string proxyUsername = Olympia::Platform::Client::get()->getProxyUsername();
        string proxyPassword = Olympia::Platform::Client::get()->getProxyPassword();

        if(proxyUsername.size() && proxyPassword.size()) {
            tmp = strm_dict_set(dict, "OPT_PROXYUSERNAME", proxyUsername.c_str());
            if (tmp) {
                dict = tmp;
            }

            tmp = strm_dict_set(dict, "OPT_PROXYPASSWORD", proxyPassword.c_str());
            if (tmp) {
                dict = tmp;
            }
        }
    } else {
        tmp = strm_dict_set(dict, "OPT_NOPROXY", "1");
        if (tmp) {
            dict = tmp;
        }
    }

    return dict;
}

MMrendererContext::MMrendererContext *MMrendererContext::take(
    MediaPlayerPrivate *mp,
    const char *name,
    pps_dir_func_t ppsHandler,
    const char *url,
    const char *userAgent,
    const char *caPath
)
{
    // Don't bother checking the name is the same.
    if (! s_instance) {
        bool        success;
        s_instance = new MMrendererContext(success);
        if (! success) {
            // already logged.
            delete s_instance;
            s_instance = 0;
            return 0;
        }
        Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Connected to renderer at %s.", "<default>");
    } else if (s_instance->m_user) {
        // Existing context is in use. Need to get it to stop.
        Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Forcing release of renderer at %s.", "<default>");
        s_instance->m_user->stop();
        // Now, the user should have called release, clearing itself of state.
        if (s_instance->m_user) {
            Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to force release of renderer context.");
            return 0;
        }
    }
    s_instance->m_user = mp;
    if (s_instance->open(name, ppsHandler, url, userAgent, caPath) == -1) {
        // already logged.
        s_instance->m_user = 0;
        return 0;
    }

    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Renderer at %s taken.", "<default>");
    return s_instance;
}

void MMrendererContext::release(MMrendererContext *renderer)
{
    if (renderer == s_instance) {
        s_instance->close();
        s_instance->m_user = 0;
    }
}

pps_cache_t   *MMrendererContext::s_ppsCache = 0;
pps_obj_t     *MMrendererContext::s_mmfRouteObject = 0;
strm_string_t *MMrendererContext::s_mimeTypes = 0;

int MMrendererContext::ppsInit()
{
    s_ppsCache = pps_cache_init(8 /* TODO: Provide some value that we know works all the time */);
    if (s_ppsCache == 0) {
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to initialize PPS cache.");
        return -1;
    }

    // Monitor the list of MIME types supported.
    s_mmfRouteObject = pps_cache_object_start(s_ppsCache, pps_mmf_route_path);
    if (s_mmfRouteObject == 0) {
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to monitor PPS object \"%s\".", pps_mmf_route_path);
        pps_cache_terminate(s_ppsCache);
        s_ppsCache = 0;
        return -1;
    }

    // This is the mime types attribute; PPS cache will keep it up to date.
    s_mimeTypes = pps_cache_object_attr_get(s_ppsCache, s_mmfRouteObject, pps_mime_obj_name);
    if (s_mimeTypes == 0) {
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to get mime types attribute from PPS.");
        pps_cache_object_stop(s_ppsCache, s_mmfRouteObject);
        s_mmfRouteObject = 0;
        pps_cache_terminate(s_ppsCache);
        s_ppsCache = 0;
        return -1;
    }
    //Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Initial MIME types supported: %s.", strm_string_get(s_mimeTypes));
    return 0;
}

bool MMrendererContext::mimeTypeSupported(const char *mimeType)
{
    if (s_mimeTypes == 0 && ppsInit() == -1) {
        return false;
    }

    return (strstr(strm_string_get(s_mimeTypes), mimeType) || extraMimeTypesSupported(mimeType));
}

void MMrendererContext::initializeExtraSupportedMimeTypes()
{
    ASSERT(!s_extraSupportedMimeTypes);
    s_extraSupportedMimeTypes = new HashSet<String>;
    addExtraSupportedMimeTypes(*s_extraSupportedMimeTypes);
}

void MMrendererContext::addExtraSupportedMimeTypes(HashSet<String>& set)
{
// More supported MIME types on PlayBook
    static const char* playbookMediaTypes[] = {
        "video/mp4",
        "video/m4v",
        "audio/m4a",
        "audio/aac",
        "audio/wav",
        "audio/mp1",
        "audio/mp2",
        "audio/mp3",
        "audio/mp4",
        "audio/x-mp3",
        "audio/mpeg",
        "audio/x-mpeg",
        "audio/mpg",
        "audio/x-mpg",
        "audio/mpeg3",
        "audio/x-mpeg3",
        "audio/x-mpegaudio",
        "video/avi",
        "audio/avi",
        "audio/wma",
        "video/x-mp4",
        "video/x-m4v",
        "audio/x-m4a",
        "audio/3gpp",
        "video/3gpp",
        "video/3gpp2",
        "audio/3gpp2",
        "video/quicktime",
        "video/mpeg",
        "audio/x-wav",
        "video/x-msvideo",
        "video/x-ms-wmv",
        "audio/x-ms-wma",
        "video/x-ms-asf"
    };

    for (size_t i = 0; i < sizeof(playbookMediaTypes)/sizeof(playbookMediaTypes[0]); ++i)
        set.add(String(playbookMediaTypes[i]));
}

// MIME types that we can play but aren't present in s_mimeTypes.
bool MMrendererContext::extraMimeTypesSupported(const char* mimeType)
{
    if (!s_extraSupportedMimeTypes)
        initializeExtraSupportedMimeTypes();

    return s_extraSupportedMimeTypes->contains(String(mimeType));
}

HashSet<String> MMrendererContext::allSupportedMimeTypes()
{
    // For now, build this list once. If and when the system supports a dynamic
    // change to the plug-ins that mm-renderer has access to, this will have
    // to be done dynamically.
    static HashSet<String> cache;
    static bool typeListInitialized = false;

    if (!typeListInitialized) {
        if (s_mimeTypes == 0 && ppsInit() == -1) {
            return cache;
        }
        char *typesList = strdup(strm_string_get(s_mimeTypes));
        if (typesList == 0) {
            Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to get list of MIME types: no memory");
            return cache;
        }

        char *type = typesList;
        char *end = type;
        bool done = false;
        do {
            // Replace ',' characters with 0
            while(*end && *end != ',') {
                ++end;
            }
            if (*end) {
                *end = 0;
            } else {
                done = true;
            }

            // Add the type to the cache.
            cache.add(String(type));

            // Look for the next
            type = ++end;
        } while (! done);

        free(typesList);
        typeListInitialized = true;
    }

    addExtraSupportedMimeTypes(cache);

    return cache;
}

MMrendererContext::MMrendererContext(bool &success)
    : m_user(0)
    , m_ppsPath(0)
    , m_connection(0)
    , m_context(0)
    , m_ppsRendererDir(0)
    , m_ppsAudioDir(0)
    , m_ppsCallback(0)
    , m_inputID(-1)
{
    success = false;

    for (size_t i = 0; i < c_numOutputs; ++i) {
        m_outputIDs[i].m_id = -1;
    }

    if (s_ppsCache == 0 && ppsInit() == -1) {
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to open PPS cache.");
        return;
    }

    // For now, m_ppsPath is null, so use default path.
    m_connection = mmr_connect(m_ppsPath);
    if (m_connection == 0) {
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to connect to renderer: %s (%d).", strerror(errno), errno);
    } else {
        Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Renderer context at PPS path %s opened.",
                    m_ppsPath ? m_ppsPath : "<default>");
        success = true;
    }
}

/**
 * Currently, this will never be called. However, all web pages should use
 * this same context.
 */
MMrendererContext::~MMrendererContext()
{
    // Now, the context is really closed.
    if (m_ppsRendererDir) {
        pps_dir_stop(s_ppsCache, m_ppsRendererDir);
    }
    if (m_ppsAudioDir) {
        pps_dir_stop(s_ppsCache, m_ppsAudioDir);
    }
    // TODO: Destroy or close?
    if (m_context) {
        mmr_context_close(m_context);
    }

    // Get rid of the connections.
    if (m_connection) {
        mmr_disconnect(m_connection);
    }
}

int mmrPpsHandler(const char *name, pps_obj_event_t event, const strm_dict_t *dict, void *renderer)
{
    MMrendererContext *mmrenderer = static_cast<MMrendererContext *>(renderer);

    if (mmrenderer && mmrenderer->m_ppsCallback && mmrenderer->m_user)
        return mmrenderer->m_ppsCallback(name, event, dict, mmrenderer->m_user);

    return 0;
}

int mmrPpsAudioHandler(const char *name, pps_obj_event_t event, const strm_dict_t *dict, void *renderer)
{
    MMrendererContext *mmrenderer = static_cast<MMrendererContext *>(renderer);

    char newName[strlen("audio_") + strlen(name) + 1];
    sprintf(newName, "audio_%s", name);

    if (mmrenderer && mmrenderer->m_ppsCallback && mmrenderer->m_user)
        return mmrenderer->m_ppsCallback(newName, event, dict, mmrenderer->m_user);

    return 0;
}

int MMrendererContext::open(const char *name, pps_dir_func_t ppsHandler, const char *url, const char *userAgent, const char *caPath)
{
    if (m_context) {
        // Always use the same context, but update the handler for PPS.
        m_ppsCallback = ppsHandler;
        return 0;
    }

    // Add the pid to the context to make it unique (in case there's another
    // browser process).
    snprintf(m_contextName, sizeof(m_contextName), "%s-%d", name, getpid());

    // Let this fail and do a create if it does.
    m_context = mmr_context_open(m_connection, m_contextName);
    if (m_context == 0) {
        m_context = mmr_context_create(m_connection, m_contextName, 0, S_IRWXU|S_IRWXG|S_IRWXO);
        if (m_context == 0) {
            Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to create renderer context '%s': %s (%d).",
                      m_contextName, strerror(errno), errno);
            return -1;
        }
        // Set the context parameters since it had to be created.
        strm_dict_t *dict = contextDictMake(url, userAgent, caPath);
        if (dict) {
            if (mmr_context_parameters(m_context, dict) == -1) {
#if (!ERROR_DISABLED)
                const mmr_error_info_t *ei = mmr_error_info(m_context);
                Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to set context parameters on %s: %s.",
                          name, strerror(ei->error_code));
#endif
                mmr_context_close(m_context);
                m_context = 0;
                return -1;
            }
        }
    }

    const char *ppsPath = m_ppsPath ? m_ppsPath : pps_ps_path;
    char       *ps_path;
    ps_path = (char *) alloca(strlen(ppsPath) + strlen(m_contextName) + 2);
    if (ps_path == NULL) {
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to allocate space for playstate context name.");
        return -1;
    }

    // Set the PPS handler before starting the directory.
    m_ppsCallback = ppsHandler;

    // Monitor the renderer's PPS directory.
    sprintf(ps_path, "%s/%s", ppsPath, m_contextName);
    m_ppsRendererDir = pps_dir_start(s_ppsCache, ps_path, mmrPpsHandler, (void *) this, 0);
    if (m_ppsRendererDir == 0) {
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to monitor PPS path \"%s\".", ps_path);
        return -1;
    }
    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Monitoring PPS path \"%s\".", ps_path);

    // Monitor the audio PPS directory.
    m_ppsAudioDir = pps_dir_start(s_ppsCache, PPS_AUDIO_PATH, mmrPpsAudioHandler, (void *) this, 0);
    if (m_ppsAudioDir == 0) {
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Unable to monitor audio PPS path \"%s\".", PPS_AUDIO_PATH);
        return -1;
    }
    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Monitoring audio PPS path \"%s\".", PPS_AUDIO_PATH);

    return 0;
}

int MMrendererContext::close()
{
    // Keep the context, but mark it as not in use.
    m_ppsCallback = 0;

    // Clear the input for the next user.
    inputDetach();

    return 0;
}

int MMrendererContext::outputAttach(const char *url, const char *type)
{
    // Look for an existing output of the same type
    for (size_t i = 0; i < c_numOutputs; ++i) {
        if (m_outputIDs[i].typeMatch(type)) {
            // Make sure the url's match
            if (m_outputIDs[i].urlMatch(url)) {
                Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Returning existing output '%s' with ID %d.",
                            type, m_outputIDs[i].m_id);
                return m_outputIDs[i].m_id;
            } else {
                mmr_output_detach(m_context, m_outputIDs[i].m_id);
                m_outputIDs[i].m_id = -1;
            }
        }
    }

    // If here, didn't have a type of the same kind , or the url changed.
    int outputID = mmr_output_attach(m_context, url, type);
    if (outputID != -1) {
        for (size_t i = 0; i < c_numOutputs; ++i) {
            if (m_outputIDs[i].m_id == -1) {
                m_outputIDs[i].m_id = outputID;
                m_outputIDs[i].typeSet(type);
                m_outputIDs[i].urlSet(url);
                Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Attached output '%s' with ID %d as output %d.",
                            type, outputID, i);
                return outputID;
            }
        }
        // Someone has asked for a type that isn't one of the saved ones; too many.
        Olympia::Platform::log(Olympia::Platform::LogLevelCritical, "Renderer output type of '%s' requested; cannot save.", type);
        mmr_output_detach(m_context, outputID);
    }
    return -1;
}

}

#endif /* ENABLE(VIDEO) */
