# Raspberry Pi Zero v1.3 board setup

Build with `./scripts/build-image.sh` from the project root. Buildroot is pinned
in `configs/buildroot-revision`; board customizations live in this external tree.
`config.txt` and `cmdline.txt` are the boot-file sources. The generated firmware
has boot, ext4 rootfs and KERNIX_MUSIC partitions; music is mounted at `/mnt/music`.

## TLV320DAC3100 headphone output

| Pi signal | Physical pin | Breakout signal |
|---|---|---|
| 3.3 V | 1 | Breakout power input (check board requirements) |
| GND | 6 | GND |
| GPIO2 / I²C SDA | 3 | SDA |
| GPIO3 / I²C SCL | 5 | SCL |
| GPIO18 / I²S BCLK | 12 | BCLK |
| GPIO19 / I²S frame clock | 35 | LRCLK / WSEL |
| GPIO21 / I²S output | 40 | DIN |
| GPIO5 | 29 | RESET |

The overlay uses address 0x18 and an active-low GPIO5 reset. If reset is not
wired, adapt the overlay to the actual hardware. Regulator nodes describe
external rails; they do not create physical voltages.

There is no separate MCLK. Local kernel patches add the opt-in `ti,pll-bclk`
property; the overlay enables it. The Pi supplies continuous I²S clocks with
unpadded stereo frames. Do not add fixed TDM slot widths without revisiting the
codec's BCLK calculation. Normal AUX playback uses ALSA plug conversion to
48 kHz, stereo S16_LE.

`kernix-audio aux` initializes both headphone channels with balanced hardware
volume and routes left/right DAC data to the corresponding headphone outputs.
Saved asymmetric mixer state is not restored automatically. UI volume is
software controlled. `dac` remains an alias for `aux`.

## Display and touch

ST7789 uses SPI0 CE0, DC GPIO25 and reset GPIO24. XPT2046 uses CE1 and IRQ GPIO22.
Shared SPI pins are GPIO9/10/11. The tested display is landscape 320×240,
rotate=90, SPI 27 MHz. Backlight power is a hardware connection; no software
backlight control pin is configured. Touch transforms live in
`rootfs-overlay/etc/default/kernix-ui`.

## USB Bluetooth

Use a USB OTG host adapter. Pi Zero v1.3 has no onboard Bluetooth and needs no
UART Bluetooth attachment daemon. The tested TP-Link adapter uses RTL8761BU;
Realtek firmware is included. Other adapters may require different firmware.

BlueALSA runs as an A2DP source, sending Pi audio to headphones or a speaker.
The hardware-tested low SBC quality setting avoids the previously observed
periodic pauses. Output selection retains the negotiated sample rate/channel
count in its ALSA plug configuration to avoid transport reconfiguration errors.

Configure `/etc/default/kernix-audio` with `BT_DEVICE=AA:BB:CC:DD:EE:FF` (the
headset address). A private `.local/kernix-audio` file overrides it at build time.
Bluetooth services start on demand. There are no automatic audio service boot
hooks; the UI starts at boot, paused. TFT pairing supports configured devices
using NoInputNoOutput pairing; use bluetoothctl for PIN/passkey devices.

For terminal pairing:

```sh
kernix-audio-service bluetooth start
bluetoothctl
# Inside bluetoothctl, wait for each result:
# power on
# agent on
# default-agent
# scan on
# pair AA:BB:CC:DD:EE:FF
# trust AA:BB:CC:DD:EE:FF
# scan off
# quit
kernix-audio bluetooth AA:BB:CC:DD:EE:FF
```

## Playback and diagnosis

```sh
kernix-audio aux
speaker-test -D default -c 2 -r 48000 -F S16_LE -t sine -f 440 -l 1
# Stop playback before changing terminal output:
kernix-audio bluetooth
kernix-audio status
bluealsa-cli list-pcms
aplay -l
kernix-bluetooth-diagnose
```

Terminal route changes affect new streams; the UI stops/reopens its decoder
itself. Output state persists in `/var/lib/kernix-audio/asound.conf`. A failed
Bluetooth connection leaves the selected output unchanged; there is no automatic
fallback to AUX. Diagnostics may contain real device addresses; redact them
before sharing logs. Relevant logs are `/tmp/bluealsa.log`,
`/tmp/bluetoothd.log`, `/tmp/kernix-audio.log` and `/tmp/kernix-calibrate.log`.
