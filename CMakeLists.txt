cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
endif()

# Compiler settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

if(CMAKE_COMPILER_IS_GNUCC
   OR CMAKE_COMPILER_IS_GNUCXX
   OR CMAKE_COMPILER_IS_CLANG)
  set(CMAKE_CXX_FLAGS
      "-Wall -Wextra -Wvla -Wno-unused-function -Wno-missing-field-initializers ${CMAKE_CXX_FLAGS} -fno-strict-aliasing"
  )
  set(CMAKE_C_FLAGS
      "-Wall -Wextra -Wvla -Wno-unused-function -Werror-implicit-function-declaration -Wno-missing-braces -Wno-missing-field-initializers ${CMAKE_C_FLAGS} -std=gnu99 -fno-strict-aliasing"
  )

  option(USE_LIBC++ "Use libc++ instead of libstdc++" ${APPLE})
  if(USE_LIBC++)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif()
elseif(MSVC)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()

  # Disable pointless constant condition warnings
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} /wd4127 /wd4201 /wd4456 /wd4457 /wd4458 /wd4459 /wd4595"
  )
  add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
                  -D_CRT_NONSTDC_NO_WARNINGS)
endif()

# Generate version info
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp" @ONLY)

# Windows installer
if(WIN32)
  get_filename_component(ISS_PLUGIN_FILES_DIR
                         "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
  file(TO_NATIVE_PATH "${ISS_PLUGIN_FILES_DIR}" ISS_PLUGIN_FILES_DIR)
  get_filename_component(ISS_MSVC_REDIST_HELPER_DIR "${CMAKE_BINARY_DIR}\\.."
                         ABSOLUTE)
  file(TO_NATIVE_PATH "${ISS_MSVC_REDIST_HELPER_DIR}"
       ISS_MSVC_REDIST_HELPER_DIR)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/CI/windows/setup.iss.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/CI/windows/setup.iss" @ONLY)
endif()

# Out of tree specific settings
if(BUILD_OUT_OF_TREE)
  set(CMAKE_PREFIX_PATH "${QTDIR}")
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  find_package(Qt5Core REQUIRED)
  find_package(Qt5Widgets REQUIRED)
  find_package(LibObs)
  find_package(LibObs-frontend-api)
  if(LibObs_FOUND)
    set(LIBOBS_LIB ${LIBOBS_LIBRARIES})
    set(LIBOBS_INCLUDE_DIR ${LIBOBS_INCLUDE_DIRS})
  endif()
  if(LibObs-frontend-api_FOUND)
    set(LIBOBS_FRONTEND_API_LIB ${LIBOBS-FRONTEND-API_LIBRARIES})
    set(LIBOBS_FRONTEND_INCLUDE_DIR ${LIBOBS-FRONTEND-API_INCLUDE_DIR})
  endif()

  if(NOT LIBOBS_LIB)
    message(FATAL_ERROR "obs library not found - please set LIBOBS_LIB")
  endif()
  if(NOT LIBOBS_FRONTEND_API_LIB)
    message(
      FATAL_ERROR
        "libobs frontend-api library not found - please set LIBOBS_FRONTEND_API_LIB"
    )
  endif()
  if(NOT LIBOBS_INCLUDE_DIR)
    message(
      FATAL_ERROR "obs.hpp header not found - please set LIBOBS_INCLUDE_DIR")
  endif()
  if(NOT LIBOBS_FRONTEND_INCLUDE_DIR)
    message(
      FATAL_ERROR
        " obs-frontend-api.h not found - please set LIBOBS_FRONTEND_INCLUDE_DIR"
    )
  endif()

  include_directories("${LIBOBS_INCLUDE_DIR}" "${LIBOBS_FRONTEND_INCLUDE_DIR}"
                      ${Qt5Core_INCLUDES} ${Qt5Widgets_INCLUDES})

  find_package(CURL REQUIRED)
  include_directories("${CURL_INCLUDE_DIRS}")

  # Backwards compatability with older OBS versions
  if(LibObs_VERSION_MAJOR GREATER_EQUAL 27)
    add_definitions(-DVCAM_SUPPORTED)
  else()
    message(
      WARNING
        "OBS version ${LibObs_VERSION_MAJOR} found - disabling virtual camera functionality"
    )
  endif()
  if(LibObs_VERSION_MAJOR GREATER_EQUAL 26)
    add_definitions(-DREPLAYBUFFER_SUPPORTED)
  else()
    message(
      WARNING
        "OBS version ${LibObs_VERSION_MAJOR} found - disabling replay buffer functionality"
    )
  endif()
else()
  find_package(Libcurl REQUIRED)
  include_directories("${LIBCURL_INCLUDE_DIRS}")
  add_definitions(-DVCAM_SUPPORTED)
  add_definitions(-DREPLAYBUFFER_SUPPORTED)
endif()

# Platform specific settings
if(APPLE)
  find_library(COCOA Cocoa)
  if(BUILD_OUT_OF_TREE)
    find_package(Qt5MacExtras REQUIRED)
  endif()
  include_directories(${COCOA})
endif()

if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED COMPONENTS Xtst Xss)
  link_libraries(${X11_LIBRARIES})
  include_directories("${X11_INCLUDE_DIR}" "${X11_Xtst_INCLUDE_PATH}"
                      "${X11_Xss_INCLUDE_PATH}")
