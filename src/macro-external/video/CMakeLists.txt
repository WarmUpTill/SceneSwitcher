cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-opencv)

# --- Check OpenCV requirements ---

find_package(OpenCV)
if(NOT OpenCV_FOUND)
  message(WARNING "OpenCV not found!\n" "Video condition will be disabled!\n\n"
                  "OpenCV sources are available under: ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/opencv")
  return()
endif()

# --- Check optional OCR dependencies ---

find_package(Leptonica)
find_package(Tesseract)

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

if(Leptonica_FOUND AND Tesseract_FOUND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE OCR_SUPPORT)
  target_link_libraries(${PROJECT_NAME} PRIVATE Tesseract::libtesseract ${Leptonica_LIBRARIES})
  target_include_directories(${PROJECT_NAME} PRIVATE ${Tesseract_INCLUDE_DIRS} ${Leptonica_INCLUDE_DIRS})
else()
  message(WARNING "OCR capabilities of video condition disabled!\n"
                  "Leptonica or Tesseract dependencies were not found!\n\n"
                  "Sources are available under: ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/")
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE area-selection.cpp
          area-selection.hpp
          macro-condition-video.cpp
          macro-condition-video.hpp
          opencv-helpers.cpp
          opencv-helpers.hpp
          paramerter-wrappers.cpp
          paramerter-wrappers.hpp
          preview-dialog.cpp
          preview-dialog.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
if(OS_WINDOWS)
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARIES})

install_advss_plugin(${PROJECT_NAME})
if(NOT OS_LINUX)
  install_advss_plugin_dependency(TARGET ${PROJECT_NAME} DEPENDENCIES ${OpenCV_LIBS})
endif()
