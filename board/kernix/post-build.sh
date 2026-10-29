#!/bin/sh
set -eu
# Remove retired sample audio from incremental target trees.
for retired in "$1/usr/share/kernix/music" "$1/usr/share/kernix/test-audio"; do
 [ ! -d "$retired" ] || find "$retired" -maxdepth 1 -type f \( -iname '*.mp3' -o -iname '*.wav' \) -delete
done
# Local headset settings are deliberately excluded from source distributions.
LOCAL_AUDIO="${BR2_EXTERNAL_KERNIX_PATH}/.local/kernix-audio"
if [ -f "$LOCAL_AUDIO" ]; then
 install -m 0644 "$LOCAL_AUDIO" "$1/etc/default/kernix-audio"
fi

# Keep audio services available for terminal use, without boot/shutdown hooks.
rm -f "$1/etc/init.d/S39alsa" "$1/etc/init.d/S40bluetoothd" "$1/etc/init.d/S45bluealsa"

