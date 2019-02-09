# SceneSwitcher
An automated scene switcher for OBS Studio

More information can be found on https://obsproject.com/forum/resources/automatic-scene-switching.395/


## Compiling
### Prerequisites
You'll need CMake and a working development environment for OBS Studio installed
on your computer. Once you've done that, do the following:
```
git clone https://github.com/WarmUpTill/SceneSwitcher.git
cd SceneSwitcher
mkdir build && cd build
```

### Windows
In cmake-gui, you'll have to set these CMake variables :
- **QTDIR** (path) : location of the Qt environment suited for your compiler and architecture
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source code of OBS Studio
- **LIBOBS_LIB** (filepath) : location of the obs.lib file
- **OBS_FRONTEND_LIB** (filepath) : location of the obs-frontend-api.lib file

### Linux
On Debian/Ubuntu :  
```
git clone https://github.com/WarmUpTill/SceneSwitcher.git
cd SceneSwitcher
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR="<path to the libobs sub-folder in obs-studio's source code>" -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
sudo make install
```
NOTE: The Linux version of this plugin is dependent on libXScrnSaver.

### OS X
In cmake-gui, you'll have to set these CMake variables :
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source
code of OBS Studio, located at [source_directory]/libobs/.
- **LIBOBS_LIB** (filepath) : location of the libobs.0.dylib file (usually
in /Applications/OBS.app/Contents/Resources/bin/libobs.0.dylib)

Assuming that you installed Qt via the regular Qt App way:
- **Qt5Core_DIR** (filepath) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5Core
- **Qt5Widgets_DIR** (filepath) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5Widgets
- **Qt5MacExtras_DIR** (filepath) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5MacExtras

- **OBS_FRONTEND_API_LIB** (filepath) : location of the libobs-frontend-api.0.dylib
file (usually in usually in /Applications/OBS.app/Contents/Resources/bin/libobs-frontend-api.0.dylib)
- **OBS_FRONTEND_INCLUDE_DIR** (path) : location of the obs-frontend-api subfolder
in the source code of OBS Studio, located at [source_directory]/UI/obs-frontend-api.

Just keep hitting configure until all the vars are filled out. Then hit generate.

Open xcode (or a terminal, depending on the build type you chose), build and copy
the advanced-scene-switcher.so file to /Applications/OBS.app/Contents/Resources/obs-plugins/
