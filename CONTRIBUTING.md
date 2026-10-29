# Contributing

Read the top-level README, then run the host test suite before changing code.
Use disposable music fixtures; do not commit personal catalogs, MP3s, SD images,
Bluetooth addresses, generated builds or local configuration.

Keep UI drawing and touch geometry separate from blocking playback/Bluetooth
work. Audio changes must preserve the codec's stereo BCLK setup and the output
selector's failure handling. Include relevant tests with behavior changes.
Describe which hardware you tested, and distinguish host tests from physical
playback/display validation.

Buildroot is a pinned local dependency. Put board customizations in this external
tree, not inside the downloaded Buildroot checkout. Preserve third-party license
notices and document any additional dependencies.

The project license is awaiting the maintainer's choice. Do not treat a review
source archive as a licensed public release until that choice is recorded.
