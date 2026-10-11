#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
TEST_BUILD=$(mktemp -d)
trap 'rm -rf "$TEST_BUILD"' EXIT
for suite in appearance nvs_budget best_effect_sound device_backup sticks3 sticks3_level sticks3_power launch_feedback launch_motion motion_replay motion_store practice_history practice_store orientation_estimator analog_tachometer paired_rpm_estimators rpm_estimator_setting sensor_profile_setting display_flip wake_ui_state controls controls_ui power_diagnostics weak_slowdown store_presence practice_views history_idle_checkpoint usb_awake; do
  "${CXX:-c++}" -std=c++17 -O1 -g -DVQF_SINGLE_PRECISION -fsanitize=address,undefined -fno-omit-frame-pointer -Ifirmware/tests/fakes -Ifirmware/LaunchLabRpm -Ifirmware/LaunchLabMini "firmware/tests/test_${suite}.cpp" firmware/LaunchLabMini/src/vqf/vqf.cpp -o "$TEST_BUILD/$suite"
  "$TEST_BUILD/$suite"
done
python3 firmware/tests/check_motion_playback.py
python3 firmware/tests/check_motion_pipeline.py
python3 firmware/tests/check_power_log_retry.py
python3 firmware/tests/check_history_retry.py
python3 firmware/tests/check_usb_pipeline.py
python3 firmware/tests/check_imu_startup.py
python3 firmware/tests/check_controls_pipeline.py
python3 firmware/tests/check_best_sound_pipeline.py
python3 firmware/tests/check_main_layout.py
