#!/usr/bin/env bash
# MQTT downlink on the Max, headless (no broker is reachable, so the commands enter through the serial "mqtt cmd" hook, which runs the
# same handler as a payload received on .../cmd): Settings > Cellular is driven by keys (uplink on, downlink on, token typed, Allow send on),
# then ping, a channel send, the lists, a wrong token, an oversize text (161 bytes, the serial line holds 256), an unknown contact, an advert, Allow send off and the rate limit
# are answered as documented; the sent text lands in the chat journal and in the .../messages queue; Generate replaces the token.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d)
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $*";grep -a -E 'MQTT|RADIO_MESSAGE|Guru|abort' "$d"/log*.txt|cut -c1-200|tail -25;exit 1; }
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
tok=Tok3nTok3n1234
python3 - "$d/script1.emu" "$tok" <<'P'
import sys
out=[];t=[3000]
def at(line,gap=400):
    out.append("%d %s"%(t[0],line));t[0]+=gap
def keys(k,n=1):
    for _ in range(n):at("serial key "+k,250)
def edit(text):
    keys("ENTER");at("serial type "+text,300);keys("ENTER")
tok=sys.argv[2]
at("serial section 51",800)
keys("j",7);keys("ENTER")            # MQTT uplink on (so the chat events queue)
keys("j");edit("localhost")             # broker
keys("j",4);edit("tst")                 # region
keys("j",2);keys("ENTER")               # TLS passed, Downlink on
keys("j");edit(tok)                     # command token
keys("j",2);keys("ENTER")               # Generate passed, Allow send on
at("shot",600)
def cmd(j):at("serial mqtt cmd "+j,500)
T='"token":"%s"'%tok
cmd('{"id":"p1",%s,"cmd":"ping"}'%T)
cmd('{"id":"s1",%s,"cmd":"send_channel","channel":"0","text":"hello from mqtt"}'%T)
cmd('{"id":"lc",%s,"cmd":"list_channels"}'%T)
cmd('{"id":"lk",%s,"cmd":"list_contacts"}'%T)
cmd('{"id":"bt","token":"wrong-token","cmd":"ping"}')
cmd('{"id":"big",%s,"cmd":"send_channel","channel":"0","text":"%s"}'%(T,"x"*161))
cmd('{"id":"dm",%s,"cmd":"send_dm","to":"nobody","text":"hi"}'%T)
cmd('{"id":"ad",%s,"cmd":"advert"}'%T)
at("serial mqtt messages",500)
keys("ENTER")                          # Allow send off again (the focus is still on it)
cmd('{"id":"off",%s,"cmd":"send_channel","channel":"0","text":"blocked"}'%T)
cmd('{"id":"p2",%s,"cmd":"ping"}'%T)
cmd('{"id":"p3",%s,"cmd":"ping"}'%T)    # the 11th payload within a minute
cmd('{"id":"p4",%s,"cmd":"ping"}'%T)
keys("k");keys("ENTER");at("shot",600)  # Generate: the token is shown once
keys("j");at("shot",600)                # moving on hides it
at("serial section 7",500);at("serial status",500);at("quit",500)
open(sys.argv[1],"w").write("\n".join(out)+"\n")
P
timeout 120 artifacts/emu-max --headless --data "$d" --air off --script "$d/script1.emu" < /dev/null > "$d/log1.txt" 2>&1||true
has(){ grep -aq -F -- "$1" "$d/log1.txt"||fail "$2"; }
has 'MQTT_RESP {"id":"p1","ok":true,"result":{"name":' "ping answered"
has '"allow_send":true' "ping reports Allow send"
has 'MQTT_RESP {"id":"s1","ok":true,"result":{"message_id":"' "channel send accepted"
has 'RADIO_MESSAGE_QUEUED ok=1' "the UI send path ran"
has 'MQTT_RESP {"id":"lc","ok":true,"result":{"items":[{"index":0' "channel list"
has 'MQTT_RESP {"id":"lk","ok":true,"result":{"items":[' "contact list"
has 'MQTT_RESP {"id":"bt","ok":false,"error":"unauthorized"}' "wrong token refused"
has 'MQTT_RESP {"id":"big","ok":false,"error":"Message full / 160-byte limit"}' "oversize text refused"
has 'MQTT_RESP {"id":"dm","ok":false,"error":"unknown contact"}' "unknown contact refused"
has 'MQTT_RESP {"id":"ad","ok":true}' "advert accepted"
has 'MQTT_RESP {"id":"off","ok":false,"error":"sending is disabled"}' "Allow send off refuses sends"
has 'MQTT_RESP {"id":"p2","ok":true' "ping still works with Allow send off"
has 'MQTT_RESP {"id":null,"ok":false,"error":"rate limited"}' "rate limit"
grep -a '^MQTT_MSG ' "$d/log1.txt"|grep -aq '"kind":"channel".*"text":"hello from mqtt","direction":"out"'||fail "sent message not queued for .../messages"
grep -aq 'MQTT_MESSAGES count=[1-9]' "$d/log1.txt"||fail "message queue empty"
grep -aqr 'hello from mqtt' "$d/sd"||fail "message not in the chat journal"
! grep -aqr 'blocked' "$d/sd"||fail "refused text reached the journal"
python3 - "$d/nvs.json" "$tok" <<'P'||fail "settings in NVS"
import sys
rows=[l.split() for l in open(sys.argv[1]) if l.startswith('V "mmcell"')]
v={r[2].strip('"'):bytes.fromhex(r[4]) if len(r)>4 else b'' for r in rows}
assert v.get("dl")==b'\x01' and v.get("dlsend")==b'\x00',v
t=v["dltok"].rstrip(b"\0").decode();assert len(t)==24 and t!=sys.argv[2] and all(c in "23456789ABCDEFGHJKLMNPQRSTUVWXYZ" for c in t),t
P
shots=($(ls "$d"/screenshot-*.ppm|sort -V|tail -3));[ ${#shots[@]} = 3 ]||fail "screenshots missing"
cmp -s "${shots[0]}" "${shots[1]}"&&fail "settings page and generated token look the same"
cmp -s "${shots[1]}" "${shots[2]}"&&fail "the token is still shown after the focus moved"
grep -aq 'STATUS reset' "$d/log1.txt"||fail "firmware not answering"
grep -aE 'Guru|abort\(\)|Backtrace' "$d"/log*.txt&&fail "crash"
[ -n "${KEEP_DIR:-}" ]&&cp "$d"/screenshot-*.ppm "$KEEP_DIR/"||true
echo "PASS mqtt_downlink_max"
