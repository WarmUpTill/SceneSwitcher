# CMake Windows defaults module

include_guard(GLOBAL)

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

include(buildspec)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      "$ENV{ALLUSERSPROFILE}/obs-studio/plugins"
      CACHE STRING "Default plugin installation directory" FORCE)
endif()

# OBS 33 loads plugins from <name>/<name>.dll instead of <name>/bin/64bit
option(ADVSS_WINDOWS_LEGACY_LAYOUT
       "Install using the bin/64bit plugin layout required by OBS 32 and older"
       OFF)
if(ADVSS_WINDOWS_LEGACY_LAYOUT)
  set(ADVSS_WINDOWS_BIN_SUBDIR "/bin/64bit")
else()
  set(ADVSS_WINDOWS_BIN_SUBDIR "")
endif()
