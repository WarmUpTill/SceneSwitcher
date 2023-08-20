cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-usb)

# --- Check libusb requirements ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
set(LIBUSB_DEPS_DIR "${ADVSS_SOURCE_DIR}/deps/libusb")

if(OS_LINUX)
  find_package(PkgConfig)
  pkg_check_modules(libusb OPTIONAL libusb-1.0)
else()
  find_path(
    LIBUSB_INCLUDE_DIR
    NAMES libusb.h
    PATHS "${LIBUSB_DEPS_DIR}/libusb")
  find_library(
    LIBUSB_LIBRARY
    NAMES libusb-1.0
    PATHS "${LIBUSB_DEPS_DIR}/x64/Release/dll")
  find_file(
    LIBUSB_RUNTIME_LIBRARY
    NAMES libusb-1.0.dll
    PATHS "${LIBUSB_DEPS_DIR}/x64/Release/dll")
endif()

if(NOT LIBUSB_INCLUDE_DIR)
  message(WARNING "libusb headers not found. Make sure libusb is installed." "USB condition will be disabled!\n\n"
                  "libusb sources are available under: ${LIBUSB_DEPS_DIR}")
  return()
endif()
if(NOT LIBUSB_LIBRARY)
  message(WARNING "libusb library not found. Make sure libusb is installed." "USB condition will be disabled!\n\n"
                  "libusb sources are available under: ${LIBUSB_DEPS_DIR}")
  return()
endif()

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

target_sources(${PROJECT_NAME} PRIVATE macro-condition-usb.cpp macro-condition-usb.hpp usb-helpers.cpp usb-helpers.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUSB_LIBRARY})
install_advss_plugin(${PROJECT_NAME})
if(NOT OS_LINUX)
  install_advss_plugin_dependency(TARGET ${PROJECT_NAME} DEPENDENCIES ${LIBUSB_RUNTIME_LIBRARY})
endif()
