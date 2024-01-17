cmake_minimum_required(VERSION 3.16.3)

project(advanced-scene-switcher VERSION 1.0.0)
set(LIB_NAME "${PROJECT_NAME}-lib")
add_library(${PROJECT_NAME} MODULE)
add_library(${LIB_NAME} SHARED)

set(PLUGIN_AUTHOR "WarmUpTill")
set(MACOS_BUNDLEID "com.warmuptill.${PROJECT_NAME}")
set(LINUX_MAINTAINER_EMAIL "noone@nothing.com")
set(MACOS_PACKAGE_UUID "3F0D2A6A-2583-11ED-861D-0242AC120002")
set(MACOS_INSTALLER_UUID "B7F15A6E-2583-11ED-861D-0242AC120002")
set(WINDOWS_INSTALLER_UUID "A4ADDF26-4426-4D2E-B26A-C7C878DA8FC9")

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
               "${CMAKE_CURRENT_BINARY_DIR}/lib/version.cpp" @ONLY)

# --- Set target sources ---

# Module sources
target_sources(${PROJECT_NAME} PRIVATE module/advanced-scene-switcher-module.c)

# Generic sources
target_sources(
  ${LIB_NAME}
  PRIVATE lib/advanced-scene-switcher.cpp
          lib/advanced-scene-switcher.hpp
          lib/general.cpp
          lib/platform-funcs.hpp
          lib/switcher-data.cpp
          lib/switcher-data.hpp
          lib/version.cpp
          lib/version.h)

# Legacy function sources
target_sources(
  ${LIB_NAME}
  PRIVATE lib/legacy/scene-group.cpp
          lib/legacy/scene-group.hpp
          lib/legacy/scene-trigger.cpp
          lib/legacy/scene-trigger.hpp
          lib/legacy/switch-audio.cpp
          lib/legacy/switch-audio.hpp
          lib/legacy/switch-executable.cpp
          lib/legacy/switch-executable.hpp
          lib/legacy/switch-file.cpp
          lib/legacy/switch-file.hpp
          lib/legacy/switch-generic.cpp
          lib/legacy/switch-generic.hpp
          lib/legacy/switch-idle.cpp
          lib/legacy/switch-idle.hpp
          lib/legacy/switch-media.cpp
          lib/legacy/switch-media.hpp
          lib/legacy/switch-network.cpp
          lib/legacy/switch-network.hpp
          lib/legacy/switch-pause.cpp
          lib/legacy/switch-pause.hpp
          lib/legacy/switch-random.cpp
          lib/legacy/switch-random.hpp
          lib/legacy/switch-screen-region.cpp
          lib/legacy/switch-screen-region.hpp
          lib/legacy/switch-sequence.cpp
          lib/legacy/switch-sequence.hpp
          lib/legacy/switch-time.cpp
          lib/legacy/switch-time.hpp
          lib/legacy/switch-transitions.cpp
          lib/legacy/switch-transitions.hpp
          lib/legacy/switch-video.cpp
          lib/legacy/switch-video.hpp
          lib/legacy/switch-window.cpp
          lib/legacy/switch-window.hpp)

# Maro sources
target_sources(
  ${LIB_NAME}
  PRIVATE lib/macro/macro-action-edit.cpp
          lib/macro/macro-action-edit.hpp
          lib/macro/macro-action-factory.cpp
          lib/macro/macro-action-factory.hpp
          lib/macro/macro-action-macro.cpp
          lib/macro/macro-action-macro.hpp
          lib/macro/macro-action-queue.cpp
          lib/macro/macro-action-queue.hpp
          lib/macro/macro-action-variable.cpp
          lib/macro/macro-action-variable.hpp
          lib/macro/macro-action.cpp
          lib/macro/macro-action.hpp
          lib/macro/macro-condition-edit.cpp
          lib/macro/macro-condition-edit.hpp
          lib/macro/macro-condition-factory.cpp
          lib/macro/macro-condition-factory.hpp
          lib/macro/macro-condition-macro.cpp
          lib/macro/macro-condition-macro.hpp
          lib/macro/macro-condition-queue.cpp
          lib/macro/macro-condition-queue.hpp
          lib/macro/macro-condition-tempvar.cpp
          lib/macro/macro-condition-tempvar.hpp
          lib/macro/macro-condition-variable.cpp
          lib/macro/macro-condition-variable.hpp
          lib/macro/macro-condition.cpp
          lib/macro/macro-condition.hpp
          lib/macro/macro-dock.cpp
          lib/macro/macro-dock.hpp
          lib/macro/macro-export-import-dialog.cpp
          lib/macro/macro-export-import-dialog.hpp
          lib/macro/macro-helpers.cpp
          lib/macro/macro-helpers.hpp
          lib/macro/macro-list.cpp
          lib/macro/macro-list.hpp
          lib/macro/macro-properties.cpp
          lib/macro/macro-properties.hpp
          lib/macro/macro-ref.cpp
          lib/macro/macro-ref.hpp
          lib/macro/macro-run-button.cpp
          lib/macro/macro-run-button.hpp
          lib/macro/macro-segment-list.cpp
          lib/macro/macro-segment-list.hpp
          lib/macro/macro-segment-selection.cpp
          lib/macro/macro-segment-selection.hpp
          lib/macro/macro-segment.cpp
          lib/macro/macro-segment.hpp
          lib/macro/macro-selection.cpp
          lib/macro/macro-selection.hpp
          lib/macro/macro-tab.cpp
          lib/macro/macro-tree.cpp
          lib/macro/macro-tree.hpp
          lib/macro/macro.cpp
          lib/macro/macro.hpp)

