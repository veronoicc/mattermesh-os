#!/usr/bin/env bash
# Quick messages on the Max: Settings > Quick messages (section 61) edits a preset (keys), flips its position pin, moves it, resets to
# the defaults after the confirm panel; the picker over a chat (serial, Alt+Q once the Quick key is chosen) sends presets to the
# other emulated node over emu-air (heard there, journaled here); every state draws a distinct screenshot.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7800+RANDOM%400));offA=$((2500+RANDOM%200))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $1";grep -a -E 'QUICK|Guru|abort' "$d/Alpha.log"|cut -c1-170|tail -20;exit 1; }
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
cat > "$d/alpha.emu" <<'S'
6000 serial ui unlock
6500 serial chat open 0
7000 serial quick key 0
7500 serial quick mode 0
8000 serial quick picker
8500 shot
9000 serial key 2
10000 serial quick status
10500 serial quick send 3
11000 serial quick key 1
11500 key 30 down
11600 key 10 down
11700 key 10 up
11800 key 30 up
14000 serial quick status
14500 shot
15000 serial key BACK
16000 serial quick status
16500 serial section 61
17000 serial quick status
17500 shot
18000 serial key s
18100 serial key s
18200 serial key ENTER
18300 serial key DEL
18400 serial key DEL
18500 serial key H
18600 serial key i
18800 shot
19000 serial key ENTER
19200 serial quick list
19400 serial key d
19600 serial quick list
19800 serial key n
20000 serial quick list
20200 serial key s
20300 serial key s
20400 serial key s
20500 serial key s
20600 serial key s
20700 serial key s
20900 serial key ENTER
21100 shot
21300 serial key w
21500 serial key ENTER
21700 serial quick list
21900 serial quick add Back soon
22100 serial quick list
22300 serial quick key 0
22500 serial quick mode 0
40000 quit
S
printf '3000 serial ui unlock\n45000 quit\n' > "$d/bravo.emu"
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
i=0;for n in Alpha Bravo;do script=$(echo $n|tr A-Z a-z);timeout 100 artifacts/emu-max --headless --data "$d/$n" --air $port --port-offset $((offA+i*100)) --script "$d/$script.emu" < /dev/null > "$d/$n.log" 2>&1 &i=$((i+1));done
wait %2 %3 2>/dev/null||true
python3 - "$d/Alpha.log" <<'P'||fail "log checks"
import re,sys
log=open(sys.argv[1],errors="replace").read().splitlines()
lists=[];cur=[]
for l in log:
    m=re.match(r"QUICK_ROW n=(\d+) position=(\d) text=(.*)$",l)
    if m:cur.append((m.group(3),int(m.group(2))))
    elif l.startswith("QUICK_END"):lists.append(cur);cur=[]
assert len(lists)==5,len(lists)
edited,pinned,moved,reset,added=lists
assert edited[0]==("Hi",0) and len(edited)==6,edited
assert pinned[0]==("Hi",1),pinned
assert moved[:2]==[("On my way",0),("Hi",1)],moved
assert [t for t,_ in reset]==["OK","On my way","At camp","Need help","Call me","Where are you?"] and not any(p for _,p in reset),reset
assert added[-1]==("Back soon",0) and len(added)==7,added
st=[l for l in log if l.startswith("QUICK_STATUS")]
assert "picker=0" in st[0] and "count=6" in st[0],st[0]        # after picking a preset the picker is gone
assert "key=1" in st[1] and "picker=1" in st[1],st[1]         # Alt+Q opened it
assert "picker=0" in st[2],st[2]                              # BACK closed it
assert any(l.startswith("QUICK_PICKER open=1") for l in log)
assert any(re.match(r"QUICK_SEND n=3 ok=1 chat=0",l) for l in log)
assert any(l.startswith("QUICK_KEY ok=1 key=1 label=Alt+Q") for l in log)
P
grep -qa "On my way" "$d/Alpha/sd/mattermesh/messages.tsv"||fail "the picked preset is missing from the journal"
grep -qa "At camp" "$d/Alpha/sd/mattermesh/messages.tsv"||fail "the serial preset is missing from the journal"
[ "$(grep -c "sender=Alpha receivers=Bravo:-70/10/ok, length=[1-9][0-9]\? " "$d/air.log")" -ge 2 ]||fail "Bravo did not hear both presets on the air"
python3 -c "import sys;t=open('$d/Alpha/nvs.json').read();sys.exit(0 if 'mmquick' in t else 1)"||fail "settings not saved to NVS"
shots=($(ls "$d"/Alpha/screenshot-*.ppm "$d"/screenshot-*.ppm 2>/dev/null|sort -V||true))
[ ${#shots[@]} -ge 5 ]||fail "expected 5 screenshots, got ${#shots[@]}"
for i in 1 2 3 4;do cmp -s "${shots[0]}" "${shots[$i]}"&&fail "screenshot $i looks like the first page";done
grep -aE 'Guru|abort\(\)|Backtrace' "$d"/*.log&&fail "crash"
[ -n "${EMU_SHOTS:-}" ]&&cp "${shots[@]}" "$EMU_SHOTS"/||true
echo "PASS: quick messages - edit, pin, move, reset with confirm, picker + Quick key over a chat, presets delivered to the other node"
