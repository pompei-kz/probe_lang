#!/usr/bin/env bash

cd "$(dirname "$0")" || exit 113

mkdir -p build
cd build || exit 115

CC=clang-23 \
CXX=clang++-23 \
cmake ../libpqxx \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local
