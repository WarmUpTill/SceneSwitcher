cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-speech)

# --- Check requirements ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
set(WHISPER_DIR "${ADVSS_SOURCE_DIR}/deps/whisper.cpp")
if(NOT EXISTS "${WHISPER_DIR}/CMakeLists.txt")
  message(WARNING "whisper.cpp directory \"${WHISPER_DIR}\" not found!\n"
                  "Speech condition will be disabled!\n\n"
                  "Clone whisper.cpp into: ${WHISPER_DIR}")
  return()
endif()

set(WHISPER_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS
    OFF
    CACHE BOOL "" FORCE)

# Vulkan GPU backend. Auto-enabled when VULKAN_SDK is set (e.g. in CI).
option(ADVSS_SPEECH_VULKAN "Use Vulkan GPU backend for speech recognition" OFF)

if(NOT ADVSS_SPEECH_VULKAN AND DEFINED ENV{VULKAN_SDK})
  set(ADVSS_SPEECH_VULKAN ON)
endif()

if(ADVSS_SPEECH_VULKAN)
  set(GGML_VULKAN
      ON
      CACHE BOOL "" FORCE)
endif()

# Suppress warnings-as-errors on all targets in a whisper.cpp source subtree.
function(_advss_whisper_suppress_werror dir)
  get_property(
    _subdirs
    DIRECTORY "${dir}"
    PROPERTY SUBDIRECTORIES)
  foreach(_sub IN LISTS _subdirs)
    _advss_whisper_suppress_werror("${_sub}")
  endforeach()
  get_property(
    _targets
    DIRECTORY "${dir}"
    PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_target IN LISTS _targets)
    get_target_property(_type ${_target} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY" OR _type STREQUAL "UTILITY")
      continue()
    endif()
    get_target_property(_opts ${_target} COMPILE_OPTIONS)
    if(_opts)
      list(FILTER _opts EXCLUDE REGEX "^-Werror")
      list(REMOVE_ITEM _opts /WX)
      set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${_opts}")
    endif()
    target_compile_options(
      ${_target}
      PRIVATE $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-error>
              $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-error>
              $<$<C_COMPILER_ID:MSVC>:/WX-> $<$<CXX_COMPILER_ID:MSVC>:/WX->)
  endforeach()
endfunction()

set(_advss_saved_werror ${CMAKE_COMPILE_WARNING_AS_ERROR})
set(_advss_saved_pic ${CMAKE_POSITION_INDEPENDENT_CODE})
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT TARGET whisper)
  add_subdirectory("${WHISPER_DIR}" "${CMAKE_BINARY_DIR}/whisper.cpp"
                   EXCLUDE_FROM_ALL)
  _advss_whisper_suppress_werror("${WHISPER_DIR}")
  # ggml-metal's .m files use manual retain/release; disable ARC on that target.
  if(TARGET ggml-metal)
    set_target_properties(ggml-metal
                          PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC NO)
    target_compile_options(ggml-metal
                           PRIVATE $<$<COMPILE_LANGUAGE:OBJC>:-fno-objc-arc>)
  endif()
endif()
set(CMAKE_COMPILE_WARNING_AS_ERROR ${_advss_saved_werror})
set(CMAKE_POSITION_INDEPENDENT_CODE ${_advss_saved_pic})

# ggml-vulkan finds SPIRV-Headers but does not link it.
if(ADVSS_SPEECH_VULKAN
   AND TARGET ggml-vulkan
   AND TARGET SPIRV-Headers::SPIRV-Headers)
  target_link_libraries(ggml-vulkan PRIVATE SPIRV-Headers::SPIRV-Headers)
endif()

# OBS deps ship older Vulkan headers; put the SDK headers first so ggml-vulkan
# sees the full API.
if(ADVSS_SPEECH_VULKAN
   AND TARGET ggml-vulkan
   AND DEFINED ENV{VULKAN_SDK})
  if(WIN32)
    set(_vk_sdk_include "$ENV{VULKAN_SDK}/Include")
  else()
    set(_vk_sdk_include "$ENV{VULKAN_SDK}/include")
  endif()
  if(EXISTS "${_vk_sdk_include}")
    target_include_directories(ggml-vulkan BEFORE PRIVATE "${_vk_sdk_include}")
  endif()
endif()

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

target_sources(
  ${PROJECT_NAME} PRIVATE macro-condition-speech.cpp macro-condition-speech.hpp
                          speech-recognizer.cpp speech-recognizer.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

target_include_directories(${PROJECT_NAME} PRIVATE "${WHISPER_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE whisper)

install_advss_plugin(${PROJECT_NAME})
