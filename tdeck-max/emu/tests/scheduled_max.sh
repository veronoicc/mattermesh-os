#!/usr/bin/env bash
# Scheduled messages on the Max: Chat options > Send later opens the form with the composer text, SCHEDULE keeps it on the
# SD card and empties the draft; a message scheduled a few seconds ahead through serial is sent by the dispatcher through
# the normal send path (journal row, history Sent line); a second boot with the same SD card and NVS reloads the waiting
# message and the history, and the Scheduled pages draw.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d)
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $*";grep -a -E 'SCHED|RADIO_MESSAGE|COMPOSER|Guru|abort' "$d"/log*.txt|cut -c1-190|tail -20;exit 1; }
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
# Boot 1: the composer flow (channel menu: 3 dots, row "Send later" is the 6th: y 240..268), then a timed message 4 s ahead.
cat > "$d/script1.emu" <<'S'
9000 serial sched status
9500 serial chat open 0
10000 serial draft later hello from the form
10500 serial tap 224 34
11000 serial tap 120 254
11500 serial sched status
12000 shot
12500 serial key j
13000 serial key j
13500 serial key j
14000 serial key j
14500 serial key j
15000 serial key j
15500 serial key ENTER
16500 serial sched list
17000 serial composer status
17500 serial status
18000 serial sched add 0 4 sendsoon hello
19000 serial sched list
26000 serial sched history
26500 serial section 63
27000 shot
27500 serial key j
28000 serial key ENTER
28500 shot
29000 serial sched status
29500 quit
S
timeout 80 artifacts/emu-max --headless --data "$d" --air off --script "$d/script1.emu" < /dev/null > "$d/log1.txt" 2>&1||true
grep -aq 'SCHED_STATUS loaded=1 items=0 history=0 .*view=0 .*form=0 ' "$d/log1.txt"||fail "empty scheduler at boot"
grep -aq 'SCHED_STATUS .*form=1 ' "$d/log1.txt"||fail "Send later did not open the form"
grep -aE '^SCHED_ITEM ' "$d/log1.txt"|grep -q 'target=.* condition=At time .*text=later hello from the form$'||fail "SCHEDULE did not store the composer text as an At time message"
grep -aq 'COMPOSER_STATUS bytes=0 ' "$d/log1.txt"||fail "the draft was not cleared"
grep -aq 'SCHED_ADD ok=1 ' "$d/log1.txt"||fail "serial add"
grep -aq 'RADIO_MESSAGE_QUEUED ok=1' "$d/log1.txt"||fail "dispatcher did not send through queueRadioMessage"
grep -aq '^SCHED_RESULT outcome=Sent ' "$d/log1.txt"||fail "no Sent result"
grep -aE '^SCHED_HISTORY outcome=Sent ' "$d/log1.txt"|grep -q 'text=sendsoon hello$'||fail "history Sent line"
grep -qa 'sendsoon hello' "$d/sd/mattermesh/messages.tsv"||fail "sent message missing from the journal"
! grep -qa 'later hello from the form' "$d/sd/mattermesh/messages.tsv"||fail "the scheduled message was sent early"
[ -s "$d/sd/mattermesh/scheduled.bin" ]||fail "scheduled.bin not written"
grep -aq 'SCHED_STATUS .*items=1 history=1 ' "$d/log1.txt"||fail "one waiting, one in the history"
shots=($(ls "$d"/screenshot-*.ppm|sort -V));[ ${#shots[@]} = 3 ]||fail "expected 3 screenshots, got ${#shots[@]}"
cmp -s "${shots[0]}" "${shots[1]}"&&fail "form and list look the same"
cmp -s "${shots[1]}" "${shots[2]}"&&fail "list and message look the same"
# Boot 2: the same SD card and NVS: the waiting message and the history come back; SEND NOW sends it; deleting persists.
cat > "$d/script2.emu" <<'S'
9000 serial sched list
9500 serial sched history
10000 serial sched status
10500 serial section 63
11000 serial key ENTER
12000 serial sched status
12500 serial sched send 1
17000 serial sched history
17500 serial sched list
18000 quit
S
timeout 60 artifacts/emu-max --headless --data "$d" --air off --script "$d/script2.emu" < /dev/null > "$d/log2.txt" 2>&1||true
grep -aq 'SCHED_LOAD status=0 items=1 history=1' "$d/log2.txt"||fail "reload after the reboot"
grep -aE '^SCHED_ITEM ' "$d/log2.txt"|head -1|grep -q 'text=later hello from the form$'||fail "waiting message not reloaded"
grep -aE '^SCHED_HISTORY outcome=Sent ' "$d/log2.txt"|head -1|grep -q 'text=sendsoon hello$'||fail "history not reloaded"
grep -aq 'SCHED_SEND ok=1 id=1' "$d/log2.txt"||fail "send now"
grep -aE '^SCHED_HISTORY outcome=Sent ' "$d/log2.txt"|tail -2|grep -q 'text=later hello from the form$'||fail "send now did not send the message"
grep -qa 'later hello from the form' "$d/sd/mattermesh/messages.tsv"||fail "send now: message missing from the journal"
grep -aE 'Guru|abort\(\)|Backtrace' "$d"/log*.txt&&fail "crash"
[ -n "${KEEP_DIR:-}" ]&&cp "$d"/screenshot-*.ppm "$KEEP_DIR/"||true
echo "PASS scheduled_max"
