cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-now-playing)

if(NOT WIN32)
  message(WARNING "Now playing condition is only supported on Windows.")
  return()
endif()

add_library(${PROJECT_NAME} MODULE)
target_sources(${PROJECT_NAME} PRIVATE macro-condition-now-playing.cpp
                                       macro-condition-now-playing.hpp)

target_link_libraries(${PROJECT_NAME} PRIVATE windowsapp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
install_advss_plugin(${PROJECT_NAME})
