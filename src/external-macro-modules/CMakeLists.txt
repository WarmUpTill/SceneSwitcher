# Helper function to install plugins to correct location
function(install_advss_plugin target)
  set(plugin_folder "adv-ss-plugins")
  if(APPLE)
    set(_bit_suffix "")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_bit_suffix "64bit/")
  else()
    set(_bit_suffix "32bit/")
  endif()

  set_target_properties(${target} PROPERTIES PREFIX "")

  install(
    TARGETS ${target}
    LIBRARY DESTINATION "${OBS_PLUGIN_DESTINATION}/${plugin_folder}"
    RUNTIME DESTINATION "${OBS_PLUGIN_DESTINATION}/${plugin_folder}")
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
      "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/obs-plugins/${_bit_suffix}/${plugin_folder}/$<TARGET_FILE_NAME:${target}>"
    VERBATIM)

  if(DEFINED ENV{obsInstallerTempDir})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
        "$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}/${plugin_folder}/$<TARGET_FILE_NAME:${target}>"
      VERBATIM)
  endif()

  if(MSVC)
    obs_debug_copy_helper(
      ${target}
      "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/obs-plugins/${_bit_suffix}/${plugin_folder}"
    )

    if(DEFINED ENV{obsInstallerTempDir})
      obs_debug_copy_helper(
        ${target}
        "$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}/${plugin_folder}")
    endif()

    install(
      DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdbs/"
      DESTINATION "${OBS_PLUGIN_DESTINATION}/${plugin_folder}"
      CONFIGURATIONS Debug RelWithDebInfo)
  endif()
endfunction()

# Add macro conditions or actions which have dependencies to external libraries
# or other components which might potentially not be fulfilled by the user and
# thus cause issues.
add_subdirectory(opencv)
add_subdirectory(openvr)
