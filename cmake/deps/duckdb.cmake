# Configure DuckDB build options - following the release build configuration from Makefile
set(CMAKE_BUILD_TYPE Release CACHE STRING "")
set(BUILD_STATIC_LIBS ON CACHE BOOL "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

# Windows-specific settings
if(WIN32)
    set(CMAKE_GENERATOR_PLATFORM x64)
    set(ENABLE_EXTENSION_AUTOLOADING 1)
    set(ENABLE_EXTENSION_AUTOINSTALL 1)
    set(DUCKDB_EXTENSION_CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/deps/duckdb/.github/config/bundled_extensions.cmake")
endif()

# Force colored output when using Ninja
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

# Other important options from the Makefile's CMAKE_VARS
set(BUILD_SHELL ON CACHE BOOL "")               # Not disabled by default
set(BUILD_EXTENSIONS "" CACHE STRING "")        # Empty by default
set(DISABLE_BUILTIN_EXTENSIONS OFF CACHE BOOL "")

# Add DuckDB as subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/duckdb EXCLUDE_FROM_ALL)

# Create aliases for both static and C API libraries
add_library(DuckDB::duckdb ALIAS duckdb_static)
if(WIN32)
    add_library(DuckDB::duckdb_api ALIAS duckdb)
endif()