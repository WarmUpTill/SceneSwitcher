Note that you will have to use OBS version 27 or newer!

--- Windows ---
Recommended: Run the provided installer. (You might have to click 'More info' and select 'Run anyway' if it is blocked by Windows)

Alternatively manually copy the 'obs-plugins' and 'data' folders in the respective OBS Studio installation directory.
It is usually located at 'C:\Program Files (x86)\obs-studio\'.
Remember to install the Visual C++ Redistributable for Visual Studio 2019, if you have not done so already. (See plugin overview page)

--- macOS ---
Run the provided installer. (You might have to right-click or control-click and select 'open' if it is blocked)

--- Linux ---
Note that the plugin has dependencies to:
 * libXss
 * libopencv-imgproc
 * libopencv-objdetect
Optional:
 * libXtst
 * libcurl

Copy the advanced-scene-switcher.so file and into the OBS Studio plugin folder.
The location of this folder can vary, so you might have to look around a bit.

Examples are ...
/usr/lib/obs-plugins/
/usr/lib/x86_64-linux-gnu/obs-plugins/
/usr/share/obs/obs-plugins/
~/.config/obs-studio/plugins/advanced-scene-switcher/bin/64bit/
~/.local/share/flatpak/app/com.obsproject.Studio/x86_64/stable/active/files/lib/obs-plugins/
/var/lib/flatpak/app/com.obsproject.Studio/x86_64/stable/active/files/lib/obs-plugins/

Unfortunately the expected location of the locale, which can be found in the data folder, can vary also.
The data folder usually should be copied to:
~/.config/obs-studio/plugins/advanced-scene-switcher/

If this does not work you can try to copy the "locale" folder found inside the data folder to:
/usr/share/obs/obs-plugins/advanced-scene-switcher/locale
~/.local/share/flatpak/app/com.obsproject.Studio/x86_64/stable/active/files/share/obs/obs-plugins/advanced-scene-switcher/locale
/var/lib/flatpak/app/com.obsproject.Studio/x86_64/stable/active/files/share/obs/obs-plugins/advanced-scene-switcher/locale

In doubt please check where other "en-US.ini" files are located on your system.
