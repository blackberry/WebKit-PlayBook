# - Try to find BBNSL
# Once done, this will define
#
#  BBNSL_FOUND
#  BBNSL_INCLUDE_DIRS
#  There are no BBNSL libraries worth finding.

include(LibFindMacros)

# Include directories
find_path(BBNSL_INCLUDE_DIR
    NAMES bridge/Bridge.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include
)

find_path(BBNSL_INCLUDE_JPEG_DIR
    NAMES jpeglib.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/jpeg
)

find_path(BBNSL_INCLUDE_LIBPNG_DIR
    NAMES png.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/libpng
)

find_path(BBNSL_INCLUDE_LIBXML2_DIR
    NAMES libxml/xmlversion.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/libxml2
)

find_path(BBNSL_INCLUDE_LIBXSLT_DIR
    NAMES libxslt/libxslt.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/libxslt
)

find_path(BBNSL_INCLUDE_TID_DIR
    NAMES text_api.h TranscoderRegistry.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/tid
)

find_path(BBNSL_INCLUDE_ZLIB_DIR
    NAMES zlib.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/zlib
)

find_path(BBNSL_INCLUDE_WEBKITPLATFORM_DIR
    NAMES OlympiaPlatformClient.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/webkitplatform
)

find_path(BBNSL_INCLUDE_POSIX_DIR
    NAMES assert.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/posix
)

find_path(BBNSL_INCLUDE_SQLITE_DIR
    NAMES sqlite3.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/sqlite
)

find_path(BBNSL_INCLUDE_SLIPSTREAM_DIR
    NAMES MemoryInterface.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/slipstream/libimage/public/cpp/include/common
)

find_path(BBNSL_INCLUDE_SLIPSTREAM_IMAGE_DIR
    NAMES rimrxi_api.h
    HINTS $ENV{BBNSLINSTALLPREFIX} 
    PATH_SUFFIXES include/slipstream/libimage/public/cpp/include/rxi
)

# Set the include dir variable and let libfind_process do the rest.
set(BBNSL_PROCESS_INCLUDES 
    BBNSL_INCLUDE_DIR
    BBNSL_INCLUDE_JPEG_DIR
    BBNSL_INCLUDE_LIBPNG_DIR
    BBNSL_INCLUDE_LIBXML2_DIR
    BBNSL_INCLUDE_LIBXSLT_DIR
    BBNSL_INCLUDE_TID_DIR
    BBNSL_INCLUDE_ZLIB_DIR
    BBNSL_INCLUDE_WEBKITPLATFORM_DIR
    BBNSL_INCLUDE_POSIX_DIR
    BBNSL_INCLUDE_SQLITE_DIR
    BBNSL_INCLUDE_SLIPSTREAM_DIR
    BBNSL_INCLUDE_SLIPSTREAM_IMAGE_DIR
)
libfind_process(BBNSL)
