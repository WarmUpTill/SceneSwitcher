cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-scripting)

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

target_sources(
  ${PROJECT_NAME}
  PRIVATE macro-action-script.cpp
          macro-action-script.hpp
          macro-action-script-inline.cpp
          macro-action-script-inline.hpp
          macro-condition-script.cpp
          macro-condition-script.hpp
          macro-condition-script-inline.cpp
          macro-condition-script-inline.hpp
          macro-script-handler.cpp
          macro-script-handler.hpp
          macro-segment-script.cpp
          macro-segment-script.hpp
          macro-segment-script-inline.cpp
          macro-segment-script-inline.hpp)

target_sources(
  ${PROJECT_NAME}
  PRIVATE utils/inline-script.cpp
          utils/inline-script.hpp
          utils/obs-script-helpers.cpp
          utils/obs-script-helpers.hpp
          utils/properties-view.cpp
          utils/properties-view.hpp
          utils/properties-view.moc.hpp)

target_include_directories(${PROJECT_NAME}
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utils")

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
install_advss_plugin(${PROJECT_NAME})
