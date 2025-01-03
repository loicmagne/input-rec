cmake_minimum_required(VERSION 3.16...3.26)

set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)

# Include dependencies configurations
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/duckdb.cmake)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.hpp")

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SOURCES} ${HEADERS})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add third-party dependencies
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE SDL3::SDL3)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE DuckDB::duckdb)

# required on windows to build a static library
if(WIN32)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE DUCKDB_STATIC_BUILD)
endif()