endif()

if(WIN32)
  set(advanced-scene-switcher_PLATFORM_SOURCES
      src/win/advanced-scene-switcher-win.cpp)
elseif(APPLE)
  set(advanced-scene-switcher_PLATFORM_SOURCES
      src/osx/advanced-scene-switcher-osx.mm)
  set_source_files_properties(advanced-scene-switcher-osx.mm
                              PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  set(advanced-scene-switcher_PLATFORM_LIBS ${COCOA})
else()
  set(advanced-scene-switcher_PLATFORM_SOURCES
      src/linux/advanced-scene-switcher-nix.cpp)
  set(advanced-scene-switcher_PLATFORM_LIBS Xss)
endif()

# asio and websocketpp
add_definitions(-DASIO_STANDALONE)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include"
                    "${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp")
if(WIN32)
  add_definitions(-D_WEBSOCKETPP_CPP11_STL_)
endif()

# opencv
find_package(OpenCV REQUIRED)
include_directories("${OpenCV_INCLUDE_DIRS}")

# Setup QT tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/forms")
set(advanced-scene-switcher_UI ${advanced-scene-switcher_UI}
                               forms/advanced-scene-switcher.ui)
qt5_wrap_ui(advanced-scene-switcher_UI_HEADERS ${advanced-scene-switcher_UI}
            ${advanced-scene-switcher_PLATFORM_UI})

# The plugin sources
set(advanced-scene-switcher_HEADERS
    ${advanced-scene-switcher_HEADERS}
    src/headers/advanced-scene-switcher.hpp
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
    src/headers/macro-action-edit.hpp
    src/headers/macro-action-audio.hpp
    src/headers/macro-action-file.hpp
    src/headers/macro-action-filter.hpp
    src/headers/macro-action-hotkey.hpp
    src/headers/macro-action-macro.hpp
    src/headers/macro-action-media.hpp
    src/headers/macro-action-plugin-state.hpp
    src/headers/macro-action-preview-scene.hpp
    src/headers/macro-action-recording.hpp
    src/headers/macro-action-replay-buffer.hpp
    src/headers/macro-action-run.hpp
    src/headers/macro-action-scene-order.hpp
    src/headers/macro-action-scene-swap.hpp
    src/headers/macro-action-scene-switch.hpp
    src/headers/macro-action-scene-transform.hpp
    src/headers/macro-action-scene-visibility.hpp
    src/headers/macro-action-source.hpp
    src/headers/macro-action-streaming.hpp
    src/headers/macro-action-virtual-cam.hpp
    src/headers/macro-action-wait.hpp
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
    src/headers/macro-condition-plugin-state.hpp
    src/headers/macro-condition-process.hpp
    src/headers/macro-condition-recording.hpp
    src/headers/macro-condition-replay-buffer.hpp
    src/headers/macro-condition-scene-order.hpp
    src/headers/macro-condition-scene-transform.hpp
    src/headers/macro-condition-scene.hpp
    src/headers/macro-condition-source.hpp
    src/headers/macro-condition-streaming.hpp
    src/headers/macro-condition-timer.hpp
    src/headers/macro-condition-video.hpp
    src/headers/macro-condition-virtual-cam.hpp
    src/headers/macro-condition-window.hpp
    src/headers/macro.hpp
    src/headers/macro-controls.hpp
    src/headers/macro-segment.hpp
    src/headers/macro-selection.hpp
    src/headers/curl-helper.hpp
    src/headers/hotkey.hpp
    src/headers/scene-selection.hpp
    src/headers/screenshot-helper.hpp
    src/headers/name-dialog.hpp
    src/headers/duration-control.hpp
    src/headers/file-selection.hpp
    src/headers/section.hpp
    src/headers/platform-funcs.hpp
    src/headers/utility.hpp
    src/headers/volume-control.hpp
    src/headers/version.h)

