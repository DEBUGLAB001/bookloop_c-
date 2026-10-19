#!/usr/bin/env bash
set -euo pipefail
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
exec ./build/server
