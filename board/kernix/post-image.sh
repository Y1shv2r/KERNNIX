#!/bin/sh
set -eu
# Refresh editable boot files on every image build, even on incremental builds.
install -m 0644 "${BR2_EXTERNAL_KERNIX_PATH}/board/kernix/config.txt" "${BINARIES_DIR}/rpi-firmware/config.txt"
install -m 0644 "${BR2_EXTERNAL_KERNIX_PATH}/board/kernix/cmdline.txt" "${BINARIES_DIR}/rpi-firmware/cmdline.txt"
# Firmware loads overlays from the overlays directory on the FAT partition.
install -D -m 0644 "${BINARIES_DIR}/kernix-tlv320dac3100.dtbo" "${BINARIES_DIR}/rpi-firmware/overlays/kernix-tlv320dac3100.dtbo"
# Refresh the display overlay even when the kernel is already built.
"${HOST_DIR}/bin/dtc" -@ -I dts -O dtb -o "${BINARIES_DIR}/kernix-display.dtbo" "${BR2_EXTERNAL_KERNIX_PATH}/configs/device-tree/broadcom/kernix-display.dtso"
install -D -m 0644 "${BINARIES_DIR}/kernix-display.dtbo" "${BINARIES_DIR}/rpi-firmware/overlays/kernix-display.dtbo"
# Build a separate, initial music volume. Later syncs update only this volume.
MUSIC_STAGE="${BINARIES_DIR}/music-stage"
rm -rf "$MUSIC_STAGE"
"${KERNIX_BUILD_PYTHON:-/usr/bin/python3}" "${BR2_EXTERNAL_KERNIX_PATH}/tools/music_library.py" stage "$MUSIC_STAGE"
rm -f "${BINARIES_DIR}/music.ext4"
"${HOST_DIR}/sbin/mkfs.ext4" -q -F -b 1024 -O ^orphan_file -E root_owner="$(id -u):$(id -g)" -L KERNIX_MUSIC -d "$MUSIC_STAGE" "${BINARIES_DIR}/music.ext4" 262144
"${BR2_EXTERNAL_KERNIX_PATH}/buildroot/board/raspberrypi0/post-image.sh" "$@"
# Retain upstream boot-file generation and add the independently synced volume.
python3 - "$BINARIES_DIR/genimage.cfg" <<'PYCFG'
from pathlib import Path
import sys
p=Path(sys.argv[1]);text=p.read_text();index=text.rfind('}')
text=text[:index]+'\n partition music {\n  partition-type = 0x83\n  image = "music.ext4"\n }\n'+text[index:]
p.with_name('genimage-music.cfg').write_text(text)
PYCFG
ROOT_TMP=$(mktemp -d)
trap 'rm -rf "$ROOT_TMP"' EXIT
"${HOST_DIR}/bin/genimage" --rootpath "$ROOT_TMP" --tmppath "${BUILD_DIR}/genimage-music.tmp" \
 --inputpath "$BINARIES_DIR" --outputpath "$BINARIES_DIR" --config "$BINARIES_DIR/genimage-music.cfg"
