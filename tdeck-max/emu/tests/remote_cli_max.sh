#!/usr/bin/env bash
# Remote CLI to this device (opt-in). Bravo sends CLI commands (txt_type 3) to Alpha over the virtual air; the answer
# (txt_type 1) reaches Bravo's app queue. Run 1: setting OFF (default), contact flagged: no answer. Run 2: Alpha's
# "remote cli" switch ON, contact unflagged: no answer; flagged by the app: "get name" answers the node name and a
# device-local command ("help") answers "Unknown command". Run 3: Alpha restarted, setting and flag survive: answered again.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7500+RANDOM%400));off=$((2300+RANDOM%100))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;[ -n "${KEEP:-}" ]||rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
fail(){ echo "FAIL: $1";for f in Alpha Bravo;do echo "--- $f";grep -vE '^\[I\]|EMU_EPD|UI_READY|_Update|_PowerOn' "$d/$f.log"|tail -12;done;echo "--- air";tail -12 "$d/air.log";exit 1; }
# run N QUIT_MS ALPHA_SCRIPT_LINES: both nodes up (Alpha with the extra serial lines), then the python phase N.
run(){
 local n=$1 quit=$2 extra=$3;local o=$((off+n*10))
 for f in alpha bravo;do { echo '3000 serial wifi on';echo '3500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}';if [ $n = 1 ];then echo '7200 serial app wifi';fi
  if [ $f = alpha ];then printf '%s' "$extra";fi;echo "$quit quit"; } > "$d/$f$n.emu";done
 timeout 200 artifacts/emu-max --headless --data "$d/Alpha" --air $port --port-offset $o --script "$d/alpha$n.emu" < /dev/null > "$d/Alpha.log" 2>&1 &
 local a=$!
 timeout 200 artifacts/emu-max --headless --data "$d/Bravo" --air $port --port-offset $((o+50)) --script "$d/bravo$n.emu" < /dev/null > "$d/Bravo.log" 2>&1 &
 local b=$!
 for f in Alpha Bravo;do for i in $(seq 1 60);do grep -q "EMU_WIFI listening" "$d/$f.log" 2>/dev/null&&break;sleep 0.5;done;done
 if [ $n = 2 ];then for i in $(seq 1 60);do grep -q "REMOTE_CLI 1" "$d/Alpha.log"&&break;sleep 0.5;done;fi
 python3 - $n $((5000+o)) $((5000+o+50)) <<'P'||fail "phase $n"
import sys
sys.path.insert(0, "emu/tools")
from pair import *
n, a, b = int(sys.argv[1]), connect(int(sys.argv[2])), connect(int(sys.argv[3]))
ak, bk = own_key(a), own_key(b)
add_contact(b, ak, "Alpha", 1, 0)
pings = 0
def command(text, answered=False):
    """The command, then (after a pause: the virtual radio can lose a packet that follows another at once) plain pings
    until one reaches Alpha's app queue, which proves the link while no answer is expected. Messages queued before the
    ping are collected. Returns (answer or None, messages queued to Alpha's app). A command that is expected to be
    answered is sent again when its answer does not come."""
    global pings
    queued = []
    for attempt in range(3 if answered else 1):
        send_text(b, ak, 3, text)
        time.sleep(4)
        for _ in range(3):
            pings += 1
            send_text(b, ak, 0, "ping%d" % pings)
            m = next_message(a, 15)
            while m and not m[2].startswith("ping"):
                queued.append(m)
                m = next_message(a, 15)
            if m: break
        assert m and m[:2] == (bk[:6], 0) and m[2].startswith("ping"), "the link to Alpha is down: %r" % (m,)
        answer = next_message(b, 20)
        if answer: return answer, queued
    return None, queued
if n == 1:
    add_contact(a, bk, "Bravo", 1, 0x10)       # flagged for remote CLI, the setting is off
    answer, queued = command("get name")
    assert answer is None, "answered with the setting off"
    assert queued == [(bk[:6], 3, "get name")], "a command that did not run must reach the app as a CLI command: %r" % (queued,)
elif n == 2:
    add_contact(a, bk, "Bravo", 1, 0)          # the setting is on, the contact is not flagged
    answer, queued = command("get name")
    assert answer is None, "answered an unflagged contact"
    assert queued == [(bk[:6], 3, "get name")], "an unflagged contact's command must reach the app as a CLI command (stock MyMesh): %r" % (queued,)
    add_contact(a, bk, "Bravo", 1, 0x10)
    answer, queued = command("get name", True)
    assert answer == (ak[:6], 1, "Alpha"), "no answer with the setting on and the contact flagged"
    assert queued == [], "a command that ran must not also be queued: %r" % (queued,)
    answer, queued = command("help", True)
    assert answer == (ak[:6], 1, "Unknown command"), "a device-local command was not refused"
else:
    assert contact_flags(a, bk) == 0x10, "the contact's flag was not saved"
    assert command("get name", True)[0] == (ak[:6], 1, "Alpha"), "no answer after the restart"
P
 wait $a $b 2>/dev/null||true
}
run 1 70000 ""
run 2 140000 "9000 serial remote cli
"
run 3 90000 "9000 serial app status
"
grep -q 'APP_STATUS .*remote_cli=1' "$d/Alpha.log"||fail "the setting did not survive the restart"
echo "PASS: remote CLI - off: no answer; on but unflagged: no answer; on and flagged: command output and Unknown command for device-local ones; setting and flag survive a restart"
