#!/bin/bash
set -e

# 1. Force the compiler into the PATH *inside* this script's subshell
WORKSPACE_DIR=$(pwd)
export PATH="$WORKSPACE_DIR/gcc-mipsel-none-elf/bin:$PATH"

# 2. Verify the compiler is accessible (This stops the phantom CMake errors)
echo "Checking for MIPS toolchain..."
mipsel-none-elf-gcc --version

echo "Downloading official ps1-bare-metal SDK environment..."
if [ ! -d "sdk" ]; then
    git clone https://github.com/spicyjpeg/ps1-bare-metal.git sdk
fi

echo "Injecting custom project files..."
# Remove default examples to speed up the build, but KEEP the SDK's core libc/common
rm -rf sdk/src/0*

# Inject our assets and tools into the SDK directory
cp -r tools sdk/tools
cp -r assets sdk/assets

# Create our demo folder inside the SDK's source directory
mkdir -p sdk/src/xmb_wave
cp src/main.c sdk/src/xmb_wave/main.c

# Create a localized CMakeLists.txt so the SDK builds our module natively
cat << 'EOF' > sdk/src/xmb_wave/CMakeLists.txt
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/diss_00_png.c
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/png_to_c.py
            ${CMAKE_SOURCE_DIR}/assets/diss_00.png
            ${CMAKE_CURRENT_BINARY_DIR}/diss_00_png.c
    DEPENDS ${CMAKE_SOURCE_DIR}/assets/diss_00.png ${CMAKE_SOURCE_DIR}/tools/png_to_c.py
)

add_executable(xmb_wave main.c ${CMAKE_CURRENT_BINARY_DIR}/diss_00_png.c)
target_link_libraries(xmb_wave PRIVATE common)
EOF

echo "Setting up the SDK's Python virtual environment..."
cd sdk
python3 -m venv env
source env/bin/activate
python3 -m pip install Pillow

echo "Building PlayStation Executable..."
mkdir -p build
cd build

# 3. Use default Unix Makefiles (Always installed on GitHub Runners)
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="Release"
make xmb_wave

echo "Build complete! Your executable is generated successfully in sdk/build/src/xmb_wave/"
