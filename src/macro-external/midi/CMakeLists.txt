cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-midi)

# --- Check libremidi requirements ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
add_subdirectory("${ADVSS_SOURCE_DIR}/deps/libremidi"
                 "${ADVSS_SOURCE_DIR}/deps/libremidi/build")

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

target_sources(
  ${PROJECT_NAME}
  PRIVATE macro-condition-midi.cpp macro-condition-midi.hpp
          macro-action-midi.cpp macro-action-midi.hpp midi-helpers.cpp
          midi-helpers.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME}
                           PRIVATE "${ADVSS_SOURCE_DIR}/deps/libremidi/include")
target_link_libraries(${PROJECT_NAME} PRIVATE libremidi)
install_advss_plugin(${PROJECT_NAME})
