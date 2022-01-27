# Helper function to install plugins to correct location
function(install_advss_plugin target)
  if(MSVC)
    install_obs_pdb(PLUGIN ${target})
    return()
  endif(MSVC)

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
    LIBRARY DESTINATION "${OBS_PLUGIN_DESTINATION}/adv-ss-plugins"
    RUNTIME DESTINATION "${OBS_PLUGIN_DESTINATION}/adv-ss-plugins")
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
      "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/obs-plugins/${_bit_suffix}/adv-ss-plugins/$<TARGET_FILE_NAME:${target}>"
    VERBATIM)

  if(DEFINED ENV{obsInstallerTempDir})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
        "$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}/adv-ss-plugins/$<TARGET_FILE_NAME:${target}>"
      VERBATIM)
  endif()
endfunction()

# Add macro conditions or actions which have dependencies to external libraries
# or other components which might potentially not be fulfilled by the user and
# thus cause issues.
add_subdirectory(opencv)
add_subdirectory(openvr)
