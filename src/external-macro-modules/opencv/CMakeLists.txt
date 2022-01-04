cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-opencv)

add_definitions(-DADVSS_MODULE)

find_package(OpenCV)
if(OpenCV_FOUND)
  include_directories("${OpenCV_INCLUDE_DIRS}")
else()
  set(OpenCV_LIBRARIES "")
  message(
    WARNING
      "OpenCV not found! Functionality relying on OpenCV will be disabled!\nOpenCV sources are available under: ${CMAKE_CURRENT_SOURCE_DIR}/deps/opencv"
  )
  return()
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../headers")
set(module_SOURCES macro-condition-video.cpp macro-condition-video.hpp)
add_library(advanced-scene-switcher-opencv MODULE ${module_SOURCES})

if(BUILD_OUT_OF_TREE)
  target_link_libraries(
    advanced-scene-switcher-opencv
    advanced-scene-switcher
    ${LIBOBS_LIB}
    ${LIBOBS_FRONTEND_API_LIB}
    ${OpenCV_LIBRARIES}
    Qt5::Core
    Qt5::Widgets)

  if(UNIX AND NOT APPLE)
    if(NOT LIB_OUT_DIR)
      set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    set_target_properties(advanced-scene-switcher-opencv PROPERTIES PREFIX "")
    install(TARGETS advanced-scene-switcher-opencv
            LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
  endif()
else()
  target_link_libraries(
    advanced-scene-switcher-opencv
    advanced-scene-switcher
    obs-frontend-api
    ${OpenCV_LIBRARIES}
    Qt5::Core
    Qt5::Widgets
    libobs)
  install_obs_plugin(advanced-scene-switcher-opencv)
endif()
