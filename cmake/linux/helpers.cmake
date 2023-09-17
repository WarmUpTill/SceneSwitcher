# CMake Linux helper functions module

include_guard(GLOBAL)

include(helpers_common)

# set_target_properties_plugin: Set target properties for use in obs-studio
function(set_target_properties_plugin target)
  set(options "")
  set(oneValueArgs "")
  set(multiValueArgs PROPERTIES)
  cmake_parse_arguments(PARSE_ARGV 0 _STPO "${options}" "${oneValueArgs}" "${multiValueArgs}")

  message(DEBUG "Setting additional properties for target ${target}...")

  while(_STPO_PROPERTIES)
    list(POP_FRONT _STPO_PROPERTIES key value)
    set_property(TARGET ${target} PROPERTY ${key} "${value}")
  endwhile()

  set_target_properties(
    ${target}
    PROPERTIES VERSION 0
               SOVERSION ${PLUGIN_VERSION}
               PREFIX "")

  install(
    TARGETS ${target}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/obs-plugins)

  if(TARGET plugin-support)
    target_link_libraries(${target} PRIVATE plugin-support)
  endif()

  target_install_resources(${target})

  get_target_property(target_sources ${target} SOURCES)
  set(target_ui_files ${target_sources})
  list(FILTER target_ui_files INCLUDE REGEX ".+\\.(ui|qrc)")
  source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    PREFIX "UI Files"
    FILES ${target_ui_files})
endfunction()

# Helper function to add resources into bundle
function(target_install_resources target)
  message(DEBUG "Installing resources for target ${target}...")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    file(GLOB_RECURSE data_files "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
    foreach(data_file IN LISTS data_files)
      cmake_path(RELATIVE_PATH data_file BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/" OUTPUT_VARIABLE
                 relative_path)
      cmake_path(GET relative_path PARENT_PATH relative_path)
      target_sources(${target} PRIVATE "${data_file}")
      source_group("Resources/${relative_path}" FILES "${data_file}")
    endforeach()

    install(
      DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
      DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${target}
      USE_SOURCE_PERMISSIONS)
  endif()
endfunction()

# Helper function to add a specific resource to a bundle
function(target_add_resource target resource)
  message(DEBUG "Add resource '${resource}' to target ${target} at destination '${target_destination}'...")

  install(FILES "${resource}" DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${target})

  source_group("Resources" FILES "${resource}")
endfunction()

# advss additions:
set(OBS_PLUGIN_DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")

function(plugin_install_helper what where where_deb)
  if(DEB_INSTALL)
    if(NOT LIB_OUT_DIR)
      set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    install(TARGETS ${what} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR}/${where_deb})
  else()
    install(
      TARGETS ${what}
      LIBRARY DESTINATION "${where}"
              COMPONENT ${what}_Runtime
              NAMELINK_COMPONENT ${what}_Development)

    install(
      FILES $<TARGET_FILE:${what}>
      DESTINATION $<CONFIG>/${where}
      COMPONENT ${what}_rundir
      EXCLUDE_FROM_ALL)
  endif()
  if(OBS_OUTPUT_DIR)
    add_custom_command(
      TARGET ${what}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -DCMAKE_INSTALL_PREFIX=${OBS_OUTPUT_DIR} -DCMAKE_INSTALL_COMPONENT=${what}_rundir
              -DCMAKE_INSTALL_CONFIG_NAME=$<CONFIG> -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake
      COMMENT "Installing ${what} to plugin rundir ${OBS_OUTPUT_DIR}/${where}\n"
      VERBATIM)
  endif()
endfunction()

function(install_advss_lib target)
  plugin_install_helper("${target}" "${OBS_PLUGIN_DESTINATION}" "")
endfunction()

function(install_advss_plugin target)
  plugin_install_helper("${target}" "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}" "${_PLUGIN_FOLDER}")
  message(STATUS "ADVSS: ENABLED PLUGIN     ${target}")
endfunction()

function(install_advss_plugin_dependency_target target dep)
  install(
    IMPORTED_RUNTIME_ARTIFACTS
    ${dep}
    RUNTIME
    DESTINATION
    "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
    COMPONENT
    ${dep}_Runtime
    LIBRARY
    DESTINATION
    "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
    COMPONENT
    ${dep}_Runtime
    NAMELINK_COMPONENT
    ${dep}_Development)

  install(
    IMPORTED_RUNTIME_ARTIFACTS
    ${dep}
    RUNTIME
    DESTINATION
    "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
    COMPONENT
    obs_${dep}
    EXCLUDE_FROM_ALL
    LIBRARY
    DESTINATION
    "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
    COMPONENT
    obs_${dep}
    EXCLUDE_FROM_ALL)
endfunction()

function(install_advss_plugin_dependency_file target dep)
  get_filename_component(_FILENAME ${dep} NAME)
  string(REGEX REPLACE "\\.[^.]*$" "" _FILENAMENOEXT ${_FILENAME})
  set(_DEP_NAME "${target}-${_FILENAMENOEXT}")

  install(
    FILES "${dep}"
    DESTINATION "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
    COMPONENT ${_DEP_NAME}_Runtime
    DESTINATION "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
    COMPONENT ${_DEP_NAME}_Runtime
    NAMELINK_COMPONENT ${_DEP_NAME}_Development)

  install(
    FILES "${dep}"
    DESTINATION "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
    COMPONENT obs_${_DEP_NAME}
    EXCLUDE_FROM_ALL
    DESTINATION "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
    COMPONENT obs_${_DEP_NAME}
    EXCLUDE_FROM_ALL)
endfunction()
