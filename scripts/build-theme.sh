#!/bin/sh
# Build a visual variant and archive its image independently.
set -eu
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "${1:-}" in moonlit|ukiyo|eclipse|crimson) THEME_NAME=$1 ;; *) echo "Usage: $0 {moonlit|ukiyo|eclipse|crimson}" >&2; exit 2 ;; esac
printf '/* Selected visual theme. */\n#include "themes/%s.h"\n' "$THEME_NAME" > "$PROJECT_DIR/ui/theme.h"
"$PROJECT_DIR/scripts/build-image.sh" kernix-calibrate-rebuild
"$PROJECT_DIR/scripts/build-image.sh"
python3 "$PROJECT_DIR/scripts/theme-images.py" save "$THEME_NAME" \
 --note "Built from selectable $THEME_NAME theme source. Hardware display validation pending."
