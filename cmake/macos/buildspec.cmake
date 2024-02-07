# CMake macOS build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

# Hacky workaround to ensure all libobs headers are available at build time
function(_check_for_libobs_headers)
  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")

  set(headers_to_copy "util/config-file.h" "util/platform.h"
                      "graphics/matrix4.h" "graphics/axisang.h")

  foreach(header IN LISTS headers_to_copy)
    set(libobs_header_location
        "${dependencies_dir}/Frameworks/libobs.framework/Versions/A/Headers/${header}"
    )

    if(EXISTS ${libobs_header_location})
      continue()
    endif()

    string(
      JSON
      version
      GET
      ${buildspec}
      dependencies
      obs-studio
      version)
    set(input_file "${dependencies_dir}/obs-studio-${version}/libobs/${header}")
    message(STATUS "Will copy ${input_file} to ${libobs_header_location} ...")
    configure_file(${input_file} ${libobs_header_location} COPYONLY)
  endforeach()
endfunction()

# _check_dependencies_macos: Set up macOS slice for _check_dependencies
function(_check_dependencies_macos)
  set(arch universal)
  set(platform macos)

  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" buildspec)

  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  set(prebuilt_filename "macos-deps-VERSION-ARCH_REVISION.tar.xz")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "macos-deps-qt6-VERSION-ARCH-REVISION.tar.xz")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  set(obs-studio_filename "VERSION.tar.gz")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()

  execute_process(COMMAND "xattr" -r -d com.apple.quarantine
                          "${dependencies_dir}" RESULT_VARIABLE result)

  list(APPEND CMAKE_FRAMEWORK_PATH "${dependencies_dir}/Frameworks")
  set(CMAKE_FRAMEWORK_PATH
      ${CMAKE_FRAMEWORK_PATH}
      PARENT_SCOPE)
endfunction()

_check_dependencies_macos()
_check_for_libobs_headers()
