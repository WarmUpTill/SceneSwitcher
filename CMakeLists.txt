project(advanced-scene-switcher)

if(APPLE)
	find_library(COCOA Cocoa)
	include_directories(${COCOA})
endif()

if(UNIX)
	find_package(X11 REQUIRED)
	link_libraries(${X11_LIBRARIES})
	include_directories(${X11_INCLUDE_DIR})
endif()

set(advanced-scene-switcher_HEADERS
	${advanced-scene-switcher_HEADERS}
	advanced-scene-switcher.hpp
	switcher-data-structs.hpp
	utility.hpp
	)
set(advanced-scene-switcher_SOURCES
	${advanced-scene-switcher_SOURCES}
	advanced-scene-switcher.cpp
	advanced-scene-switcher-module.c
	scene-transitions.cpp
	screen-region-switch.cpp
	priority.cpp
	executable-switch.cpp
	idle-switch.cpp
	scene-round-trip.cpp
	file-switch.cpp
	window-title-switch.cpp
	hotkey.cpp
	general.cpp
	pause-switch.cpp
	random.cpp
	)
set(advanced-scene-switcher_UI
	${advanced-scene-switcher_UI}
	forms/advanced-scene-switcher.ui
	)

if(WIN32)
	set(advanced-scene-switcher_PLATFORM_SOURCES
		advanced-scene-switcher-win.cpp)

elseif(APPLE)
	set(advanced-scene-switcher_PLATFORM_SOURCES
		advanced-scene-switcher-osx.mm)
	set_source_files_properties(advanced-scene-switcher-osx.mm
		PROPERTIES COMPILE_FLAGS "-fobjc-arc")

	set(advanced-scene-switcher_PLATFORM_LIBS
		${COCOA})
else()
	set(advanced-scene-switcher_PLATFORM_SOURCES
		advanced-scene-switcher-nix.cpp)
	set(advanced-scene-switcher_PLATFORM_LIBS
		Xss)
endif()

qt5_wrap_ui(advanced-scene-switcher_UI_HEADERS
	${advanced-scene-switcher_UI}
	${advanced-scene-switcher_PLATFORM_UI})

add_library(advanced-scene-switcher MODULE
	${advanced-scene-switcher_HEADERS}
	${advanced-scene-switcher_SOURCES}
	${advanced-scene-switcher_UI_HEADERS}
	${advanced-scene-switcher_PLATFORM_SOURCES}
	${advanced-scene-switcher_PLATFORM_HEADERS}
	)
target_link_libraries(advanced-scene-switcher
	${advanced-scene-switcher_PLATFORM_LIBS}
	obs-frontend-api
	Qt5::Widgets
	libobs)

install_obs_plugin(advanced-scene-switcher data)
