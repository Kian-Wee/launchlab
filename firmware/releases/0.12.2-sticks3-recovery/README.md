# LaunchLab Mini 0.12.2

October 11, 2026.
Runtime: `0.12.2-sticks3-flip-colors` for M5StickS3 K150, ESP32-S3 with 8 MB flash and 8 MB OPI PSRAM.
Release identity: `0.12.2-sticks3-recovery`.
One application supports the saved QRE1113 and TCRT5000 profiles.

## Changes

The learned optical swing-ring index now resets with its count after detector discontinuity and history-checkpoint recovery.
Generated regressions demonstrate that a stale ring position could miss an initial short burst or confirm subsidiary marks at a falsely higher rate.
Single Turn startup now retains a completed, qualified predecessor peak while a uniquely fastest latest arrival is still being checked.
This prevents a qualifying detected launch from producing a zero Single Turn value when that latest arrival is rejected.
The latest arrival still passes the existing delayed optical check, and a valid faster interval can supersede the predecessor.

The isolated extra-mark guard, genuine acceleration coverage, saved profile thresholds, paired launch boundaries, 50 kS/s acquisition and history/settings formats are retained.
Ten themes, five personal-best effects, Random/Off, optional sound, neutral branding and the expanded main layout are unchanged.
Flip Off uses mounted rotation 2, and Flip On uses native rotation 0.
Saved switch values are preserved; updating from a pre-0.12.1 application rotates existing choices 180 degrees, so change Flip if your mount needs the other orientation.

The owner identified a soldering issue as the cause of the intermittent high and missed readings.
The two software corrections are supported independently by generated optical regressions and are not presented as proof of that physical diagnosis.
The withdrawn 0.12.1 release remains preserved with its original source and assets; it is not offered in the updater's version list.

## Application and source

Application: 808,144 bytes at `0x10000`.
SHA-256: `9548c322ae1f7383641e62ec5b81f7a2f54777c05034f8c5aac47a40b4f25b49`.
Compiled source: 49 inputs, source-set SHA-256 `4cba14781316dbff76c3314ff3b26420a99a3c7c39be6e70c99f4a3266be61e6`.
The production inputs differ from the tested production predecessor only in the runtime version string.
The release ships the exact compiled bytes, without adding the private diagnostic patch.
Support images match the previous public installation layout.
The source ZIP includes licenses, pinned build context and synthetic tests, including historical paired-estimator baselines.
Private practice recordings, backups, serial logs, readbacks and ELF debug output are excluded.

## Validation and limits

The public source passed 31 ASan/UBSan native suites and nine production harnesses.
Pinned M5 compilation and stack checks passed, with an 800-byte loop frame within the 1536-byte loop and 2560-byte other-frame budgets.
The same-source private recorded-data checks retain 18 confirmed configurations and 12 diagnostic-only configurations.
Final recorded results and launch boundaries remain unchanged; startup alternative peaks and update counters intentionally differ where a qualified predecessor is retained.
These recordings are not a calibrated shaft-speed oracle and are not distributed.

Both software corrections were also exercised in the related installed diagnostic build.
Complete application readback and portable-backup retention passed, and observed registrations matched metric, history and UI delivery.
Device install pending: the exact production 0.12.2 application has not been flashed or read back.
Physical first install, browser write, personal-best/Random sequencing, calibrated RPM and endurance remain unqualified.
Optically indistinguishable genuine transients and split marks, terminal subdivisions without a return interval, and already corroborated intervals retain the pulse guard's bounded limitations.
Earlier diagnostic waveform cases remain unqualified.

## Installation

Use Update existing LaunchLab for an app-only upgrade through the [USB updater](https://shadohead.github.io/launchlab-mini/) or [Beywatch updater](https://launchlab.beywatch.gg/update).
The updater checks the complete installed application, refuses accidental downgrades and skips an identical image.
Known earlier private builds are recognized by their exact application identity; their binaries are not distributed.
An intentional same-version repair or rollback still requires backup and deliberate Repair / rollback.
The first-install ZIP uses the established four-image map: bootloader `0x0`, partitions `0x8000`, boot support `0xe000`, application `0x10000`.
All earlier published image bytes remain available.
