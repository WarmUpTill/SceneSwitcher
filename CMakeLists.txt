cmake_minimum_required(VERSION 3.21...3.26)

project(advanced-scene-switcher VERSION 1.0.0)

message(STATUS "CMAKE_PROJECT_NAME is ${CMAKE_PROJECT_NAME}")
if(${CMAKE_PROJECT_NAME} STREQUAL "obs-studio")
  if(NOT DEFINED BUILD_OUT_OF_TREE)
    message(STATUS "${PROJECT_NAME} configured for in-tree build")
  endif()
else()
  set(BUILD_OUT_OF_TREE ON)
  message(STATUS "${PROJECT_NAME} configured for out-of-tree build")
endif()

if(BUILD_OUT_OF_TREE)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake"
          NO_POLICY_SCOPE)
  include(compilerconfig)
  include(defaults)
  include(helpers)
endif()

# OBS 31 no longer defines find_qt so check if we need to include it for in-tree
# builds
if(NOT COMMAND find_qt)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/helpers_common.cmake")
endif()

set(LIB_NAME "${PROJECT_NAME}-lib")
add_library(${PROJECT_NAME} MODULE)
add_library(${LIB_NAME} SHARED)

include(cmake/common/get_git_revision_description.cmake)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
git_describe(GIT_TAG)

if(${GIT_TAG} STREQUAL "GIT-NOTFOUND")
  set(GIT_TAG ${PROJECT_VERSION})
endif()
message(STATUS "${PROJECT_NAME} version: ${GIT_TAG}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/version.cpp.in"
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
          lib/macro/macro-dock-settings.hpp
          lib/macro/macro-dock-settings.cpp
          lib/macro/macro-dock-window.cpp
          lib/macro/macro-dock-window.hpp
          lib/macro/macro-edit.cpp
          lib/macro/macro-edit.hpp
          lib/macro/macro-export-import-dialog.cpp
          lib/macro/macro-export-import-dialog.hpp
          lib/macro/macro-helpers.cpp
          lib/macro/macro-helpers.hpp
          lib/macro/macro-input.cpp
          lib/macro/macro-input.hpp
          lib/macro/macro-list.cpp
          lib/macro/macro-list.hpp
          lib/macro/macro-ref.cpp
          lib/macro/macro-ref.hpp
          lib/macro/macro-run-button.cpp
          lib/macro/macro-run-button.hpp
          lib/macro/macro-segment-copy-paste.cpp
          lib/macro/macro-segment-copy-paste.hpp
          lib/macro/macro-segment-list.cpp
          lib/macro/macro-segment-list.hpp
          lib/macro/macro-segment-selection.cpp
          lib/macro/macro-segment-selection.hpp
          lib/macro/macro-segment-unknown.hpp
          lib/macro/macro-segment.cpp
          lib/macro/macro-segment.hpp
          lib/macro/macro-selection.cpp
          lib/macro/macro-selection.hpp
          lib/macro/macro-settings.cpp
          lib/macro/macro-settings.hpp
          lib/macro/macro-tab.cpp
          lib/macro/macro-tree.cpp
          lib/macro/macro-tree.hpp
          lib/macro/macro.cpp
          lib/macro/macro.hpp)

