cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-openvr)

# --- Check OpenCV requirements ---

if(NOT WIN32)
  message(
    WARNING "OpenVR condition is only supported on Windows builds for now.")
  return()
endif(NOT WIN32)

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)
target_sources(${PROJECT_NAME} PRIVATE macro-condition-openvr.cpp
                                       macro-condition-openvr.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

# --- OpenVR build settings ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)

if(NOT OpenVR_DIR)
  set(OpenVR_DIR ${ADVSS_SOURCE_DIR}/deps/openvr)
endif()

if(EXISTS ${OpenVR_DIR})
  set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})
  if((NOT APPLE) AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
    set(PROCESSOR_ARCH "64")
  else()
    set(PROCESSOR_ARCH "32")
  endif()
  if(WIN32)
    set(PLATFORM_NAME "win")
  elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
      set(PLATFORM_NAME "linux")
    endif()
  elseif(APPLE)
    if(CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*" OR CMAKE_SYSTEM_NAME MATCHES
                                                 ".*MacOS.*")
      set(PLATFORM_NAME "osx")
    endif()
  endif()
  set(OpenVR_INCLUDE_DIRS ${OpenVR_DIR}/headers)
  set(OpenVR_BINARIES
      ${OpenVR_DIR}/bin/${PLATFORM_NAME}${PROCESSOR_ARCH}/${CMAKE_SHARED_LIBRARY_PREFIX}openvr_api${CMAKE_SHARED_LIBRARY_SUFFIX}
  )
  set(OpenVR_LIBRARIES
      ${OpenVR_DIR}/lib/${PLATFORM_NAME}${PROCESSOR_ARCH}/${CMAKE_SHARED_LIBRARY_PREFIX}openvr_api${CMAKE_IMPORT_LIBRARY_SUFFIX}
  )
  set(OpenVR_FOUND TRUE)
endif()

if(OpenVR_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE "${OpenVR_INCLUDE_DIRS}")
else()
  set(OpenVR_LIBRARIES "")
  message(
    WARNING
      "OpenVR not found! Functionality relying on OpenVR will be disabled!\nOpenVR sources are available under: ${CMAKE_CURRENT_SOURCE_DIR}/deps/openvr"
  )
  return()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenVR_LIBRARIES})

# --- End of section ---

install_advss_plugin(${PROJECT_NAME})
install_advss_plugin_dependency(TARGET ${PROJECT_NAME} DEPENDENCIES
                                ${OpenVR_BINARIES})
