# Kernix

Kernix is a touchscreen music player for the Raspberry Pi Zero v1.3, built
with Buildroot. It plays a local MP3 library through a TLV320DAC3100 headphone
output (AUX) or Bluetooth headphones using a USB Bluetooth adapter.

The TFT provides playlists, playback controls, shuffle, repeat, volume,
output switching and pairing for a configured Bluetooth device. A 30-second
inactivity screen shows the song title and playback progress; one touch returns
to the previous page. The PC music manager imports MP3s, edits metadata, manages
playlists and syncs the SD music partition without rebuilding firmware.

## Hardware

- Raspberry Pi Zero v1.3; Bluetooth requires a USB adapter and OTG connection.
- Tested Bluetooth adapter: TP-Link using Realtek RTL8761BU firmware.
- TLV320DAC3100 breakout, using I²C and I²S with BCLK as its PLL reference.
- ST7789 TFT, 320×240 landscape, with XPT2046 touch on SPI.

See [board wiring and audio setup](board/kernix/README.md). Other Pi models,
codec boards and displays are not validated by this configuration.

## Build on Linux

Install the usual Buildroot host dependencies. On Debian/Ubuntu the starting
set is:

```sh
sudo apt install build-essential git bash bc binutils bzip2 cpio file g++ gcc \
  gzip make patch perl rsync sed tar unzip wget xz-utils libncurses-dev \
  python3 python3-tk python3-dbus python3-gi libsqlite3-dev libdbus-1-dev \
  pkg-config dbus e2fsprogs fdisk util-linux
./scripts/setup-buildroot.sh
./scripts/build-image.sh
```

Run builds as a regular user. The setup script downloads the exact Buildroot
commit in `configs/buildroot-revision`; it refuses a different existing revision
without resetting local changes. The Linux revision is pinned in the defconfig.
Downloads and build products stay local under `buildroot/` and `output/`.
A fresh source checkout builds an empty music library until you import songs.
The defconfig is reapplied on each build; save deliberate configuration changes
back to `configs/kernix_defconfig` first.

The image is `output/images/sdcard.img`. It contains boot, rootfs and
KERNIX_MUSIC partitions. Check the SD device before flashing:

```sh
lsblk -p -o NAME,SIZE,MODEL,TYPE,MOUNTPOINTS
sudo ./scripts/flash-sd.sh /dev/sdX
```

Flashing erases the whole selected card, including its music and pairing data.
The script verifies the write and expands the music filesystem. Routine music
updates use the PC music manager instead: run `./scripts/music-manager.sh`,
import files, assign playlists, then use Sync to SD with the mounted
KERNIX_MUSIC partition. Safely eject before returning the card to the Pi.

## Configure Bluetooth

Set the remote headset address in `/etc/default/kernix-audio` on the Pi:

```sh
BT_DEVICE=AA:BB:CC:DD:EE:FF
```

For a private build-time override, create `.local/kernix-audio` containing that
setting. This file is ignored and excluded from source exports. Put the headset
in pairing mode and use PAIR BLUETOOTH on the TFT Controls page. Devices requiring
a PIN/passkey need terminal pairing. Select the playback output separately.

The development image currently uses root password `1234`, as recorded in the
defconfig. Change it for deployment. Runtime behavior and account policy have
not been changed by the repository cleanup.

## Development

```sh
python3 -m unittest discover -s tests -v
./scripts/build-image.sh kernix-calibrate-rebuild
./scripts/build-image.sh
```

Tests compile host C code and use disposable databases and mocked audio/Bluetooth
services. They require a session that permits local D-Bus sockets. They do not
replace hardware tests. AUX and Bluetooth playback have been confirmed on the
project hardware; the newest inactivity screen still needs physical acceptance.

Theme variants: `crimson`, `moonlit`, `ukiyo`, `eclipse`. Use
`./scripts/build-theme.sh THEME` to build and archive one. See [UI notes](ui/README.md).

## Source layout

| Path | Purpose |
|---|---|
| `ui/` | Native C renderer, touch handling, playback worker and library loader |
| `tools/` | PC music manager and sync tool |
| `board/kernix/` | Boot settings, rootfs helpers and codec kernel patches |
| `configs/` | Buildroot defconfig, kernel fragment, overlays and dependency pin |
| `package/` | Buildroot package recipe |
| `scripts/` | Build, flash, asset generation and source export tools |
| `tests/` | Host integration tests and mocks |
| `library/`, `music/`, `.local/` | Personal data; excluded from public exports |

## Preparing a release

`python3 scripts/export-source.py` creates `dist/kernix-source.tar.gz` and a SHA-256
checksum. It exports maintained source,
excluding personal music/catalogs, headset settings, caches, SD images and the
Buildroot checkout. Nothing is uploaded or published by this command.

**License selection for Kernix's own code is pending.** This source bundle is
for review until a license has been selected and added. Dependencies retain
their own terms; see [third-party notices](THIRD_PARTY_NOTICES.md).
