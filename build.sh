#!/bin/bash
set -e

echo "=== Building LicenseManager ==="

mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

echo ""
echo "Build complete!"
echo ""
echo "Executables:"
echo "  build/bin/server"
echo "  build/bin/client"
echo "  build/bin/admin"
echo ""
echo "Run:"
echo "  ./build/bin/server"
echo "  ./build/bin/client"
echo "  ./build/bin/admin"

