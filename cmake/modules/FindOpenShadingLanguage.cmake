###########################################################################
# CMake module to find OpenShadingLanguage
#
# This module will set
#   OSL_FOUND          True, if found
#   OSL_INCLUDE_DIR    directory where headers are found
#   OSL_LIBRARIES      libraries for OSL
#   OSL_LIBRARY_DIRS   library dirs for OSL
#   OSL_VERSION        Version ("major.minor.patch")
#   OSL_VERSION_MAJOR  Version major number
#   OSL_VERSION_MINOR  Version minor number
#   OSL_VERSION_PATCH  Version minor patch
#
# Special inputs:
#   OSLHOME - custom "prefix" location of OSL installation
#                      (expecting bin, lib, include subdirectories)
#


# If 'OSLHOME' not set, use the env variable of that name if available
if (NOT OSLHOME AND NOT $ENV{OSLHOME} STREQUAL "")
    set (OSLHOME $ENV{OSLHOME})
endif ()


if (NOT OpenShadingLanguage_FIND_QUIETLY)
    message ( STATUS "OSLHOME = ${OSLHOME}" )
endif ()

find_library ( OSL_EXEC_LIBRARY
               NAMES oslexec
               HINTS ${OSLHOME}/lib
               PATH_SUFFIXES lib64 lib
               PATHS "${OSLHOME}/lib" )
find_library ( OSL_QUERY_LIBRARY
               NAMES oslquery
               HINTS ${OSLHOME}/lib
               PATH_SUFFIXES lib64 lib
               PATHS "${OSLHOME}/lib" )

set ( OSL_LIBRARIES "${OSL_EXEC_LIBRARY};${OSL_QUERY_LIBRARY}" )

find_path ( OSL_INCLUDE_DIR
            NAMES OSL/imageio.h
            HINTS ${OSLHOME}/include
            PATH_SUFFIXES include )

# Try to figure out version number
set (OSL_VERSION_HEADER "${OSL_INCLUDE_DIR}/OSL/oslversion.h")
if (EXISTS "${OSL_VERSION_HEADER}")
    file (STRINGS "${OSL_VERSION_HEADER}" TMP REGEX "^#define OSL_VERSION_MAJOR .*$")
    string (REGEX MATCHALL "[0-9]+" OSL_VERSION_MAJOR ${TMP})
    file (STRINGS "${OSL_VERSION_HEADER}" TMP REGEX "^#define OSL_VERSION_MINOR .*$")
    string (REGEX MATCHALL "[0-9]+" OSL_VERSION_MINOR ${TMP})
    file (STRINGS "${OSL_VERSION_HEADER}" TMP REGEX "^#define OSL_VERSION_PATCH .*$")
    string (REGEX MATCHALL "[0-9]+" OSL_VERSION_PATCH ${TMP})
    set (OSL_VERSION "${OSL_VERSION_MAJOR}.${OSL_VERSION_MINOR}.${OSL_VERSION_PATCH}")
endif ()

get_filename_component (OSL_LIBRARY_DIRS ${OSL_EXEC_LIBRARY} DIRECTORY CACHE)

if (NOT OpenShadingLanguage_FIND_QUIETLY)
    message ( STATUS "OpenShadingLanguage includes     = ${OSL_INCLUDE_DIR}" )
    message ( STATUS "OpenShadingLanguage libraries    = ${OSL_LIBRARIES}" )
    message ( STATUS "OpenShadingLanguage library_dirs = ${OSL_LIBRARY_DIRS}" )
    message ( STATUS "OpenShadingLanguage bin          = ${OSL_BIN}" )
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (OpenShadingLanguage
    FOUND_VAR     OpenShadingLanguage_FOUND
    REQUIRED_VARS OSL_INCLUDE_DIR OSL_LIBRARIES
                  OSL_LIBRARY_DIRS OSL_VERSION
    VERSION_VAR   OSL_VERSION
    )

mark_as_advanced (
    OSL_INCLUDE_DIR
    OSL_LIBRARIES
    OSL_LIBRARY_DIRS
    OSL_VERSION
    OSL_VERSION_MAJOR
    OSL_VERSION_MINOR
    OSL_VERSION_PATCH
    )
