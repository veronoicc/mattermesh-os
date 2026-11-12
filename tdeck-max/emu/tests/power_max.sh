#!/usr/bin/env bash
# Power pages on the Max: Power / timeouts (sleep switch, delay, USB, deep sleep confirm), Battery (graph, usage tab), Power profile
# (apply through the real setters, Custom after a change, persisted in NVS), the sleep / battery / profile USB commands, deep sleep.
# The emulator cannot light-sleep (a PC): the policy runs and reports why, the hardware nap is for the device.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d)
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $*";grep -a -E 'SLEEP|PROFILE|BATTERY|DEEP|Guru|abort' "$d"/log.txt|cut -c1-200|tail -20;exit 1; }
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
# Three history records (services/battery_math.h BatterySample, 24 bytes little endian) left by an earlier session.
python3 - "$d/sd/mattermesh/battery.bin" <<'P'
import struct,sys
open(sys.argv[1],"wb").write(b"".join(struct.pack("<IBBHhHHHHHHH",1800000000+k*300,90-k,0,4000-k*20,-40,300,60,0,0,300,120,10) for k in range(3)))
P
# Every `shot` follows its input by two seconds: the panel refresh is simulated and a shot is the last finished frame.
cat > "$d/script.emu" <<'S'
8000 serial section 20
10000 shot
10500 serial key j
11000 serial key j
11500 serial key ENTER
13500 serial sleep status
14000 shot
14500 serial key j
15000 serial key ENTER
15500 serial key j
16000 serial key ENTER
16500 serial sleep status
17000 serial key j
17500 serial key ENTER
18000 serial deep sleep
20000 shot
20500 serial key ENTER
22500 shot
23000 serial section 5
25000 shot
25500 serial key l
27500 shot
28000 serial battery status
28500 serial section 57
30500 shot
31000 serial profile apply field
33000 shot
33500 serial profile
34000 serial power display 30
34500 serial profile
35000 serial profile apply max
35500 serial profile
36000 serial profile apply normal
36500 serial profile
37000 serial section 19
39000 shot
39500 serial status
40000 serial deep sleep now
42000 quit
S
timeout 80 artifacts/emu-max --headless --data "$d" --air off --script "$d/script.emu" < /dev/null > "$d/log.txt" 2>&1||true
grep -aq 'SLEEP_STATUS enabled=0 delay=60 usb_awake=1 ' "$d/log.txt"||fail "Enter on Sleep when idle did not switch it off"
grep -aq 'SLEEP_STATUS enabled=0 delay=300 usb_awake=0 ' "$d/log.txt"||fail "delay step / Stay awake on USB switch"
grep -aq 'env=USB serial' "$d/log.txt"||fail "the policy does not name the USB serial link as a blocker"
grep -aq 'DEEP_SLEEP_ASK confirm=1' "$d/log.txt"||fail "Deep sleep now did not ask first"
grep -aq 'BATTERY_STATUS percent=.* samples=3 stored=1 ' "$d/log.txt"||fail "the 24 h history on the SD card was not loaded"
grep -aq 'PROFILE_APPLY ok=1 name=Field' "$d/log.txt"||fail "applying Field"
grep -aq 'PROFILE name=Field saved=Field gps=0 modem=0 motion=0 raise=0 wifi=0 ble=0 display=15 keyboard=15 sleep=1 delay=10 usb=0' "$d/log.txt"||fail "Field does not set what it lists"
grep -aq 'PROFILE name=Custom saved=Field ' "$d/log.txt"||fail "a changed setting does not mark the profile Custom"
grep -aq 'PROFILE_APPLY ok=1 name=Max' "$d/log.txt"||fail "applying Max"
grep -aq 'PROFILE name=Max saved=Max gps=1 modem=1 motion=1 raise=1 wifi=1 ble=1 display=120 keyboard=120 sleep=0 ' "$d/log.txt"||fail "Max does not set what it lists"
grep -aq 'PROFILE name=Normal saved=Normal gps=1 modem=0 motion=1 raise=1 wifi=0 ble=0 display=30 keyboard=30 sleep=1 delay=60 usb=1' "$d/log.txt"||fail "Normal does not set what it lists"
python3 -c "import json;t=open('$d/nvs.json').read();import sys;sys.exit(0 if 'mmprofile' in t and 'mmsleep' in t else 1)"||fail "profile / sleep settings not saved to NVS"
shots=($(ls "$d"/screenshot-*.ppm|sort -V));[ ${#shots[@]} = 9 ]||fail "screenshots (${#shots[@]})"
# 1 Power, 2 sleep switch off + focus, 3 deep sleep confirm, 4 confirm cancelled, 5 Battery graph, 6 Battery usage, 7 Power profile, 8 Field applied, 9 Settings
differ(){ cmp -s "${shots[$1]}" "${shots[$2]}"&&fail "$3 look the same"||true; }
differ 0 1 "Power page and Power page with the sleep switch off";differ 1 2 "Power page and the deep sleep confirm";differ 2 3 "deep sleep confirm and the page after Cancel"
differ 3 4 "Power page and Battery graph";differ 4 5 "Battery graph and usage tabs";differ 5 6 "Battery usage and Power profile";differ 6 7 "Power profile before and after applying Field";differ 7 8 "Power profile and Settings"
grep -aq '^DEEP_SLEEP' "$d/log.txt"||fail "deep sleep"
grep -aE 'Guru|abort\(\)|Backtrace' "$d"/log.txt&&fail "crash"
[ -n "${KEEP_DIR:-}" ]&&cp "$d"/screenshot-*.ppm "$KEEP_DIR/"||true
echo "PASS power_max"
