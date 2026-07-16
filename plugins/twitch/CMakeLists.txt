cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-twitch)

# --- Check requirements ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
set(CPP_HTTPLIB_DIR "${ADVSS_SOURCE_DIR}/deps/cpp-httplib")
if(NOT EXISTS "${CPP_HTTPLIB_DIR}/CMakeLists.txt")
  message(WARNING "cpp-httplib directory \"${CPP_HTTPLIB_DIR}\" not found!\n"
                  "Twitch support will be disabled!")
  return()
endif()
if(NOT TARGET httplib)
  add_subdirectory("${CPP_HTTPLIB_DIR}" "${CPP_HTTPLIB_DIR}/build"
                   EXCLUDE_FROM_ALL)
endif()

if(OS_MACOS OR OS_WINDOWS)
  set(OPENSSL_USE_STATIC_LIBS
      ON
      CACHE BOOL "Use static OpenSSL" FORCE)
endif()
find_package(OpenSSL)
if(NOT OPENSSL_FOUND)
  message(WARNING "OpenSSL not found!\n" "Twitch support will be disabled!\n\n")
  return()
endif()
if(OS_WINDOWS AND NOT OPENSSL_CRYPTO_LIBRARY MATCHES "_static\\.lib$")
  message(
    WARNING
      "Static OpenSSL libraries (libcrypto_static.lib / libssl_static.lib) not found!\n"
      "Twitch support will be disabled to avoid DLL name collisions with other plugins.\n\n"
  )
  return()
endif()

find_package(ZLIB)
if(NOT ZLIB_FOUND)
  message(WARNING "zlib not found!\n" "Twitch support will be disabled!\n\n")
  return()
endif()

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)
target_compile_definitions(
  ${PROJECT_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT=1 ASIO_STANDALONE=1
                          ASIO_HAS_OPENSSSL=1)
target_include_directories(
  ${PROJECT_NAME} PUBLIC "${ADVSS_SOURCE_DIR}/deps/asio/asio/include"
                         "${ADVSS_SOURCE_DIR}/deps/websocketpp")
if(OS_MACOS)
  target_compile_definitions(
    ${PROJECT_NAME} PRIVATE CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN=1)
  target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")
  target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Security")
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /wd4834)
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE category-selection.cpp
          category-selection.hpp
          channel-selection.cpp
          channel-selection.hpp
          chat-connection.cpp
          chat-connection.hpp
          chat-message-pattern.cpp
          chat-message-pattern.hpp
          content-classification.cpp
          content-classification.hpp
          event-sub.cpp
          event-sub.hpp
          twitch-timestamp.cpp
          twitch-timestamp.hpp
          language-selection.cpp
          language-selection.hpp
          macro-action-twitch.cpp
          macro-action-twitch.hpp
          macro-condition-twitch.cpp
          macro-condition-twitch.hpp
          points-reward-selection.cpp
          points-reward-selection.hpp
          tag-selection.cpp
          tag-selection.hpp
          token.cpp
          token.hpp
          twitch-helpers.cpp
          twitch-helpers.hpp
          twitch-tab.cpp
          twitch-tab.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME} PRIVATE "${CPP_HTTPLIB_DIR}/"
                                                   "${OPENSSL_INCLUDE_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSSL_LIBRARIES} ZLIB::ZLIB)
if(OS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 crypt32 bcrypt)
endif()
install_advss_plugin(${PROJECT_NAME})
