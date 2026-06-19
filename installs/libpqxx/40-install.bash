#!/usr/bin/env bash

set -e

cd "$(dirname "$0")" || exit 113

sudo cmake --install build
sudo ldconfig
