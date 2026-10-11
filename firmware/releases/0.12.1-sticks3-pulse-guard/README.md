# LaunchLab Mini 0.12.1

October 11, 2026.
Runtime: `0.12.1-sticks3-flip-colors` for M5StickS3 K150, ESP32-S3 with 8 MB flash and 8 MB OPI PSRAM.
One application supports the saved QRE1113 and TCRT5000 profiles.

## What changed

The single-turn peak estimator now discounts an isolated, unconfirmed extra optical mark when its following intervals return to the preceding revolution cadence.
The reproduced generated notch now reports the expected optical fundamental instead of a false high peak.
Confirmed faster motion and valid final-turn peaks remain supported.
Optical acquisition thresholds, three-turn launch confirmation, boundaries and persistent formats retain their existing behavior.

Ten saved accent themes are available: Pegasus Cyan, Wolf Mint, Wizard Gold, Meteor Purple, Golem Green, Phoenix Red, Cobalt Blue, Shark Pink, Valkyrie White and Tyranno Orange.
The original four persistent theme IDs and RGB565 colors remain compatible.
Flip Off uses mounted rotation 2, and Flip On uses native rotation 0.
The saved switch value is retained, so an existing screen rotates 180 degrees after updating; change Flip if your mount needs the other orientation.
The main screen removes the product heading, raises the RPM readout, enlarges the live level and retains the original small status text.

Personal-best celebrations offer Orbit, Flames, Sparkles, Shockwave and Lightning, plus Random and Off.
Random avoids repeating the last celebration actually shown.
The Best effect editor previews a draft once; B cycles, A applies and hold B cancels.
Optional Sound Off, Low or High plays a matching clip with the first displayed effect frame; a new pull or closing the editor cancels queued or playing audio.
Sound defaults to Off.
The internal display canvas and NVS orphan cleanup are retained.
Optical sampling remains 50 kS/s at an 80 MHz CPU setting.

## Exact application and source

Application: 808064 bytes, SHA-256 `a13d590f11c3c8e8f8ab84a0b8ebd8d69761cf1046e2a0f9fd1d9c162d01ad41`, address `0x10000`.
Compiled source: 49 inputs, source-set SHA-256 `5015e71d8eedd68ed68747539e5624e86267d857c115f79f3aede84bda08f9f1`.
The publication ships the exact owner-tested bytes without rebuilding.
The support images match the previously published installation layout.
The source ZIP includes licenses, pinned build context and synthetic tests, including both historical paired-estimator baselines.
Private practice recordings, backups, serial logs, readbacks and ELF debug output are excluded.

## Validation and limits

The public source passed 28 ASan/UBSan native suites and nine production harnesses.
The recorded-data runner passed 21 contract tests.
The private corpus contains 12 recordings across 28 configurations: 18 confirmed configurations passed twice, while ten remain diagnostic.
Recorded input hashes, three-turn control rows and final RPM values remained unchanged.
Independent optical review passed 48 fixture pairs with 42 independent selected-peak assertions.
Target compilation and stack checks passed, with an 800-byte loop frame within the 1536-byte loop and 2560-byte other-frame budgets.

The exact application passed app-only installation, complete readback and complete backup retention.
The owner reported solid behavior, including pulls while a preceding reading was showing.
Five results were saved, and the device returned Ready with active acquisition, no unsaved pulls and zero acquisition/save errors.
Prior palette, Flip, layout and effects checks are carried with their original variant identity because those UI source inputs are unchanged.

Identical genuine transient motion and a split optical mark cannot always be distinguished.
Terminal split marks without a recovery interval and a first interval already corroborated by three-turn confirmation retain their documented limitations.
The separate recorded high three-turn result and older missed-pull root cause remain unresolved.
Attempted physical pull count, absolute RPM/tilt calibration, first install, physical browser write, physical personal-best/Random sequencing and endurance are outside qualification.

## Installation

Use Update existing LaunchLab for an app-only upgrade through the [USB updater](https://shadohead.github.io/launchlab-mini/) or [Beywatch updater](https://launchlab.beywatch.gg/update).
The updater verifies the complete installed application, refuses accidental downgrades and skips an identical image.
A different private build carrying the same 0.12.1 version requires deliberate Repair / rollback after backup.
The first-install ZIP uses the established four-image map: bootloader `0x0`, partitions `0x8000`, boot support `0xe000`, application `0x10000`.
All earlier published image bytes remain available.