# Utility function sources
target_sources(
  ${LIB_NAME}
  PRIVATE lib/queue/action-queue.cpp
          lib/queue/action-queue.hpp
          lib/queue/action-queue-tab.cpp
          lib/queue/action-queue-tab.hpp
          lib/utils/auto-update-tooltip-label.cpp
          lib/utils/auto-update-tooltip-label.hpp
          lib/utils/backup.cpp
          lib/utils/backup.hpp
          lib/utils/canvas-helpers.cpp
          lib/utils/canvas-helpers.hpp
          lib/utils/condition-logic.cpp
          lib/utils/condition-logic.hpp
          lib/utils/curl-helper.cpp
          lib/utils/curl-helper.hpp
          lib/utils/cursor-shape-changer.cpp
          lib/utils/cursor-shape-changer.hpp
          lib/utils/double-slider.cpp
          lib/utils/double-slider.hpp
          lib/utils/duration-control.cpp
          lib/utils/duration-control.hpp
          lib/utils/duration-modifier.cpp
          lib/utils/duration-modifier.hpp
          lib/utils/duration.cpp
          lib/utils/duration.hpp
          lib/utils/export-symbol-helper.hpp
          lib/utils/file-selection.cpp
          lib/utils/file-selection.hpp
          lib/utils/filter-combo-box.cpp
          lib/utils/filter-combo-box.hpp
          lib/utils/help-icon.hpp
          lib/utils/help-icon.cpp
          lib/utils/item-selection-helpers.cpp
          lib/utils/item-selection-helpers.hpp
          lib/utils/json-helpers.cpp
          lib/utils/json-helpers.hpp
          lib/utils/layout-helpers.cpp
          lib/utils/layout-helpers.hpp
          lib/utils/list-controls.cpp
          lib/utils/list-controls.hpp
          lib/utils/list-editor.cpp
          lib/utils/list-editor.hpp
          lib/utils/log-helper.cpp
          lib/utils/log-helper.hpp
          lib/utils/math-helpers.cpp
          lib/utils/math-helpers.hpp
          lib/utils/message-buffer.hpp
          lib/utils/message-dispatcher.hpp
          lib/utils/mouse-wheel-guard.cpp
          lib/utils/mouse-wheel-guard.hpp
          lib/utils/name-dialog.cpp
          lib/utils/name-dialog.hpp
          lib/utils/non-modal-dialog.cpp
          lib/utils/non-modal-dialog.hpp
          lib/utils/obs-module-helper.cpp
          lib/utils/obs-module-helper.hpp
          lib/utils/path-helpers.cpp
          lib/utils/path-helpers.hpp
          lib/utils/plugin-state-helpers.cpp
          lib/utils/plugin-state-helpers.hpp
          lib/utils/priority-helper.cpp
          lib/utils/priority-helper.hpp
          lib/utils/regex-config.cpp
          lib/utils/regex-config.hpp
          lib/utils/resizable-widget.cpp
          lib/utils/resizable-widget.hpp
          lib/utils/resizing-text-edit.cpp
          lib/utils/resizing-text-edit.hpp
          lib/utils/resource-table.cpp
          lib/utils/resource-table.hpp
          lib/utils/scene-selection.cpp
          lib/utils/scene-selection.hpp
          lib/utils/scene-switch-helpers.cpp
          lib/utils/scene-switch-helpers.hpp
          lib/utils/screenshot-helper.cpp
          lib/utils/screenshot-helper.hpp
          lib/utils/section.cpp
          lib/utils/section.hpp
          lib/utils/selection-helpers.cpp
          lib/utils/selection-helpers.hpp
          lib/utils/single-char-selection.cpp
          lib/utils/single-char-selection.hpp
          lib/utils/slider-spinbox.cpp
          lib/utils/slider-spinbox.hpp
          lib/utils/source-helpers.cpp
          lib/utils/source-helpers.hpp
          lib/utils/source-selection.cpp
          lib/utils/source-selection.hpp
          lib/utils/splitter-helpers.cpp
          lib/utils/splitter-helpers.hpp
          lib/utils/status-control.cpp
          lib/utils/status-control.hpp
          lib/utils/string-list.cpp
          lib/utils/string-list.hpp
          lib/utils/switch-button.cpp
          lib/utils/switch-button.hpp
          lib/utils/sync-helpers.cpp
          lib/utils/sync-helpers.hpp
          lib/utils/tab-helpers.cpp
          lib/utils/tab-helpers.hpp
          lib/utils/temp-variable.cpp
          lib/utils/temp-variable.hpp
          lib/utils/time-helpers.cpp
          lib/utils/time-helpers.hpp
          lib/utils/ui-helpers.cpp
          lib/utils/ui-helpers.hpp
          lib/utils/utility.cpp
          lib/utils/utility.hpp
          lib/utils/volume-control.cpp
          lib/utils/volume-control.hpp
          lib/utils/websocket-api.cpp
          lib/utils/websocket-api.hpp
          lib/variables/variable-line-edit.cpp
          lib/variables/variable-line-edit.hpp
          lib/variables/variable-number.hpp
          lib/variables/variable-number.tpp
          lib/variables/variable-spinbox.cpp
          lib/variables/variable-spinbox.hpp
          lib/variables/variable-string.cpp
          lib/variables/variable-string.hpp
          lib/variables/variable-tab.cpp
          lib/variables/variable-tab.hpp
          lib/variables/variable-text-edit.cpp
          lib/variables/variable-text-edit.hpp
          lib/variables/variable.cpp
          lib/variables/variable.hpp)

# --- End of section ---

