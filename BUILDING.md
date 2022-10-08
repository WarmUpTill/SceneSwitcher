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

Then modify the obs-studio/UI/frontend-plugins/CMakeLists.txt Example and add an entry for the scene switcher:
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

If you would like to expand upon the macro system by adding a new condition or action type have a loot at the examples in `src/macro-core`.  
In general changes in the `src/legacy` folder should be avoided.  

## Macro condition
Macro conditions should inherit from the `MacroCondition` class and must implement the following functions:

```
class MacroConditionExample : public MacroCondition {
public:
    MacroConditionExample(Macro *m) : MacroCondition(m) {}
    // This function should perfrom the condition check
    bool CheckCondition();
    // This function should store the required condition data to "obj"
    // For example called on OBS shutdown
    bool Save(obs_data_t *obj);
    // This function should load the condition data from "obj"
    // For example called on OBS startup
    bool Load(obs_data_t *obj);
    // This function should return a unique id for this condition type
    // The _id is defined below
    std::string GetId() { return _id; };
    // Helper function called when new conditions are created
    // Will be used later when regeistering the new condition type
    static std::shared_ptr<MacroCondition> Create(Macro *m)
    {
        return std::make_shared<MacroConditionExample>(m);
    }

private:
    // Used to register new condition type
    static bool _registered;
    // Unique id identifying this condition type
    static const std::string _id;
};
```
When defining the widget used to control the settings of the condition type, it is important to add a static `Create()` method.  
It will be called whenever a new condition MacroConditionExample is created. (See `MacroConditionFactory::Register()`)  
```
class MacroConditionExampleEdit : public QWidget {
    Q_OBJECT

public:
    // ...
    MacroConditionExampleEdit(
        QWidget *parent,
        std::shared_ptr<MacroConditionExample> cond = nullptr);

    // Function will be used to create the widget for editing the settings of the condition
    static QWidget *Create(QWidget *parent,
                   std::shared_ptr<MacroCondition> cond)
    {
        return new MacroConditionExampleEdit(
            parent,
            std::dynamic_pointer_cast<MacroConditionExample>(cond));
    }
    // ...
};
```

To finally register the new condition type you will have to add the following call to `MacroConditionFactory::Register()`.

```
bool MacroConditionExample::_registered = MacroConditionFactory::Register(
    MacroConditionExample::id,                  // Unique string identifying this condition type
    {
        MacroConditionExample::Create,          // Function called to create the object performing the condition
        MacroConditionExampleEdit::Create,      // Function called to create the widget configure the condition
        "AdvSceneSwitcher.condition.example",   // User facing name of the condition type (will be translated)
        true                                    // Condition type supports duration modifiers (default true)
    }
);
```

## Macro action

The process of adding a new action type is very similar to adding a new condition.
The differences are highlighted in the comments below.

```
class MacroActionExample : public MacroAction {
public:
    MacroActionExample(Macro *m) : MacroAction(m) {}
    // This function should perfrom the action
    // If false is returned the macro will aborted
    bool PerformAction();
    bool Save(obs_data_t *obj);
    bool Load(obs_data_t *obj);
    std::string GetId() { return _id; };
    static std::shared_ptr<MacroAction> Create(Macro *m)
    {
        return std::make_shared<MacroActionExample>(m);
    }

private:
    static bool _registered;
    static const std::string _id;
};
```
```
class MacroActionExampleEdit : public QWidget {
	Q_OBJECT

public:
    // ...
	MacroActionExampleEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionExample> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionExampleEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionExample>(action));
	}
    // ...
};
```
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
## External dependencies
If your intention is to add macro functionality which depends on external libraries, which is likely not to exist on all user setups, try to follow the examples under `src/macro-external`.
These are basically plugins themselves that get attempted to be loaded on startup of the advanced scene switcher.
