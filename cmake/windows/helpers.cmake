# CMake Windows helper functions module

# cmake-format: off
# cmake-lint: disable=C0103
# cmake-format: on

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

  string(TIMESTAMP CURRENT_YEAR "%Y")

  set_target_properties(${target} PROPERTIES VERSION 0 SOVERSION ${PLUGIN_VERSION})

  install(
    TARGETS ${target}
    RUNTIME DESTINATION bin/64bit
    LIBRARY DESTINATION obs-plugins/64bit)

  install(
    FILES "$<TARGET_PDB_FILE:${target}>"
    CONFIGURATIONS RelWithDebInfo Debug
    DESTINATION obs-plugins/64bit
    OPTIONAL)

  if(OBS_BUILD_DIR)
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_BUILD_DIR}/obs-plugins/64bit"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${target}>"
              "$<$<CONFIG:Debug,RelWithDebInfo>:$<TARGET_PDB_FILE:${target}>>" "${OBS_BUILD_DIR}/obs-plugin/64bit"
      COMMENT "Copy ${target} to obs-studio directory ${OBS_BUILD_DIR}"
      VERBATIM)
  endif()

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

  set(valid_uuid FALSE)
  check_uuid(${_windowsAppUUID} valid_uuid)
  if(NOT valid_uuid)
    message(FATAL_ERROR "Specified Windows package UUID is not a valid UUID value: ${_windowsAppUUID}")
  else()
    set(UUID_APP ${_windowsAppUUID})
  endif()

  configure_file(cmake/windows/resources/installer-Windows.iss.in
                 "${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss")

  configure_file(cmake/windows/resources/resource.rc.in "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc")
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc")
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
      DESTINATION data/obs-plugins/${target}
      USE_SOURCE_PERMISSIONS)

    if(OBS_BUILD_DIR)
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_BUILD_DIR}/data/obs-plugins/${target}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data"
                "${OBS_BUILD_DIR}/data/obs-plugins/${target}"
        COMMENT "Copy ${target} resources to data directory"
        VERBATIM)
    endif()
  endif()
endfunction()

# Helper function to add a specific resource to a bundle
function(target_add_resource target resource)
  message(DEBUG "Add resource '${resource}' to target ${target} at destination '${target_destination}'...")

  install(
    FILES "${resource}"
    DESTINATION data/obs-plugins/${target}
    COMPONENT Runtime)

  if(OBS_BUILD_DIR)
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_BUILD_DIR}/data/obs-plugins/${target}"
      COMMAND "${CMAKE_COMMAND}" -E copy "${resource}" "${OBS_BUILD_DIR}/data/obs-plugins/${target}"
      COMMENT "Copy ${target} resource ${resource} to library directory"
      VERBATIM)
  endif()
  source_group("Resources" FILES "${resource}")
endfunction()

# advss additions:
set(OBS_PLUGIN_DESTINATION "obs-plugins/64bit")

function(plugin_install_helper what where)
  install(
    FILES "$<TARGET_PDB_FILE:${what}>"
    CONFIGURATIONS RelWithDebInfo Debug
    DESTINATION ${where}
    OPTIONAL)
  install(
    FILES "$<TARGET_FILE:${what}>"
    CONFIGURATIONS RelWithDebInfo Debug
    DESTINATION ${where}
    OPTIONAL)

  if(OBS_BUILD_DIR)
    add_custom_command(
      TARGET ${what}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_BUILD_DIR}/${where}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${what}>"
              "$<$<CONFIG:Debug,RelWithDebInfo>:$<TARGET_PDB_FILE:${what}>>" "${OBS_BUILD_DIR}/${where}"
      COMMENT "Copy ${what} to obs-studio directory ${OBS_BUILD_DIR}/${where}"
      VERBATIM)
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
  plugin_install_helper("${target}" "${OBS_PLUGIN_DESTINATION}")
endfunction()

function(install_advss_plugin target)
  plugin_install_helper("${target}" "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}")
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

  if(OBS_OUTPUT_DIR)
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" --install .. --config $<CONFIG> --prefix ${OBS_OUTPUT_DIR}/$<CONFIG> --component
              obs_${dep} > "$<IF:$<PLATFORM_ID:Windows>,nul,/dev/null>"
      COMMENT "Installing ${dep} to OBS rundir\n"
      VERBATIM)
  endif()
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

  if(OBS_OUTPUT_DIR)
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" --install .. --config $<CONFIG> --prefix ${OBS_OUTPUT_DIR}/$<CONFIG> --component
              obs_${_DEP_NAME} > "$<IF:$<PLATFORM_ID:Windows>,nul,/dev/null>"
      COMMENT "Installing ${_DEP_NAME} to OBS rundir\n"
      VERBATIM)
  endif()
endfunction()
