#!/usr/bin/env bash
# A plain direct message from a repeater contact reaches the app's offline queue (stock MeshCore queues every received
# direct message) but is not shown in the chat of the device. Bravo (known to Alpha as a repeater) sends to Alpha; then
# Alpha's app turns Bravo into a chat contact and the next message is shown on the device as well (the control).
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7400+RANDOM%400));offA=$((2100+RANDOM%200))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
cat > "$d/node.emu" <<'S'
3000 serial wifi on
3500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}
7200 serial app wifi
75000 quit
S
artifacts/emu-air --port $port > "$d/broker.out" 2>&1 &
sleep 0.5
i=0;for n in Alpha Bravo;do timeout 100 artifacts/emu-max --headless --data "$d/$n" --air $port --port-offset $((offA+i*100)) --script "$d/node.emu" < /dev/null > "$d/$n.log" 2>&1 &i=$((i+1));done
for n in Alpha Bravo;do for i in $(seq 1 60);do grep -q "EMU_WIFI listening" "$d/$n.log" 2>/dev/null&&break;sleep 0.5;done;done
fail(){ echo "FAIL: $1";grep -v '^\[I\]' "$d/Alpha.log"|tail -8;exit 1; }
python3 - $((5000+offA)) $((5000+offA+100)) <<'P'||fail "companion driver"
import sys
sys.path.insert(0, "emu/tools")
from pair import *
a, b = connect(int(sys.argv[1])), connect(int(sys.argv[2]))
ak, bk = own_key(a), own_key(b)
add_contact(a, bk, "Bravo", 2, 0)   # a repeater to Alpha
add_contact(b, ak, "Alpha", 1, 0)
send_text(b, ak, 0, "repeaterplainone")
m = next_message(a)
assert m == (bk[:6], 0, "repeaterplainone"), "the repeater's plain message is not in the app queue: %r" % (m,)
add_contact(a, bk, "Bravo", 1, 0)   # now a chat contact
send_text(b, ak, 0, "chatplaintwo")
m = next_message(a)
assert m == (bk[:6], 0, "chatplaintwo"), "the chat contact's message is not in the app queue: %r" % (m,)
P
wait %2 %3 2>/dev/null||true
grep -rqa chatplaintwo "$d/Alpha/sd"||fail "control: a chat contact's message is not shown on the device"
! grep -rqa repeaterplainone "$d/Alpha/sd"||fail "the repeater's message is shown on the device"
echo "PASS: repeater plain DM - queued for the app, kept out of the device chat; a chat contact's DM is in both"
