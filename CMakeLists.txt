cmake_minimum_required(VERSION 3.21)

project(advanced-scene-switcher VERSION 1.0.0)
set(LIB_NAME "${PROJECT_NAME}-lib")
add_library(${PROJECT_NAME} MODULE)
add_library(${LIB_NAME} SHARED)

set(PLUGIN_AUTHOR "WarmUpTill")
set(MACOS_BUNDLEID "com.warmuptill.${PROJECT_NAME}")
set(LINUX_MAINTAINER_EMAIL "noone@nothing.com")

message(STATUS "CMAKE_PROJECT_NAME is ${CMAKE_PROJECT_NAME}")
if(${CMAKE_PROJECT_NAME} STREQUAL "obs-studio")
  if(NOT DEFINED BUILD_OUT_OF_TREE)
    message(STATUS "${PROJECT_NAME} configured for in-tree build")
  endif()
else()
  set(BUILD_OUT_OF_TREE ON)
  message(STATUS "${PROJECT_NAME} configured for out-of-tree build")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
git_describe(GIT_TAG)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp" @ONLY)

# --- Set target sources ---

# Module sources
target_sources(${PROJECT_NAME} PRIVATE src/advanced-scene-switcher-module.c)

# Generic sources
target_sources(
  ${LIB_NAME}
  PRIVATE src/advanced-scene-switcher.cpp
          src/advanced-scene-switcher.hpp
          src/general.cpp
          src/hotkey.cpp
          src/hotkey.hpp
          src/hotkey.hpp
          src/platform-funcs.hpp
          src/scene-group.cpp
          src/scene-group.hpp
          src/status-control.cpp
          src/status-control.hpp
          src/switcher-data-structs.cpp
          src/switcher-data-structs.hpp
          src/version.cpp
          src/version.h)

# Legacy function sources
target_sources(
  ${LIB_NAME}
  PRIVATE src/legacy/scene-trigger.cpp
          src/legacy/scene-trigger.hpp
          src/legacy/switch-audio.cpp
          src/legacy/switch-audio.hpp
          src/legacy/switch-executable.cpp
          src/legacy/switch-executable.hpp
          src/legacy/switch-file.cpp
          src/legacy/switch-file.hpp
          src/legacy/switch-generic.cpp
          src/legacy/switch-generic.hpp
          src/legacy/switch-idle.cpp
          src/legacy/switch-idle.hpp
          src/legacy/switch-media.cpp
          src/legacy/switch-media.hpp
          src/legacy/switch-network.cpp
          src/legacy/switch-network.hpp
          src/legacy/switch-pause.cpp
          src/legacy/switch-pause.hpp
          src/legacy/switch-priority.cpp
          src/legacy/switch-random.cpp
          src/legacy/switch-random.hpp
          src/legacy/switch-screen-region.cpp
          src/legacy/switch-screen-region.hpp
          src/legacy/switch-sequence.cpp
          src/legacy/switch-sequence.hpp
          src/legacy/switch-time.cpp
          src/legacy/switch-time.hpp
          src/legacy/switch-transitions.cpp
          src/legacy/switch-transitions.hpp
          src/legacy/switch-video.cpp
          src/legacy/switch-video.hpp
          src/legacy/switch-window.cpp
          src/legacy/switch-window.hpp)