# Utility function sources
target_sources(
  ${LIB_NAME}
  PRIVATE lib/utils/action-queue.cpp
          lib/utils/action-queue.hpp
          lib/utils/curl-helper.cpp
          lib/utils/curl-helper.hpp
          lib/utils/duration-control.cpp
          lib/utils/duration-control.hpp
          lib/utils/duration.cpp
          lib/utils/duration.hpp
          lib/utils/export-symbol-helper.hpp
          lib/utils/file-selection.cpp
          lib/utils/file-selection.hpp
          lib/utils/filter-combo-box.cpp
          lib/utils/filter-combo-box.hpp
          lib/utils/item-selection-helpers.cpp
          lib/utils/item-selection-helpers.hpp
          lib/utils/log-helper.cpp
          lib/utils/log-helper.hpp
          lib/utils/math-helpers.cpp
          lib/utils/math-helpers.hpp
          lib/utils/mouse-wheel-guard.cpp
          lib/utils/mouse-wheel-guard.hpp
          lib/utils/name-dialog.cpp
          lib/utils/name-dialog.hpp
          lib/utils/non-modal-dialog.cpp
          lib/utils/non-modal-dialog.hpp
          lib/utils/obs-dock.hpp
          lib/utils/obs-module-helper.cpp
          lib/utils/obs-module-helper.hpp
          lib/utils/plugin-state-helpers.cpp
          lib/utils/plugin-state-helpers.hpp
          lib/utils/priority-helper.cpp
          lib/utils/priority-helper.hpp
          lib/utils/regex-config.cpp
          lib/utils/regex-config.hpp
          lib/utils/resizing-text-edit.cpp
          lib/utils/resizing-text-edit.hpp
          lib/utils/scene-selection.cpp
          lib/utils/scene-selection.hpp
          lib/utils/scene-switch-helpers.cpp
          lib/utils/scene-switch-helpers.hpp
          lib/utils/screenshot-helper.cpp
          lib/utils/screenshot-helper.hpp
          lib/utils/section.cpp
          lib/utils/section.hpp
          lib/utils/slider-spinbox.cpp
          lib/utils/slider-spinbox.hpp
          lib/utils/source-selection.cpp
          lib/utils/source-selection.hpp
          lib/utils/status-control.cpp
          lib/utils/status-control.hpp
          lib/utils/switch-button.cpp
          lib/utils/switch-button.hpp
          lib/utils/sync-helpers.cpp
          lib/utils/sync-helpers.hpp
          lib/utils/temp-variable.cpp
          lib/utils/temp-variable.hpp
          lib/utils/utility.cpp
          lib/utils/utility.hpp
          lib/utils/variable-line-edit.cpp
          lib/utils/variable-line-edit.hpp
          lib/utils/variable-number.hpp
          lib/utils/variable-number.tpp
          lib/utils/variable-spinbox.cpp
          lib/utils/variable-spinbox.hpp
          lib/utils/variable-string.cpp
          lib/utils/variable-string.hpp
          lib/utils/variable-text-edit.cpp
          lib/utils/variable-text-edit.hpp
          lib/utils/variable.cpp
          lib/utils/variable.hpp
          lib/utils/volume-control.cpp
          lib/utils/volume-control.hpp)

# --- End of section ---

if(BUILD_OUT_OF_TREE)
  include(cmake/ObsPluginHelpers.cmake)
endif()
include(cmake/AdvSSHelpers.cmake)
setup_obs_lib_dependency(${LIB_NAME})
setup_obs_lib_dependency(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC ${LIB_NAME})

find_qt(COMPONENTS Widgets Core)
target_link_libraries(${LIB_NAME} PUBLIC Qt::Core Qt::Widgets)

# --- Platform-independent build settings ---

