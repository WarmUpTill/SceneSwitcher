# CMake common helper functions module

# cmake-format: off
# cmake-lint: disable=C0103
# cmake-format: on

include_guard(GLOBAL)

# * Use QT_VERSION value as a hint for desired Qt version
# * If "AUTO" was specified, prefer Qt6 over Qt5
# * Creates versionless targets of desired component if none had been created by Qt itself (Qt versions < 5.15)
if(NOT QT_VERSION)
  set(QT_VERSION
      AUTO
      CACHE STRING "OBS Qt version [AUTO, 5, 6]" FORCE)
  set_property(CACHE QT_VERSION PROPERTY STRINGS AUTO 5 6)
endif()

# find_qt: Macro to find best possible Qt version for use with the project:
macro(find_qt)
  set(multiValueArgs COMPONENTS COMPONENTS_WIN COMPONENTS_MAC COMPONENTS_LINUX)
  cmake_parse_arguments(find_qt "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Do not use versionless targets in the first step to avoid Qt::Core being clobbered by later opportunistic
  # find_package runs
  set(QT_NO_CREATE_VERSIONLESS_TARGETS TRUE)

  message(DEBUG "Start Qt version discovery...")
  # Loop until _QT_VERSION is set or FATAL_ERROR aborts script execution early
  while(NOT _QT_VERSION)
    message(DEBUG "QT_VERSION set to ${QT_VERSION}")
    if(QT_VERSION STREQUAL AUTO AND NOT qt_test_version)
      set(qt_test_version 6)
    elseif(NOT QT_VERSION STREQUAL AUTO)
      set(qt_test_version ${QT_VERSION})
    endif()
    message(DEBUG "Attempting to find Qt${qt_test_version}")

    find_package(
      Qt${qt_test_version}
      COMPONENTS Core
      QUIET)

    if(TARGET Qt${qt_test_version}::Core)
      set(_QT_VERSION
          ${qt_test_version}
          CACHE INTERNAL "")
      message(STATUS "Qt version found: ${_QT_VERSION}")
      unset(qt_test_version)
      break()
    elseif(QT_VERSION STREQUAL AUTO)
      if(qt_test_version EQUAL 6)
        message(WARNING "Qt6 was not found, falling back to Qt5")
        set(qt_test_version 5)
        continue()
      endif()
    endif()
    message(FATAL_ERROR "Neither Qt6 nor Qt5 found.")
  endwhile()

  # Enable versionless targets for the remaining Qt components
  set(QT_NO_CREATE_VERSIONLESS_TARGETS FALSE)

  set(qt_components ${find_qt_COMPONENTS})
  if(OS_WINDOWS)
    list(APPEND qt_components ${find_qt_COMPONENTS_WIN})
  elseif(OS_MACOS)
    list(APPEND qt_components ${find_qt_COMPONENTS_MAC})
  else()
    list(APPEND qt_components ${find_qt_COMPONENTS_LINUX})
  endif()
  message(DEBUG "Trying to find Qt components ${qt_components}...")

  find_package(Qt${_QT_VERSION} REQUIRED ${qt_components})

  list(APPEND qt_components Core)

  if("Gui" IN_LIST find_qt_COMPONENTS_LINUX)
    list(APPEND qt_components "GuiPrivate")
  endif()

  # Check for versionless targets of each requested component and create if necessary
  foreach(component IN LISTS qt_components)
    message(DEBUG "Checking for target Qt::${component}")
    if(NOT TARGET Qt::${component} AND TARGET Qt${_QT_VERSION}::${component})
      add_library(Qt::${component} INTERFACE IMPORTED)
      set_target_properties(Qt::${component} PROPERTIES INTERFACE_LINK_LIBRARIES Qt${_QT_VERSION}::${component})
    endif()
    set_property(TARGET Qt::${component} PROPERTY INTERFACE_COMPILE_FEATURES "")
  endforeach()

endmacro()

# check_uuid: Helper function to check for valid UUID
function(check_uuid uuid_string return_value)
  set(valid_uuid TRUE)
  set(uuid_token_lengths 8 4 4 4 12)
  set(token_num 0)

  string(REPLACE "-" ";" uuid_tokens ${uuid_string})
  list(LENGTH uuid_tokens uuid_num_tokens)

  if(uuid_num_tokens EQUAL 5)
    message(DEBUG "UUID ${uuid_string} is valid with 5 tokens.")
    foreach(uuid_token IN LISTS uuid_tokens)
      list(GET uuid_token_lengths ${token_num} uuid_target_length)
      string(LENGTH "${uuid_token}" uuid_actual_length)
      if(uuid_actual_length EQUAL uuid_target_length)
        string(REGEX MATCH "[0-9a-fA-F]+" uuid_hex_match ${uuid_token})
        if(NOT uuid_hex_match STREQUAL uuid_token)
          set(valid_uuid FALSE)
          break()
        endif()
      else()
        set(valid_uuid FALSE)
        break()
      endif()
      math(EXPR token_num "${token_num}+1")
    endforeach()
  else()
    set(valid_uuid FALSE)
  endif()
  message(DEBUG "UUID ${uuid_string} valid: ${valid_uuid}")
  set(${return_value}
      ${valid_uuid}
      PARENT_SCOPE)
endfunction()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-support.c.in")
  configure_file(src/plugin-support.c.in plugin-support.c @ONLY)
  add_library(plugin-support STATIC)
  target_sources(
    plugin-support
    PRIVATE plugin-support.c
    PUBLIC src/plugin-support.h)
  target_include_directories(plugin-support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

# advss additions:
set(_PLUGIN_FOLDER "adv-ss-plugins")

function(setup_obs_lib_dependency target)
  if(BUILD_OUT_OF_TREE)
    find_package(libobs)
    if(libobs_FOUND AND NOT LIBOBS_LIB)
      target_link_libraries(${target} PUBLIC OBS::libobs)
    else()
      if(NOT LIBOBS_LIB)
        message(FATAL_ERROR "obs library not found - please set LIBOBS_LIB")
      endif()
      target_link_libraries(${target} PUBLIC ${LIBOBS_LIB})
      if(NOT LIBOBS_INCLUDE_DIR)
        message(FATAL_ERROR "obs.hpp header not found - please set LIBOBS_INCLUDE_DIR")
      endif()
      target_include_directories(${target} PRIVATE ${LIBOBS_INCLUDE_DIR})
    endif()
    find_package(obs-frontend-api)
    if(obs-frontend-api_FOUND AND NOT LIBOBS_FRONTEND_API_LIB)
      target_link_libraries(${target} PUBLIC OBS::obs-frontend-api)
    else()
      if(NOT LIBOBS_FRONTEND_API_LIB)
        message(FATAL_ERROR "libobs frontend-api library not found - please set LIBOBS_FRONTEND_API_LIB")
      endif()
      target_link_libraries(${target} PUBLIC ${LIBOBS_FRONTEND_API_LIB})
      if(NOT LIBOBS_FRONTEND_INCLUDE_DIR)
        message(FATAL_ERROR " obs-frontend-api.h not found - please set LIBOBS_FRONTEND_INCLUDE_DIR")
      endif()
      target_include_directories(${target} PRIVATE ${LIBOBS_FRONTEND_INCLUDE_DIR})
    endif()
  else()
    target_link_libraries(${target} PUBLIC OBS::libobs OBS::frontend-api)
  endif()
endfunction()

function(setup_advss_plugin target)
  setup_obs_lib_dependency(${target})
  find_qt(COMPONENTS Widgets Core)
  target_link_libraries(${target} PRIVATE Qt::Core Qt::Widgets)

  set_target_properties(
    ${target}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)

  target_link_libraries(${target} PRIVATE advanced-scene-switcher-lib)

  get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
  get_target_property(ADVSS_BINARY_DIR advanced-scene-switcher-lib BINARY_DIR)

  if(OS_MACOS)
    set(_INSTALL_RPATH "@loader_path" "@loader_path/..")
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "${_INSTALL_RPATH}")
  endif()

  # Set up include directories for headers generated by Qt
  target_include_directories(${target} PRIVATE "${ADVSS_BINARY_DIR}/advanced-scene-switcher-lib_autogen/include")
  foreach(_CONF Release RelWithDebInfo Debug MinSizeRe)
    target_include_directories(${target}
                               PRIVATE "${ADVSS_BINARY_DIR}/advanced-scene-switcher-lib_autogen/include_${_CONF}")
  endforeach()

  # General includes
  target_include_directories(
    ${target} PRIVATE "${ADVSS_SOURCE_DIR}/src" "${ADVSS_SOURCE_DIR}/src/legacy" "${ADVSS_SOURCE_DIR}/src/macro-core"
                      "${ADVSS_SOURCE_DIR}/src/utils" "${ADVSS_SOURCE_DIR}/forms")
endfunction()

function(install_advss_plugin_dependency)
  cmake_parse_arguments(PARSED_ARGS "" "TARGET" "DEPENDENCIES" ${ARGN})
  if(NOT PARSED_ARGS_TARGET)
    message(FATAL_ERROR "You must provide a target")
  endif()
  set(_PLUGIN_FOLDER "adv-ss-plugins")
  foreach(_DEPENDENCY ${PARSED_ARGS_DEPENDENCIES})
    if(EXISTS ${_DEPENDENCY})
      install_advss_plugin_dependency_file(${PARSED_ARGS_TARGET} ${_DEPENDENCY})
    else()
      install_advss_plugin_dependency_target(${PARSED_ARGS_TARGET} ${_DEPENDENCY})
    endif()
  endforeach()
endfunction()
