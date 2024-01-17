macro(add_plugin plugin)
  option(ENABLE_${plugin}_PLUGIN "Enable the ${plugin} plugin" ON)
  if(ENABLE_${plugin}_PLUGIN)
    add_subdirectory(${plugin})
  endif()
endmacro()

# ---------------------------------------------------------------------------- #

add_subdirectory(base)

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

add_plugin(midi)
add_plugin(openvr)
add_plugin(twitch)
add_plugin(video)
