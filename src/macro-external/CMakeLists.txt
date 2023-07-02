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

option(ENABLE_MIDI_PLUGIN "Enable the midi plugin" ON)
if(ENABLE_MIDI_PLUGIN)
  add_subdirectory(midi)
endif()

option(ENABLE_OPENVR_PLUGIN "Enable the openvr plugin" ON)
if(ENABLE_OPENVR_PLUGIN)
  add_subdirectory(openvr)
endif()

option(ENABLE_VIDEO_PLUGIN "Enable the video plugin" ON)
if(ENABLE_VIDEO_PLUGIN)
  add_subdirectory(video)
endif()
