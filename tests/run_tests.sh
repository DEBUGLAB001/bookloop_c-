#!/usr/bin/env bash
set -euo pipefail
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target challenge_tests -j2
exec ./build/challenge_tests
