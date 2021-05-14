cmake_minimum_required(VERSION 3.5)

project(advanced-scene-switcher)
add_definitions(-DASIO_STANDALONE)

# generate version info
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp" @ONLY)
if(WIN32)
	get_filename_component(ISS_PLUGIN_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PLUGIN_FILES_DIR}" ISS_PLUGIN_FILES_DIR)
	get_filename_component(ISS_MSVC_REDIST_HELPER_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_MSVC_REDIST_HELPER_DIR}" ISS_MSVC_REDIST_HELPER_DIR)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/CI/windows/setup.iss.in" "${CMAKE_CURRENT_BINARY_DIR}/CI/windows/setup.iss" @ONLY)
endif()

if(WIN32)
	get_filename_component(ISS_PLUGIN_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PLUGIN_FILES_DIR}" ISS_PLUGIN_FILES_DIR)
	get_filename_component(ISS_MSVC_REDIST_HELPER_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_MSVC_REDIST_HELPER_DIR}" ISS_MSVC_REDIST_HELPER_DIR)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/CI/windows/setup.iss.in" "${CMAKE_CURRENT_BINARY_DIR}/CI/windows/setup.iss" @ONLY)
endif()

if(BUILD_OUT_OF_TREE)
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
endif()

if(APPLE)
	find_library(COCOA Cocoa)
	if(BUILD_OUT_OF_TREE)
		find_package(Qt5MacExtras REQUIRED)
	endif()
	include_directories(${COCOA})
endif()


if(UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
	link_libraries(${X11_LIBRARIES})
	include_directories(${X11_INCLUDE_DIR})
endif()

include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp"
	)

set(advanced-scene-switcher_HEADERS
	${advanced-scene-switcher_HEADERS}
	src/headers/advanced-scene-switcher.hpp
	src/headers/switcher-data-structs.hpp
	src/headers/utility.hpp
	src/headers/scene-group.hpp
	src/headers/scene-trigger.hpp
	src/headers/switch-audio.hpp
	src/headers/switch-executable.hpp
	src/headers/switch-file.hpp
	src/headers/switch-idle.hpp
	src/headers/switch-media.hpp
	src/headers/switch-network.hpp
	src/headers/switch-pause.hpp
	src/headers/switch-random.hpp
	src/headers/switch-screen-region.hpp
	src/headers/switch-time.hpp
	src/headers/switch-transitions.hpp
	src/headers/switch-window.hpp
	src/headers/switch-sequence.hpp
	src/headers/switch-video.hpp
	src/headers/switch-generic.hpp
	src/headers/macro-action-edit.hpp
	src/headers/macro-action-audio.hpp
	src/headers/macro-action-recording.hpp
	src/headers/macro-action-replay-buffer.hpp
	src/headers/macro-action-run.hpp
	src/headers/macro-action-streaming.hpp
	src/headers/macro-action-switch-scene.hpp
	src/headers/macro-action-wait.hpp
	src/headers/macro-condition-edit.hpp
	src/headers/macro-condition-audio.hpp
	src/headers/macro-condition-file.hpp
	src/headers/macro-condition-idle.hpp
	src/headers/macro-condition-media.hpp
	src/headers/macro-condition-plugin-state.hpp
	src/headers/macro-condition-process.hpp
	src/headers/macro-condition-recording.hpp
	src/headers/macro-condition-region.hpp
	src/headers/macro-condition-scene.hpp
	src/headers/macro-condition-streaming.hpp
	src/headers/macro-condition-video.hpp
	src/headers/macro-condition-virt-desktop.hpp
	src/headers/macro-condition-window.hpp
	src/headers/macro.hpp
	src/headers/curl-helper.hpp
	src/headers/screenshot-helper.hpp
	src/headers/name-dialog.hpp
	src/headers/duration-control.hpp
	src/headers/section.hpp
	src/headers/volume-control.hpp
	src/headers/version.h
	)

set(advanced-scene-switcher_SOURCES
	${advanced-scene-switcher_SOURCES}
	src/advanced-scene-switcher.cpp
	src/advanced-scene-switcher-module.c
	src/switcher-data-structs.cpp
	src/scene-group.cpp
	src/scene-trigger.cpp
	src/switch-transitions.cpp
	src/switch-screen-region.cpp
	src/switch-priority.cpp
	src/switch-executable.cpp
	src/switch-idle.cpp
	src/switch-sequence.cpp
	src/switch-file.cpp
	src/switch-window.cpp
	src/switch-media.cpp
	src/switch-network.cpp
	src/hotkey.cpp
	src/general.cpp
	src/switch-pause.cpp
	src/switch-random.cpp
	src/switch-time.cpp
	src/switch-audio.cpp
	src/switch-video.cpp
	src/switch-generic.cpp
	src/macro-action-edit.cpp
	src/macro-action-audio.cpp
	src/macro-action-recording.cpp
	src/macro-action-replay-buffer.cpp
	src/macro-action-run.cpp
	src/macro-action-streaming.cpp
	src/macro-action-switch-scene.cpp
	src/macro-action-wait.cpp
	src/macro-condition-edit.cpp
	src/macro-condition-audio.cpp
	src/macro-condition-file.cpp
	src/macro-condition-idle.cpp
	src/macro-condition-media.cpp
	src/macro-condition-plugin-state.cpp
	src/macro-condition-process.cpp
	src/macro-condition-recording.cpp
	src/macro-condition-region.cpp
	src/macro-condition-scene.cpp
	src/macro-condition-streaming.cpp
	src/macro-condition-video.cpp
	src/macro-condition-virt-desktop.cpp
	src/macro-condition-window.cpp
	src/macro.cpp
	src/macro-tab.cpp
	src/curl-helper.cpp
	src/screenshot-helper.cpp
	src/name-dialog.cpp
	src/duration-control.cpp
	src/section.cpp
	src/volume-control.cpp
	src/version.cpp
	)

set(advanced-scene-switcher_UI
	${advanced-scene-switcher_UI}
	forms/advanced-scene-switcher.ui
	)

if(WIN32)
	add_definitions(-D_WEBSOCKETPP_CPP11_STL_)

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

if(BUILD_OUT_OF_TREE)
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
else()
	target_link_libraries(advanced-scene-switcher
		${advanced-scene-switcher_PLATFORM_LIBS}
		obs-frontend-api
		Qt5::Widgets
		libobs)

	install_obs_plugin_with_data(advanced-scene-switcher data)
endif()
