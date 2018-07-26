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
src/aes.cpp
src/aes.h
src/arm.cpp
src/arm.h
src/clock.cpp
src/clock.h
src/common.h
src/cp15.cpp
src/cp15.h
src/dmac.cpp
src/dmac.h
src/gpio.cpp
src/gpio.h
src/iphone2g.cpp
src/iphone2g.h
src/main.cpp
src/power.cpp
src/power.h
src/sha1.cpp
src/sha1.h
src/spi.cpp
src/spi.h
src/timer.cpp
src/timer.h
src/usb_otg.cpp
src/usb_otg.h
src/usb_phy.cpp
src/usb_phy.h
src/vic.cpp
src/vic.h
src/wdt.cpp
src/wdt.h
)

add_executable(purplesapphire ${SOURCES})

target_link_libraries(purplesapphire -lcrypto)