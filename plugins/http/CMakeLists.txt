cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-http)

# --- Check requirements ---

get_target_property(ADVSS_SOURCE_DIR advanced-scene-switcher-lib SOURCE_DIR)
set(CPP_HTTPLIB_DIR "${ADVSS_SOURCE_DIR}/deps/cpp-httplib")
if(NOT EXISTS "${CPP_HTTPLIB_DIR}/CMakeLists.txt")
  message(WARNING "cpp-httplib directory \"${CPP_HTTPLIB_DIR}\" not found!\n"
                  "HTTP support will be disabled!")
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
  message(WARNING "OpenSSL not found!\n" "HTTP support will be disabled!\n\n")
  return()
endif()
if(OS_WINDOWS AND NOT OPENSSL_CRYPTO_LIBRARY MATCHES "_static\\.lib$")
  message(
    WARNING
      "Static OpenSSL libraries (libcrypto_static.lib / libssl_static.lib) not found!\n"
      "HTTP support will be disabled to avoid DLL name collisions with other plugins.\n\n"
  )
  return()
endif()

find_package(ZLIB)
if(NOT ZLIB_FOUND)
  message(WARNING "zlib not found!\n" "HTTP support will be disabled!\n\n")
  return()
endif()

# --- End of section ---

add_library(${PROJECT_NAME} MODULE)
target_compile_definitions(${PROJECT_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT=1)
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
  PRIVATE macro-action-http.cpp
          macro-action-http.hpp
          macro-condition-http.cpp
          macro-condition-http.hpp
          http-server.cpp
          http-server.hpp
          http-server-tab.cpp
          http-server-tab.hpp
          key-value-list.cpp
          key-value-list.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME} PRIVATE "${CPP_HTTPLIB_DIR}/"
                                                   "${OPENSSL_INCLUDE_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSSL_LIBRARIES} ZLIB::ZLIB)
if(OS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 crypt32 bcrypt)
endif()
install_advss_plugin(${PROJECT_NAME})
