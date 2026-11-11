#!/usr/bin/env bash
# GPS tracks on the Max: a track recorded from injected NMEA fixes (interval and distance both met) lands on the SD card as
# 20-byte rows; a power cut (the closing row lost, a row cut short) is closed at the next boot, the track lists with its
# statistics, exports to a well-formed GPX 1.1 file listed by the Files app, and both Tracks pages draw.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d)
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $*";grep -a -E 'TRACK|GPS_|Guru|abort' "$d"/log*.txt|cut -c1-170|tail -12;exit 1; }
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
python3 - "$d/script1.emu" <<'P'
import sys,time
t0=time.gmtime();lines=["3000 serial gps inject on","3300 serial tracks cfg 5 5","3600 serial tracks start"]
def nmea(body):
    c=0
    for ch in body:c^=ord(ch)
    return "$%s*%02X"%(body,c)
for k in range(5):
    at=5000+k*6000;lat=44.4+k*0.001;m=(lat-44)*60;hh=time.strftime("%H%M%S",time.gmtime(time.time()+k*6));date=time.strftime("%d%m%y",t0)
    gga=nmea("GNGGA,%s.00,%02d%07.4f,N,00854.0000,E,1,08,1.2,%.1f,M,48.0,M,,"%(hh,44,m,420+k*5))
    rmc=nmea("GNRMC,%s.00,A,%02d%07.4f,N,00854.0000,E,0.0,0.0,%s,,,A"%(hh,44,m,date))
    lines+=["%d serial gps inject %s"%(at,gga),"%d serial gps inject %s"%(at+60,rmc)]
lines+=["31500 serial tracks","32000 serial tracks stop","32500 serial tracks list","33000 serial tracks cfg 5 5","34000 quit"]
open(sys.argv[1],"w").write("\n".join(lines)+"\n")
P
timeout 60 artifacts/emu-max --headless --data "$d" --air off --script "$d/script1.emu" < /dev/null > "$d/log1.txt" 2>&1||true
grep -aq 'TRACKS start=1' "$d/log1.txt"||fail "start"
grep -aq 'TRACKS recording=1 sd=1 file=1 points=[45] ' "$d/log1.txt"||fail "recording status (a file and 4-5 points)"
grep -aq 'TRACKS stopped' "$d/log1.txt"||fail "stop"
grep -aE '^TRACK trk-' "$d/log1.txt"|grep -q 'points=[45] .*closed=1 live=0'||fail "closed track in the list"
grep -aq 'TRACKS cfg=1 interval=5 distance=5' "$d/log1.txt"||fail "settings"
python3 -c "import json,sys;t=open('$d/nvs.json').read();sys.exit(0 if 'mmtracks' in t else 1)"||fail "settings not saved to NVS"
tr=$(ls "$d"/sd/mattermesh/tracks/*.mmt);stem=$(basename "$tr" .mmt)
size=$(stat -c %s "$tr");[ $((size%20)) = 0 ]&&[ "$size" -ge 140 ]||fail "file size $size"
# A power cut: the closing row never written, a point cut after 13 bytes.
head -c $((size-20)) "$tr" > "$d/cut";head -c 13 "$tr" | tail -c 13 >> "$d/cut";cp "$d/cut" "$tr"
printf '3000 serial tracks list\n3500 serial tracks export %s\n4000 serial section 49\n4500 serial key j\n5000 serial key ENTER\n7500 shot\n8000 serial key ENTER\n10500 shot\n10800 serial map status\n11000 serial section 50\n13500 shot\n14000 serial section 7\n14500 serial status\n16000 quit\n' "$stem" > "$d/script2.emu"
timeout 50 artifacts/emu-max --headless --data "$d" --air off --script "$d/script2.emu" < /dev/null > "$d/log2.txt" 2>&1||true
size2=$(stat -c %s "$tr");[ $((size2%20)) = 0 ]||fail "file still cut short ($size2)"
[ "$(tail -c 20 "$tr"|head -c 1)" = S ]||fail "no closing row after the reboot"
grep -aE '^TRACK trk-' "$d/log2.txt"|grep -q 'points=[3-5] .*closed=1'||fail "recovered track not listed closed"
grep -aq "TRACKS export=1 .*$stem.gpx" "$d/log2.txt"||fail "export"
python3 - "$d/sd/mattermesh/tracks/$stem.gpx" <<'P'||fail "GPX content"
import sys,xml.dom.minidom as m
doc=m.parse(sys.argv[1]);root=doc.documentElement
assert root.tagName=="gpx" and root.getAttribute("version")=="1.1" and root.namespaceURI=="http://www.topografix.com/GPX/1/1"
pts=doc.getElementsByTagName("trkpt");assert 3<=len(pts)<=5,len(pts)
lat=[float(p.getAttribute("lat")) for p in pts];assert lat==sorted(lat) and lat[-1]-lat[0]>=0.001*(len(pts)-1)-1e-5,lat
for p in pts:
    t=p.getElementsByTagName("time")[0].firstChild.data;assert t.endswith("Z") and "T" in t,t
    assert p.getElementsByTagName("ele")
P
shots=($(ls "$d"/screenshot-*.ppm|sort -V|tail -3));[ ${#shots[@]} = 3 ]||fail "no screenshots of the Tracks pages"
cmp -s "${shots[0]}" "${shots[1]}"&&fail "detail and map look the same"
cmp -s "${shots[1]}" "${shots[2]}"&&fail "map and settings look the same"
grep -aq 'MAP_STATUS zoom=1[56] lat=44\.40' "$d/log2.txt"||fail "View on map did not fit the track"
grep -aq 'STATUS reset' "$d/log2.txt"||fail "firmware not answering after the Tracks pages"
grep -aE 'Guru|abort\(\)|Backtrace' "$d"/log*.txt&&fail "crash"
[ -n "${KEEP_DIR:-}" ]&&cp "$d"/screenshot-*.ppm "$KEEP_DIR/"||true
echo "PASS tracks_max"
