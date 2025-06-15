cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-usb)

# --- Check libusb requirements ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
set(libusb_DEPS_DIR "${ADVSS_SOURCE_DIR}/deps/libusb")

if(OS_LINUX)

  find_package(PkgConfig)
  if(NOT PKG_CONFIG_FOUND)
    message(WARNING "pkg-config not found!\n" "USB condition will be disabled!")
    return()
  endif()
  pkg_check_modules(libusb OPTIONAL libusb)
  find_path(
    libusb_HEADER_DIR
    NAMES libusb.h
    PATHS ${libusb_INCLUDEDIR} /usr/include /usr/local/include
          /opt/local/include
    PATH_SUFFIXES libusb-1.0)
  find_library(
    libusb_LINK_LIBRARIES
    NAMES usb-1.0 usb
    PATHS /usr/lib /usr/local/lib /opt/local/lib)

elseif(OS_MACOS)

  find_path(
    libusb_HEADER_DIR
    NAMES libusb.h
    PATHS ${libusb_DEPS_DIR}/libusb)

  # The NO_DEFAULT_PATH option is set since we have to use the "universal"
  # variant of the library, which combines the x86 and arm variants
  find_library(
    libusb_LINK_LIBRARIES
    NAMES usb-1.0.0 usb-1.0 usb
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
  find_file(
    libusb_RUNTIME_LIBRARY
    NAMES libusb-1.0.0.dylib
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)

elseif(OS_WINDOWS)

  find_path(
    libusb_HEADER_DIR
    NAMES libusb.h
    PATHS ${libusb_DEPS_DIR}/libusb)

  # Intentionally not using find_library since we have to use the libusb-1.0.lib
  # file from within the dll folder instead of the lib folder to avoid linking
  # issues due to conflicting LIBCMT versions
  find_file(
    libusb_LINK_LIBRARIES
    NAMES libusb-1.0.lib
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES dll)

  find_file(
    libusb_RUNTIME_LIBRARY
    NAMES libusb-1.0.dll
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES dll)

endif()

if(NOT libusb_HEADER_DIR)
  message(WARNING "libusb headers not found. Make sure libusb is installed. "
                  "USB condition will be disabled!\n\n"
                  "libusb sources are available under: ${libusb_DEPS_DIR}")
  return()
endif()
if(NOT libusb_LINK_LIBRARIES OR NOT EXISTS "${libusb_LINK_LIBRARIES}")
  message(WARNING "libusb library not found. Make sure libusb is installed. "
                  "USB condition will be disabled!\n\n"
                  "libusb sources are available under: ${libusb_DEPS_DIR}")
  return()
endif()
if(NOT OS_LINUX AND (NOT libusb_RUNTIME_LIBRARY OR NOT EXISTS
                                                   "${libusb_RUNTIME_LIBRARY}"))
  message(
    WARNING "libusb runtime library not found. Make sure libusb is installed. "
            "USB condition will be disabled!\n\n"
            "libusb sources are available under: ${libusb_DEPS_DIR}")
  return()
endif()

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

target_sources(
  ${PROJECT_NAME} PRIVATE macro-condition-usb.cpp macro-condition-usb.hpp
                          usb-helpers.cpp usb-helpers.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME} PRIVATE ${libusb_HEADER_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${libusb_LINK_LIBRARIES})
install_advss_plugin(${PROJECT_NAME})
if(NOT OS_LINUX)
  install_advss_plugin_dependency(TARGET ${PROJECT_NAME} DEPENDENCIES
                                  ${libusb_RUNTIME_LIBRARY})
endif()
