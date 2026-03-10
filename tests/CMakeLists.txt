cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-tests)

set(CMAKE_AUTOUIC ON)

add_executable(${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE UNIT_TEST
                                                   ADVSS_EXPORT_SYMBOLS=1)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_sources(${PROJECT_NAME} PRIVATE test-main.cpp)

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
get_target_property(ADVSS_BINARY_DIR advanced-scene-switcher-lib BINARY_DIR)

target_include_directories(
  ${PROJECT_NAME}
  PRIVATE ${ADVSS_SOURCE_DIR}/lib ${ADVSS_SOURCE_DIR}/lib/macro
          ${ADVSS_SOURCE_DIR}/lib/utils ${ADVSS_SOURCE_DIR}/lib/variables
          ${ADVSS_SOURCE_DIR}/plugins/base/utils)

setup_obs_lib_dependency(${PROJECT_NAME})

# --- Qt --- #

if(TARGET Qt6::Core)
  set(_QT_VERSION
      6
      CACHE INTERNAL "")
elseif(TARGET Qt5::Core)
  set(_QT_VERSION
      5
      CACHE INTERNAL "")
endif()

# Include autogen headers so that generated ui_*.h files are found
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${ADVSS_BINARY_DIR}/advanced-scene-switcher-lib_autogen/include")
foreach(_conf Release RelWithDebInfo Debug MinSizeRe)
  target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
      "${ADVSS_BINARY_DIR}/advanced-scene-switcher-lib_autogen/include_${_conf}"
  )
endforeach()

target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core Qt::Widgets)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
             AUTOUIC_SEARCH_PATHS "${ADVSS_SOURCE_DIR}/forms")

# --- condition-logic --- #

target_sources(
  ${PROJECT_NAME} PRIVATE test-condition-logic.cpp
                          ${ADVSS_SOURCE_DIR}/lib/utils/condition-logic.cpp)

# --- duration-modifier --- #

target_sources(
  ${PROJECT_NAME}
  PRIVATE test-duration-modifier.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/duration-modifier.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/duration.cpp)

# --- json --- #

if(TARGET jsoncons)
  target_link_libraries(${PROJECT_NAME} PRIVATE jsoncons)
  target_compile_definitions(${PROJECT_NAME} PRIVATE JSONPATH_SUPPORT=1)
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)
target_sources(
  ${PROJECT_NAME} PRIVATE test-json.cpp
                          ${ADVSS_SOURCE_DIR}/lib/utils/json-helpers.cpp)

# --- math --- #

target_sources(
  ${PROJECT_NAME}
  PRIVATE test-math.cpp ${ADVSS_SOURCE_DIR}/lib/utils/math-helpers.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/obs-module-helper.cpp)
target_include_directories(${PROJECT_NAME}
                           PRIVATE ${ADVSS_SOURCE_DIR}/deps/exprtk)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PUBLIC /MP /d2FH4- /wd4267 /bigobj)
else()
  target_compile_options(
    ${PROJECT_NAME} PUBLIC -Wno-error=unused-parameter -Wno-error=conversion
                           -Wno-error=unused-value -Wno-error=unused-variable)
endif()

# --- regex --- #

target_sources(
  ${PROJECT_NAME}
  PRIVATE test-regex.cpp ${ADVSS_SOURCE_DIR}/lib/utils/regex-config.cpp
          ${ADVSS_SOURCE_DIR}/plugins/base/utils/text-helpers.cpp)

# --- utility --- #

target_sources(
  ${PROJECT_NAME} PRIVATE test-utility.cpp
                          ${ADVSS_SOURCE_DIR}/lib/utils/utility.cpp)

# --- variable --- #

if(OS_WINDOWS)
  target_compile_options(${PROJECT_NAME} PRIVATE /wd4100)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wno-error=shadow
                                                 -Wno-unused-parameter)
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE test-variable.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/filter-combo-box.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/item-selection-helpers.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/name-dialog.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/resizing-text-edit.cpp
          ${ADVSS_SOURCE_DIR}/lib/variables/variable.cpp)

# --- macro / macro-condition --- #

target_sources(
  ${PROJECT_NAME}
  PRIVATE test-macro.cpp
          test-macro-condition.cpp
          stubs/path-helpers.cpp
          stubs/ui-helpers.cpp
          stubs/plugin-state-helpers.cpp
          stubs/macro-action-macro.cpp
          stubs/macro-edit.cpp
          stubs/macro-dock-settings.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/temp-variable.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/sync-helpers.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/help-icon.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/auto-update-tooltip-label.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/duration-control.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/duration-control.hpp
          ${ADVSS_SOURCE_DIR}/lib/utils/duration-modifier.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/duration-modifier.hpp
          ${ADVSS_SOURCE_DIR}/lib/utils/duration.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/duration.hpp
          ${ADVSS_SOURCE_DIR}/lib/utils/list-controls.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/list-editor.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/layout-helpers.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/log-helper.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/mouse-wheel-guard.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/section.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/splitter-helpers.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/resizable-widget.cpp
          ${ADVSS_SOURCE_DIR}/lib/utils/string-list.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-action.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-action-macro.hpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-action-factory.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-condition.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-condition-factory.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-edit.hpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-helpers.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-input.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-ref.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-segment.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-selection.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro-signals.cpp
          ${ADVSS_SOURCE_DIR}/lib/macro/macro.cpp
          ${ADVSS_SOURCE_DIR}/lib/variables/variable-line-edit.cpp
          ${ADVSS_SOURCE_DIR}/lib/variables/variable-spinbox.cpp
          ${ADVSS_SOURCE_DIR}/lib/variables/variable-string.cpp)

# --- Testing --- #

enable_testing()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

if(WIN32)
  # Copy all CMake-known transitive DLLs (Qt, OBS, etc.) next to the binary.
  # This makes the test executable runnable directly without PATH changes.
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS)

  # Copy remaining OBS deps (zlib, srt, librist, ffmpeg, etc.) that are not
  # CMake targets but are required at runtime. Derive the path from ZLIB::ZLIB
  # which lives in the same deps directory.
  get_target_property(_zlib_lib ZLIB::ZLIB IMPORTED_LOCATION_RELWITHDEBINFO)
  if(NOT _zlib_lib)
    get_target_property(_zlib_lib ZLIB::ZLIB IMPORTED_LOCATION_RELEASE)
  endif()
  if(_zlib_lib)
    get_filename_component(_obs_deps_bin "${_zlib_lib}" DIRECTORY)
    get_filename_component(_obs_deps_bin "${_obs_deps_bin}/../bin" ABSOLUTE)
    add_custom_command(
      TARGET ${PROJECT_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${_obs_deps_bin}"
              $<TARGET_FILE_DIR:${PROJECT_NAME}>)
  else()
    message(
      WARNING "Could not determine OBS deps bin directory from ZLIB::ZLIB - "
              "some DLLs may be missing at runtime")
  endif()

elseif(APPLE)
  # On macOS, binaries use RPATH so copying is usually not needed. Fall back to
  # DYLD_LIBRARY_PATH in case RPATH is not set correctly.
  set_tests_properties(
    ${PROJECT_NAME}
    PROPERTIES
      ENVIRONMENT
      "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:OBS::libobs>:$<TARGET_FILE_DIR:Qt${_QT_VERSION}::Core>"
  )

else()
  # On Linux, same approach using LD_LIBRARY_PATH.
  set_tests_properties(
    ${PROJECT_NAME}
    PROPERTIES
      ENVIRONMENT
      "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:OBS::libobs>:$<TARGET_FILE_DIR:Qt${_QT_VERSION}::Core>"
  )
endif()
