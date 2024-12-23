set(CMAKE_BUILD_TYPE Release CACHE STRING "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

if(WIN32)
    set(CMAKE_GENERATOR_PLATFORM x64)
    set(ENABLE_EXTENSION_AUTOLOADING 1)
    set(ENABLE_EXTENSION_AUTOINSTALL 1)
    set(DUCKDB_EXTENSION_CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/deps/duckdb/.github/config/bundled_extensions.cmake")
    set(DISABLE_UNITY 1)
endif()

if(CMAKE_GENERATOR MATCHES "Ninja")
    set(FORCE_COLORED_OUTPUT ON CACHE BOOL "")
endif()

# These match the Makefile variables used in the release target
set(ENABLE_SANITIZER OFF CACHE BOOL "")
set(ENABLE_UBSAN OFF CACHE BOOL "")
set(TREAT_WARNINGS_AS_ERRORS OFF CACHE BOOL "")  # ${WARNINGS_AS_ERRORS}
set(FORCE_WARN_UNUSED OFF CACHE BOOL "")         # ${FORCE_WARN_UNUSED_FLAG}
set(FORCE_32_BIT OFF CACHE BOOL "")             # ${FORCE_32_BIT_FLAG}
set(DISABLE_UNITY OFF CACHE BOOL "")            # ${DISABLE_UNITY_FLAG}
set(STATIC_LIBCPP OFF CACHE BOOL "")            # ${STATIC_LIBCPP}
set(BUILD_SHELL OFF CACHE BOOL "")               # Not disabled by default

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/duckdb EXCLUDE_FROM_ALL)
add_library(DuckDB::duckdb ALIAS duckdb_static)