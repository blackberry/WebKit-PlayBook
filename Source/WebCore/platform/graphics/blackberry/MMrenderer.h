/*
 * Copyright (C) 2010 QNX Software Systems.
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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

#ifndef MMrenderer_h
#define MMrenderer_h

#if ENABLE(VIDEO)

#include <mm/renderer.h>
#include <set>
#include <string.h>
#include <string>
#include <sys/ppscache.h>
#include <vector>

namespace WebCore {

class MediaPlayerPrivate;

// Function to make a dictionary of configuration for mm-renderer and libmd
// contexts.
strm_dict_t* contextDictMake(const char* url, const char* userAgent,
                             const char* caPath, const char* cookies);

// This object is an OO wrapper for mm-renderer, with the following behaviourial
// changes:
// 1/ Previous contexts are reused for the object.
// 2/ The PPS monitoring thread is created for a context.
// 3/ The callback for PPS monitoring may be changed without changing the
//    context. This is buried in the open() call, as use of a context is
//    transferred from user to user.
//
// Others to be determined.
class MMrendererContext {
    friend int mmrPpsHandler(const char* name, pps_obj_event_t,
                             const strm_dict_t*, void* renderer);
    friend int mmrPpsAudioHandler(const char* name, pps_obj_event_t,
                             const strm_dict_t*, void* renderer);

public:
    static MMrendererContext* take(MediaPlayerPrivate*,
                                   const char* name,
                                   pps_dir_func_t ppsHandler,
                                   const char* url, const char* userAgent,
                                   const char* caPath, const char* cookies);
    static void release(MMrendererContext*);

    int parametersSet(strm_dict_t* parms) { return mmr_context_parameters(m_context, parms); }

    const mmr_error_info_t* errorInfo() { return mmr_error_info(m_context); }

    int inputAttach(const char* url, const char* type) { return (m_inputID = mmr_input_attach(m_context, url, type)); }
    int inputDetach() { mmr_input_detach(m_context); m_inputID = -1; return 0; }
    int inputParameters(strm_dict_t* parms) { return mmr_input_parameters(m_context, parms); }

    int outputAttach(const char* url, const char* type);
    int outputDetach(unsigned outputId) { return mmr_output_detach(m_context, outputId); }
    int outputParameters(unsigned outputId, strm_dict_t* parms) { return mmr_output_parameters(m_context, outputId, parms); }

    int play() { return mmr_play(m_context); }
    int stop() { return mmr_stop(m_context); }
    // For now, this API uses mm-renderer's API.
    int seek(const char* position) { return mmr_seek(m_context, position); }
    // For now, this API uses mm-renderer's API.
    int speedSet(int speed) { return mmr_speed_set(m_context, speed); }

    // used for fullscreen control
    const char* contextNameGet() const { return m_contextName; }
    mmr_context_t* contextGet() { return m_context; }

    // Tests support for a given MIME type. This has to be static, and PPS
    // has to running, before any renderer contexts are taken since the query
    // from webkit comes before any attempt to play media is made.
    static bool mimeTypeSupported(const char* mimeType);
    static std::set<std::string> allSupportedMimeTypes();

private:
    MMrendererContext(bool& success);
    ~MMrendererContext();

    int open(const char* name, int index, pps_dir_func_t ppsHandler,
             const char* url, const char* userAgent, const char* caPath, const char* cookies);
    int close();

    // Vector of renderer contexts. We reuse previously created contexts
    // as much as possible for performance reasons, i.e. there is a fair
    // bit of overhead in creating a new renderer context so we don't
    // do it unless we run out of contexts.
    static std::vector<MMrendererContext*> s_instances;
    static unsigned s_useCount;

    MediaPlayerPrivate* m_user;
    char* m_ppsPath;
    char m_contextName[64];
    mmr_connection_t* m_connection;
    mmr_context_t* m_context;
    pps_dir_t* m_ppsRendererDir;
    pps_dir_t* m_ppsAudioDir;
    pps_dir_func_t m_ppsCallback;

    class Output {
    public:
        Output() : m_id(-1) { typeSet(""); urlSet(""); }
        ~Output() { free(m_url); }

        void typeSet(const char* type)
        {
            strncpy(m_type, type, sizeof(m_type));
            m_type[sizeof(m_type)-1] = 0;
        }

        bool typeMatch(const char* type) const { return !strncmp(m_type, type, sizeof(m_type)); }
        void urlSet(const char* url) { m_url = strdup(url); }
        bool urlMatch(const char* url) const { return !strcmp(m_url, url); }

        int m_id;
        char m_type[64];
        char* m_url;
    };

    // Assume one audio and one video.
    static const size_t s_numOutputs = 2;
    Output m_outputIDs[s_numOutputs];
    int m_inputID;

    static int ppsInit();

    // These PPS items are shared across all connection contexts.
    static pps_cache_t* s_ppsCache;
    static pps_obj_t* s_mmfRouteObject;
    static strm_string_t* s_mimeTypes;

    static std::set<std::string>* s_extraSupportedMimeTypes;
    static void initializeExtraSupportedMimeTypes();
    static void addExtraSupportedMimeTypes(std::set<std::string>&);
    static bool extraMimeTypesSupported(const char* mimeType);
};

}

#endif // ENABLE(VIDEO)

#endif // MMrenderer_h
