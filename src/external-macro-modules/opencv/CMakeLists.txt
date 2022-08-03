cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-opencv)

# --- Check OpenCV requirements ---

find_package(OpenCV)
if(NOT OpenCV_FOUND)
  message(
    WARNING
      "OpenCV not found! Video condition will be disabled!\nOpenCV sources are available under: ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/opencv"
  )
  return()
endif()

# --- End of section ---

add_library(advanced-scene-switcher-opencv MODULE)
target_sources(
  ${PROJECT_NAME}
  PRIVATE area-selection.cpp
          area-selection.hpp
          macro-condition-video.cpp
          macro-condition-video.hpp
          opencv-helpers.cpp
          opencv-helpers.hpp
          preview-dialog.cpp
          preview-dialog.hpp
          threshold-slider.cpp
          threshold-slider.hpp
          video-selection.cpp
          video-selection.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

# --- OpenCV build settings ---

target_include_directories(${PROJECT_NAME} PRIVATE "${OpenCV_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARIES})

# --- End of section ---

install_advss_plugin(${PROJECT_NAME})
if(NOT OS_LINUX)
  install_advss_plugin_dependency(TARGET ${PROJECT_NAME} DEPENDENCIES
                                  ${OpenCV_LIBS})
endif()
