#!/bin/bash
# Every command in the image starts here: source the KallistiOS environment,
# then run what was asked. The mounted repo's copy wins so an edit to
# scripts/dc_env.sh takes effect without rebuilding the image.
set -e

if [ -f /work/scripts/dc_env.sh ]; then
    . /work/scripts/dc_env.sh
else
    . /opt/ssb64-dc/scripts/dc_env.sh
fi

exec "$@"
