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

target_sources(${PROJECT_NAME} PRIVATE src/advanced-scene-switcher-module.c)

target_sources(
  ${LIB_NAME}
  PRIVATE src/headers/advanced-scene-switcher.hpp
          src/headers/switcher-data-structs.hpp
          src/headers/scene-group.hpp
          src/headers/scene-trigger.hpp
          src/headers/switch-audio.hpp
          src/headers/switch-executable.hpp
          src/headers/switch-file.hpp
          src/headers/switch-idle.hpp
          src/headers/switch-media.hpp
          src/headers/switch-network.hpp
          src/headers/switch-pause.hpp
          src/headers/switch-random.hpp
          src/headers/switch-screen-region.hpp
          src/headers/switch-time.hpp
          src/headers/switch-transitions.hpp
          src/headers/switch-window.hpp
          src/headers/switch-sequence.hpp
          src/headers/switch-video.hpp
          src/headers/switch-generic.hpp
          src/headers/macro-action.hpp
          src/headers/macro-action-edit.hpp
          src/headers/macro-action-audio.hpp
          src/headers/macro-action-file.hpp
          src/headers/macro-action-filter.hpp
          src/headers/macro-action-hotkey.hpp
          src/headers/macro-action-macro.hpp
          src/headers/macro-action-media.hpp
          src/headers/macro-action-plugin-state.hpp
          src/headers/macro-action-profile.hpp
          src/headers/macro-action-random.hpp
          src/headers/macro-action-recording.hpp
          src/headers/macro-action-replay-buffer.hpp
          src/headers/macro-action-run.hpp
          src/headers/macro-action-scene-collection.hpp
          src/headers/macro-action-scene-order.hpp
          src/headers/macro-action-scene-switch.hpp
          src/headers/macro-action-scene-transform.hpp
          src/headers/macro-action-scene-visibility.hpp
          src/headers/macro-action-screenshot.hpp
          src/headers/macro-action-sequence.hpp
          src/headers/macro-action-source.hpp
          src/headers/macro-action-streaming.hpp
          src/headers/macro-action-studio-mode.hpp
          src/headers/macro-action-systray.hpp
          src/headers/macro-action-timer.hpp
          src/headers/macro-action-transition.hpp
          src/headers/macro-action-virtual-cam.hpp
          src/headers/macro-action-wait.hpp
          src/headers/macro-condition.hpp
          src/headers/macro-condition-edit.hpp
          src/headers/macro-condition-audio.hpp
          src/headers/macro-condition-cursor.hpp
          src/headers/macro-condition-date.hpp
          src/headers/macro-condition-file.hpp
          src/headers/macro-condition-filter.hpp
          src/headers/macro-condition-hotkey.hpp
          src/headers/macro-condition-idle.hpp
          src/headers/macro-condition-macro.hpp
          src/headers/macro-condition-media.hpp
          src/headers/macro-condition-obs-stats.hpp
          src/headers/macro-condition-plugin-state.hpp
          src/headers/macro-condition-process.hpp
          src/headers/macro-condition-profile.hpp
          src/headers/macro-condition-recording.hpp
          src/headers/macro-condition-replay-buffer.hpp
          src/headers/macro-condition-scene-order.hpp
          src/headers/macro-condition-scene-transform.hpp
          src/headers/macro-condition-scene-visibility.hpp
          src/headers/macro-condition-scene.hpp
          src/headers/macro-condition-source.hpp
          src/headers/macro-condition-streaming.hpp
          src/headers/macro-condition-studio-mode.hpp
          src/headers/macro-condition-timer.hpp
          src/headers/macro-condition-transition.hpp
          src/headers/macro-condition-virtual-cam.hpp
          src/headers/macro-condition-window.hpp
          src/headers/macro.hpp
          src/headers/macro-ref.hpp
          src/headers/macro-list-entry-widget.hpp
          src/headers/macro-properties.hpp
          src/headers/macro-segment.hpp
          src/headers/macro-segment-list.hpp
          src/headers/macro-selection.hpp
          src/headers/curl-helper.hpp
          src/headers/hotkey.hpp
          src/headers/scene-item-selection.hpp
          src/headers/scene-selection.hpp
          src/headers/screenshot-helper.hpp
          src/headers/transition-selection.hpp
          src/headers/name-dialog.hpp
          src/headers/duration-control.hpp
          src/headers/file-selection.hpp
          src/headers/section.hpp
          src/headers/status-control.hpp
          src/headers/platform-funcs.hpp
          src/headers/resizing-text-edit.hpp
          src/headers/utility.hpp
          src/headers/volume-control.hpp
          src/headers/version.h
          src/advanced-scene-switcher.cpp
          src/switcher-data-structs.cpp
          src/scene-group.cpp
          src/scene-trigger.cpp
          src/switch-transitions.cpp
          src/switch-screen-region.cpp
          src/switch-priority.cpp
          src/switch-executable.cpp
          src/switch-idle.cpp
          src/switch-sequence.cpp
          src/switch-file.cpp
          src/switch-window.cpp
          src/switch-media.cpp
          src/switch-network.cpp
          src/file-selection.cpp
          src/hotkey.cpp
          src/general.cpp
          src/switch-pause.cpp
          src/switch-random.cpp
          src/switch-time.cpp
          src/switch-audio.cpp
          src/switch-video.cpp
          src/switch-generic.cpp
          src/macro-action.cpp
          src/macro-action-edit.cpp
          src/macro-action-audio.cpp
          src/macro-action-file.cpp
          src/macro-action-filter.cpp
          src/macro-action-hotkey.cpp
          src/macro-action-macro.cpp
          src/macro-action-media.cpp
          src/macro-action-plugin-state.cpp
          src/macro-action-profile.cpp
          src/macro-action-random.cpp
          src/macro-action-recording.cpp
          src/macro-action-replay-buffer.cpp
          src/macro-action-run.cpp
          src/macro-action-scene-collection.cpp
          src/macro-action-scene-order.cpp
          src/macro-action-scene-switch.cpp
          src/macro-action-scene-transform.cpp
          src/macro-action-scene-visibility.cpp
          src/macro-action-screenshot.cpp
          src/macro-action-sequence.cpp
          src/macro-action-source.cpp
          src/macro-action-streaming.cpp
          src/macro-action-studio-mode.cpp
          src/macro-action-systray.cpp
          src/macro-action-timer.cpp
          src/macro-action-transition.cpp
          src/macro-action-virtual-cam.cpp
          src/macro-action-wait.cpp
          src/macro-condition.cpp
          src/macro-condition-edit.cpp
          src/macro-condition-audio.cpp
          src/macro-condition-cursor.cpp
          src/macro-condition-date.cpp
          src/macro-condition-file.cpp
          src/macro-condition-filter.cpp
          src/macro-condition-hotkey.cpp
          src/macro-condition-idle.cpp
          src/macro-condition-macro.cpp
          src/macro-condition-media.cpp
          src/macro-condition-obs-stats.cpp
          src/macro-condition-plugin-state.cpp
          src/macro-condition-process.cpp
          src/macro-condition-profile.cpp
          src/macro-condition-recording.cpp
          src/macro-condition-replay-buffer.cpp
          src/macro-condition-scene-order.cpp
          src/macro-condition-scene-transform.cpp
          src/macro-condition-scene-visibility.cpp
          src/macro-condition-scene.cpp
          src/macro-condition-source.cpp
          src/macro-condition-streaming.cpp
          src/macro-condition-studio-mode.cpp
          src/macro-condition-timer.cpp
          src/macro-condition-transition.cpp
          src/macro-condition-virtual-cam.cpp
          src/macro-condition-window.cpp
          src/macro.cpp
          src/macro-ref.cpp
          src/macro-list-entry-widget.cpp
          src/macro-properties.cpp
          src/macro-segment.cpp
          src/macro-segment-list.cpp
          src/macro-selection.cpp
          src/macro-tab.cpp
          src/curl-helper.cpp
          src/scene-item-selection.cpp
          src/scene-selection.cpp
          src/screenshot-helper.cpp
          src/transition-selection.cpp
          src/name-dialog.cpp
          src/resizing-text-edit.cpp
          src/duration-control.cpp
          src/status-control.cpp
          src/section.cpp
          src/utility.cpp
          src/volume-control.cpp
          src/version.cpp)

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
  ${LIB_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
                      "${CMAKE_CURRENT_BINARY_DIR}/forms")

set_target_properties(
  ${LIB_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
             AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/forms")

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

add_subdirectory(src/external-macro-modules)
