cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-tests)

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
add_executable(${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE UNIT_TEST)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_sources(
  ${PROJECT_NAME} PRIVATE test-main.cpp mocks/obs-data.cpp
                          mocks/path-helpers.cpp mocks/ui-helpers.cpp)
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE mocks ${ADVSS_SOURCE_DIR}/lib/utils ${ADVSS_SOURCE_DIR}/lib/variables
          ${ADVSS_SOURCE_DIR}/plugins/base/utils)

# --- Qt --- #

target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core Qt::Widgets)

if(TARGET Qt6::Core)
  set(_QT_VERSION
      6
      CACHE INTERNAL "")
elseif(TARGET Qt5::Core)
  set(_QT_VERSION
      5
      CACHE INTERNAL "")
endif()

macro(add_qt_lib lib_name)
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:Qt${_QT_VERSION}::${lib_name}>
      $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endmacro()

add_qt_lib(Core)
add_qt_lib(Gui)
add_qt_lib(Widgets)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

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
  target_compile_options(${PROJECT_NAME} PUBLIC /MP /d2FH4- /wd4267 /wd4267
                                                /bigobj)
else()
  target_compile_options(
    ${PROJECT_NAME} PUBLIC -Wno-error=unused-parameter -Wno-error=conversion
                           -Wno-error=unused-value)
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

# --- #

enable_testing()
