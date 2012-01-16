# - Try to find OSNDK
# Once done, this will define
#
#  OSNDK_FOUND
#  OSNDK_INCLUDE_DIRS
#  There are no OSNDK libraries worth finding.

include(LibFindMacros)

# Include directories
find_path(OSNDK_INCLUDE_DIR
    NAMES osndk.h
    HINTS $ENV{OSNDKDIR} 
    PATH_SUFFIXES inc
)

find_path(OSNDK_INCLUDE_GRAPHICS_DIR
    NAMES egl.h
    HINTS $ENV{OSNDKDIR}
    PATH_SUFFIXES inc/graphics
)

find_path(OSNDK_INCLUDE_INTERNAL_DIR
    NAMES basetype.h
    HINTS $ENV{OSNDKDIR}
    PATH_SUFFIXES inc/internal
)

# Set the include dir variable and let libfind_process do the rest.
set(OSNDK_PROCESS_INCLUDES 
    OSNDK_INCLUDE_DIR 
    OSNDK_INCLUDE_GRAPHICS_DIR 
    OSNDK_INCLUDE_INTERNAL_DIR
)
libfind_process(OSNDK)
