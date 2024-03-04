cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-base)

add_library(${PROJECT_NAME} MODULE)

target_sources(
  ${PROJECT_NAME}
  PRIVATE macro-action-audio.cpp
          macro-action-audio.hpp
          macro-action-clipboard.cpp
          macro-action-clipboard.hpp
          macro-action-file.cpp
          macro-action-file.hpp
          macro-action-filter.cpp
          macro-action-filter.hpp
          macro-action-hotkey.cpp
          macro-action-hotkey.hpp
          macro-action-http.cpp
          macro-action-http.hpp
          macro-action-log.cpp
          macro-action-log.hpp
          macro-action-media.cpp
          macro-action-media.hpp
          macro-action-osc.cpp
          macro-action-osc.hpp
          macro-action-plugin-state.cpp
          macro-action-plugin-state.hpp
          macro-action-profile.cpp
          macro-action-profile.hpp
          macro-action-projector.cpp
          macro-action-projector.hpp
          macro-action-random.cpp
          macro-action-random.hpp
          macro-action-recording.cpp
          macro-action-recording.hpp
          macro-action-replay-buffer.cpp
          macro-action-replay-buffer.hpp
          macro-action-run.cpp
          macro-action-run.hpp
          macro-action-scene-collection.cpp
          macro-action-scene-collection.hpp
          macro-action-scene-lock.cpp
          macro-action-scene-lock.hpp
          macro-action-scene-order.cpp
          macro-action-scene-order.hpp
          macro-action-scene-switch.cpp
          macro-action-scene-switch.hpp
          macro-action-scene-transform.cpp
          macro-action-scene-transform.hpp
          macro-action-scene-visibility.cpp
          macro-action-scene-visibility.hpp
          macro-action-screenshot.cpp
          macro-action-screenshot.hpp
          macro-action-sequence.cpp
          macro-action-sequence.hpp
          macro-action-source.cpp
          macro-action-source.hpp
          macro-action-streaming.cpp
          macro-action-streaming.hpp
          macro-action-studio-mode.cpp
          macro-action-studio-mode.hpp
          macro-action-systray.cpp
          macro-action-systray.hpp
          macro-action-timer.cpp
          macro-action-timer.hpp
          macro-action-transition.cpp
          macro-action-transition.hpp
          macro-action-virtual-cam.cpp
          macro-action-virtual-cam.hpp
          macro-action-wait.cpp
          macro-action-wait.hpp
          macro-action-websocket.cpp
          macro-action-websocket.hpp
          macro-action-window.cpp
          macro-action-window.hpp
          macro-condition-audio.cpp
          macro-condition-audio.hpp
          macro-condition-clipboard.cpp
          macro-condition-clipboard.hpp
          macro-condition-cursor.cpp
          macro-condition-cursor.hpp
          macro-condition-date.cpp
          macro-condition-date.hpp
          macro-condition-display.cpp
          macro-condition-display.hpp
          macro-condition-file.cpp
          macro-condition-file.hpp
          macro-condition-filter.cpp
          macro-condition-filter.hpp
          macro-condition-hotkey.cpp
          macro-condition-hotkey.hpp
          macro-condition-idle.cpp
          macro-condition-idle.hpp
          macro-condition-media.cpp
          macro-condition-media.hpp
          macro-condition-obs-stats.cpp
          macro-condition-obs-stats.hpp
          macro-condition-plugin-state.cpp
          macro-condition-plugin-state.hpp
          macro-condition-process.cpp
          macro-condition-process.hpp
          macro-condition-profile.cpp
          macro-condition-profile.hpp
          macro-condition-recording.cpp
          macro-condition-recording.hpp
          macro-condition-replay-buffer.cpp
          macro-condition-replay-buffer.hpp
          macro-condition-run.cpp
          macro-condition-run.hpp
          macro-condition-scene-order.cpp
          macro-condition-scene-order.hpp
          macro-condition-scene-transform.cpp
          macro-condition-scene-transform.hpp
          macro-condition-scene-visibility.cpp
          macro-condition-scene-visibility.hpp
          macro-condition-scene.cpp
          macro-condition-scene.hpp
          macro-condition-slideshow.cpp
          macro-condition-slideshow.hpp
          macro-condition-source.cpp
          macro-condition-source.hpp
          macro-condition-streaming.cpp
          macro-condition-streaming.hpp
          macro-condition-studio-mode.cpp
          macro-condition-studio-mode.hpp
          macro-condition-timer.cpp
          macro-condition-timer.hpp
          macro-condition-transition.cpp
          macro-condition-transition.hpp
          macro-condition-virtual-cam.cpp
          macro-condition-virtual-cam.hpp
          macro-condition-websocket.cpp
          macro-condition-websocket.hpp
          macro-condition-window.cpp
          macro-condition-window.hpp)

target_sources(
  ${PROJECT_NAME}
  PRIVATE utils/audio-helpers.cpp
          utils/audio-helpers.hpp
          utils/connection-manager.cpp
          utils/connection-manager.hpp
          utils/cursor-helpers.cpp
          utils/cursor-helpers.hpp
          utils/filter-selection.cpp
          utils/filter-selection.hpp
          utils/hotkey-helpers.cpp
          utils/hotkey-helpers.hpp
          utils/json-helpers.cpp
          utils/json-helpers.hpp
          utils/monitor-helpers.cpp
          utils/monitor-helpers.hpp
          utils/osc-helpers.cpp
          utils/osc-helpers.hpp
          utils/process-config.cpp
          utils/process-config.hpp
          utils/profile-helpers.cpp
          utils/profile-helpers.hpp
          utils/scene-item-selection.cpp
          utils/scene-item-selection.hpp
          utils/scene-item-transform-helpers.cpp
          utils/scene-item-transform-helpers.hpp
          utils/source-settings-helpers.cpp
          utils/source-settings-helpers.hpp
          utils/source-setting.cpp
          utils/source-setting.hpp
          utils/string-list.cpp
          utils/string-list.hpp
          utils/striped-frame.cpp
          utils/striped-frame.hpp
          utils/text-helpers.cpp
          utils/text-helpers.hpp
          utils/transition-selection.cpp
          utils/transition-selection.hpp
          utils/websocket-helpers.cpp
          utils/websocket-helpers.hpp)

if(OS_WINDOWS)
  target_sources(${PROJECT_NAME} PRIVATE utils/windows/windows.cpp)
elseif(OS_MACOS)
  target_sources(${PROJECT_NAME} PRIVATE utils/osx/osx.mm)
else()
  target_sources(${PROJECT_NAME} PRIVATE utils/linux/linux.cpp)
  find_package(X11 REQUIRED COMPONENTS Xtst)
  target_include_directories(${PROJECT_NAME} PRIVATE "${X11_INCLUDE_DIR}"
                                                     "${X11_Xtst_INCLUDE_PATH}")
  target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
endif()

target_include_directories(${PROJECT_NAME}
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utils")
get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
add_definitions(-DASIO_STANDALONE)
target_include_directories(
  ${LIB_NAME}
  PUBLIC "${ADVSS_SOURCE_DIR}/deps/asio/asio/include"
         "${ADVSS_SOURCE_DIR}/deps/websocketpp"
         "${ADVSS_SOURCE_DIR}/deps/obs-websocket/lib")

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
install_advss_plugin(${PROJECT_NAME})
