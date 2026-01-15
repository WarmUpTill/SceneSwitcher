cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-mqtt)

# --- Check paho.mqtt.cpp requirements ---

if(OS_MACOS)
  set(OPENSSL_USE_STATIC_LIBS
      ON
      CACHE BOOL "Use static OpenSSL" FORCE)
endif()
find_package(OpenSSL)

get_cmake_property(_variableNames VARIABLES)
list(SORT _variableNames)
foreach(_variableName ${_variableNames})
  message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

find_package(PahoMqttCpp)
if(NOT PahoMqttCpp_FOUND)
  message(WARNING "PahoMqttCpp not found!\n"
                  "MQTT support will be disabled!\n\n")
  return()
endif()

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

if(PAHO_MQTT_CPP_VERSION VERSION_GREATER_EQUAL "1.1.0")
  target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_MQTT5_SUPPORT=1)
endif()

# PDB was not found with paho-mqttpp3-static.lib
if(MSVC)
  target_link_options(${PROJECT_NAME} PRIVATE /IGNORE:4099)
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE macro-action-mqtt.cpp
          macro-action-mqtt.hpp
          macro-condition-mqtt.hpp
          macro-condition-mqtt.cpp
          mqtt-helpers.cpp
          mqtt-helpers.hpp
          mqtt-tab.cpp
          mqtt-tab.hpp
          topic-selection.cpp
          topic-selection.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
if(OS_LINUX)
  target_link_libraries(${PROJECT_NAME} PRIVATE PahoMqttCpp::paho-mqttpp3)
else()
  target_link_libraries(${PROJECT_NAME}
                        PRIVATE PahoMqttCpp::paho-mqttpp3-static)
endif()
install_advss_plugin(${PROJECT_NAME})
