option(SDL_SYSTEM "Use system SDL library" OFF)
set(SDL_STATIC ON CACHE BOOL "Build the static version of SDL" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Do not build the shared version of SDL" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ${SDL_STATIC})

if(SDL_SYSTEM)
  find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
else()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL EXCLUDE_FROM_ALL)
endif()