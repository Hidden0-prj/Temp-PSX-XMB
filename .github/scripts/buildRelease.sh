#!/bin/bash
set -e

echo "Downloading official ps1-bare-metal SDK environment..."
if [ ! -d "sdk" ]; then
    # Corrected the repository author to 'spicyjpeg'
    git clone https://github.com/spicyjpeg/ps1-bare-metal.git sdk
fi

echo "Injecting custom project files..."
# 1. Clean out the default examples so they don't compile and waste time
rm -rf sdk/src/*
mkdir -p sdk/src

# 2. Inject our custom files directly into the SDK root
cp src/main.c sdk/src/main.c
cp CMakeLists.txt sdk/CMakeLists.txt
cp linker.ld sdk/linker.ld
cp -r tools sdk/tools
cp -r assets sdk/assets
cp -r include sdk/include

echo "Setting up the SDK's Python virtual environment..."
cd sdk
python3 -m venv env
source env/bin/activate
python3 -m pip install Pillow

echo "Building PlayStation Executable..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE="Release"
ninja

echo "Build complete! main.elf generated successfully."
