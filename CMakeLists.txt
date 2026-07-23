cmake_minimum_required(VERSION 3.10)
project(ps1_xmb_wave C)

set(CMAKE_C_STANDARD 99)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Python3 REQUIRED)

# Auto-convert diss_00.png texture to C byte array during build
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/diss_00_png.c
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/png_to_c.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/diss_00.png
            ${CMAKE_CURRENT_BINARY_DIR}/diss_00_png.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/diss_00.png ${CMAKE_CURRENT_SOURCE_DIR}/tools/png_to_c.py
)

set(SOURCES
    src/main.c
    ${CMAKE_CURRENT_BINARY_DIR}/diss_00_png.c
)

add_executable(main.elf ${SOURCES})

target_link_options(main.elf PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -nostdlib)