# Maro sources
target_sources(
  ${LIB_NAME}
  PRIVATE src/macro-core/macro-action-audio.cpp
          src/macro-core/macro-action-audio.hpp
          src/macro-core/macro-action-edit.cpp
          src/macro-core/macro-action-edit.hpp
          src/macro-core/macro-action-file.cpp
          src/macro-core/macro-action-file.hpp
          src/macro-core/macro-action-filter.cpp
          src/macro-core/macro-action-filter.hpp
          src/macro-core/macro-action-hotkey.cpp
          src/macro-core/macro-action-hotkey.hpp
          src/macro-core/macro-action-macro.cpp
          src/macro-core/macro-action-macro.hpp
          src/macro-core/macro-action-media.cpp
          src/macro-core/macro-action-media.hpp
          src/macro-core/macro-action-plugin-state.cpp
          src/macro-core/macro-action-plugin-state.hpp
          src/macro-core/macro-action-profile.cpp
          src/macro-core/macro-action-profile.hpp
          src/macro-core/macro-action-random.cpp
          src/macro-core/macro-action-random.hpp
          src/macro-core/macro-action-recording.cpp
          src/macro-core/macro-action-recording.hpp
          src/macro-core/macro-action-replay-buffer.cpp
          src/macro-core/macro-action-replay-buffer.hpp
          src/macro-core/macro-action-run.cpp
          src/macro-core/macro-action-run.hpp
          src/macro-core/macro-action-scene-collection.cpp
          src/macro-core/macro-action-scene-collection.hpp
          src/macro-core/macro-action-scene-order.cpp
          src/macro-core/macro-action-scene-order.hpp
          src/macro-core/macro-action-scene-switch.cpp
          src/macro-core/macro-action-scene-switch.hpp
          src/macro-core/macro-action-scene-transform.cpp
          src/macro-core/macro-action-scene-transform.hpp
          src/macro-core/macro-action-scene-visibility.cpp
          src/macro-core/macro-action-scene-visibility.hpp
          src/macro-core/macro-action-screenshot.cpp
          src/macro-core/macro-action-screenshot.hpp
          src/macro-core/macro-action-sequence.cpp
          src/macro-core/macro-action-sequence.hpp
          src/macro-core/macro-action-source.cpp
          src/macro-core/macro-action-source.hpp
          src/macro-core/macro-action-streaming.cpp
          src/macro-core/macro-action-streaming.hpp
          src/macro-core/macro-action-studio-mode.cpp
          src/macro-core/macro-action-studio-mode.hpp
          src/macro-core/macro-action-systray.cpp
          src/macro-core/macro-action-systray.hpp
          src/macro-core/macro-action-timer.cpp
          src/macro-core/macro-action-timer.hpp
          src/macro-core/macro-action-transition.cpp
          src/macro-core/macro-action-transition.hpp
          src/macro-core/macro-action-virtual-cam.cpp
          src/macro-core/macro-action-virtual-cam.hpp
          src/macro-core/macro-action-wait.cpp
          src/macro-core/macro-action-wait.hpp
          src/macro-core/macro-action.cpp
          src/macro-core/macro-action.hpp
          src/macro-core/macro-condition-audio.cpp
          src/macro-core/macro-condition-audio.hpp
          src/macro-core/macro-condition-cursor.cpp
          src/macro-core/macro-condition-cursor.hpp
          src/macro-core/macro-condition-date.cpp
          src/macro-core/macro-condition-date.hpp
          src/macro-core/macro-condition-edit.cpp
          src/macro-core/macro-condition-edit.hpp
          src/macro-core/macro-condition-file.cpp
          src/macro-core/macro-condition-file.hpp
          src/macro-core/macro-condition-filter.cpp
          src/macro-core/macro-condition-filter.hpp
          src/macro-core/macro-condition-hotkey.cpp
          src/macro-core/macro-condition-hotkey.hpp
          src/macro-core/macro-condition-idle.cpp
          src/macro-core/macro-condition-idle.hpp
          src/macro-core/macro-condition-macro.cpp
          src/macro-core/macro-condition-macro.hpp
          src/macro-core/macro-condition-media.cpp
          src/macro-core/macro-condition-media.hpp
          src/macro-core/macro-condition-obs-stats.cpp
          src/macro-core/macro-condition-obs-stats.hpp
          src/macro-core/macro-condition-plugin-state.cpp
          src/macro-core/macro-condition-plugin-state.hpp
          src/macro-core/macro-condition-process.cpp
          src/macro-core/macro-condition-process.hpp
          src/macro-core/macro-condition-profile.cpp
          src/macro-core/macro-condition-profile.hpp
          src/macro-core/macro-condition-recording.cpp
          src/macro-core/macro-condition-recording.hpp
          src/macro-core/macro-condition-replay-buffer.cpp
          src/macro-core/macro-condition-replay-buffer.hpp
          src/macro-core/macro-condition-scene-order.cpp
          src/macro-core/macro-condition-scene-order.hpp
          src/macro-core/macro-condition-scene-transform.cpp
          src/macro-core/macro-condition-scene-transform.hpp
          src/macro-core/macro-condition-scene-visibility.cpp
          src/macro-core/macro-condition-scene-visibility.hpp
          src/macro-core/macro-condition-scene.cpp
          src/macro-core/macro-condition-scene.hpp
          src/macro-core/macro-condition-source.cpp
          src/macro-core/macro-condition-source.hpp
          src/macro-core/macro-condition-streaming.cpp
          src/macro-core/macro-condition-streaming.hpp
          src/macro-core/macro-condition-studio-mode.cpp
          src/macro-core/macro-condition-studio-mode.hpp
          src/macro-core/macro-condition-timer.cpp
          src/macro-core/macro-condition-timer.hpp
          src/macro-core/macro-condition-transition.cpp
          src/macro-core/macro-condition-transition.hpp
          src/macro-core/macro-condition-virtual-cam.cpp
          src/macro-core/macro-condition-virtual-cam.hpp
          src/macro-core/macro-condition-window.cpp
          src/macro-core/macro-condition-window.hpp
          src/macro-core/macro-condition.cpp
          src/macro-core/macro-condition.hpp
          src/macro-core/macro-list-entry-widget.cpp
          src/macro-core/macro-list-entry-widget.hpp
          src/macro-core/macro-properties.cpp
          src/macro-core/macro-properties.hpp
          src/macro-core/macro-ref.cpp
          src/macro-core/macro-ref.hpp
          src/macro-core/macro-segment-list.cpp
          src/macro-core/macro-segment-list.hpp
          src/macro-core/macro-segment.cpp
          src/macro-core/macro-segment.hpp
          src/macro-core/macro-selection.cpp
          src/macro-core/macro-selection.hpp
          src/macro-core/macro-tab.cpp
          src/macro-core/macro.cpp
          src/macro-core/macro.hpp)

