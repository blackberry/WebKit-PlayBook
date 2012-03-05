/*
 * Copyright (C) 2010 QNX Software Systems.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// FIXME: THIS FILE SHOULD BE REPLACED BY MMRContext.h WHEN REFACTOR COMPLETE.

#include "config.h"

#if ENABLE(VIDEO)
#include "MMrenderer.h"

#include "MediaPlayerPrivateMMrenderer.h"
#include <BlackBerryPlatformClient.h>
#include <BlackBerryPlatformLog.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

using namespace std;

namespace WebCore {

// FIXME: Determine if this is variable, and if so, what it is each time and build it up.
#define PPS_ROOT_PATH "/pps/services/multimedia/renderer/"

// Location of audio status directory in PPS.
#define PPS_AUDIO_PATH "/pps/services/audio/"

// Location of renderer context directories in PPS.
static const char ppsPsPath[] = PPS_ROOT_PATH"context";

// The mime attribute of this object has a comma separated list of the MIME
// types supported by mm-renderer.
//
// FIXME: Update the way we read PPS objects here. We should not need to specify the exact filesystem location
// of the file we want to read that contains the "mime" object, which contains the supported MIME types.
// See PR 92803.
static const char ppsMMFRoutePath[] = PPS_ROOT_PATH"component/mmr-mmf-routing";

static const char ppsMIMEObjectName[] = "mime";

vector<MMrendererContext*> MMrendererContext::s_instances;

//  This is null until someone calls initializeExtraSupportedMimeTypes().
set<string>* MMrendererContext::s_extraSupportedMimeTypes = 0;

// This function is used to make a dictionary for the context.
strm_dict_t* contextDictMake(const char* url, const char* userAgent, const char* caPath, const char* cookies)
{
    strm_dict_t* dict = strm_dict_new();
    strm_dict_t* tmp;

    if (!dict)
        return dict;

    if (url && cookies) {
        tmp = strm_dict_set(dict, "OPT_COOKIE", cookies); // CURLOPT_COOKIE
        if (tmp)
            dict = tmp;
    }

    // Note that if a user agent is not specified here, http_streamer.so
    // will use the contents of env var MM_HTTP_USERAGENT, and if that is
    // also not set it simply uses the hard coded "io-media". In other words
    // this is mandatory.
    if (userAgent && *userAgent) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Passing OPT_USERAGENT \"%s\".", userAgent);
        tmp = strm_dict_set(dict, "OPT_USERAGENT", userAgent); // CURLOPT_USERAGENT
        if (tmp)
            dict = tmp;
    }

    if (caPath && *caPath) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Using CAPATH directory \"%s\".", caPath);
        tmp = strm_dict_set(dict, "OPT_CAPATH", caPath); // CURLOPT_CAPATH
        if (tmp)
            dict = tmp;
    }
    if (!strncasecmp(url, "https:", 6)) {
        // The http_streamer.so defaults these both to zero. The libcurl
        // defaults are 1 and 2 respectively. I am using values that are
        // a simplification of the code in CURLHandle::setRequest().
        tmp = strm_dict_set(dict, "OPT_SSL_VERIFYPEER", "1"); // CURLOPT_SSL_VERIFYPEER
        if (tmp)
            dict = tmp;
        tmp = strm_dict_set(dict, "OPT_SSL_VERIFYHOST", "1"); // CURLOPT_SSL_VERIFYHOST
        if (tmp)
            dict = tmp;
    }

    if (getenv("WK_CURL_DEBUG")) {
        tmp = strm_dict_set(dict, "OPT_VERBOSE", "1"); // CURLOPT_VERBOSE
        if (tmp)
            dict = tmp;
    }

    string proxyAddress = BlackBerry::Platform::Client::get()->getProxyAddress();
    string proxyPort = BlackBerry::Platform::Client::get()->getProxyPort();
    if (proxyAddress.size() && proxyPort.size()) {
        tmp = strm_dict_set(dict, "OPT_PROXY", proxyAddress.c_str()); // CURLOPT_PROXY
        if (tmp)
            dict = tmp;

        // FIXME: Plumb CURLOPT_NOPROXY from a single location. Until then,
        // ensure that this matches corresponding line in blackberryplatform/network/CurlHandle.cpp
        tmp = strm_dict_set(dict, "OPT_NOPROXY", "127.0.0.1,localhost"); // CURLOPT_NOPROXY
        if (tmp)
            dict = tmp;

        tmp = strm_dict_set(dict, "OPT_PROXYPORT", proxyPort.c_str()); // CURLOPT_PROXYPORT
        if (tmp)
            dict = tmp;

        tmp = strm_dict_set(dict, "OPT_PROXYTYPE", "0"); // CURLOPT_PROXYTYPE
        if (tmp)
            dict = tmp;

        string proxyUsername = BlackBerry::Platform::Client::get()->getProxyUsername();
        string proxyPassword = BlackBerry::Platform::Client::get()->getProxyPassword();

        if (proxyUsername.size() && proxyPassword.size()) {
            tmp = strm_dict_set(dict, "OPT_PROXYUSERNAME", proxyUsername.c_str()); // CURLOPT_PROXYUSERNAME
            if (tmp)
                dict = tmp;

            tmp = strm_dict_set(dict, "OPT_PROXYPASSWORD", proxyPassword.c_str()); // CURLOPT_PROXYPASSWORD
            if (tmp)
                dict = tmp;
        }
    } else {
        tmp = strm_dict_set(dict, "OPT_NOPROXY", "1"); // CURLOPT_NOPROXY
        if (tmp)
            dict = tmp;
    }
    return dict;
}

MMrendererContext* MMrendererContext::take(MediaPlayerPrivate* mp, const char* name, pps_dir_func_t ppsHandler,
                                                              const char* url, const char* userAgent, const char* caPath, const char* cookies)
{
    // Check if we can reuse an existing renderer context
    int reuseIndex = -1;
    for (unsigned int i = 0; i < s_instances.size(); ++i) {
        if (!s_instances[i]->m_user) {
            reuseIndex = i;
            break; // for i
        }
    }

    // Don't bother checking the name is the same.
    if (reuseIndex == -1) {
        bool success;
        MMrendererContext* instance = new MMrendererContext(success);
        if (!success) {
            // already logged.
            delete instance;
            return 0;
        }
        s_instances.push_back(instance);
        reuseIndex = s_instances.size() - 1;
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Connected to renderer index %d at %s.", reuseIndex, "<default>");
    }
    s_instances[reuseIndex]->m_user = mp;
    if (s_instances[reuseIndex]->open(name, reuseIndex, ppsHandler, url, userAgent, caPath, cookies) == -1) {
        // already logged.
        s_instances[reuseIndex]->m_user = 0;
        return 0;
    }

    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Renderer at %s taken.", "<default>");
    return s_instances[reuseIndex];
}

void MMrendererContext::release(MMrendererContext* renderer)
{
    for (unsigned int i = 0; i < s_instances.size(); i++) {
        if (renderer == s_instances[i]) {
            s_instances[i]->close();
            s_instances[i]->m_user = 0;
            break; // for i
        }
    }
}

pps_cache_t* MMrendererContext::s_ppsCache = 0;
pps_obj_t* MMrendererContext::s_mmfRouteObject = 0;
strm_string_t* MMrendererContext::s_mimeTypes = 0;

int MMrendererContext::ppsInit()
{
    s_ppsCache = pps_cache_init(8); // FIXME: Provide some value that we know works all the time.
    if (!s_ppsCache) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to initialize PPS cache.");
        return -1;
    }

    // Monitor the list of MIME types supported.
    s_mmfRouteObject = pps_cache_object_start(s_ppsCache, ppsMMFRoutePath);
    if (!s_mmfRouteObject) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to monitor PPS object \"%s\".", ppsMMFRoutePath);
        pps_cache_terminate(s_ppsCache);
        s_ppsCache = 0;
        return -1;
    }

    // This is the mime types attribute; PPS cache will keep it up to date.
    s_mimeTypes = pps_cache_object_attr_get(s_ppsCache, s_mmfRouteObject, ppsMIMEObjectName);
    if (!s_mimeTypes) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to get mime types attribute from PPS.");
        pps_cache_object_stop(s_ppsCache, s_mmfRouteObject);
        s_mmfRouteObject = 0;
        pps_cache_terminate(s_ppsCache);
        s_ppsCache = 0;
        return -1;
    }
    return 0;
}

bool MMrendererContext::mimeTypeSupported(const char* mimeType)
{
    if (!s_mimeTypes && ppsInit() == -1)
        return false;
    return strstr(strm_string_get(s_mimeTypes), mimeType) || extraMimeTypesSupported(mimeType);
}

void MMrendererContext::initializeExtraSupportedMimeTypes()
{
    ASSERT(!s_extraSupportedMimeTypes);
    s_extraSupportedMimeTypes = new set<string>;
    addExtraSupportedMimeTypes(*s_extraSupportedMimeTypes);
}

void MMrendererContext::addExtraSupportedMimeTypes(set<string>& set)
{
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
        "video/x-ms-asf",
        "application/vnd.apple.mpegurl",
        "application/vnd.apple.x-mpegurl",
        "application/mpegurl",
        "application/x-mpegurl",
        "audio/mpegurl",
        "audio/x-mpegurl",
        "video/x-matroska",
        "audio/amr"
    };

    size_t numberOfMediaTypes = sizeof(playbookMediaTypes) / sizeof(playbookMediaTypes[0]);
    for (size_t i = 0; i < numberOfMediaTypes; ++i)
        set.insert(string(playbookMediaTypes[i]));
}

// MIME types that we can play but aren't present in s_mimeTypes.
bool MMrendererContext::extraMimeTypesSupported(const char* mimeType)
{
    if (!s_extraSupportedMimeTypes)
        initializeExtraSupportedMimeTypes();
    return s_extraSupportedMimeTypes->find(string(mimeType)) != s_extraSupportedMimeTypes->end();
}

set<string> MMrendererContext::allSupportedMimeTypes()
{
    // For now, build this list once. If and when the system supports a dynamic
    // change to the plug-ins that mm-renderer has access to, this will have
    // to be done dynamically.
    static set<string> cache;
    static bool typeListInitialized = false;

    if (!typeListInitialized) {
        if (!s_mimeTypes && ppsInit() == -1)
            return cache;
        char* typesList = strdup(strm_string_get(s_mimeTypes));
        if (!typesList) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to get list of MIME types: no memory");
            return cache;
        }

        char* type = typesList;
        char* end = type;
        bool done = false;
        do {
            // Replace ',' characters with 0
            while (*end && *end != ',')
                ++end;
            if (*end)
                *end = 0;
            else
                done = true;

            // Add the type to the cache.
            cache.insert(string(type));

            // Look for the next
            type = ++end;
        } while (!done);
        free(typesList);
        typeListInitialized = true;
    }

    addExtraSupportedMimeTypes(cache);

    return cache;
}

MMrendererContext::MMrendererContext(bool& success)
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

    for (size_t i = 0; i < s_numOutputs; ++i)
        m_outputIDs[i].m_id = -1;

    if (!s_ppsCache && ppsInit() == -1) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to open PPS cache.");
        return;
    }

    // For now, m_ppsPath is null, so use default path.
    m_connection = mmr_connect(m_ppsPath);
    if (!m_connection)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to connect to renderer: %s (%d).", strerror(errno), errno);
    else {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Renderer context at PPS path %s opened.", m_ppsPath ? m_ppsPath : "<default>");
        success = true;
    }
}

// Currently, this will never be called. However, all web pages will try to
// reuse previously created contexts.
MMrendererContext::~MMrendererContext()
{
    // Now, the context is really closed.
    if (m_ppsRendererDir)
        pps_dir_stop(s_ppsCache, m_ppsRendererDir);

    if (m_ppsAudioDir)
        pps_dir_stop(s_ppsCache, m_ppsAudioDir);

    // FIXME: Destroy or close?
    if (m_context)
        mmr_context_close(m_context);

    if (m_connection)
        mmr_disconnect(m_connection);
}

int mmrPpsHandler(const char* name, pps_obj_event_t event, const strm_dict_t* dict, void* renderer)
{
    MMrendererContext* mmrenderer = static_cast<MMrendererContext*>(renderer);
    if (mmrenderer && mmrenderer->m_ppsCallback && mmrenderer->m_user)
        return mmrenderer->m_ppsCallback(name, event, dict, mmrenderer->m_user);
    return 0;
}

int mmrPpsAudioHandler(const char* name, pps_obj_event_t event, const strm_dict_t* dict, void* renderer)
{
    MMrendererContext* mmrenderer = static_cast<MMrendererContext*>(renderer);

    char newName[strlen("audio_") + strlen(name) + 1];
    sprintf(newName, "audio_%s", name);

    if (mmrenderer && mmrenderer->m_ppsCallback && mmrenderer->m_user)
        return mmrenderer->m_ppsCallback(newName, event, dict, mmrenderer->m_user);
    return 0;
}

int MMrendererContext::open(const char* name, int index, pps_dir_func_t ppsHandler,
        const char* url, const char* userAgent, const char* caPath, const char* cookies)
{
    if (m_context) {
        // Always reuse previously created contexts, but update the handler for PPS.
        m_ppsCallback = ppsHandler;
        return 0;
    }

    // Add the pid to the context to make it unique (in case there's another
    // browser process) and the index (to allow concurrent contexts).
    snprintf(m_contextName, sizeof(m_contextName), "%s-%d-%d", name, getpid(), index);

    // Let this fail and do a create if it does.
    m_context = mmr_context_open(m_connection, m_contextName);
    if (!m_context) {
        m_context = mmr_context_create(m_connection, m_contextName, 0, S_IRWXU | S_IRWXG | S_IRWXO);
        if (!m_context) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to create renderer context '%s': %s (%d).",
                      m_contextName, strerror(errno), errno);
            return -1;
        }
        // Set the context parameters since it had to be created.
        strm_dict_t* dict = contextDictMake(url, userAgent, caPath, cookies);
        if (dict) {
            if (mmr_context_parameters(m_context, dict) == -1) {
#if (!ERROR_DISABLED)
                const mmr_error_info_t* ei = mmr_error_info(m_context);
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to set context parameters on %s: %s.", name, strerror(ei->error_code));
#endif
                mmr_context_close(m_context);
                m_context = 0;
                return -1;
            }
        }
    }

    const char* ppsPath = m_ppsPath ? m_ppsPath : ppsPsPath;
    char* psPath;
    psPath = (char*)alloca(strlen(ppsPath) + strlen(m_contextName) + 2);
    if (!psPath) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to allocate space for playstate context name.");
        return -1;
    }

    // Set the PPS handler before starting the directory.
    m_ppsCallback = ppsHandler;

    // Monitor the renderer's PPS directory.
    sprintf(psPath, "%s/%s", ppsPath, m_contextName);
    m_ppsRendererDir = pps_dir_start(s_ppsCache, psPath, mmrPpsHandler, (void*)this, 0);
    if (!m_ppsRendererDir) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to monitor PPS path \"%s\".", psPath);
        return -1;
    }
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Monitoring PPS path \"%s\".", psPath);

    // Monitor the audio PPS directory.
    m_ppsAudioDir = pps_dir_start(s_ppsCache, PPS_AUDIO_PATH, mmrPpsAudioHandler, (void*)this, 0);
    if (!m_ppsAudioDir) {
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Unable to monitor audio PPS path \"%s\".", PPS_AUDIO_PATH);
        return -1;
    }
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Monitoring audio PPS path \"%s\".", PPS_AUDIO_PATH);
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

int MMrendererContext::outputAttach(const char* url, const char* type)
{
    // Look for an existing output of the same type
    for (size_t i = 0; i < s_numOutputs; ++i) {
        if (m_outputIDs[i].typeMatch(type)) {
            // Make sure the url's match
            if (m_outputIDs[i].urlMatch(url)) {
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Returning existing output '%s' with ID %d.", type, m_outputIDs[i].m_id);
                return m_outputIDs[i].m_id;
            }
            mmr_output_detach(m_context, m_outputIDs[i].m_id);
            m_outputIDs[i].m_id = -1;
        }
    }

    // If here, didn't have a type of the same kind , or the url changed.
    int outputID = mmr_output_attach(m_context, url, type);
    if (outputID != -1) {
        for (size_t i = 0; i < s_numOutputs; ++i) {
            if (m_outputIDs[i].m_id == -1) {
                m_outputIDs[i].m_id = outputID;
                m_outputIDs[i].typeSet(type);
                m_outputIDs[i].urlSet(url);
                BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Attached output '%s' with ID %d as output %d.",
                            type, outputID, i);
                return outputID;
            }
        }
        // Someone has asked for a type that isn't one of the saved ones; too many.
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "Renderer output type of '%s' requested; cannot save.", type);
        mmr_output_detach(m_context, outputID);
    }
    return -1;
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
