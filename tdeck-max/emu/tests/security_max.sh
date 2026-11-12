#!/usr/bin/env bash
# Settings > Security (Max): set a PIN with the pad (keys), lock, unlock with the on-screen pad (touch) and with keys, the wrong-PIN
# backoff (5 wrong tries: 30 s, kept across a restart), "ask after", hidden previews, the USB block while locked, change and
# remove with the old PIN. One emulator run per step; the NVS carries the PIN from one to the next.
set -euo pipefail
cd "$(dirname "$0")/../.."
data=$(mktemp -d)
mkdir -p artifacts/security-shots
trap 'cp "$data"/*/*.ppm artifacts/security-shots/ 2>/dev/null || true; rm -rf "$data"' EXIT
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$data/"
# mk NAME < steps: one step per line at 1.3 s intervals (an e-paper refresh takes 0.65 s); "wait MS" adds time, "gesture" is two
# SPACE taps (locks, or starts the unlock), "touch X Y" a tap on the panel, "key C" / "type S" go over the serial harness.
mk(){ awk -v t=9000 '/^wait /{t+=$2;next}/^shot$/{print t" shot";t+=700;next}/^sshot$/{print t" serial shot";t+=700;next}/^gesture$/{print t" tap 33";print t+250" tap 33";t+=3500;next}/^touch /{print t" click "$2" "$3;t+=1300;next}/^enter$/{print t" tap 21";t+=1300;next}{print t" serial "$0;t+=1300}END{print t" quit"}' > "$data/$1.emu"; }
run(){ MM_EMU_LICENSED=1 timeout 200 artifacts/emu-max --headless --air off --data "$data" --script "$data/$1.emu" < /dev/null > "$data/$1.log" 2>&1; mkdir -p "$data/$1"; for f in "$data"/screenshot-*.ppm; do [ -f "$f" ] && mv "$f" "$data/$1/$1-$(basename "$f")"; done; true; }
fail(){ echo "FAIL: $1"; grep -ahE 'SECURITY_|LOCK_STATUS|USB_BLOCKED|UI_READY locked|LIST_END' "$data"/*.log | tail -40; exit 1; }
# the pad keys by touch: 1 (46,171) 2 (120,171) 3 (194,171) 4 (46,211) OK (194,291)
mk a <<'S'
security status
section 58
shot
enter
shot
type 12
key ENTER
type 1234
key ENTER
type 1235
key ENTER
type 1234
key ENTER
type 1234
key ENTER
security status
shot
gesture
lock status
gesture
security status
shot
type 1111
key ENTER
security status
shot
touch 46 171
touch 120 171
touch 194 171
touch 46 211
touch 194 291
security status
lock status
shot
gesture
gesture
type 1111
key ENTER
type 2222
key ENTER
type 3333
key ENTER
type 4444
key ENTER
security status
type 5555
key ENTER
security status
shot
type 1234
key ENTER
security status
lock status
S
run a
grep -aq 'SECURITY_STATUS pin=0 ' "$data/a.log" || fail 'a fresh device has no PIN'
python3 - "$data/a.log" <<'PY' || fail 'set, lock, unlock by touch, backoff'
import re,sys
lines=open(sys.argv[1],errors='replace').read().splitlines()
st=[l for l in lines if l.startswith('SECURITY_STATUS')];lk=[l for l in lines if l.startswith('LOCK_STATUS')]
ms=lambda l:int(re.search(r'wait_ms=(\d+)',l)[1])
# st: 0 fresh, 1 PIN set, 2 pad open, 3 one wrong, 4 after the touch entry, 5 four wrong, 6 five wrong (30 s), 7 right PIN in the wait
assert 'pin=1' in st[1] and 'fails=0' in st[1] and 'locked=0' in st[1] and 'setup=0' in st[1], 'a short PIN and a wrong confirmation restart the step, 1234 twice saves: '+st[1]
assert 'locked=1' in lk[0], 'two SPACE taps lock: '+lk[0]
assert 'locked=1' in st[2] and 'pad=1' in st[2] and 'fails=0' in st[2], 'the unlock gesture opens the pad: '+st[2]
assert 'locked=1' in st[3] and 'pad=1' in st[3] and 'fails=1' in st[3], 'a wrong PIN is counted, the pad stays: '+st[3]
assert 'locked=0' in st[4] and 'fails=0' in st[4] and 'pad=0' in st[4], 'the right PIN by touch unlocks and clears the failures: '+st[4]
assert 'locked=0' in lk[1], lk[1]
assert 'fails=4' in st[5] and ms(st[5])==0, 'four wrong tries are free: '+st[5]
assert 'fails=5' in st[6] and 0<ms(st[6])<=30000 and 'locked=1' in st[6], 'the fifth wrong try starts the 30 s wait: '+st[6]
assert 'fails=5' in st[7] and 'locked=1' in st[7] and ms(st[7])>0, 'the right PIN is refused during the wait: '+st[7]
PY
# run B: after a restart the PIN is asked at once, the failures and a new 30 s wait are still there; the right PIN opens it later
mk b <<'S'
security status
gesture
security status
type 1234
key ENTER
security status
shot
wait 34000
gesture
type 1234
key ENTER
security status
lock status
S
run b
python3 - "$data/b.log" <<'PY' || fail 'a restart keeps the PIN, the lock and the backoff'
import re,sys
st=[l for l in open(sys.argv[1],errors='replace').read().splitlines() if l.startswith('SECURITY_STATUS')]
ms=lambda l:int(re.search(r'wait_ms=(\d+)',l)[1])
assert 'pin=1' in st[0] and 'locked=1' in st[0] and 'boot=1' in st[0] and 'fails=5' in st[0] and ms(st[0])>0, 'locked at boot with the failures and a new wait: '+st[0]
assert 'pad=1' in st[1] and 'fails=5' in st[1] and ms(st[1])>0, st[1]
assert 'locked=1' in st[2] and ms(st[2])>0, 'digits typed during the wait do not unlock: '+st[2]
assert 'locked=0' in st[3] and 'fails=0' in st[3] and 'pad=0' in st[3], 'the right PIN after the wait: '+st[3]
PY
# run C: ask after 1 min, hidden previews, USB block, change and remove with the old PIN
mk c <<'S'
gesture
type 1234
key ENTER
section 58
shot
key j
key j
key l
key j
key ENTER
shot
security status
gesture
lock test-rx
security status
shot
gesture
security status
section 58
key j
key j
key j
key ENTER
gesture
lock test-rx
security status
gesture
security status
section 58
key j
key j
key j
key j
key ENTER
gesture
lock status
ls /
sd ls
read /mattermesh/radio.conf
sshot
security status
gesture
security status
ls /
section 58
key j
key ENTER
type 9999
key ENTER
security status
type 1234
key ENTER
type 5555
key ENTER
type 5555
key ENTER
security status
key k
key ENTER
type 5555
key ENTER
security status
gesture
gesture
lock status
S
run c
python3 - "$data/c.log" <<'PY' || fail 'ask after, previews, USB block, change, remove'
import re,sys
lines=open(sys.argv[1],errors='replace').read().splitlines()
st=[l for l in lines if l.startswith('SECURITY_STATUS')];lk=[l for l in lines if l.startswith('LOCK_STATUS')]
def field(l,k):return re.search(k+r'=(\S*)',l)[1]
assert field(st[0],'require')=='1' and field(st[0],'hide')=='1', 'ask after 1 min and hide previews saved: '+st[0]
assert field(st[1],'locked')=='1' and 'Unlock to read' in st[1] and 'MATTERMESH LOCK' not in st[1], 'the preview is hidden on the lock screen: '+st[1]
assert field(st[2],'locked')=='0' and field(st[2],'pad')=='0', 'within the ask-after time the unlock gesture needs no PIN: '+st[2]
assert field(st[3],'locked')=='1' and field(st[3],'hide')=='0' and 'MATTERMESH LOCK callback test' in st[3], 'with the option off the preview shows: '+st[3]
assert field(st[4],'locked')=='0', st[4]
assert field(st[5],'usb')=='1' and field(st[5],'locked')=='1', st[5]
blocked=[n for n,l in enumerate(lines) if l.startswith('USB_BLOCKED')]
assert len(blocked)>=4, 'ls, sd ls, read and shot are refused while locked and blocked: %d'%len(blocked)
between=lines[blocked[0]:lines.index(st[5])+1]
assert not any(l.startswith(('FILE_BEGIN','SD_LS_BEGIN','PBM_BEGIN','LIST_END','FILE_ERROR')) for l in between), 'nothing was read'
assert field(st[6],'locked')=='0', st[6]
assert any(l.startswith('LIST_END') for l in lines[lines.index(st[6]):]), 'USB commands answer again once unlocked'
assert field(st[7],'setup')=='1' and field(st[7],'fails')=='1', 'a wrong current PIN is refused and counted: '+st[7]
assert field(st[8],'pin')=='1' and field(st[8],'fails')=='0' and field(st[8],'setup')=='0', 'PIN changed: '+st[8]
assert field(st[9],'pin')=='0' and field(st[9],'setup')=='0', 'PIN removed with the new PIN: '+st[9]
assert lk[-1].startswith('LOCK_STATUS locked=0'), 'without a PIN the unlock gesture opens the device: '+lk[-1]
PY
# run D: the PIN stays removed after a restart (no lock at boot)
mk d <<'S'
security status
lock status
S
run d
grep -aq 'SECURITY_STATUS pin=0 .*locked=0 ' "$data/d.log" || fail 'no PIN, no lock at boot'
echo 'PASS: security - set by keys, lock, unlock by touch and keys, 5 wrong tries = 30 s kept across a restart, ask after, hidden previews, USB block, change / remove with the old PIN'
