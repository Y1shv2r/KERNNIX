#!/bin/bash
# Erase and verify a removable whole-disk SD card. Requires an explicit device.
set -euo pipefail
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
[[ $# == 1 || $# == 2 ]] || { echo "Usage: sudo $0 /dev/sdX [image.img]" >&2;exit 2; }
CARD_DEVICE=$(readlink -f -- "$1")
IMAGE_FILE=$(readlink -f -- "${2:-$PROJECT_DIR/output/images/sdcard.img}")
[[ -b "$CARD_DEVICE" ]] || { echo "Not a block device: $CARD_DEVICE; nothing written" >&2;exit 1; }
[[ $(lsblk -dnro TYPE "$CARD_DEVICE") == disk && $(lsblk -dnro RM "$CARD_DEVICE") == 1 ]] || {
 echo "Refusing a non-removable device or partition: $CARD_DEVICE" >&2;exit 1;
}
[[ $EUID == 0 ]] || { echo "Run this script with sudo" >&2;exit 1; }
[[ -f "$IMAGE_FILE" ]] || { echo "Missing $IMAGE_FILE" >&2;exit 1; }
# Verify saved image integrity before unmounting or writing any drive.
if [[ -f "$(dirname "$IMAGE_FILE")/SHA256SUMS" ]]; then
 (cd "$(dirname "$IMAGE_FILE")" && sha256sum --check SHA256SUMS)
fi
HAS_MUSIC=$(python3 - "$IMAGE_FILE" <<'PYIMAGE'
import sys,struct
with open(sys.argv[1],'rb') as f:
 m=f.read(512)
 print(int(len(m)==512 and m[482]==0x83 and struct.unpack_from('<I',m,490)[0]>0))
PYIMAGE
)
if [[ "$HAS_MUSIC" == 1 ]]; then
 for tool in sfdisk resize2fs e2fsck tune2fs blkid; do command -v "$tool" >/dev/null || { echo "Missing $tool" >&2;exit 1; }; done
fi
IMAGE_BYTES=$(stat -c %s "$IMAGE_FILE")
[[ $(blockdev --getsize64 "$CARD_DEVICE") -ge $IMAGE_BYTES ]] || { echo 'Card is too small' >&2;exit 1; }
mapfile -t CARD_NODES < <(lsblk -nrpo NAME "$CARD_DEVICE")
# Refuse system disks even if a device incorrectly advertises removable media.
for node in "${CARD_NODES[@]}"; do
 while IFS= read -r mount_path; do
  case "$mount_path" in /|/home|/boot|/boot/efi|/usr|/var) echo "Refusing system filesystem $mount_path" >&2;exit 1;; esac
 done < <(findmnt -rn -S "$node" -o TARGET || true)
done
lsblk -p -o NAME,SIZE,MODEL,MOUNTPOINTS "$CARD_DEVICE"
for node in "${CARD_NODES[@]}"; do
 if findmnt -rn -S "$node" >/dev/null; then umount -- "$node";fi
done
# nocreat prevents a missing /dev node from becoming a regular image file.
dd if="$IMAGE_FILE" of="$CARD_DEVICE" bs=4M status=progress conv=fsync,nocreat
blockdev --flushbufs "$CARD_DEVICE"
cmp -n "$IMAGE_BYTES" "$IMAGE_FILE" "$CARD_DEVICE"
sync
echo 'Base image flash and byte-for-byte verification passed.'
if [[ "$HAS_MUSIC" == 1 ]]; then
 case "$CARD_DEVICE" in *[0-9]) MUSIC_PART="${CARD_DEVICE}p3" ;; *) MUSIC_PART="${CARD_DEVICE}3" ;; esac
 blockdev --rereadpt "$CARD_DEVICE"
 if command -v udevadm >/dev/null; then udevadm settle; fi
 [[ $(blkid -s LABEL -o value "$MUSIC_PART") == KERNIX_MUSIC ]] || { echo 'Unexpected third partition; refusing to resize' >&2;exit 1; }
 if findmnt -rn -S "$MUSIC_PART" >/dev/null; then umount "$MUSIC_PART"; fi
 printf ', +\n' | sfdisk --no-reread -N 3 "$CARD_DEVICE"
 blockdev --rereadpt "$CARD_DEVICE"
 if command -v udevadm >/dev/null; then udevadm settle; fi
 if findmnt -rn -S "$MUSIC_PART" >/dev/null; then umount "$MUSIC_PART"; fi
 check_status=0
 e2fsck -pf "$MUSIC_PART" || check_status=$?
 [[ "$check_status" -le 1 ]] || exit "$check_status"
 # Older host resize2fs (<1.47.2) can corrupt moved orphan-file checksums.
 # This optional performance feature is unnecessary for a music data volume.
 tune2fs -O ^orphan_file "$MUSIC_PART"
 resize2fs "$MUSIC_PART"
 sync
 blockdev --flushbufs "$CARD_DEVICE"
 check_status=0
 e2fsck -pf "$MUSIC_PART" || check_status=$?
 [[ "$check_status" -le 1 ]] || exit "$check_status"
 e2fsck -fn "$MUSIC_PART"
 sync
 echo 'Music partition expanded to remaining card capacity.'
fi
eject "$CARD_DEVICE"