# Utility function sources
target_sources(
  ${LIB_NAME}
  PRIVATE src/utils/curl-helper.cpp
          src/utils/curl-helper.hpp
          src/utils/duration-control.cpp
          src/utils/duration-control.hpp
          src/utils/file-selection.cpp
          src/utils/file-selection.hpp
          src/utils/macro-list.cpp
          src/utils/macro-list.hpp
          src/utils/name-dialog.cpp
          src/utils/name-dialog.hpp
          src/utils/resizing-text-edit.cpp
          src/utils/resizing-text-edit.hpp
          src/utils/scene-item-selection.cpp
          src/utils/scene-item-selection.hpp
          src/utils/scene-selection.cpp
          src/utils/scene-selection.hpp
          src/utils/screenshot-helper.cpp
          src/utils/screenshot-helper.hpp
          src/utils/section.cpp
          src/utils/section.hpp
          src/utils/transition-selection.cpp
          src/utils/transition-selection.hpp
          src/utils/utility.cpp
          src/utils/utility.hpp
          src/utils/volume-control.cpp
          src/utils/volume-control.hpp)

# --- End of section ---

if(BUILD_OUT_OF_TREE)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  include(cmake/ObsPluginHelpers.cmake)
  target_link_libraries(${LIB_NAME} PUBLIC OBS::libobs OBS::obs-frontend-api)
  target_link_libraries(${PROJECT_NAME} PUBLIC ${LIB_NAME})
else()
  target_link_libraries(${LIB_NAME} PUBLIC OBS::libobs OBS::frontend-api)
  target_link_libraries(${PROJECT_NAME} PUBLIC ${LIB_NAME})
endif()

find_qt(COMPONENTS Widgets Core)
target_link_libraries(${LIB_NAME} PUBLIC Qt::Core Qt::Widgets)

include(cmake/AdvSSHelpers.cmake)

# --- Platform-independent build settings ---

