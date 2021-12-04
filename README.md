# SceneSwitcher
An automated scene switcher for OBS Studio.

More information can be found on https://obsproject.com/forum/resources/automatic-scene-switching.395/.

## Downloads

Binaries for Windows, MacOS, and Linux are available in the [Releases](https://github.com/WarmUpTill/SceneSwitcher/releases) section.

## Installing the plugin

For the Windows and MacOS platforms, it is recommended to run the provided installers.

For Linux the Snap package manager offers an OBS Studio installation which is bundled with the plugin:
```
sudo snap install obs-studio
```
The plugin is also available via the Flatpak package manager for users who installed OBS via Flatpak:
```
flatpak install com.obsproject.Studio.Plugin.SceneSwitcher
```

If that is not an option you will have to ... 
1. Copy the library to the plugins folder of you obs installation.
2. Copy the contents of the data directory to its respective folders of your obs installation.

Unfortunately the exact location of these folders may vary from system to system.

## Compiling the plugin

See the [build instructions](BUILDING.md).
