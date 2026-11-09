#!/usr/bin/env bash
# Emulator test of the companion v14 dispatch (src/mesh/companion.inc, service.cpp callbacks) over the virtual Wi-Fi,
# then the factory reset on a scratch SD/NVS: user data is moved to /mm-previous-1 and the NVS is erased.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);offset=$((2500+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $1";grep -v '^\[I\]' "$d/log.txt"|tail -8;exit 1; }
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
printf 'N "zzmark"\nV "zzmark" "k" 4 2a000000\n' >> "$d/nvs.json"
cat > "$d/script.emu" <<'S'
3000 serial wifi on
3500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}
7200 serial app wifi
70000 quit
S
timeout 100 artifacts/emu-max --headless --data "$d" --air off --port-offset $offset --script "$d/script.emu" < /dev/null > "$d/log.txt" 2>&1 &
for i in $(seq 1 40);do grep -q "EMU_WIFI listening" "$d/log.txt" 2>/dev/null&&break;sleep 0.5;done
python3 emu/tools/companion_v14_client.py 127.0.0.1 $((5000+offset)) dispatch||fail "dispatch"
grep -q zzmark "$d/nvs.json"||fail "the NVS marker is missing before the reset"
cmp -s emu/profiles/configured-max/sd/mattermesh/radio.conf "$d/sd/mattermesh/radio.conf"||fail "radio.conf differs before the reset"
python3 emu/tools/companion_v14_client.py 127.0.0.1 $((5000+offset)) reset
for i in $(seq 1 40);do [ -d "$d/sd/mm-previous-1" ]&&break;sleep 0.5;done
[ -f "$d/sd/mm-previous-1/mattermesh/radio.conf" ]||fail "user data was not moved to /mm-previous-1"
cmp -s emu/profiles/configured-max/sd/mattermesh/radio.conf "$d/sd/mm-previous-1/mattermesh/radio.conf"||fail "archived radio.conf changed"
for i in $(seq 1 20);do ! grep -q zzmark "$d/nvs.json"&&break;sleep 0.5;done
! grep -q zzmark "$d/nvs.json"||fail "the NVS was not erased"
echo "PASS: companion v14 over the emulator - dispatch, factory reset archives the SD data and erases the NVS"
