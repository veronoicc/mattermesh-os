#!/usr/bin/env bash
# Message delivery history on the Max: Alpha sends a channel message to Bravo over emu-air; its timeline (Queued, Transmitted)
# is recorded, written to the journal as an E1 row, opened in the Delivery view (section 62) by serial, scrolls, RESENDs as
# a new message whose history opens next, and BACK returns to the chat. Every view state draws a distinct screenshot.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((8200+RANDOM%400));offA=$((2900+RANDOM%200))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $1";grep -a -E 'DELIVERY|TX_STATE|UI_STATE|Guru|abort' "$d/Alpha.log"|cut -c1-190|tail -24;exit 1; }
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
cat > "$d/alpha.emu" <<'S'
3000 serial ui unlock
3500 serial chat open 0
4000 serial type deliveryprobe
4500 serial key ENTER
12000 serial delivery dump last
12500 serial delivery open last
13000 serial delivery status
13500 shot
14000 serial key j
14500 serial delivery status
15000 shot
15500 serial key ENTER
16500 serial delivery status
17000 shot
17500 serial key BACK
18000 serial ui state
18500 serial delivery status
19000 serial delivery open deadbeef
19500 serial delivery dump 0
40000 quit
S
printf '3000 serial ui unlock\n40000 quit\n' > "$d/bravo.emu"
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
i=0;for n in Alpha Bravo;do script=$(echo $n|tr A-Z a-z);timeout 90 artifacts/emu-max --headless --data "$d/$n" --air $port --port-offset $((offA+i*100)) --script "$d/$script.emu" < /dev/null > "$d/$n.log" 2>&1 &i=$((i+1));done
wait %2 %3 2>/dev/null||true
python3 - "$d/Alpha.log" <<'P'||fail "log checks"
import re,sys
log=open(sys.argv[1],errors="replace").read()
dump=re.findall(r"DELIVERY_EVENT token=(\w+) index=(\d+) at=(\d+) kind=(\w+) arg=(.*)",log)
assert dump,"no DELIVERY_EVENT lines"
token=dump[0][0];kinds=[k for t,i,a,k,_ in dump if t==token]
assert kinds[0]=="QUE","first event is not Queued: %r"%kinds
assert "TX" in kinds,"no Transmitted event: %r"%kinds
times=[int(a) for t,i,a,k,_ in dump if t==token];assert times==sorted(times),times
first=re.search(r"DELIVERY_DUMP ok=1 token=%s events=(\d+) .*summary=(.*)"%token,log);assert first,"no DELIVERY_DUMP for the sent message"
assert first.group(2) in("Transmitted","Queued") or first.group(2).startswith("Heard by") or first.group(2).startswith("Delivered"),first.group(2)
assert re.search(r"DELIVERY_OPEN ok=1 token=%s"%token,log),"the view did not open"
states=re.findall(r"DELIVERY_STATUS open=(\d) token=(\w+) found=(\d) events=(\d+)",log)
assert states[0][0]=="1" and states[0][1]==token and states[0][2]=="1","status while open: %r"%(states[0],)
resent=[s for s in states if s[0]=="1" and s[1]!=token]
assert resent,"RESEND did not open the new message's history: %r"%states
assert states[-1][0]=="0","the view is still open after BACK: %r"%(states[-1],)
assert "DELIVERY_OPEN ok=0 token=deadbeef" in log and re.search(r"DELIVERY_DUMP ok=0 token=00000000",log),"unknown tokens are not refused"
P
grep -aq $'^E1\t' "$d/Alpha/sd/mattermesh/messages.tsv"||fail "no E1 row in the journal"
grep -qa "Deliveryprobe" "$d/Alpha/sd/mattermesh/messages.tsv"||fail "the message is missing from the journal"
[ "$(grep -c "sender=Alpha receivers=Bravo:-70/10/ok, length=[1-9][0-9]\? " "$d/air.log")" -ge 1 ]||fail "Bravo did not hear the message on the air"
shots=($(ls "$d"/Alpha/screenshot-*.ppm "$d"/screenshot-*.ppm 2>/dev/null|sort -V||true))
[ ${#shots[@]} -ge 3 ]||fail "expected 3 screenshots, got ${#shots[@]}"
cmp -s "${shots[0]}" "${shots[2]}"&&fail "the view after RESEND looks like the first one"
grep -aE 'Guru|abort\(\)|Backtrace' "$d"/*.log&&fail "crash"
[ -n "${EMU_SHOTS:-}" ]&&cp "${shots[@]}" "$EMU_SHOTS"/||true
echo "PASS: delivery history - Queued and Transmitted recorded, journaled (E1), Delivery view opens, resends, BACK returns to the chat"
