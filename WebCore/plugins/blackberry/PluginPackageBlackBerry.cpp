/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */

#include "config.h"
#include "PluginPackage.h"

#include "CString.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "npruntime_impl.h"
#include "PluginDebug.h"
#include <dlfcn.h>
#include <string.h>
#include <errno.h>

extern int errno;

namespace WebCore {

void npnMemFree(void* ptr)
{
    // We could use fastFree here, but there might be plug-ins that mix NPN_MemAlloc/NPN_MemFree with malloc and free,
    // so having them be equivalent seems like a good idea.
    free(ptr);
}

bool PluginPackage::equal(PluginPackage const& a, PluginPackage const& b)
{
    if (a.m_name != b.m_name)
        return false;

    if (a.m_description != b.m_description)
        return false;

    if (a.m_mimeToExtensions.size() != b.m_mimeToExtensions.size())
        return false;

    MIMEToExtensionsMap::const_iterator::Keys end = a.m_mimeToExtensions.end().keys();
    for (MIMEToExtensionsMap::const_iterator::Keys it = a.m_mimeToExtensions.begin().keys(); it != end; ++it) {
        if (!b.m_mimeToExtensions.contains(*it))
            return false;
    }

    return true;
}

bool PluginPackage::fetchInfo()
{
    if (!load())
        return false;

    // TODO:  Find a better way to get around the fetchinfo load.
    // We are fetching the info. Technically we have not loaded the plugin.  PluginView::Init will load the plugin so decrement the counter
    m_loadCount--;

    typedef char *(*NPP_GetMIMEDescriptionProcPtr)();
    NPP_GetMIMEDescriptionProcPtr gm = (NPP_GetMIMEDescriptionProcPtr) dlsym (m_module,"NP_GetMIMEDescription");
    NPP_GetValueProcPtr gv = (NPP_GetValueProcPtr) dlsym (m_module, "NP_GetValue");

    if (!gm || !gv)
        return false;

    char *buf = 0;
    NPError err = gv(0, NPPVpluginNameString, (void *) &buf);
    if (err != NPERR_NO_ERROR)
        return false;

    m_name = buf;

    err = gv(0, NPPVpluginDescriptionString, (void *) &buf);
    if (err != NPERR_NO_ERROR)
        return false;

    m_description = buf;
    determineModuleVersionFromDescription();

    String s = gm();
    Vector<String> types;
    s.split(UChar(';'), false, types);
    for (unsigned int i = 0; i < types.size(); ++i) {
        Vector<String> mime;
        types[i].split(UChar(':'), true, mime);
        if (mime.size() > 0) {
            Vector<String> exts;
            if (mime.size() > 1)
                mime[1].split(UChar(','), false, exts);
            determineQuirks(mime[0]);
            m_mimeToExtensions.add(mime[0], exts);
            if (mime.size() > 2)
                m_mimeToDescriptions.add(mime[0], mime[2]);
        }
    }

    return true;
}

Vector<String> PluginPackage::sitesWithData()
{
    Vector<String> sites;
    tryGetSitesWithData(sites);

    return sites;
}

bool PluginPackage::clearSiteData(const String& site, uint64_t flags, uint64_t maxAge)
{
    bool result = tryClearSiteData(site, flags, maxAge);

    return result;
}

bool PluginPackage::tryGetSitesWithData(Vector<String>& sites)
{
    if (!load())
        return false;

    // TODO:  Find a better way to ensure the plugin is loaded.
    // PluginView::Init will load the plugin so decrement the counter
    m_loadCount--;

    // Check if the plug-in supports NPP_GetSitesWithData.
    if (!m_pluginFuncs.getsiteswithdata)
        return false;

    char** siteArray = m_pluginFuncs.getsiteswithdata();

    // There were no sites with data.
    if (!siteArray)
        return true;

    for (int i = 0; siteArray[i]; ++i) {
        char* site = siteArray[i];

        String siteString = String::fromUTF8(site);
        if (!siteString.isNull())
            sites.append(siteString);

        npnMemFree(site);
    }

    npnMemFree(siteArray);
    return true;
}

bool PluginPackage::tryClearSiteData(const String& site, uint64_t flags, uint64_t maxAge)
{
    if (!load())
        return false;

    // TODO:  Find a better way to ensure the plugin is loaded.
    // PluginView::Init will load the plugin so decrement the counter
    m_loadCount--;

    // Check if the plug-in supports NPP_ClearSiteData.
    if (!m_pluginFuncs.clearsitedata)
        return false;

    CString siteString;
    if (!site.isNull())
        siteString = site.utf8();

    return m_pluginFuncs.clearsitedata(siteString.data(), flags, maxAge) == NPERR_NO_ERROR;
}

unsigned int PluginPackage::hash() const
{
    const unsigned hashCodes[] = {
        m_name.impl()->hash(),
        m_description.impl()->hash(),
        m_mimeToExtensions.size()
    };

    return StringImpl::computeHash(reinterpret_cast<const UChar*>(hashCodes), sizeof(hashCodes) / sizeof(UChar));
}

bool PluginPackage::load()
{
    if (m_isLoaded) {
        m_loadCount++;
        return true;
    }

    if (m_freeLibraryTimer.isActive()) {
        ASSERT(m_module);
        m_freeLibraryTimer.stop();
    } else {
        ASSERT(m_loadCount == 0);
        m_module = dlopen(m_path.utf8().data(), RTLD_LAZY);
    }

    if (!m_module) {
        LOG(Plugins, "%s not loaded (%s)", m_path.utf8().data(), strerror(errno));
        printf("PluginPackage::load() - %s not loaded (%s)", m_path.utf8().data(), strerror(errno));
        return false;
    }

    m_isLoaded = true;

    NP_InitializeFuncPtr NP_Initialize;
    NPError npErr;

    NP_Initialize = (NP_InitializeFuncPtr) dlsym(m_module, "NP_Initialize");
    m_NPP_Shutdown = (NPP_ShutdownProcPtr) dlsym(m_module,"NP_Shutdown");

    if (!NP_Initialize || !m_NPP_Shutdown)
        goto abort;

    memset(&m_pluginFuncs, 0, sizeof(m_pluginFuncs));
    m_pluginFuncs.size = sizeof(m_pluginFuncs);

    initializeBrowserFuncs();

    npErr = NP_Initialize(&m_browserFuncs,  &m_pluginFuncs);

    if (npErr != NPERR_NO_ERROR)
        goto abort;

    m_loadCount = 1;
    return true;

abort:
    unloadWithoutShutdown();
    return false;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
uint16_t PluginPackage::NPVersion() const
{
    return NPVERS_HAS_PLUGIN_THREAD_ASYNC_CALL;
}
#endif

} // namespace WebCore
