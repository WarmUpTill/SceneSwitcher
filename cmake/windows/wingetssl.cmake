# ---------------------------------------------------------------------------
# Detects OpenSSL installed via winget or other common Windows locations,
# without requiring the user to manually set OPENSSL_ROOT_DIR.
# ---------------------------------------------------------------------------

if(WIN32 AND (NOT OpenSSL_FOUND))
  set(_openssl_roots
      "$ENV{ProgramFiles}/OpenSSL-Win64" "$ENV{ProgramFiles}/OpenSSL"
      "$ENV{ProgramW6432}/OpenSSL-Win64")

  set(_openssl_lib_suffixes "lib/VC/x64/MD" "lib/VC/x64/MDd" "lib/VC/x64/MT"
                            "lib/VC/x64/MTd" "lib")

  # Determine which configuration we're building
  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(_is_debug TRUE)
  else()
    set(_is_debug FALSE)
  endif()

  # Determine which runtime we use Default to /MD (shared CRT)
  set(_crt_kind "MD")
  if(MSVC)
    if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultiThreaded")
      if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "Debug")
        set(_crt_kind "MTd")
      else()
        set(_crt_kind "MT")
      endif()
    else()
      if(_is_debug)
        set(_crt_kind "MDd")
      else()
        set(_crt_kind "MD")
      endif()
    endif()
  endif()

  message(STATUS "Looking for OpenSSL built with CRT variant: ${_crt_kind}")

  # Try to find the root and corresponding lib path
  foreach(_root ${_openssl_roots})
    if(EXISTS "${_root}/include/openssl/ssl.h")
      foreach(_suffix ${_openssl_lib_suffixes})
        if(_suffix MATCHES "${_crt_kind}$"
           AND EXISTS "${_root}/${_suffix}/libcrypto.lib")
          set(OPENSSL_ROOT_DIR
              "${_root}"
              CACHE PATH "Path to OpenSSL root" FORCE)
          set(OPENSSL_CRYPTO_LIBRARY
              "${_root}/${_suffix}/libcrypto.lib"
              CACHE FILEPATH "OpenSSL crypto lib" FORCE)
          set(OPENSSL_SSL_LIBRARY
              "${_root}/${_suffix}/libssl.lib"
              CACHE FILEPATH "OpenSSL ssl lib" FORCE)
          set(OPENSSL_INCLUDE_DIR
              "${_root}/include"
              CACHE PATH "OpenSSL include dir" FORCE)
          set(OpenSSL_FOUND
              TRUE
              CACHE BOOL "Whether OpenSSL was found" FORCE)
          message(STATUS "Found OpenSSL at: ${_root}/${_suffix}")
          # Recursively list all files under OPENSSL_ROOT_DIR
          file(GLOB_RECURSE OPENSSL_FILES "${OPENSSL_ROOT_DIR}/*")

          message(STATUS "Listing contents of OPENSSL_ROOT_DIR:")
          foreach(file ${OPENSSL_FILES})
            message(STATUS "  ${file}")
          endforeach()

          get_cmake_property(_variableNames VARIABLES)
          list(SORT _variableNames)
          foreach(_variableName ${_variableNames})
            message(STATUS "${_variableName}=${${_variableName}}")
          endforeach()
          return()
        endif()
      endforeach()
    endif()
    if(OpenSSL_FOUND)
      break()
    endif()
  endforeach()

  if(NOT OpenSSL_FOUND)
    message(WARNING "Could not auto-detect OpenSSL under Program Files. "
                    "Might have to set OPENSSL_ROOT_DIR manually.")
  endif()

endif()
