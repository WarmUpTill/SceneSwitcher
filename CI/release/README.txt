Note that you will have to use OBS version 25 or newer!

--- WINDOWS ---
Recommended: Run the provided installer. (You might have to click 'More info' and select 'Run anyway' if it is blocked by Windows)

Alternatively manually copy the 'obs-plugins' and 'data' folders in the respective OBS Studio installation directory.
It is usually located at 'C:\Program Files (x86)\obs-studio\'.
Remember to install the Visual C++ Redistributable for Visual Studio 2019, if you have not done so already. (See plugin overview page)

--- MAC OS ---
Recommended: Run the provided installer. (You might have to right click and select 'open' if it is blocked)

Alternatively extract the *so file and data folder and either ...

... right click the OBS app inside your Applications folder and choose 'Show Package Contents'.
Copy the advanced-scene-switcher.so file to 'Contents/Plugins' and the 'data' folder to 'Contents/Resources'.

... or copy the advanced-scene-switcher.so file to Library/Application Support/obs-studio/plugins/advanced-scene-switcher/bin/ .
And the 'data' folder to 'Library/Application Support/obs-studio/plugins/advanced-scene-switcher/'.

--- Linux ---
Copy the advanced-scene-switcher.so file and data folder into the OBS Studio plugin folder.
The location of this folder can vary so you might have to look around a bit.

Examples are ...
/usr/lib/obs-plugins/
/usr/lib/x86_64-linux-gnu/obs-plugins/
~/.config/obs-studio/plugins/advanced-scene-switcher/bin/64bit/

The data folder usually should be copied to:
~/.config/obs-studio/plugins/advanced-scene-switcher/
