cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-stream-deck)

add_library(${PROJECT_NAME} MODULE)

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)

target_sources(${PROJECT_NAME} PRIVATE macro-condition-stream-deck.cpp
                                       macro-condition-stream-deck.hpp)
setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
install_advss_plugin(${PROJECT_NAME})
