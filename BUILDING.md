# Compiling the plugin

You have the option to ...
- either add the plugin to the OBS source tree directly and build the plugin while building OBS itself. (**in tree**)
- or you can move the sources of this plugin outside of the OBS source tree and build it separately from OBS. (**out of tree**)

As both methods require you to have a working [OBS Studio development environment](https://obsproject.com/wiki/Building-OBS-Studio) and [CMake](https://cmake.org/download/) it is recommended to build the plugin in tree as it is easier to set up and will enable straightforward debugging.

The plugin can be compiled for OBS 25 and above, although using the latest version of OBS is recommended to support all features.

## Compiling in tree (recommended for development)
Add the "SceneSwitcher" source directory to your obs-studio source directory under obs-studio/UI/frontend-plugins/:
```
cd obs-studio/UI/frontend-plugins/
git clone --recursive https://github.com/WarmUpTill/SceneSwitcher.git
```

Then modify the obs-studio/UI/frontend-plugins/CMakeLists.txt file and add an entry for the scene switcher:
```
add_subdirectory(SceneSwitcher)
```

Now follow the [build instructions for obs-studio](https://obsproject.com/wiki/Building-OBS-Studio) for your particular platform.

Note that on Linux systems it might be necessary to additionally install the following packages to fulfill the dependencies to `XTest`, `XScreensaver` and `OpenCV` - exact command may differ:
```
sudo apt-get install \
    libxtst-dev \
    libxss-dev \
    libprocps-dev \
    libopencv-dev
```


## Compiling out of tree

First you will need to clone the plugin sources by running the following command:
```
git clone --recursive https://github.com/WarmUpTill/SceneSwitcher.git
cd SceneSwitcher
```

You'll need [CMake](https://cmake.org/download/) and a working [OBS Studio development environment](https://obsproject.com/wiki/Building-OBS-Studio) installed on your computer.  
The easiest way to set this up is to call the corresponding CI scripts, as they will automatically download all required dependencies and start build of the plugin:

| Platform      | Command |
| ----------- | ----------- |
| Windows      | `./.github/scripts/Build-Windows.ps1`      |
| Linux   | `./.github/scripts/build-linux.sh` |
| MacOS   |  `./.github/scripts/build-macos.zsh` |

Alternatively you can download the OBS dependencies from https://github.com/obsproject/obs-deps/releases and manually configure and start the build.

Start by creating a build directory:
```
mkdir build && cd build
```
Next configure the build.
```
cmake -DCMAKE_PREFIX_PATH=<path-to-obs-deps> -Dlibobs_DIR=<path-to-libobs-dir> -Dobs-frontend-api_DIR=<path-to-frontend-api-dir> ..
```
It might be necessary to provide additional variables depending on your build setup.  
Finally, start the plugin build using your provided generator. (E.g. Ninja on Linux or a Visual Studio solution on Windows)

# Contributing

Contributions to the plugin are always welcome and if you need any assistance do not hesitate to reach out.

In general changes in the `src/legacy` folder should be avoided.  

If you would like to expand upon the macro system by adding a new condition or action type have a loot at the examples in `src/macro-core`.  
The key functions to add conditions or are the Register() functions.

```
MacroActionFactory::Register(
    MacroActionExample::id,                 // Unique string identifying this action type
    {
        MacroActionExample::Create,         // Function called to create the object performing the action
        MacroActionExampleEdit::Create,     // Function called to create the widget configure the action
        "AdvSceneSwitcher.action.example"   // User facing name of the action type
    }
);
```

If your intention is to add macro functionality which depends on external libraries, which is likely not to exist on all user setups, try to follow the examples under `src/macro-external`.
These are basically plugins themselves that get attempted to be loaded on startup of the advanced scene switcher.