target_include_directories(
  ${LIB_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
          "${CMAKE_CURRENT_SOURCE_DIR}/src/legacy"
          "${CMAKE_CURRENT_SOURCE_DIR}/src/macro-core"
          "${CMAKE_CURRENT_SOURCE_DIR}/src/utils"
          "${CMAKE_CURRENT_BINARY_DIR}/forms")

set_target_properties(
  ${LIB_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
             AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/forms")

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_features(${LIB_NAME} PUBLIC cxx_std_17)

add_definitions(-DASIO_STANDALONE)
target_include_directories(
  ${LIB_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include"
                     "${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp")

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(OS_WINDOWS)
  configure_file(cmake/bundle/windows/installer-Windows.iss.in
                 ${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss)

  if(MSVC)
    target_compile_options(${LIB_NAME} PUBLIC /MP /d2FH4- /wd4267 /wd4267)
  endif()
  target_sources(${LIB_NAME} PRIVATE src/win/advanced-scene-switcher-win.cpp)
  add_definitions(-D_WEBSOCKETPP_CPP11_STL_)
  set_property(TARGET ${LIB_NAME} PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS true)
  # --- End of section ---

  # -- macOS specific build settings and tasks --
elseif(OS_MACOS)
  configure_file(cmake/bundle/macos/installer-macos.pkgproj.in
                 ${CMAKE_CURRENT_BINARY_DIR}/installer-macos.generated.pkgproj)

  set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
  set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
  set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")

  target_compile_options(
    ${LIB_NAME} PRIVATE -Wall -Wextra -Werror-implicit-function-declaration
                        -stdlib=libc++ -fvisibility=default)

  set_target_properties(${LIB_NAME} PROPERTIES PREFIX "" SUFFIX ".so")

  find_library(COCOA Cocoa)
  target_include_directories(${LIB_NAME} PRIVATE ${COCOA})
  target_sources(${LIB_NAME} PRIVATE src/osx/advanced-scene-switcher-osx.mm)
  set_source_files_properties(advanced-scene-switcher-osx.mm
                              PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  set("${PROJECT_NAME}_PLATFORM_LIBS" ${COCOA})
  find_package(CURL)
  find_package(Libcurl)
  if(CURL_FOUND)
    target_include_directories(${LIB_NAME} PRIVATE "${CURL_INCLUDE_DIRS}")
  elseif(Libcurl_FOUND)
    target_include_directories(${LIB_NAME} PRIVATE "${LIBCURL_INCLUDE_DIRS}")
  else()
    message(FATAL_ERROR "Couldn't find CURL or Libcurl - abort")
  endif()
  # --- End of section ---

  # --- Linux-specific build settings and tasks ---
else()
  target_compile_options(${LIB_NAME} PRIVATE -Wall -Wextra)

  set_target_properties(${LIB_NAME} PROPERTIES PREFIX "")

  find_package(X11 REQUIRED COMPONENTS Xtst Xss)
  find_path(PROCPS_INCLUDE_DIR NAMES proc/procps.h)
  if(NOT PROCPS_INCLUDE_DIR)
    message(
      FATAL_ERROR "procps include dir not found - please set PROCPS_INCLUDE_DIR"
    )
  endif()
  find_library(PROCPS_LIBRARY NAMES procps)
  if(NOT PROCPS_LIBRARY)
    message(FATAL_ERROR "procps lib not found - please set PROCPS_LIBRARY")
  endif()
  target_link_libraries(${LIB_NAME} PRIVATE ${X11_LIBRARIES}
                                            ${procps_LIBRARIES})
  target_include_directories(
    ${LIB_NAME} PRIVATE "${X11_INCLUDE_DIR}" "${X11_Xtst_INCLUDE_PATH}"
                        "${X11_Xss_INCLUDE_PATH}" "${PROCPS_INCLUDE_DIR}")
  target_sources(${LIB_NAME} PRIVATE src/linux/advanced-scene-switcher-nix.cpp)
  set("${PROJECT_NAME}_PLATFORM_LIBS" Xss ${PROCPS_LIBRARY})
  find_package(CURL)
  find_package(Libcurl)
  if(CURL_FOUND)
    target_include_directories(${LIB_NAME} PRIVATE "${CURL_INCLUDE_DIRS}")
  elseif(Libcurl_FOUND)
    target_include_directories(${LIB_NAME} PRIVATE "${LIBCURL_INCLUDE_DIRS}")
  else()
    message(FATAL_ERROR "Couldn't find CURL or Libcurl - abort")
  endif()
endif()
# --- End of section ---

target_link_libraries(${LIB_NAME} PUBLIC ${${PROJECT_NAME}_PLATFORM_LIBS})
setup_plugin_target(${PROJECT_NAME})
install_advss_lib(${LIB_NAME})

# --- Helpers for debian package creation ---

if(DEB_INSTALL)
  # Additional commands to install the module in the correct place. Find all the
  # translation files so we can copy them to the correct place later on.
  file(GLOB ASS_TRANSLATION_FILES "data/locale/*.ini")

  # Linux
  if(UNIX AND NOT APPLE)
    if(NOT LIB_OUT_DIR)
      set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
      set(DATA_OUT_DIR "/share/obs/obs-plugins/${PROJECT_NAME}")
    endif()
    install(TARGETS ${PROJECT_NAME}
            LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
            DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
    install(DIRECTORY data/res
            DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
  endif()
endif()

# --- End of section ---

add_subdirectory(src/macro-external)
