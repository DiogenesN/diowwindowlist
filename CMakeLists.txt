cmake_minimum_required(VERSION 3.10)
project(diowwindowlist C)

set(CMAKE_C_STANDARD 11)

# Check for pkg-config
find_program(PKG_CONFIG_EXECUTABLE NAMES pkg-config)
if(NOT PKG_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "pkg-config not found. Please install the library.")
endif()

# Check for required libraries
find_package(PkgConfig REQUIRED)

pkg_check_modules(WAYLAND_SCANNER REQUIRED wayland-scanner)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)
pkg_check_modules(LIBRSVG REQUIRED librsvg-2.0)
pkg_check_modules(CAIRO_SVG REQUIRED cairo-svg)

# Set compiler flags and include directories
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -flto -Wextra -Wpedantic -march=native -funroll-loops -export-dynamic -fomit-frame-pointer")
include_directories(${WAYLAND_SCANNER_INCLUDE_DIRS} ${WAYLAND_CLIENT_INCLUDE_DIRS} ${WAYLAND_CURSOR_INCLUDE_DIRS} ${LIBRSVG_INCLUDE_DIRS} ${CAIRO_SVG_INCLUDE_DIRS})
add_definitions(-DWLR_USE_UNSTABLE)

# Generate .c and .h files from XML
message("Generate header files.")
set(XML_FILES xdg-shell.xml wlr-layer-shell-unstable-v1.xml wlr-foreign-toplevel-management-unstable-v1.xml)

foreach(XML_FILE ${XML_FILES})
    get_filename_component(BASENAME ${XML_FILE} NAME_WE)
	execute_process(
		INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/XML/${XML_FILE}
		COMMAND wayland-scanner private-code
		OUTPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/${BASENAME}-client-protocol.c
	)
endforeach()

foreach(XML_FILE ${XML_FILES})
    get_filename_component(BASENAME ${XML_FILE} NAME_WE)
	execute_process(
		INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/XML/${XML_FILE}
		COMMAND wayland-scanner client-header
		OUTPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/${BASENAME}-client-protocol.h
	)
endforeach()

# Source files
set(SRCFILES src/getstrfromsubstrinfile.c src/generate-icon-cache.c src/getvaluefromconf.c src/create-shm.c src/xdg-shell-client-protocol.c src/wlr-layer-shell-unstable-v1-client-protocol.c src/wlr-foreign-toplevel-management-unstable-v1-client-protocol.c src/diowwindowlist.c)

# Build executable with explicit source files
add_executable(diowwindowlist ${SRCFILES})
target_link_libraries(diowwindowlist ${WAYLAND_CLIENT_LIBRARIES} ${WAYLAND_CURSOR_LIBRARIES} ${LIBRSVG_LIBRARIES} ${CAIRO_SVG_LIBRARIES})

# Installation
install(TARGETS diowwindowlist DESTINATION bin)

# Uninstallation
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -E remove /usr/local/bin/diowwindowlist
)