# Subfolder for advanced scene switcher plugins
set(ADVSS_PLUGIN_FOLDER "advanced-scene-switcher-plugins")
target_compile_definitions(
  ${LIB_NAME} PRIVATE ADVSS_PLUGIN_FOLDER=\"${ADVSS_PLUGIN_FOLDER}\")

include(cmake/common/advss_helpers.cmake)
setup_obs_lib_dependency(${LIB_NAME})
setup_obs_lib_dependency(${PROJECT_NAME})

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
target_link_libraries(${LIB_NAME} PRIVATE Qt6::Core Qt6::Widgets)

# Ignore QCheckBox::stateChanged deprecation warning until minimum supported Qt
# version is at least Qt 6.7, which introduces QCheckBox::checkStateChanged
if(Qt6_VERSION VERSION_GREATER "6.0.0")
  target_compile_definitions(${LIB_NAME} PRIVATE QT_NO_DEPRECATED_WARNINGS)
endif()

target_compile_options(
  ${PROJECT_NAME}
  PRIVATE
    $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
    -Wno-comma>)
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

target_link_libraries(${PROJECT_NAME} PUBLIC ${LIB_NAME})

# --- Platform-independent build settings ---

target_include_directories(
  ${LIB_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib"
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/legacy"
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/macro"
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/queue"
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/utils"
          "${CMAKE_CURRENT_SOURCE_DIR}/lib/variables"
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

target_include_directories(
  ${LIB_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/obs-websocket/lib"
                     "${CMAKE_CURRENT_SOURCE_DIR}/deps/exprtk")

if(NOT nlohmann_json_DIR
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/json/CMakeLists.txt")
  add_subdirectory(deps/json)
else()
  find_package(nlohmann_json REQUIRED)
endif()
target_link_libraries(${LIB_NAME} PUBLIC nlohmann_json::nlohmann_json)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/jsoncons/CMakeLists.txt")
  # Don't build jsoncons unit tests as they are causing compilation issues and
  # won't be executed either way
  if(OS_MACOS)
    cmake_policy(SET CMP0077 NEW)
  endif()
  set(JSONCONS_BUILD_TESTS
      OFF
      CACHE BOOL "" FORCE)

  add_subdirectory(deps/jsoncons EXCLUDE_FROM_ALL)
  target_link_libraries(${LIB_NAME} PRIVATE jsoncons)
  target_compile_definitions(${LIB_NAME} PRIVATE JSONPATH_SUPPORT=1)
endif()

find_package(CURL QUIET)
find_package(Libcurl QUIET)
if(CURL_FOUND)
  if(NOT DEFINED CURL_INCLUDE_DIRS AND TARGET CURL::libcurl)
    get_target_property(CURL_INCLUDE_DIR CURL::libcurl
                        INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(${LIB_NAME} PUBLIC "${CURL_INCLUDE_DIR}")
  else()
    target_include_directories(${LIB_NAME} PUBLIC "${CURL_INCLUDE_DIRS}")
  endif()
elseif(Libcurl_FOUND)
  target_include_directories(${LIB_NAME} PUBLIC "${LIBCURL_INCLUDE_DIRS}")
else()
  message(FATAL_ERROR "Couldn't find CURL or Libcurl - abort")
endif()

target_compile_definitions(${LIB_NAME} PRIVATE ADVSS_EXPORT_SYMBOLS=1)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(OS_WINDOWS)
  target_compile_definitions(${LIB_NAME} PRIVATE UNICODE _UNICODE)
  if(MSVC)
    target_compile_options(${LIB_NAME} PUBLIC /MP /d2FH4- /wd4267 /wd4267
                                              /bigobj)

    # Workaround for MSVC incompatibility in CI environment
    add_compile_definitions(${target_name} _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
  endif()
  target_sources(${LIB_NAME} PRIVATE lib/win/advanced-scene-switcher-win.cpp)
  add_definitions(-D_WEBSOCKETPP_CPP11_STL_)
  # --- End of section ---

  # -- macOS specific build settings and tasks --
elseif(OS_MACOS)
  set_target_properties(${LIB_NAME} PROPERTIES PREFIX "" SUFFIX ".so")

  find_library(COCOA Cocoa)
  target_include_directories(${LIB_NAME} PRIVATE ${COCOA})
  target_link_libraries(${LIB_NAME} PRIVATE ${COCOA})

  target_sources(${LIB_NAME} PRIVATE lib/osx/advanced-scene-switcher-osx.mm)
  set_source_files_properties(advanced-scene-switcher-osx.mm
                              PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  # --- End of section ---

  # --- Linux-specific build settings and tasks ---
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN")
  set_target_properties(${LIB_NAME} PROPERTIES PREFIX "")
  set_target_properties(${LIB_NAME} PROPERTIES SOVERSION 1)

  find_package(Qt6 REQUIRED COMPONENTS DBus)
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
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libproc2 IMPORTED_TARGET libproc2<4.0.5 QUIET)
    if(libproc2_FOUND)
      target_compile_definitions(${LIB_NAME} PRIVATE PROCPS2_USE_INFO)
    endif()
    pkg_check_modules(libproc2 REQUIRED IMPORTED_TARGET libproc2)
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
  target_sources(${LIB_NAME} PRIVATE lib/linux/advanced-scene-switcher-nix.cpp
                                     lib/linux/kwin-helpers.cpp)

  # Don't include irrelevant folders into sources archive
  list(APPEND CPACK_SOURCE_IGNORE_FILES "\\.deps/.*")
endif()

if(NOT OS_WINDOWS)
  target_compile_options(
    ${LIB_NAME}
    PUBLIC -Wno-error=unused-parameter -Wno-error=conversion -Wno-error=shadow
           -Wno-error=float-conversion -Wno-error=enum-conversion)
endif()

# --- End of section ---

add_subdirectory(plugins)
add_subdirectory(tests)

# --- Install ---

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
  install_advss_lib(${LIB_NAME})
  if(BUILD_OUT_OF_TREE)
    set_target_properties_plugin(${PROJECT_NAME} PROPERTIES OUTPUT_NAME
                                 ${_name})
  else()
    set_target_properties_obs(${PROJECT_NAME} PROPERTIES PREFIX "")
  endif()
endif()
