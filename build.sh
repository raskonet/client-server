#!/bin/bash

echo "Building System..."

mkdir -p build
cd build

cmake ..
make

echo ""
echo "Build complete!"
echo "Executables are in: build/bin/"
echo ""
echo "To run:"
echo "  Terminal 1: ./build/bin/server"
echo "  Terminal 2: ./build/bin/client"
