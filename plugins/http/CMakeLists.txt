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

if(NOT OPENSSL_INCLUDE_DIR OR NOT OPENSSL_LIBRARIES)
  find_package(OpenSSL)
  if(NOT OPENSSL_FOUND)
    message(WARNING "OpenSSL not found!\n" "HTTP support will be disabled!\n\n")
    return()
  endif()
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
  ${PROJECT_NAME} PRIVATE macro-action-http.cpp macro-action-http.hpp
                          key-value-list.cpp key-value-list.hpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_include_directories(${PROJECT_NAME} PRIVATE "${CPP_HTTPLIB_DIR}/"
                                                   "${OPENSSL_INCLUDE_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSSL_LIBRARIES} ZLIB::ZLIB)
install_advss_plugin(${PROJECT_NAME})
if(OS_WINDOWS)
  # Couldn't really find a better way to install runtime dependencies for
  # Windows TODO: Clean this up at some point
  function(FIND_FILES_WITH_PATTERN result pattern dir)
    execute_process(
      COMMAND
        powershell -Command
        "Get-ChildItem -Path '${dir}' -Recurse -Include ${pattern} |"
        "Select-Object -First 1 |"
        "ForEach-Object { $_.FullName -replace '\\\\', '\\\\' }"
      OUTPUT_VARIABLE files
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(${result}
        ${files}
        PARENT_SCOPE)
  endfunction()

  set(OPENSSL_DLL_SEARCH_DIR "${OPENSSL_INCLUDE_DIR}/..")
  find_files_with_pattern(CRYPTO_DLL_FILES "libcrypto*.dll"
                          "${OPENSSL_DLL_SEARCH_DIR}")
  find_files_with_pattern(SSL_DLL_FILES "libssl*.dll"
                          "${OPENSSL_DLL_SEARCH_DIR}")
  install_advss_plugin_dependency(TARGET ${PROJECT_NAME} DEPENDENCIES
                                  "${CRYPTO_DLL_FILES}" "${SSL_DLL_FILES}")
endif()
