#!/usr/bin/env bash

cd "$(dirname "$0")" || exit 113

cmake --build build "-j$(nproc)"
