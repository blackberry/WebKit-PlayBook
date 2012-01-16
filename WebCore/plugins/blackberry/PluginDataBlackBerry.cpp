/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */

#include "config.h"

#include "PluginData.h"
#include "PluginDatabase.h"
#include "PluginPackage.h"

namespace WebCore {

void PluginData::initPlugins()
{
    PluginDatabase *db = PluginDatabase::installedPlugins();
    const Vector<PluginPackage*> &plugins = db->plugins();

    for (unsigned int i = 0; i < plugins.size(); ++i) {
        PluginPackage* package = plugins[i];

        PluginInfo info;
        info.name = package->name();
        info.file = package->fileName();
        info.desc = package->description();

        const MIMEToDescriptionsMap& mimeToDescriptions = package->mimeToDescriptions();
        MIMEToDescriptionsMap::const_iterator end = mimeToDescriptions.end();
        for (MIMEToDescriptionsMap::const_iterator it = mimeToDescriptions.begin(); it != end; ++it) {
            MimeClassInfo mime;

            mime.type = it->first;
            mime.desc = it->second;
            mime.extensions = package->mimeToExtensions().get(mime.type);

            info.mimes.append(mime);
        }

        m_plugins.append(info);
    }

}

void PluginData::refresh()
{
    PluginDatabase *db = PluginDatabase::installedPlugins();
    db->refresh();
}

} // namespace WebCore
