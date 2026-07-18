cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-opencv)

# --- Check OpenCV requirements ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)

find_package(OpenCV)
if(NOT OpenCV_FOUND)
  message(
    WARNING
      "OpenCV not found!\n" "Video condition will be disabled!\n\n"
      "OpenCV sources are available under: ${ADVSS_SOURCE_DIR}/deps/opencv")
  return()
endif()

# --- Check optional OCR dependencies ---

find_package(Leptonica QUIET)
find_package(Tesseract QUIET)

if(OS_LINUX AND NOT Leptonica_FOUND)
  find_package(PkgConfig REQUIRED)

  pkg_check_modules(Tesseract IMPORTED_TARGET tesseract lept)
  if(Tesseract_FOUND)
    add_library(Tesseract::libtesseract ALIAS PkgConfig::Tesseract)
    set(Leptonica_FOUND TRUE)
    set(Tesseract_FOUND TRUE)
  endif()
endif()

# --- Check cascade classifier availability ---
# CascadeClassifier was moved to the xobjdetect contrib module in OpenCV 5
if(OpenCV_VERSION_MAJOR LESS 5
   OR OpenCV_xobjdetect_FOUND
   OR "opencv_xobjdetect" IN_LIST OpenCV_LIBS)
  set(ADVSS_CASCADE_SUPPORT ON)
else()
  message(
    WARNING
      "Cascade classifier support disabled!\n"
      "OpenCV 5+ requires the opencv_xobjdetect contrib module.\n\n"
      "Rebuild OpenCV with -DBUILD_LIST=...,xobjdetect and -DOPENCV_EXTRA_MODULES_PATH=<opencv_contrib>/modules"
  )
endif()

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

if(ADVSS_CASCADE_SUPPORT)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ADVSS_CASCADE_SUPPORT)
endif()

if(Leptonica_FOUND AND Tesseract_FOUND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE OCR_SUPPORT)
  target_link_libraries(${PROJECT_NAME} PRIVATE Tesseract::libtesseract
                                                ${Leptonica_LIBRARIES})
  target_include_directories(${PROJECT_NAME} PRIVATE ${Tesseract_INCLUDE_DIRS}
                                                     ${Leptonica_INCLUDE_DIRS})
  if(OS_WINDOWS)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
  endif()
else()
  message(
    WARNING "OCR capabilities of video condition disabled!\n"
            "Leptonica or Tesseract dependencies were not found!\n\n"
            "Sources are available under: ${ADVSS_SOURCE_DIR}/deps/leptonica")
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE area-selection.cpp
          area-selection.hpp
          cascade-classifier-detector.cpp
          cascade-classifier-detector.hpp
          macro-condition-video.cpp
          macro-condition-video.hpp
          object-detector.hpp
          opencv-helpers.cpp
          opencv-helpers.hpp
          parameter-wrappers.cpp
          parameter-wrappers.hpp
          preview-dialog.cpp
          preview-dialog.hpp
          screenshot-dialog.cpp
          screenshot-dialog.hpp
          video-edit-area.cpp
          video-edit-brightness.cpp
          video-edit-color.cpp
          video-edit-object.cpp
          video-edit-ocr.cpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARIES})

install_advss_plugin(${PROJECT_NAME})
if(NOT OS_LINUX)
  install_advss_plugin_dependency(TARGET ${PROJECT_NAME} DEPENDENCIES
                                  ${OpenCV_LIBS})
endif()
