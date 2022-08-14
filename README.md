# SceneSwitcher
An automation plugin for OBS Studio.

More information can be found on https://obsproject.com/forum/resources/automatic-scene-switching.395/.

## Downloads

Binaries for Windows, MacOS, and Linux are available in the [Releases](https://github.com/WarmUpTill/SceneSwitcher/releases) section.

## Installing the plugin

For the **Windows** and **MacOS** platforms, it is recommended to run the provided installers.

For **Linux** the **Snap** package manager offers an OBS Studio installation which is bundled with the plugin:
```
sudo snap install obs-studio
```
The plugin is also available via the **Flatpak** package manager for users who installed OBS via Flatpak:
```
flatpak install com.obsproject.Studio.Plugin.SceneSwitcher
```

Also note that the Linux version of this plugin has the following dependencies to `XTest`, `XScreensaver` and optionally `OpenCV`.  
If `apt` is supported on your system they can be installed using:
```
sudo apt-get install \
    libxtst-dev \
    libxss-dev \
    libopencv-dev
```

## Contributing

- If you wish to contribute code to the project, have a look at this [section](BUILDING.md) describing how to compile the plugin.
- If you wish to contribute translations, feel free to submit pull requests for the corresponding files under `data/locale`.

