/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 */

#include "config.h"
#include "FileSystem.h"

#include "CString.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "Vector.h"

#include <algorithm>
#if OS(QNX)
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>
#endif

#include <BlackBerryPlatformClient.h>

namespace WebCore {

String homeDirectoryPath()
{
    return Olympia::Platform::Client::get()->getApplicationDataDirectory().c_str();
}

#if OS(BLACKBERRY)
bool deleteEmptyDirectory(String const& path)
{
    return Olympia::Platform::deleteEmptyDirectory(path.latin1().data());
}

bool deleteFile(String const& path)
{
    return Olympia::Platform::deleteFile(path.latin1().data());
}

bool fileExists(String const& path)
{
    return Olympia::Platform::fileExists(path.latin1().data());
}

bool getFileModificationTime(const String & path, time_t& result)
{
    return Olympia::Platform::getFileModificationTime(path.latin1().data(), result);
}

bool getFileSize(String const& path, long long& result)
{
    return Olympia::Platform::getFileSize(path.latin1().data(), result);
}

String pathGetFileName(String const& path)
{
    return path.substring(path.reverseFind('/') + 1);
}

String directoryName(String const& path)
{
    String dirName = path;
    dirName.truncate(dirName.length() - pathGetFileName(path).length());
    return dirName;
}

bool unloadModule(void*)
{
    notImplemented();
    return false;
}

int writeToFile(PlatformFileHandle handle, char const* data, int length)
{
    if (isHandleValid(handle))
        return Olympia::Platform::FileWrite((Olympia::Platform::FileHandle)handle, data, length);

    return 0;
}

void closeFile(PlatformFileHandle& handle)
{
    if (isHandleValid(handle))
        Olympia::Platform::FileClose((Olympia::Platform::FileHandle)handle);
}

CString openTemporaryFile(char const* prefix, PlatformFileHandle& handle)
{
    std::string tempfile = Olympia::Platform::openTemporaryFile(prefix, (Olympia::Platform::FileHandle&)handle);
    return tempfile.c_str();
}

String pathByAppendingComponent(const String& path, const String& component)
{
    if (component.isEmpty())
        return path;

    Vector<UChar, Olympia::Platform::FileNameMaxLength> buffer;

    buffer.append(path.characters(), path.length());

    if (buffer.last() != L'/' && component[0] != L'/')
        buffer.append(L'/');

    buffer.append(component.characters(), component.length());

    return String(buffer.data(), buffer.size());
}

bool makeAllDirectories(const String& path)
{
    int lastDivPos = path.reverseFind('/');
    int endPos = path.length();
    if (lastDivPos == path.length() - 1) {
        endPos -= 1;
        lastDivPos = path.reverseFind('/', lastDivPos);
    }

    if ((lastDivPos > 0) && !makeAllDirectories(path.substring(0, lastDivPos)))
        return false;

    String folder(path.substring(0, endPos));
    return Olympia::Platform::createDirectory(folder.latin1().data());
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;

    std::vector<std::string> lists;
    if (!Olympia::Platform::listDirectory(path.latin1().data(), filter.latin1().data(), lists))
        return entries;

    for (int i = 0; i < lists.size(); ++i)
        entries.append(lists.at(i).c_str());

    return entries;
}

#elif OS(QNX)
Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;
    CString cpath = path.utf8();
    CString cfilter = filter.utf8();
    char filePath[PATH_MAX];
    char* fileName;
    size_t fileNameSpace;
    DIR* dir;

    if (cpath.length() + NAME_MAX >= sizeof(filePath))
        return entries;
    // loop invariant: directory part + '/'
    memcpy(filePath, cpath.data(), cpath.length());
    fileName = filePath + cpath.length();
    if (cpath.length() > 0 && filePath[cpath.length() - 1] != '/') {
        fileName[0] = '/';
        fileName++;
    }
    fileNameSpace = sizeof(filePath) - (fileName - filePath) - 1;

    dir = opendir(cpath.data());
    if (!dir)
        return entries;

    struct dirent* de;
    while (de = readdir(dir)) {
        size_t nameLen;
        if (de->d_name[0] == '.') {
            if (de->d_name[1] == '\0')
                continue;
            if (de->d_name[1] == '.' && de->d_name[2] == '\0')
                continue;
        }
        if (fnmatch(cfilter.data(), de->d_name, 0))
            continue;

        nameLen = strlen(de->d_name);
        if (nameLen >= fileNameSpace)
            continue; // maybe assert? it should never happen anyway...

        memcpy(fileName, de->d_name, nameLen + 1);
        entries.append(filePath);
    }
    closedir(dir);
    return entries;
}

CString fileSystemRepresentation(const String& path)
{
    return path.utf8();
}

bool unloadModule(PlatformModule)
{
    return false;
}

CString openTemporaryFile(const char* prefix, PlatformFileHandle&)
{
    return "";
}
#endif

} // namespace WebCore
