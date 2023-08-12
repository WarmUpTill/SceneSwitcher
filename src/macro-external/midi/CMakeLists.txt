cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-midi)

# --- Check libremidi requirements ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
set(LIBREMIDI_DIR "${ADVSS_SOURCE_DIR}/deps/libremidi")
if(NOT EXISTS "${LIBREMIDI_DIR}/CMakeLists.txt")
  message(WARNING "libremidi directory \"${LIBREMIDI_DIR}\" not found!\n"
                  "MIDI support will be disabled!")
  return()
endif()
add_subdirectory("${LIBREMIDI_DIR}" "${LIBREMIDI_DIR}/build" EXCLUDE_FROM_ALL)

if(MSVC)
  target_compile_options(libremidi PRIVATE /wd4251 /wd4267 /wd4275)
endif(MSVC)

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

target_sources(
  ${PROJECT_NAME}
  PRIVATE macro-condition-midi.cpp macro-condition-midi.hpp
          macro-action-midi.cpp macro-action-midi.hpp midi-helpers.cpp
          midi-helpers.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME} PRIVATE "${LIBREMIDI_DIR}/include")
target_link_libraries(${PROJECT_NAME} PRIVATE libremidi)
install_advss_plugin(${PROJECT_NAME})
