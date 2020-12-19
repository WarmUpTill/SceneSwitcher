# SceneSwitcher
An automated scene switcher for OBS Studio

More information can be found on https://obsproject.com/forum/resources/automatic-scene-switching.395/

## Compiling in tree (recommended)
Add the "SceneSwitcher" source directory to your obs-studio source directory under obs-studio/UI/frontend-plugins/:
```
cd obs-studio/UI/frontend-plugins/
git clone https://github.com/WarmUpTill/SceneSwitcher.git
```

Then modify the obs-studio/UI/frontend-plugins/CMakeLists.txt file and add an entry for the scene switcher:
add_subdirectory(SceneSwitcher)

## Compiling out of tree
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
- **BUILD_OUT_OF_TREE** (bool) : true
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source
code of OBS Studio, located at [source_directory]/libobs/.
- **LIBOBS_LIB** (filepath) : location of the obs.dll file (usually set to
C:\Program Files\obs-studio\bin\64bit\obs.dll)

Assuming that you installed Qt via the regular Qt App way:
- **Qt5Core_DIR** (path) : C:/Qt/5.10.1/msvc2017_64/lib/cmake/Qt5Core
- **Qt5Gui_DIR** (path): C:/Qt/5.10.1/msvc2017_64/lib/cmake/Qt5Gui
- **Qt5Widgets_DIR** (path) : C:/Qt/5.10.1/msvc2017_64/lib/cmake/Qt5Widgets

- **LIBOBS_FRONTEND_API_LIB** (filepath) : location of the libobs-frontend-api.dll
file, usually C:/Program Files/obs-studio/bin/64bit/obs-frontend-api.dll
(usually in the same place as LIBOBS_LIB)
- **LIBOBS_FRONTEND_INCLUDE_DIR** (path) : location of the obs-frontend-api
subfolder in the source code of OBS Studio, located at [source_directory]/UI/obs-frontend-api.

Just keep hitting configure until all the vars are filled out. Then hit generate.

### Linux
Most versions of Linux you can use cmake-gui or the command line.

**For the command line:**  
```
cmake -DBUILD_OUT_OF_TREE=1 -DLIBOBS_INCLUDE_DIR="<path to the libobs/ sub-folder in obs-studio's source code>"
-DLIBOBS_FRONTEND_INCLUDE_DIR="<path to the UI/obs-frontend-api/ sub-folder in obs-studio's source code>"
-DLIBOBS_FRONTEND_API_LIB="< path to the libobs-frontend-api.so file (usually in the same place as LIBOBS_LIB)>"
-DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
sudo make install
```

For cmake-gui you'll have to set the following variables:
- **BUILD_OUT_OF_TREE** (bool) : true
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source
code of OBS Studio, located at [source_directory]/libobs/.
- **LIBOBS_LIB** (filepath) : location of the libobs.so file (usually CMake finds
this, but if not it'll usually be in /usr/lib/libobs.so)

Assuming that you installed Qt via your system package manager, it should be 
found automatically. If not, then usually you'll find it in something like:
- **Qt5Core_DIR** (path) : /usr/lib64/cmake/Qt5Core
- **Qt5Gui_DIR** (path): /usr/lib64/cmake/Qt5Gui
- **Qt5Widgets_DIR** (path) : /usr/lib64/cmake/Qt5Widgets

- **LIBOBS_FRONTEND_API_LIB** (filepath) : location of the libobs-frontend-api.so
file (usually in the same place as LIBOBS_LIB)
- **LIBOBS_FRONTEND_INCLUDE_DIR** (path) : location of the obs-frontend-api
subfolder in the source code of OBS Studio, located at
[source_directory]/UI/obs-frontend-api.

Just keep hitting configure until all the vars are filled out. Then hit generate.

Then open a terminal in the build folder and type:
```
make -j4
sudo make install
```
NOTE: The Linux version of this plugin is dependent on libXScrnSaver.

### OS X
In cmake-gui, you'll have to set these CMake variables :
- **BUILD_OUT_OF_TREE** (bool) : true
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source
code of OBS Studio, located at [source_directory]/libobs/.
- **LIBOBS_LIB** (filepath) : location of the libobs.0.dylib file (usually
in /Applications/OBS.app/Contents/Resources/bin/libobs.0.dylib)

Assuming that you installed Qt via the regular Qt App way:
- **Qt5Core_DIR** (path) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5Core
- **Qt5Widgets_DIR** (path) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5Widgets
- **Qt5MacExtras_DIR** (path) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5MacExtras

- **LIBOBS_FRONTEND_API_LIB** (filepath) : location of the libobs-frontend-api.0.dylib
file (usually in usually in /Applications/OBS.app/Contents/Resources/bin/libobs-frontend-api.0.dylib)
- **LIBOBS_FRONTEND_INCLUDE_DIR** (path) : location of the obs-frontend-api subfolder
in the source code of OBS Studio, located at [source_directory]/UI/obs-frontend-api.

Just keep hitting configure until all the vars are filled out. Then hit generate.

Open xcode (or a terminal, depending on the build type you chose), build and copy
the advanced-scene-switcher.so file to /Applications/OBS.app/Contents/PlugIns

Note that you might have to adjust the library search paths using the install_name_tool if you want the plugin to run on machines other than your build machine:
```
install_name_tool -change @rpath/libobs-frontend-api.dylib @executable_path/../Frameworks/libobs-frontend-api.dylib UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
install_name_tool -change @rpath/libobs.0.dylib @executable_path/../Frameworks/libobs.0.dylib UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
install_name_tool -change /usr/local/opt/qt5/lib/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
install_name_tool -change /usr/local/opt/qt5/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
install_name_tool -change /usr/local/opt/qt5/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
```
