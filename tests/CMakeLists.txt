cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-tests)

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
add_executable(${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE UNIT_TEST)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_sources(${PROJECT_NAME} PRIVATE main.cpp mocks/obs-data.cpp
                                       mocks/ui-helpers.cpp mocks/utility.cpp)
target_include_directories(
  ${PROJECT_NAME} PRIVATE mocks ${ADVSS_SOURCE_DIR}/lib/utils
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

# --- json --- #

target_sources(
  ${PROJECT_NAME}
  PRIVATE json.cpp ${ADVSS_SOURCE_DIR}/plugins/base/utils/json-helpers.cpp)

# --- math --- #

target_sources(
  ${PROJECT_NAME}
  PRIVATE math.cpp ${ADVSS_SOURCE_DIR}/lib/utils/math-helpers.cpp
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
  PRIVATE regex.cpp ${ADVSS_SOURCE_DIR}/lib/utils/regex-config.cpp
          ${ADVSS_SOURCE_DIR}/plugins/base/utils/text-helpers.cpp)

# --- #

enable_testing()
