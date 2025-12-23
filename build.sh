#!/bin/sh
cd "$(dirname "$0")"

git submodule update --init --recursive
cmake -S . -B build
cmake --build build

echo "Build succesful"
