#!/bin/sh
set -eu
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$PROJECT_DIR/scripts/setup-buildroot.sh"
make -C "$PROJECT_DIR/buildroot" O="$PROJECT_DIR/output" BR2_EXTERNAL="$PROJECT_DIR" kernix_defconfig
make -C "$PROJECT_DIR/output" "$@"
