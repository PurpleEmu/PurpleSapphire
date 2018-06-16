cmake_minimum_required (VERSION 3.8)
set( CMAKE_VERBOSE_MAKEFILE ON )

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project (purplesapphire)

#properly disable insource building, as CMAKE_DISABLE_IN_SOURCE_BUILD never works for me
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(SEND_ERROR "Insource building is disabled.")
endif()

set(SOURCES
src/arm.cpp
src/arm.h
src/clock.cpp
src/clock.h
src/common.h
src/cp15.cpp
src/cp15.h
src/iphone2g.cpp
src/iphone2g.h
src/iphone3gs.cpp
src/iphone3gs.h
src/main.cpp
src/vic.cpp
src/vic.h
src/wdt.cpp
src/wdt.h
)

add_executable(purplesapphire ${SOURCES})

target_link_libraries(purplesapphire)