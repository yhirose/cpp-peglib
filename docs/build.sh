#!/usr/bin/env bash
source ~/Projects/emsdk/emsdk_env.sh
emcc -std=c++17 -O3 --bind -o native.js -s ALLOW_MEMORY_GROWTH -fexceptions native.cpp
