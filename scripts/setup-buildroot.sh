#!/bin/sh
# Fetch exactly the Buildroot revision recorded by Kernix. Never reset local work.
set -eu
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REVISION=$(cat "$PROJECT_DIR/configs/buildroot-revision")
if [ ! -e "$PROJECT_DIR/buildroot" ]; then
 git clone https://github.com/buildroot/buildroot.git "$PROJECT_DIR/buildroot"
 git -C "$PROJECT_DIR/buildroot" checkout --detach "$REVISION"
fi
ACTUAL=$(git -C "$PROJECT_DIR/buildroot" rev-parse HEAD)
if [ "$ACTUAL" != "$REVISION" ]; then
 echo "Buildroot revision mismatch: expected $REVISION, found $ACTUAL" >&2
 echo 'Preserve your local changes and select the pinned revision before building.' >&2
 exit 1
fi
