cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-openvr)

if(NOT WIN32)
  message(
    WARNING "OpenVR condition is only supported on Windows builds for now.")
  return()
endif(NOT WIN32)

add_definitions(-DADVSS_MODULE)

# openvr
if(NOT OpenVR_DIR)
  set(OpenVR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/openvr)
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
  include_directories("${OpenVR_INCLUDE_DIRS}")
else()
  set(OpenVR_LIBRARIES "")
  message(
    WARNING
      "OpenVR not found! Functionality relying on OpenVR will be disabled!\nOpenVR sources are available under: ${CMAKE_CURRENT_SOURCE_DIR}/deps/openvr"
      return
      ())
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../headers")
set(module_SOURCES macro-condition-openvr.cpp macro-condition-openvr.hpp)
add_library(advanced-scene-switcher-openvr MODULE ${module_SOURCES})

if(BUILD_OUT_OF_TREE)
  target_link_libraries(
    advanced-scene-switcher-openvr
    advanced-scene-switcher
    ${LIBOBS_LIB}
    ${LIBOBS_FRONTEND_API_LIB}
    ${OpenVR_LIBRARIES}
    Qt5::Core
    Qt5::Widgets)

  if(UNIX AND NOT APPLE)
    if(NOT LIB_OUT_DIR)
      set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    set_target_properties(advanced-scene-switcher-openvr PROPERTIES PREFIX "")
    install(
      TARGETS advanced-scene-switcher-openvr
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR}/adv-ss-plugins)
  endif()
else()
  target_link_libraries(
    advanced-scene-switcher-openvr
    advanced-scene-switcher
    obs-frontend-api
    ${OpenVR_LIBRARIES}
    Qt5::Core
    Qt5::Widgets
    libobs)
  install_advss_plugin(advanced-scene-switcher-openvr)
endif()
