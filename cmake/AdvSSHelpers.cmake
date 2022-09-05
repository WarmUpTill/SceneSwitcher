# --- Helper functions ---#

# Subfolder for advanced scene switcher plugins
set(_PLUGIN_FOLDER "adv-ss-plugins")

# --- MACOS section ---
if(OS_MACOS)
  set(ADVSS_BUNDLE_DIR "${CMAKE_INSTALL_PREFIX}/advanced-scene-switcher.plugin")
  set(ADVSS_BUNDLE_MODULE_DIR "${ADVSS_BUNDLE_DIR}/Contents/MacOS")
  set(ADVSS_BUNDLE_PLUGIN_DIR ${ADVSS_BUNDLE_MODULE_DIR}/${_PLUGIN_FOLDER})

  function(resign_advss target)
    set(_COMMAND "codesign --force --deep --sign - ${ADVSS_BUNDLE_DIR}")
    install(CODE "execute_process(COMMAND /bin/sh -c \"${_COMMAND}\")")
  endfunction()

  function(install_advss_lib_helper target where)
    install(
      TARGETS ${target}
      RUNTIME DESTINATION "${where}" COMPONENT advss_plugins
      LIBRARY DESTINATION "${where}" COMPONENT advss_plugins
      FRAMEWORK DESTINATION "${where}" COMPONENT advss_plugins)
    resign_advss(${target})
  endfunction()

  function(install_advss_lib target)
    install_advss_lib_helper(${target} "${ADVSS_BUNDLE_MODULE_DIR}")
    set(_COMMAND
        "${CMAKE_INSTALL_NAME_TOOL} \\
      -change @rpath/advanced-scene-switcher-lib.so @loader_path/advanced-scene-switcher-lib.so \\
      \\\"${ADVSS_BUNDLE_MODULE_DIR}/advanced-scene-switcher\\\"")
    install(CODE "execute_process(COMMAND /bin/sh -c \"${_COMMAND}\")"
            COMPONENT obs_plugins)
  endfunction()

  function(install_advss_plugin target)
    install_advss_lib_helper(${target} "${ADVSS_BUNDLE_PLUGIN_DIR}")
  endfunction()

  function(install_advss_plugin_dependency_target target dep)
    install(
      IMPORTED_RUNTIME_ARTIFACTS
      ${dep}
      RUNTIME
      DESTINATION
      "${ADVSS_BUNDLE_PLUGIN_DIR}"
      COMPONENT
      ${dep}_Runtime
      LIBRARY
      DESTINATION
      "${ADVSS_BUNDLE_PLUGIN_DIR}"
      COMPONENT
      ${dep}_Runtime
      NAMELINK_COMPONENT
      ${dep}_Development)
    resign_advss(${target})
  endfunction()

  function(install_advss_plugin_dependency_file ${target} dep)
    target_sources(advanced-scene-switcher PRIVATE ${dep})
    set_source_files_properties(${dep} PROPERTIES MACOSX_PACKAGE_LOCATION
                                                  ${ADVSS_BUNDLE_PLUGIN_DIR})
    resign_advss(${target})
  endfunction()

  # --- End of section ---
