#!/usr/bin/env bash

cd "$(dirname "$0")" || exit 113

set -a
source .env
set +a

docker compose down

docker run --rm --user root -v "$APP_DATA_ROOT:/target" alpine sh -c "rm -rf /target/* /target/.* 2>/dev/null || true"
