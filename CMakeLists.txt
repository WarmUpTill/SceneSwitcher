cmake_minimum_required(VERSION 3.5)

project(advanced-scene-switcher)

# generate version info
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp" @ONLY)

if(BUILD_OUT_OF_TREE)
	# out of tree builds
	# credits to c3r1c3
	set(CMAKE_PREFIX_PATH "${QTDIR}")
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/external")

	include(external/FindLibObs.cmake)
	find_package(LibObs REQUIRED)
	find_package(Qt5Core REQUIRED)
	find_package(Qt5Widgets REQUIRED)
	find_package(Libcurl REQUIRED)
	find_path(LIBOBS_FRONTEND_INCLUDE_DIR HINTS ${LIBOBS_INCLUDE_DIRS})
	find_file(LIBOBS_FRONTEND_API_LIB NAMES libobs-frontend-api.* HINTS ${LIBOBS_LIB} )

	if(APPLE)
		find_library(COCOA Cocoa)
		find_package(Qt5MacExtras REQUIRED)
		include_directories(${COCOA})
		add_link_options("-v")
	endif()

	if(UNIX AND NOT APPLE)
		find_package(X11 REQUIRED)
		link_libraries(${X11_LIBRARIES})
		include_directories(${X11_INCLUDE_DIR})
	endif()

	set(advanced-scene-switcher_HEADERS
		${advanced-scene-switcher_HEADERS}
		src/headers/advanced-scene-switcher.hpp
		src/headers/switcher-data-structs.hpp
		src/headers/utility.hpp
		src/headers/curl-helper.hpp
		src/headers/switch-audio.hpp
		src/headers/switch-executable.hpp
		src/headers/switch-file.hpp
		src/headers/switch-idle.hpp
		src/headers/switch-media.hpp
		src/headers/switch-random.hpp
		src/headers/switch-screen-region.hpp
		src/headers/switch-time.hpp
		src/headers/switch-transitions.hpp
		src/headers/switch-window.hpp
		src/headers/switch-sequence.hpp
		src/headers/switch-generic.hpp
		src/headers/version.h
		)

	set(advanced-scene-switcher_SOURCES
		${advanced-scene-switcher_SOURCES}
		src/advanced-scene-switcher.cpp
		src/advanced-scene-switcher-module.c
		src/switcher-data-structs.cpp
		src/switch-transitions.cpp
		src/switch-screen-region.cpp
		src/switch-priority.cpp
		src/switch-executable.cpp
		src/switch-idle.cpp
		src/switch-sequence.cpp
		src/switch-file.cpp
		src/switch-window.cpp
		src/switch-media.cpp
		src/hotkey.cpp
		src/general.cpp
		src/switch-pause.cpp
		src/switch-random.cpp
		src/switch-time.cpp
		src/switch-audio.cpp
		src/switch-generic.cpp
		src/curl-helper.cpp
		src/volume-control.cpp
		src/version.cpp
		)

	set(advanced-scene-switcher_UI
		${advanced-scene-switcher_UI}
		forms/advanced-scene-switcher.ui
		)

	if(WIN32)
		set(advanced-scene-switcher_PLATFORM_SOURCES
			src/win/advanced-scene-switcher-win.cpp)

	elseif(APPLE)
		set(advanced-scene-switcher_PLATFORM_SOURCES
			src/osx/advanced-scene-switcher-osx.mm)
		set_source_files_properties(advanced-scene-switcher-osx.mm
			PROPERTIES COMPILE_FLAGS "-fobjc-arc")
		set(advanced-scene-switcher_PLATFORM_LIBS
			${COCOA})

	elseif("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
		set(advanced-scene-switcher_PLATFORM_SOURCES
			src/linux/advanced-scene-switcher-nix.cpp)
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

	include_directories(
		"${LIBOBS_INCLUDE_DIRS}"
		"${LIBOBS_FRONTEND_INCLUDE_DIR}"
		${Qt5Core_INCLUDES}
		${Qt5Widgets_INCLUDES}
		${LIBCURL_INCLUDE_DIRS})

	target_link_libraries(advanced-scene-switcher
		${advanced-scene-switcher_PLATFORM_LIBS}
		${LIBOBS_LIB}
		${LIBOBS_FRONTEND_API_LIB}
		Qt5::Core
		Qt5::Widgets)

	# Additional commands to install the module in the correct place.
	# Find all the translation files so we can copy them to the correct
	# place later on.
	file(GLOB ASS_TRANSLATION_FILES
			"data/locale/*.ini"
			)

	# Windows
	if(WIN32)
		string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		message("${CMAKE_CXX_FLAGS_RELEASE}")

		find_package(w32-pthreads REQUIRED)

		target_link_libraries(advanced-scene-switcher
				w32-pthreads)
	endif()

	# OSX
	if(APPLE)
		set_target_properties(advanced-scene-switcher PROPERTIES PREFIX "")

	endif()

	# Linux
	if(UNIX AND NOT APPLE)
		set(CMAKE_C_FLAGS "-Wall -Wextra -Wvla -Wno-unused-function -Werror-implicit-function-declaration -Wno-missing-braces -Wno-missing-field-initializers ${CMAKE_C_FLAGS} -std=c99 -fno-strict-aliasing")

		if(ARCH EQUAL 64)
			set(ARCH_NAME "x86_64")
		else()
			set(ARCH_NAME "i686")
		endif()

		set_target_properties(advanced-scene-switcher PROPERTIES PREFIX "")

		install(TARGETS advanced-scene-switcher
				LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/obs-plugins)
		install(FILES ${ASS_TRANSLATION_FILES}
				DESTINATION "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/advanced-scene-switcher/locale")
	endif()
else ()
	# in-tree build
	if(APPLE)
		find_library(COCOA Cocoa)
		include_directories(${COCOA})
	endif()

	if(UNIX AND NOT APPLE)
		find_package(X11 REQUIRED)
		link_libraries(${X11_LIBRARIES})
		include_directories(${X11_INCLUDE_DIR})
	endif()

	set(advanced-scene-switcher_HEADERS
		${advanced-scene-switcher_HEADERS}
		src/headers/advanced-scene-switcher.hpp
		src/headers/switcher-data-structs.hpp
		src/headers/utility.hpp
		src/headers/curl-helper.hpp
		src/headers/volume-control.hpp
		src/headers/switch-audio.hpp
		src/headers/switch-executable.hpp
		src/headers/switch-file.hpp
		src/headers/switch-idle.hpp
		src/headers/switch-media.hpp
		src/headers/switch-random.hpp
		src/headers/switch-screen-region.hpp
		src/headers/switch-time.hpp
		src/headers/switch-transitions.hpp
		src/headers/switch-window.hpp
		src/headers/switch-sequence.hpp
		src/headers/switch-generic.hpp
		src/headers/version.h
		)

	set(advanced-scene-switcher_SOURCES
		${advanced-scene-switcher_SOURCES}
		src/advanced-scene-switcher.cpp
		src/advanced-scene-switcher-module.c
		src/switcher-data-structs.cpp
		src/switch-transitions.cpp
		src/switch-screen-region.cpp
		src/switch-priority.cpp
		src/switch-executable.cpp
		src/switch-idle.cpp
		src/switch-sequence.cpp
		src/switch-file.cpp
		src/switch-window.cpp
		src/switch-media.cpp
		src/hotkey.cpp
		src/general.cpp
		src/switch-pause.cpp
		src/switch-random.cpp
		src/switch-time.cpp
		src/switch-audio.cpp
		src/switch-generic.cpp
		src/curl-helper.cpp
		src/volume-control.cpp
		src/version.cpp
		)

	set(advanced-scene-switcher_UI
		${advanced-scene-switcher_UI}
		forms/advanced-scene-switcher.ui
		)

	if(WIN32)
		set(advanced-scene-switcher_PLATFORM_SOURCES
			src/win/advanced-scene-switcher-win.cpp)
	elseif(APPLE)
		set(advanced-scene-switcher_PLATFORM_SOURCES
			src/osx/advanced-scene-switcher-osx.mm)
		set_source_files_properties(advanced-scene-switcher-osx.mm
			PROPERTIES COMPILE_FLAGS "-fobjc-arc")
		set(advanced-scene-switcher_PLATFORM_LIBS
			${COCOA})
	else()
		set(advanced-scene-switcher_PLATFORM_SOURCES
			src/linux/advanced-scene-switcher-nix.cpp)
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

	install_obs_plugin_with_data(advanced-scene-switcher data)
endif()