else()
  # --- Windows / Linux section ---
  function(plugin_install_helper what where where_deb)
    install(
      TARGETS ${what}
      RUNTIME DESTINATION "${where}" COMPONENT ${what}_Runtime
      LIBRARY DESTINATION "${where}"
              COMPONENT ${what}_Runtime
              NAMELINK_COMPONENT ${what}_Development)
    install(
      FILES $<TARGET_FILE:${what}>
      DESTINATION $<CONFIG>/${where}
      COMPONENT ${what}_rundir
      EXCLUDE_FROM_ALL)

    if(OS_WINDOWS)
      install(
        FILES $<TARGET_PDB_FILE:${what}>
        CONFIGURATIONS "RelWithDebInfo" "Debug"
        DESTINATION ${where}
        COMPONENT ${what}_Runtime
        OPTIONAL)
      install(
        FILES $<TARGET_PDB_FILE:${what}>
        CONFIGURATIONS "RelWithDebInfo" "Debug"
        DESTINATION $<CONFIG>/${where}
        COMPONENT ${what}_rundir
        OPTIONAL EXCLUDE_FROM_ALL)
    endif()

    add_custom_command(
      TARGET ${what}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -DCMAKE_INSTALL_PREFIX=${OBS_OUTPUT_DIR}
        -DCMAKE_INSTALL_COMPONENT=${what}_rundir
        -DCMAKE_INSTALL_CONFIG_NAME=$<CONFIG> -P
        ${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake
      COMMENT "Installing ${what} to plugin rundir ${OBS_OUTPUT_DIR}/${where}\n"
      VERBATIM)

    if(OS_POSIX AND DEB_INSTALL)
      if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
      endif()
      install(
        TARGETS ${what}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR}/${where_deb})
    endif()
  endfunction()

  function(install_advss_lib target)
    plugin_install_helper("${target}" "${OBS_PLUGIN_DESTINATION}" "")
  endfunction()

  function(install_advss_plugin target)
    plugin_install_helper(
      "${target}" "${OBS_PLUGIN_DESTINATION}/${_PLUGIN_FOLDER}"
      "${_PLUGIN_FOLDER}")
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

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" --install .. --config $<CONFIG> --prefix
        ${OBS_OUTPUT_DIR}/$<CONFIG> --component obs_${dep} >
        "$<IF:$<PLATFORM_ID:Windows>,nul,/dev/null>"
      COMMENT "Installing ${dep} to OBS rundir\n"
      VERBATIM)
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

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" --install .. --config $<CONFIG> --prefix
        ${OBS_OUTPUT_DIR}/$<CONFIG> --component obs_${_DEP_NAME} >
        "$<IF:$<PLATFORM_ID:Windows>,nul,/dev/null>"
      COMMENT "Installing ${_DEP_NAME} to OBS rundir\n"
      VERBATIM)
  endfunction()
endif()

# --- End of section ---
function(setup_obs_lib_dependency target)
  if(BUILD_OUT_OF_TREE)
    find_package(libobs)
    if(libobs_FOUND)
      target_link_libraries(${target} PUBLIC OBS::libobs)
    else()
      if(NOT LIBOBS_LIB)
        message(FATAL_ERROR "obs library not found - please set LIBOBS_LIB")
      endif()
      target_link_libraries(${target} PUBLIC ${LIBOBS_LIB})
      if(NOT LIBOBS_INCLUDE_DIR)
        message(
          FATAL_ERROR "obs.hpp header not found - please set LIBOBS_INCLUDE_DIR"
        )
      endif()
      target_include_directories(${target} PRIVATE ${LIBOBS_INCLUDE_DIR})
    endif()
    find_package(obs-frontend-api)
    if(obs-frontend-api_FOUND)
      target_link_libraries(${target} PUBLIC OBS::obs-frontend-api)
    else()
      if(NOT LIBOBS_FRONTEND_API_LIB)
        message(
          FATAL_ERROR
            "libobs frontend-api library not found - please set LIBOBS_FRONTEND_API_LIB"
        )
      endif()
      target_link_libraries(${target} PUBLIC ${LIBOBS_FRONTEND_API_LIB})
      if(NOT LIBOBS_FRONTEND_INCLUDE_DIR)
        message(
          FATAL_ERROR
            " obs-frontend-api.h not found - please set LIBOBS_FRONTEND_INCLUDE_DIR"
        )
      endif()
      target_include_directories(${target}
                                 PRIVATE ${LIBOBS_FRONTEND_INCLUDE_DIR})
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
    set_target_properties(${target} PROPERTIES INSTALL_RPATH
                                               "${_INSTALL_RPATH}")
  endif()

  # Set up include directories for headers generated by Qt
  target_include_directories(
    ${target}
    PRIVATE "${ADVSS_BINARY_DIR}/advanced-scene-switcher-lib_autogen/include")
  foreach(_CONF Release RelWithDebInfo Debug MinSizeRe)
    target_include_directories(
      ${target}
      PRIVATE
        "${ADVSS_BINARY_DIR}/advanced-scene-switcher-lib_autogen/include_${_CONF}"
    )
  endforeach()

  # General includes
  target_include_directories(
    ${target}
    PRIVATE "${ADVSS_SOURCE_DIR}/src" "${ADVSS_SOURCE_DIR}/src/legacy"
            "${ADVSS_SOURCE_DIR}/src/macro-core"
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
      install_advss_plugin_dependency_target(${PARSED_ARGS_TARGET}
                                             ${_DEPENDENCY})
    endif()
  endforeach()
endfunction()
