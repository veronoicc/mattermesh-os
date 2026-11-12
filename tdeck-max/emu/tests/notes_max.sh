#!/usr/bin/env bash
# Notes app on the Max: a note is created from the keyboard (title, two checklist items added with the ADD ITEM button), an item is
# ticked with ENTER, the note lands on the SD card as /mattermesh/notes/<title>.txt in Markdown task form and is listed by the Files
# app; after a restart it is still there with its ticked item, then it is pinned and shown as the Home note, and every page draws.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d)
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $*";grep -a -E 'NOTE|Guru|abort' "$d"/log*.txt|cut -c1-170|tail -12;exit 1; }
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
# 1: section 56, N (new note), the title, ENTER, ADD ITEM + text twice, DONE; tick the first item (the view's focus), look at everything.
cat > "$d/script1.emu" <<'S'
3000 serial section 56
3500 serial notes status
4000 shot
4500 serial key n
6000 type shopping
8500 tap 21
9000 click 178 295
9500 type milk
12000 click 178 295
12500 type eggs
15000 shot
15500 click 60 295
16500 serial notes status
17000 shot
17500 tap 21
18500 serial notes show shopping
19000 serial key o
19500 shot
20000 serial key BACK
20500 serial section 7
21000 serial status
22000 quit
S
timeout 90 artifacts/emu-max --headless --data "$d" --air off --script "$d/script1.emu" < /dev/null > "$d/log1.txt" 2>&1||true
grep -aq 'NOTES mode=0 .*notes=0' "$d/log1.txt"||fail "empty list at the start"
grep -aq 'NOTES mode=1 stem=shopping' "$d/log1.txt"||fail "note not saved under its title"
f="$d/sd/mattermesh/notes/shopping.txt";[ -f "$f" ]||fail "no note file"
[ "$(head -n1 "$f")" = shopping ]||fail "title line"
grep -qx -- '- \[x\] milk' "$f"||fail "milk not ticked: $(cat "$f")"
grep -qx -- '- \[ \] eggs' "$f"||fail "eggs item: $(cat "$f")"
grep -aq 'NOTE_TEXT ok=1' "$d/log1.txt"||fail "notes show"
[ ! -e "$d/sd/mattermesh/notes/shopping.tmp" ]||fail "temporary file left"
shots=($(ls "$d"/screenshot-*.ppm|sort -V));[ ${#shots[@]} -ge 4 ]||fail "screenshots"
cmp -s "${shots[0]}" "${shots[2]}"&&fail "list and note look the same"
cmp -s "${shots[1]}" "${shots[2]}"&&fail "editor and note look the same"
# 2: a restart: the note is listed with 1/2 done, pinned to Home, the Home note shows it.
cat > "$d/script2.emu" <<'S'
3000 serial notes list
3500 serial section 56
4500 shot
5000 serial notes pin shopping
5500 serial home
6500 serial home main status
7000 shot
8000 serial notes status
9000 quit
S
[ -n "${KEEP_DIR:-}" ]&&for f in "$d"/screenshot-*.ppm;do cp "$f" "$KEEP_DIR/run1-$(basename "$f")";done||true
rm -f "$d"/screenshot-*.ppm
timeout 60 artifacts/emu-max --headless --data "$d" --air off --script "$d/script2.emu" < /dev/null > "$d/log2.txt" 2>&1||true
grep -aE '^NOTE shopping ' "$d/log2.txt"|grep -q 'title="shopping" done=1 total=2'||fail "note after restart"
grep -aq 'NOTES_PIN ok=1 pin=shopping' "$d/log2.txt"||fail "pin"
grep -aE '^HOME_MAIN ' "$d/log2.txt"|grep -q 'note=[1-9]'||fail "Home note empty"
python3 -c "import sys;t=open('$d/nvs.json').read();sys.exit(0 if 'mmnotes' in t else 1)"||fail "settings not saved to NVS"
shots=($(ls "$d"/screenshot-*.ppm|sort -V));[ ${#shots[@]} = 2 ]||fail "screenshots of the second run"
cmp -s "${shots[0]}" "${shots[1]}"&&fail "list and Home look the same"
grep -aE 'Guru|abort\(\)|Backtrace' "$d"/log*.txt&&fail "crash"
[ -n "${KEEP_DIR:-}" ]&&cp "$d"/screenshot-*.ppm "$KEEP_DIR/"||true
echo "PASS notes_max"
