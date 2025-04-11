cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-mqtt)

# --- Check paho.mqtt.cpp requirements ---

if(NOT OPENSSL_INCLUDE_DIR OR NOT OPENSSL_LIBRARIES)
  find_package(PahoMqttCpp)
  if(NOT PahoMqttCpp_FOUND)
    message(WARNING "PahoMqttCpp not found!\n"
                    "MQTT support will be disabled!\n\n")
    return()
  endif()
endif()

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)

target_sources(
  ${PROJECT_NAME}
  PRIVATE macro-condition-mqtt.cpp macro-condition-mqtt.hpp mqtt-helpers.cpp
          mqtt-helpers.hpp topic-selection.cpp topic-selection.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} PRIVATE PahoMqttCpp::paho-mqttpp3-static)
install_advss_plugin(${PROJECT_NAME})
