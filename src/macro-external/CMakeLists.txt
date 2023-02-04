# Add macro conditions or actions which have dependencies to external libraries
# or other components which might potentially not be fulfilled.

#[[
To add a new plugin with external dependencies append a ...

add_subdirectory(<plugin_name>)

... call to the end of this file.

In the plugins cmake file call the helper functions ...

install_advss_plugin(<plugin_name>)
... and ...
install_advss_plugin_dependency(...)

... to install the plugin and its dependencies.
#]]

add_subdirectory(openvr)
add_subdirectory(video)
