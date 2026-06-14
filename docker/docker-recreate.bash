#!/usr/bin/env bash

cd "$(dirname "$0")" || exit 113

bash remove.bash

bash docker-restart.bash
