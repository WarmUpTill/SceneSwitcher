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

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

if(OS_WINDOWS)
  if(MSVC)
    target_compile_options(
      libremidi
      PRIVATE /wd4018
              /wd4189
              /wd4251
              /wd4267
              /wd4275
              /wd4389
              /wd4505
              /wd4702
      PUBLIC /wd4068 /wd4100 /wd4101 /wd4244)
    target_compile_options(${PROJECT_NAME} PRIVATE /wd4017)
  endif()
else()
  target_compile_options(
    libremidi
    PUBLIC -Wno-error=conversion -Wno-error=unused-result -Wno-error=shadow
           -Wno-error=unused-parameter -Wno-error=unused-variable
           -Wno-error=sign-compare)
  if(OS_MACOS)
    target_compile_options(libremidi PUBLIC -Wno-error=vla-extension)
  endif()
endif()

if(OS_MACOS)
  set(PATCH_FILE
      "${CMAKE_CURRENT_SOURCE_DIR}/remove-bit-cast-for-macos-build.patch")
  execute_process(
    COMMAND git apply ${PATCH_FILE}
    WORKING_DIRECTORY ${LIBREMIDI_DIR}
    RESULT_VARIABLE PATCH_RESULT)
  if(PATCH_RESULT EQUAL 0)
    message("Libremidi patch \"${PATCH_FILE}\" applied successfully!")
  else()
    message(
      WARNING "Libremidi patch \"${PATCH_FILE}\" did NOT apply successfully!")
  endif()
endif()

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
