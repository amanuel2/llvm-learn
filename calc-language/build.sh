#!/bin/sh
./cleanup.sh
mkdir build
cd build
cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
ninja -j 12