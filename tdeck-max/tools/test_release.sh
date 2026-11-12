#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Broad native audit suite. Real radio/USB/Wi-Fi tests are deliberately separate.
python3 tools/check_line_comments.py src
bash tools/test_ui.sh
mkdir -p artifacts/release-tests
for test in services companion_client sd_replace keyboard_layout audio_pcm ringtones vibrations dictation_policy pending_keys lock repeater_observer time_sync sky home_data repeat_policy remote_cli motion_policy rx_log_view noise_stats rpt_rank track_log modem_at mqtt_uplink mqtt_downlink modem_engine net_route sms_inbox cellular_page sms_page selftest; do
  g++ -std=c++17 -O2 -Isrc "sim/test_${test}.cpp" -o "artifacts/release-tests/${test}"
  "artifacts/release-tests/${test}"
done
bash tools/test_companion_emulator.sh
for test in chat_clear heard_journal channel_remap journal_tail; do
  g++ -std=c++17 -O2 -Isim/journal-stubs -Isrc "sim/test_${test}.cpp" src/mesh/journal.cpp -o "artifacts/release-tests/${test}"
  "artifacts/release-tests/${test}"
done
# Compile shared UI implementation once for the additional page/regression cases.
ui_flags=(-std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library')
g++ "${ui_flags[@]}" -c src/ui/components.cpp -o artifacts/release-tests/components.o
g++ "${ui_flags[@]}" -c src/ui/artwork.cpp -o artifacts/release-tests/artwork.o
g++ "${ui_flags[@]}" -c src/ui/renderer.cpp -o artifacts/release-tests/renderer.o
g++ "${ui_flags[@]}" -c 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' -o artifacts/release-tests/gfx.o
gcc -O2 -c src/qr/qrcodegen.c -o artifacts/release-tests/qrcodegen.o
for test in degrees lock_wallpaper hold_banner todo_text lora_connectivity gps_settings_ui menu_status_lock chat_scopes_unlock channel_gps motion_settings_ui rx_log_ui noise_ui rpt_ui tracks_ui cellular_ui sms_ui selftest_ui; do
  g++ "${ui_flags[@]}" "sim/test_${test}.cpp" artifacts/release-tests/components.o artifacts/release-tests/artwork.o artifacts/release-tests/renderer.o artifacts/release-tests/gfx.o artifacts/release-tests/qrcodegen.o -o "artifacts/release-tests/${test}"
  "artifacts/release-tests/${test}"
done
# Dictation language: pure policy (ids, SD paths, labels, German UTF-8) and the Sound / dictation page layout.
g++ -std=c++17 -O2 -Isrc sim/test_dictation_language.cpp -o artifacts/release-tests/dictation_language
artifacts/release-tests/dictation_language
g++ "${ui_flags[@]}" sim/test_dictation_settings_ui.cpp artifacts/release-tests/components.o artifacts/release-tests/artwork.o artifacts/release-tests/renderer.o artifacts/release-tests/gfx.o artifacts/release-tests/qrcodegen.o -o artifacts/release-tests/dictation_settings_ui
artifacts/release-tests/dictation_settings_ui
# Dictation engine actually shipped (asr2, MMQIT002) on every model present: the deployed packed 6-bit Italian model and the
# exported English / German ones (tools/offline_asr), the latter also on real speech clips (tools/offline_asr/fetch_samples.py).
model="${MM_QN15_Q6:-artifacts/offline-it/qn15/export/qn15-it-q6.mmq}"
models=("$model" artifacts/offline-asr/asr-en.mmq artifacts/offline-asr/asr-de.mmq)
built=0
shopt -s nullglob
for model in "${models[@]}"; do
  if [ -f "$model" ]; then
    if [ "$built" = 0 ]; then
      g++ -std=c++14 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -pthread -Isrc \
        src/offline_it/asr2.cpp sim/test_asr2.cpp -o artifacts/release-tests/asr2-sanitized
      built=1
    fi
    lang="$(basename "$model" .mmq)"; clips=(artifacts/offline-asr/samples/"${lang#asr-}"-*.wav)
    ASAN_OPTIONS=detect_leaks=1 artifacts/release-tests/asr2-sanitized "$model" "${clips[@]}"
  else
    echo "SKIP asr2: no model at $model"
  fi
done
# Exporter numerics (bit packing, quantization, BatchNorm folding); needs the project venv of tools/offline_asr.
if [ -x tools/offline_asr/.venv/bin/python ]; then (cd tools/offline_asr && .venv/bin/python test_export.py); else echo "SKIP export_nemo self-check: no tools/offline_asr/.venv"; fi
