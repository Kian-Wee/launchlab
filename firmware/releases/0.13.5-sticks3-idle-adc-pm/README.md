# Firmware 0.13.5

Runtime: `0.13.5-sticks3-idle-adc-pm` · M5StickS3 K150 · ESP32-S3.

- Filtered movement keeps a shake-woken device awake during handling; 30 seconds without renewed activity returns an unused wake to sleep.
- RESTING pauses ADC sampling after stillness and resumes on movement or a button. The shared sensor supply stays powered.
- CPU power management uses 40–80 MHz with an 80 MHz acquisition lock and automatic light sleep disabled.
- Battery readings are filtered, idle display work is reduced, and PM1 reads use checked wake/retry handling.
- Optical startup/rearm recovery handles weak background signals and bounded slowdown tails.
- Saved Top/Sidecar mounting and a guarded Wipe all flow are included.

Both saved sensor profiles, the 1,000 RPM launch floor, history, themes, effects, optional sound and USB backup/restore remain supported. Ordinary updates write only the application at `0x10000`; first install and intentional rollback retain their separate confirmation flows.

The shipping application is the exact installed and read-back image. Source is frozen in `source/` and the compiled-source ZIP. See [build context](../../../docs/FIRMWARE_0.13.5.md).
