#!/usr/bin/env bash

set -e

cd "$(dirname "$0")" || exit 113

HERE="$PWD"
REMOVE_BUILD="NO"

if [[ ! -d libpqxx ]]; then
  git clone https://github.com/jtv/libpqxx.git
  REMOVE_BUILD="YES"
fi

cd libpqxx || exit 112

if [[ -d .git ]] ; then
  git checkout 8.0.1
  echo "#  "
  echo "#  START git status"
  echo "#  "
  git status
  echo "#  "
  echo "#  END git status"
  echo "#  "
  rm -rf .git
  REMOVE_BUILD="YES"
fi

cd "$HERE" || exit 111

if [[ "$REMOVE_BUILD" = "YES" ]]; then
  rm -rf build
fi

