# Compiling the plugin

You have the option to ...
- either add the plugin to the OBS source tree directly and build the plugin while building OBS itself. (**in tree**)
- or you can move the sources of this plugin outside of the OBS source tree and build it separately from OBS. (**out of tree**)

As both methods require you to have a working [OBS Studio development environment](https://obsproject.com/wiki/install-instructions), [Qt](https://download.qt.io/official_releases/qt/5.15/5.15.2/) and [CMake](https://cmake.org/download/) it is recommended to build the plugin in tree as it is easier to set up and will enable straightforward debugging.

Note that your Qt install must include the QtConcurrent module.

## Compiling in tree (recommended)
Add the "SceneSwitcher" source directory to your obs-studio source directory under obs-studio/UI/frontend-plugins/:
```
cd obs-studio/UI/frontend-plugins/
git clone --recursive https://github.com/WarmUpTill/SceneSwitcher.git
```

Then modify the obs-studio/UI/frontend-plugins/CMakeLists.txt file and add an entry for the scene switcher:
```
add_subdirectory(SceneSwitcher)
```

Now follow the [build instructions for obs-studio](https://obsproject.com/wiki/install-instructions) for your particular platform.

Note that on Linux systems it might be necessary to additionally install the following packages to fulfill the dependencies to `XTest` and `XScreensaver` - exact command may differ:
```
sudo apt-get install \
    libxtst-dev \
    libxss-dev
```

## Compiling out of tree
### Prerequisites
You'll need [Qt](https://download.qt.io/official_releases/qt/5.15/5.15.2/), [CMake](https://cmake.org/download/) and a working [OBS Studio development environment](https://obsproject.com/wiki/install-instructions) installed on your computer.
Once you've set this up, do the following:
```
git clone --recursive https://github.com/WarmUpTill/SceneSwitcher.git
cd SceneSwitcher
mkdir build && cd build
```

### Windows
In cmake-gui, you'll have to set these CMake variables :
- **BUILD_OUT_OF_TREE** (bool) : true

- **LIBOBS_LIB** (filepath) : location of the obs.lib file
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source
code of OBS Studio, located at [source_directory]/libobs/.

- **LIBOBS_FRONTEND_API_LIB** (filepath) : location of the obs-frontend-api.lib file
(usually in the same place as LIBOBS_LIB)
- **LIBOBS_FRONTEND_INCLUDE_DIR** (path) : location of the obs-frontend-api
subfolder in the source code of OBS Studio, located at [source_directory]/UI/obs-frontend-api.

- **CURL_LIBRARY** (filepath) : location of the libcurl.lib file
(part of the dependencies2019/win64/bin folder used to build OBS)
- **CURL_INCLUDE_DIR** (path) : location of the curl
subfolder in OBS dependencies folder: ".../dependencies2019/win64/include/"


Assuming that you set up Qt via QT installer:
- **Qt5Core_DIR** (path) : C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5Core
- **Qt5Gui_DIR** (path): C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5Gui
- **Qt5Widgets_DIR** (path) : C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5Widgets

Just keep hitting configure until all the vars are filled out. Then hit generate.

### Linux
Install dependencies `XTest` and `XScreensaver` - exact command may differ:
```
sudo apt-get install \
    libxtst-dev \
    libxss-dev
```

Most versions of Linux you can use cmake-gui or the command line.

**For the command line:**  
```
# [...] are placeholders
cmake -DBUILD_OUT_OF_TREE=1 \
-DLIBOBS_INCLUDE_DIR=[...]/obs-studio/libobs/ \
-DLIBOBS_LIB=[...]/libobs.so \
-DLIBOBS_FRONTEND_INCLUDE_DIR=[...]/obs-studio/UI/obs-frontend-api/ \
-DLIBOBS_FRONTEND_API_LIB=[...]/libobs-frontend-api.so \
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
- **LIBOBS_FRONTEND_API_LIB** (filepath) : location of the libobs-frontend-api.so
file (usually in the same place as LIBOBS_LIB)
- **LIBOBS_FRONTEND_INCLUDE_DIR** (path) : location of the obs-frontend-api
subfolder in the source code of OBS Studio, located at
[source_directory]/UI/obs-frontend-api.

Assuming that you installed Qt via your system package manager, it should be 
found automatically. If not, then usually you'll find it in something like:
- **Qt5Core_DIR** (path) : /usr/lib64/cmake/Qt5Core
- **Qt5Gui_DIR** (path): /usr/lib64/cmake/Qt5Gui
- **Qt5Widgets_DIR** (path) : /usr/lib64/cmake/Qt5Widgets

Just keep hitting configure until all the vars are filled out. Then hit generate.

Then open a terminal in the build folder and type:
```
make -j4
sudo make install
```
NOTE: The Linux version of this plugin is dependent on libXScrnSaver, libcurl and libXtst.

### OS X
In cmake-gui, you'll have to set these CMake variables :
- **BUILD_OUT_OF_TREE** (bool) : true
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source
code of OBS Studio, located at [source_directory]/libobs/.
- **LIBOBS_LIB** (filepath) : location of the libobs.0.dylib file (usually
in /Applications/OBS.app/Contents/Resources/bin/libobs.0.dylib)
- **LIBOBS_FRONTEND_API_LIB** (filepath) : location of the libobs-frontend-api.0.dylib
file (usually in usually in /Applications/OBS.app/Contents/Resources/bin/libobs-frontend-api.0.dylib)
- **LIBOBS_FRONTEND_INCLUDE_DIR** (path) : location of the obs-frontend-api subfolder
in the source code of OBS Studio, located at [source_directory]/UI/obs-frontend-api.

Assuming that you installed Qt via the regular Qt App way:
- **Qt5Core_DIR** (path) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5Core
- **Qt5Widgets_DIR** (path) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5Widgets
- **Qt5MacExtras_DIR** (path) : Usually /Applications/Qt/5.10.1/clang_64/lib/cmake/Qt5MacExtras


Just keep hitting configure until all the vars are filled out. Then hit generate.

Open xcode (or a terminal, depending on the build type you chose), build and copy
the advanced-scene-switcher.so file to 'Library/Application Support/obs-studio/plugins/advanced-scene-switcher/bin/'
And the 'data' folder to 'Library/Application Support/obs-studio/plugins/advanced-scene-switcher/'.

Note that you might have to adjust the library search paths using the install_name_tool if you want the plugin to run on machines other than your build machine:
```
install_name_tool -change @rpath/libobs-frontend-api.dylib @executable_path/../Frameworks/libobs-frontend-api.dylib UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
install_name_tool -change @rpath/libobs.0.dylib @executable_path/../Frameworks/libobs.0.dylib UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
install_name_tool -change /usr/local/opt/qt5/lib/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
install_name_tool -change /usr/local/opt/qt5/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
install_name_tool -change /usr/local/opt/qt5/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore UI/frontend-plugins/SceneSwitcher/advanced-scene-switcher.so
```
