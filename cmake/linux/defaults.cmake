# CMake Linux defaults module

# cmake-format: off
# cmake-lint: disable=C0103
# cmake-lint: disable=C0111
# cmake-format: on

include_guard(GLOBAL)

include(GNUInstallDirs)

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}"
)

set(CPACK_GENERATOR
    "DEB"
    CACHE STRING "CPack generator to use")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PLUGIN_EMAIL}")
set(CPACK_SET_DESTDIR ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25.0 OR NOT CMAKE_CROSSCOMPILING)
  set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
endif()

set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/release")

set(CPACK_SOURCE_GENERATOR "TXZ")
set(CPACK_SOURCE_IGNORE_FILES
    # cmake-format: sortable
    ".*~$"
    \\.git/
    \\.github/
    \\.gitignore
    build_.*
    cmake/\\.CMakeBuildNumber
    release/)

set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_SOURCE_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-source")
set(CPACK_ARCHIVE_THREADS 0)

include(CPack)
# CPack.cmake has a side effect that leaves binary packaging variables in the
# state intended for source packaging when it finishes. Specifically it sets
# CPACK_GENERATOR to CPACK_SOURCE_GENERATOR ("TXZ"), sets
# CPACK_INSTALLED_DIRECTORIES to the source root (which would cause CPackDeb to
# scan the entire source tree, including ccache object files that lack a Build
# ID), and clears CPACK_INSTALL_CMAKE_PROJECTS (preventing the cmake install
# mechanism from being used). In some cmake versions these overrides happen
# before CPackConfig.cmake is written. Reset all three so that binary DEB
# packaging works correctly regardless of cmake version.
set(CPACK_GENERATOR "DEB")
set(CPACK_INSTALLED_DIRECTORIES "")
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};ALL;/")

find_package(libobs QUIET)

if(NOT TARGET OBS::libobs)
  find_package(LibObs REQUIRED)
  add_library(OBS::libobs ALIAS libobs)

  if(ENABLE_FRONTEND_API)
    find_path(
      obs-frontend-api_INCLUDE_DIR
      NAMES obs-frontend-api.h
      PATHS /usr/include /usr/local/include
      PATH_SUFFIXES obs)

    find_library(
      obs-frontend-api_LIBRARY
      NAMES obs-frontend-api
      PATHS /usr/lib /usr/local/lib)

    if(obs-frontend-api_LIBRARY)
      if(NOT TARGET OBS::obs-frontend-api)
        if(IS_ABSOLUTE "${obs-frontend-api_LIBRARY}")
          add_library(OBS::obs-frontend-api UNKNOWN IMPORTED)
          set_property(TARGET OBS::obs-frontend-api
                       PROPERTY IMPORTED_LOCATION "${obs-frontend-api_LIBRARY}")
        else()
          add_library(OBS::obs-frontend-api INTERFACE IMPORTED)
          set_property(TARGET OBS::obs-frontend-api
                       PROPERTY IMPORTED_LIBNAME "${obs-frontend-api_LIBRARY}")
        endif()

        set_target_properties(
          OBS::obs-frontend-api PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                           "${obs-frontend-api_INCLUDE_DIR}")
      endif()
    endif()
  endif()

  macro(find_package)
    if(NOT "${ARGV0}" STREQUAL libobs AND NOT "${ARGV0}" STREQUAL
                                          obs-frontend-api)
      _find_package(${ARGV})
    endif()
  endmacro()
endif()
