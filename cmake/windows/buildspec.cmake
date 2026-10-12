# CMake Windows build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

# _check_dependencies_windows: Set up Windows slice for _check_dependencies
function(_check_dependencies_windows)
  if(CMAKE_GENERATOR_PLATFORM)
    set(arch ${CMAKE_GENERATOR_PLATFORM})
  else()
    set(arch x64)
  endif()
  set(platform windows-${arch})

  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  set(prebuilt_filename "windows-deps-VERSION-ARCH-REVISION.zip")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "windows-deps-qt6-VERSION-ARCH-REVISION.zip")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  set(obs-studio_filename "VERSION.zip")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()

  if(arch STREQUAL "ARM64")
    _setup_qt6_host_windows()
  endif()
endfunction()

# _setup_qt6_host_windows: Fetch native x64 Qt6 as QT_HOST_PATH for ARM64 build
# tools
function(_setup_qt6_host_windows)
  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")

  if(NOT buildspec)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" buildspec)
  endif()

  string(JSON data GET ${buildspec} dependencies qt6)
  string(JSON version GET ${data} version)
  string(JSON hash GET ${data} hashes windows-x64)
  string(JSON base_url GET ${data} baseUrl)
  string(JSON label GET ${data} label)
  string(
    JSON
    revision
    ERROR_VARIABLE
    error
    GET
    ${data}
    revision
    windows-x64)

  set(file "windows-deps-qt6-VERSION-x64-REVISION.zip")
  set(destination "obs-deps-qt6-VERSION-x64")
  string(REPLACE "VERSION" "${version}" file "${file}")
  string(REPLACE "VERSION" "${version}" destination "${destination}")
  if(revision)
    string(REPLACE "_REVISION" "_v${revision}" file "${file}")
    string(REPLACE "-REVISION" "-v${revision}" file "${file}")
  else()
    string(REPLACE "_REVISION" "" file "${file}")
    string(REPLACE "-REVISION" "" file "${file}")
  endif()

  set(url "${base_url}/${version}/${file}")

  message(STATUS "Setting up ${label} host (x64) for ARM64 cross-compilation")

  if(NOT EXISTS
     "${dependencies_dir}/${destination}/lib/cmake/Qt6/Qt6Config.cmake")
    if(NOT EXISTS "${dependencies_dir}/${file}")
      message(STATUS "Downloading ${url}")

      set(MAX_DOWNLOAD_RETRIES 3)
      set(RETRY_DELAY 60) # seconds
      set(download_success FALSE)

      foreach(i RANGE 1 ${MAX_DOWNLOAD_RETRIES})
        message(STATUS "Attempt ${i}/${MAX_DOWNLOAD_RETRIES} for ${url}")

        file(
          DOWNLOAD "${url}" "${dependencies_dir}/${file}"
          STATUS download_status
          EXPECTED_HASH SHA256=${hash})

        list(GET download_status 0 error_code)
        list(GET download_status 1 error_message)

        if(error_code EQUAL 0)
          message(STATUS "Downloading ${url} - success on attempt ${i}")
          set(download_success TRUE)
          break()
        else()
          message(WARNING "Download failed (attempt ${i}): ${error_message}")
          file(REMOVE "${dependencies_dir}/${file}")

          if(NOT i EQUAL MAX_DOWNLOAD_RETRIES)
            message(STATUS "Retrying in ${RETRY_DELAY} seconds...")
            execute_process(COMMAND ${CMAKE_COMMAND} -E sleep ${RETRY_DELAY})
          endif()
        endif()
      endforeach()

      if(NOT download_success)
        message(
          FATAL_ERROR
            "Unable to download ${url} after ${MAX_DOWNLOAD_RETRIES} attempts")
      endif()
      message(STATUS "Downloading ${url} - done")
    endif()

    file(MAKE_DIRECTORY "${dependencies_dir}/${destination}")
    file(ARCHIVE_EXTRACT INPUT "${dependencies_dir}/${file}" DESTINATION
         "${dependencies_dir}/${destination}")
  endif()

  message(STATUS "Using ${dependencies_dir}/${destination} as QT_HOST_PATH")
  set(QT_HOST_PATH
      "${dependencies_dir}/${destination}"
      CACHE PATH "Path to a native Qt6 installation used to supply host tools"
            FORCE)

  message(
    STATUS "Setting up ${label} host (x64) for ARM64 cross-compilation - done")
endfunction()

_check_dependencies_windows()
