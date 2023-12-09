cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-tests)

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
add_executable(${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE UNIT_TEST)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_sources(
  ${PROJECT_NAME}
  PRIVATE tests.cpp ${ADVSS_SOURCE_DIR}/src/utils/math-helpers.cpp
          ${ADVSS_SOURCE_DIR}/src/utils/obs-module-helper.cpp)
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${ADVSS_SOURCE_DIR}/src" "${ADVSS_SOURCE_DIR}/src/legacy"
          "${ADVSS_SOURCE_DIR}/src/macro-core" "${ADVSS_SOURCE_DIR}/src/utils"
          "${ADVSS_SOURCE_DIR}/forms" "${ADVSS_SOURCE_DIR}/deps/exprtk")

if(MSVC)
  target_compile_options(${PROJECT_NAME} PUBLIC /MP /d2FH4- /wd4267 /wd4267
                                                /bigobj)
endif()

enable_testing()
