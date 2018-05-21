# SceneSwitcher
An automated scene switcher for OBS Studio

More information can be found on https://obsproject.com/forum/resources/automatic-scene-switching.395/


To build it yourself follow these steps (changes / suggestions welcome):    
1.  Download the OBS Studio sources.  
2.  Move the sources of this plugin to the the frontend-plugins folder in the OBS sources ("obs-studio/UI/frontend-plugins/SceneSwitcher" for example).  
3.  Edit the CMakeLists.txt in the frontend-plugins folder to include the SceneSwitcher sources directory by appending the line "add_subdirectory(SceneSwitcher)".  
4.  Build OBS Studio and the plugin library will be located in the respective subdirectory of the rundir inside your build folder. (for example ~/obs-studio/build/rundir/RelWithDebInfo/obs-plugins/64bit/advanced-scene-switcher.so)  


NOTE: The linux version of this plugin is dependent on libXScrnSaver.
