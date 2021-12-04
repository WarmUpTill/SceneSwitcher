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

If you are using snap you can install the following package for an OBS installation which comes bundled with the plugin:
sudo snap install obs-studio

The plugin is also available via the Flatpak package manager for users who installed OBS via Flatpak:
flatpak install com.obsproject.Studio.Plugin.SceneSwitcher

If you have installed OBS via other means, it is most likely necessary to install the plugin manually.
To do so copy the advanced-scene-switcher.so file and into the OBS Studio plugin folder.
The location of this folder can vary, so you might have to look around a bit.

Examples are ...
/usr/lib/obs-plugins/
/usr/lib/x86_64-linux-gnu/obs-plugins/
/usr/share/obs/obs-plugins/
~/.config/obs-studio/plugins/advanced-scene-switcher/bin/64bit/
~/.local/share/flatpak/app/com.obsproject.Studio/x86_64/stable/active/files/lib/obs-plugins/
/var/lib/flatpak/app/com.obsproject.Studio/x86_64/stable/active/files/lib/obs-plugins/

Unfortunately the expected location of the "locale" and "res" folder, which can be found in the data folder, can vary also.
The data folder usually should be copied to "~/.config/obs-studio/plugins/advanced-scene-switcher/" and reflect the following structure:

/home/user/.config/obs-studio/plugins/
└── advanced-scene-switcher
    ├── bin
    │   └── 64bit
    │       └── advanced-scene-switcher.so
    └── data
        ├── locale
        │   ├── de-DE.ini
        │   ├── en-US.ini
        │   ├── ru-RU.ini
        │   └── zh-CN.ini
        └── res
            ├── cascadeClassifiers
            │   ├── haarcascade_eye_tree_eyeglasses.xml
            │   ├── haarcascade_eye.xml
            │   ├── haarcascade_frontalface_alt2.xml
            │   ├── haarcascade_frontalface_alt_tree.xml
            │   ├── haarcascade_frontalface_alt.xml
            │   ├── haarcascade_frontalface_default.xml
            │   ├── haarcascade_fullbody.xml
            │   ├── haarcascade_lefteye_2splits.xml
            │   ├── haarcascade_lowerbody.xml
            │   ├── haarcascade_profileface.xml
            │   ├── haarcascade_righteye_2splits.xml
            │   └── haarcascade_upperbody.xml
            └── time.svg

If this does not work you can try to copy the "locale" folder found inside the data folder to:
/usr/share/obs/obs-plugins/advanced-scene-switcher/locale
~/.local/share/flatpak/app/com.obsproject.Studio/x86_64/stable/active/files/share/obs/obs-plugins/advanced-scene-switcher/locale
/var/lib/flatpak/app/com.obsproject.Studio/x86_64/stable/active/files/share/obs/obs-plugins/advanced-scene-switcher/locale

In doubt, please check where other "en-US.ini" files are located on your system.
