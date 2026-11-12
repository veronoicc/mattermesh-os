#!/usr/bin/env bash
# Hardware self-test on the Max emulator: every check runs against the emulated chips (SX1262, GPS, SD, ES8311, DRV2605,
# TCA8418, CST328, BQ27220, SY6970, XL9555, e-paper, Wi-Fi) and the SELFTEST lines carry the expected results; the keyboard and
# touch checks are answered through the modelled TCA8418 / CST328; the report lands on the SD card; the app draws, its options persist.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d)
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $*";grep -a -E 'SELFTEST|Guru|abort' "$d"/log*.txt|cut -c1-190|tail -40;exit 1; }
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
cat > "$d/script.emu" <<'SCRIPT'
9000 serial section 60
9500 serial selftest list
10000 shot
10500 serial selftest keys
11500 type a
12500 serial selftest tap
13500 click 120 204
14500 serial selftest interactive off
15000 serial selftest
42000 serial selftest status
42200 serial selftest export
42500 serial selftest nosuchcheck
43000 serial selftest interactive on
43200 serial selftest output off
43500 serial selftest
44000 shot
50000 serial selftest buzz
50500 serial selftest status
51000 serial section 8
52000 quit
SCRIPT
timeout 90 artifacts/emu-max --headless --data "$d" --air off --script "$d/script.emu" < /dev/null > "$d/log1.txt" 2>&1||true
log="$d/log1.txt"
result(){ grep -a "^SELFTEST name=$1 " "$log"|head -1|sed -n 's/.* result=\([a-z]*\) .*/\1/p'; }
expect(){ [ "$(result "$1")" = "$2" ]||fail "$1: expected $2, got '$(result "$1")'"; }
grep -aq '^SELFTEST_CHECK name=sx1262 ' "$log"||fail "selftest list"
[ "$(grep -ac '^SELFTEST_CHECK ' "$log")" = 24 ]||fail "24 checks listed"
for c in sx1262 gps ubx sd codec speaker mic haptics buzz keyboard keys touch tap fuel charger xl9555 psram flash heap epd; do expect $c pass;done
# The BHI260AP is not modelled (emu/stubs/services/motion_chip.h): the firmware reports the chip as not detected, as on a board without it.
expect imu fail
grep -aq '^SELFTEST name=imu result=fail detail=BHI260AP not detected' "$log"||fail "imu detail"
case "$(result modem)" in pass|skip) ;; *) fail "modem must pass or skip";; esac
case "$(result wifi)" in pass|skip) ;; *) fail "wifi must pass or skip";; esac
case "$(result ble)" in pass|skip|fail) ;; *) fail "ble missing";; esac
grep -aq '^SELFTEST name=sx1262 result=pass detail=RSSI -' "$log"||fail "sx1262 RSSI detail"
grep -aq '^SELFTEST name=sd result=pass detail=32.0 GB' "$log"||fail "sd detail"
grep -aq '^SELFTEST name=haptics result=pass detail=DRV2605L at 0x5A' "$log"||fail "haptics detail"
grep -aq '^SELFTEST name=fuel result=pass detail=3920 mV' "$log"||fail "fuel detail"
grep -aq '^SELFTEST name=mic result=pass detail=capture ok, peak' "$log"||fail "mic detail"
grep -aq '^SELFTEST_DONE ' "$log"||fail "summary line"
grep -aq 'SELFTEST_OPTIONS interactive=0 output=1' "$log"||fail "interactive switch"
# Run with interactive checks off: they are skipped, never waited for.
sed -n '/SELFTEST_OPTIONS interactive=0/,/SELFTEST_DONE/p' "$log"|grep -aq 'name=keys result=skip detail=disabled in options'||fail "interactive checks not skipped"
# Quiet mode: the speaker and the motor are skipped.
sed -n '/SELFTEST_OPTIONS interactive=1 output=0/,$p' "$log"|grep -aq 'name=speaker result=skip detail=disabled in options'||fail "quiet mode skips the speaker"
grep -aq 'EMU_VIBRATE ' "$log"||fail "motor buzz"
grep -aq '^SELFTEST name=buzz result=pass' "$log"||fail "buzz check run by name"
grep -aq '^SELFTEST_ERROR unknown=nosuchcheck' "$log"||fail "unknown check name"
python3 - "$d" <<'P'||fail "options persistence / report"
import re,sys,pathlib
root=pathlib.Path(sys.argv[1])
nvs=(root/'nvs.json').read_text()
assert 'mmselftest' in nvs,'options not saved to NVS'
rep=sorted((root/'sd').glob('selftest-*.txt'))
assert rep,'no report on the SD card'
t=rep[-1].read_text()
assert t.startswith('MatterMesh OS self-test'),t[:60]
assert '[PASS] SX1262 radio (sx1262)' in t and '[FAIL] BHI260AP motion (imu)' in t,t
assert re.search(r'Result: \d+ pass, 1 fail, \d+ skip, \d+ not run',t),t[-120:]
assert not (root/'sd'/'selftest.tmp').exists(),'temp file left behind'
P
log="$d/log1.txt"
grep -aq '^SELFTEST_EXPORT ok=1 file=selftest-' "$log"||fail "export line"
shots=($(ls "$d"/screenshot-*.ppm|sort -V));[ ${#shots[@]} = 2 ]||fail "screenshots (${#shots[@]})"
cmp -s "${shots[0]}" "${shots[1]}"&&fail "list and results look the same"
grep -aE 'Guru|abort\(\)|Backtrace' "$d"/log*.txt&&fail "crash"
[ -n "${KEEP_DIR:-}" ]&&cp "$d"/screenshot-*.ppm "$d"/log1.txt "$KEEP_DIR/"||true
echo "PASS selftest_max"