target_include_directories(
  ${LIB_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib"
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/legacy"
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/macro"
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/utils"
          "${CMAKE_CURRENT_BINARY_DIR}/forms")

set_target_properties(
  ${LIB_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
             AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/forms")
set_target_properties(${LIB_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_features(${LIB_NAME} PUBLIC cxx_std_17)

add_definitions(-DASIO_STANDALONE)
target_include_directories(
  ${LIB_NAME}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include"
         "${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/deps/obs-websocket/lib"
         "${CMAKE_CURRENT_SOURCE_DIR}/deps/exprtk")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/json/CMakeLists.txt")
  add_subdirectory(deps/json)
else()
  find_package(nlohmann_json REQUIRED)
endif()
target_link_libraries(${LIB_NAME} PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(${LIB_NAME} PRIVATE ADVSS_EXPORT_SYMBOLS=1)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(OS_WINDOWS)
  configure_file(cmake/bundle/windows/installer-Windows.iss.in
                 ${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss)

  target_compile_definitions(${LIB_NAME} PRIVATE UNICODE _UNICODE)
  if(MSVC)
    target_compile_options(${LIB_NAME} PUBLIC /MP /d2FH4- /wd4267 /wd4267
                                              /bigobj)
  endif()
  target_sources(${LIB_NAME} PRIVATE lib/win/advanced-scene-switcher-win.cpp)
  add_definitions(-D_WEBSOCKETPP_CPP11_STL_)
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
                        -stdlib=libc++)

  set_target_properties(${LIB_NAME} PROPERTIES PREFIX "" SUFFIX ".so")

  find_library(COCOA Cocoa)
  target_include_directories(${LIB_NAME} PRIVATE ${COCOA})
  target_link_libraries(${LIB_NAME} PRIVATE ${COCOA})

  find_package(CURL)
  find_package(Libcurl)
  if(CURL_FOUND)
    target_include_directories(${LIB_NAME} PRIVATE "${CURL_INCLUDE_DIRS}")
  elseif(Libcurl_FOUND)
    target_include_directories(${LIB_NAME} PRIVATE "${LIBCURL_INCLUDE_DIRS}")
  else()
    message(FATAL_ERROR "Couldn't find CURL or Libcurl - abort")
  endif()

  target_sources(${LIB_NAME} PRIVATE lib/osx/advanced-scene-switcher-osx.mm)
  set_source_files_properties(advanced-scene-switcher-osx.mm
                              PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  # --- End of section ---

  # --- Linux-specific build settings and tasks ---
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN")
  target_compile_options(${LIB_NAME} PRIVATE -Wall -Wextra)
  set_target_properties(${LIB_NAME} PROPERTIES PREFIX "")
  set_target_properties(${LIB_NAME} PROPERTIES SOVERSION 1)

  find_package(X11 REQUIRED COMPONENTS Xss)
  target_include_directories(${LIB_NAME} PRIVATE "${X11_INCLUDE_DIR}"
                                                 "${X11_Xss_INCLUDE_PATH}")
  target_link_libraries(${LIB_NAME} PRIVATE ${X11_LIBRARIES})

  find_path(PROCPS_INCLUDE_DIR NAMES proc/procps.h)
  find_path(PROCPS2_INCLUDE_DIR NAMES libproc2/pids.h)
  if(PROCPS_INCLUDE_DIR)
    message(STATUS "${PROJECT_NAME} using procps")
    set(PROC_INCLUDE_DIR "${PROCPS_INCLUDE_DIR}")
    target_compile_definitions(${LIB_NAME} PRIVATE PROCPS_AVAILABLE)
    set(PROCESS_CONDITION_SUPPORTED 1)
  endif()
  if(PROCPS2_INCLUDE_DIR)
    message(STATUS "${PROJECT_NAME} using libproc2")
    set(PROC_INCLUDE_DIR "${PROCPS2_INCLUDE_DIR}")
    target_compile_definitions(${LIB_NAME} PRIVATE PROCPS2_AVAILABLE)
    set(PROCESS_CONDITION_SUPPORTED 1)
  endif()
  if(NOT DEFINED PROCESS_CONDITION_SUPPORTED)
    message(
      WARNING
        "found neither procps nor libproc2! Process condition will not be functional!"
    )
  endif()
  target_include_directories(${LIB_NAME} PRIVATE "${PROC_INCLUDE_DIR}")

  find_package(CURL)
  find_package(Libcurl)
  if(CURL_FOUND)
    target_include_directories(${LIB_NAME} PRIVATE "${CURL_INCLUDE_DIRS}")
  elseif(Libcurl_FOUND)
    target_include_directories(${LIB_NAME} PRIVATE "${LIBCURL_INCLUDE_DIRS}")
  else()
    message(FATAL_ERROR "Couldn't find CURL or Libcurl - abort")
  endif()

  target_sources(${LIB_NAME} PRIVATE lib/linux/advanced-scene-switcher-nix.cpp)
endif()
# --- End of section ---

if(DEB_INSTALL)
  file(GLOB ASS_TRANSLATION_FILES "data/locale/*.ini")
  if(NOT LIB_OUT_DIR)
    set(LIB_OUT_DIR "/lib/obs-plugins")
  endif()
  if(NOT DATA_OUT_DIR)
    set(DATA_OUT_DIR "/share/obs/obs-plugins/${PROJECT_NAME}")
  endif()
  install(TARGETS ${PROJECT_NAME}
          LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
  install(TARGETS ${LIB_NAME}
          LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
  install(DIRECTORY data/locale
          DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
  install(DIRECTORY data/res
          DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
else()
  setup_plugin_target(${PROJECT_NAME})
  install_advss_lib(${LIB_NAME})
endif()

# --- End of section ---

add_subdirectory(plugins)
add_subdirectory(tests)
