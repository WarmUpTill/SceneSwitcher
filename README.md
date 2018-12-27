# SceneSwitcher
An automated scene switcher for OBS Studio

More information can be found on https://obsproject.com/forum/resources/automatic-scene-switching.395/


## Compiling
### Prerequisites
You'll need CMake and a working development environment for OBS Studio installed on your computer.

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
```
git clone https://github.com/WarmUpTill/SceneSwitcher.git
cd SceneSwitcher
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR=<path to the libobs sub-folder in obs-studio's source code> -DLIBOBS_LIB=<path to libobs.0.dylib> -DOBS_FRONTEND_LIB=<path to libobs-frontend-api.dylib> -DQt5Core_DIR=/usr/local/opt/qt5/lib/cmake/Qt5Core -DQt5Widgets_DIR=/usr/local/opt/qt5/lib/cmake/Qt5Widgets ../
make -j4
# Copy advanced-scene-switcher.so to the obs-plugins folder
```


