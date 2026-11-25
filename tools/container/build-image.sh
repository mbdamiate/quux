#!/bin/sh
# Builds the Linux arm64 development image (D-30).
set -eu
repository_root=$(cd "$(dirname "$0")/../.." && pwd)
exec docker build --tag quux-development:latest "$repository_root/tools/container"
