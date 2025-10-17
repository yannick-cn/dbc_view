#!/bin/bash

# DBC Viewer Build Script

echo "Building DBC Viewer..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
make -j$(nproc)

echo "Build completed!"
echo "Run the application with: ./DBCViewer"