set(advanced-scene-switcher_SOURCES
    ${advanced-scene-switcher_SOURCES}
    src/advanced-scene-switcher.cpp
    src/advanced-scene-switcher-module.c
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
    src/macro-action-edit.cpp
    src/macro-action-audio.cpp
    src/macro-action-file.cpp
    src/macro-action-filter.cpp
    src/macro-action-hotkey.cpp
    src/macro-action-macro.cpp
    src/macro-action-media.cpp
    src/macro-action-plugin-state.cpp
    src/macro-action-preview-scene.cpp
    src/macro-action-recording.cpp
    src/macro-action-replay-buffer.cpp
    src/macro-action-run.cpp
    src/macro-action-scene-order.cpp
    src/macro-action-scene-swap.cpp
    src/macro-action-scene-switch.cpp
    src/macro-action-scene-transform.cpp
    src/macro-action-scene-visibility.cpp
    src/macro-action-source.cpp
    src/macro-action-streaming.cpp
    src/macro-action-virtual-cam.cpp
    src/macro-action-wait.cpp
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
    src/macro-condition-plugin-state.cpp
    src/macro-condition-process.cpp
    src/macro-condition-recording.cpp
    src/macro-condition-replay-buffer.cpp
    src/macro-condition-scene-order.cpp
    src/macro-condition-scene-transform.cpp
    src/macro-condition-scene.cpp
    src/macro-condition-source.cpp
    src/macro-condition-streaming.cpp
    src/macro-condition-timer.cpp
    src/macro-condition-video.cpp
    src/macro-condition-virtual-cam.cpp
    src/macro-condition-window.cpp
    src/macro.cpp
    src/macro-controls.cpp
    src/macro-segment.cpp
    src/macro-selection.cpp
    src/macro-tab.cpp
    src/curl-helper.cpp
    src/scene-selection.cpp
    src/screenshot-helper.cpp
    src/name-dialog.cpp
    src/duration-control.cpp
    src/section.cpp
    src/utility.cpp
    src/volume-control.cpp
    src/version.cpp)

add_library(
  advanced-scene-switcher MODULE
  ${advanced-scene-switcher_HEADERS}
  ${advanced-scene-switcher_SOURCES}
  ${advanced-scene-switcher_UI_HEADERS}
  ${advanced-scene-switcher_PLATFORM_SOURCES}
  ${advanced-scene-switcher_PLATFORM_HEADERS})

# Out of tree build
if(BUILD_OUT_OF_TREE)
  target_link_libraries(
    advanced-scene-switcher
    ${advanced-scene-switcher_PLATFORM_LIBS}
    ${OpenCV_LIBRARIES}
    ${LIBOBS_LIB}
    ${LIBOBS_FRONTEND_API_LIB}
    Qt5::Core
    Qt5::Widgets)

  # Additional commands to install the module in the correct place. Find all the
  # translation files so we can copy them to the correct place later on.
  file(GLOB ASS_TRANSLATION_FILES "data/locale/*.ini")

  # OSX
  if(APPLE)
    set_target_properties(advanced-scene-switcher PROPERTIES PREFIX "")
  endif()

  # Linux
  if(UNIX AND NOT APPLE)
    if(NOT LIB_OUT_DIR)
      set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
      set(DATA_OUT_DIR "/share/obs/obs-plugins/advanced-scene-switcher")
    endif()
    set_target_properties(advanced-scene-switcher PROPERTIES PREFIX "")
    install(TARGETS advanced-scene-switcher
            LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
            DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
    install(DIRECTORY data/res
            DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
  endif()
else()
  # In tree build
  target_link_libraries(
    advanced-scene-switcher
    ${advanced-scene-switcher_PLATFORM_LIBS}
    ${OpenCV_LIBRARIES}
    obs-frontend-api
    Qt5::Widgets
    libobs)
  install_obs_plugin_with_data(advanced-scene-switcher data)
endif()
