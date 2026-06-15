macro(add_plugin plugin)
  option(ENABLE_${plugin}_PLUGIN "Enable the ${plugin} plugin" ON)
  if(ENABLE_${plugin}_PLUGIN)
    set_property(GLOBAL APPEND PROPERTY ADVSS_PLUGINS_PENDING ${plugin})
    add_subdirectory(${plugin})
  else()
    set_property(GLOBAL APPEND PROPERTY ADVSS_PLUGINS_DISABLED_BY_OPTION
                                        ${plugin})
  endif()
endmacro()

# ---------------------------------------------------------------------------- #

# Add plugins below, which have dependencies to external libraries or other
# components which might potentially not be fulfilled.

#[[
To add a new plugin with external dependencies append a ...

add_plugin(<plugin_name>)

... call to the end of this file.

In the plugins cmake file call the helper functions ...

install_advss_plugin(<plugin_name>)
... and ...
install_advss_plugin_dependency(...)

... to install the plugin and its dependencies.
#]]

add_plugin(base)
add_plugin(http)
add_plugin(midi)
add_plugin(mqtt)
add_plugin(openvr)
add_plugin(schedule)
add_plugin(scripting)
add_plugin(stream-deck)
add_plugin(twitch)
add_plugin(usb)
add_plugin(video)

# ---------------------------------------------------------------------------- #

# Print plugin summary
get_property(_advss_enabled GLOBAL PROPERTY ADVSS_PLUGINS_ENABLED)
get_property(_advss_pending GLOBAL PROPERTY ADVSS_PLUGINS_PENDING)
get_property(_advss_disabled_opt GLOBAL
             PROPERTY ADVSS_PLUGINS_DISABLED_BY_OPTION)

set(_advss_disabled_deps)
foreach(_p ${_advss_pending})
  if(NOT _p IN_LIST _advss_enabled)
    list(APPEND _advss_disabled_deps ${_p})
  endif()
endforeach()

message(STATUS "")
message(STATUS "ADVSS Plugin Summary")
message(STATUS "====================")
foreach(_p ${_advss_enabled})
  message(STATUS "  ENABLED:  ${_p}")
endforeach()
foreach(_p ${_advss_disabled_deps})
  message(STATUS "  DISABLED: ${_p} (missing dependencies)")
endforeach()
foreach(_p ${_advss_disabled_opt})
  message(STATUS "  DISABLED: ${_p} (ENABLE_${_p}_PLUGIN=OFF)")
endforeach()
message(STATUS "")
