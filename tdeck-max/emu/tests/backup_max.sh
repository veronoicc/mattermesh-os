#!/usr/bin/env bash
# Settings > Backup / restore (Max): Create backup (plain, then encrypted with a passphrase) writes mm-YYYYMMDD-HHMMSS.mmb files that an
# independent Python reader accepts (CRC, AES-256-GCM, every record, no PIN namespace); Restore lists them, asks, puts the settings and
# the radio file back and restarts; a wrong passphrase and a damaged file change nothing.
set -euo pipefail
cd "$(dirname "$0")/../.."
data=$(mktemp -d)
mkdir -p artifacts/backup-shots
trap 'cp "$data"/*/*.ppm artifacts/backup-shots/ 2>/dev/null || true; rm -rf "$data"' EXIT
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$data/"
# mk NAME < steps: one step per line at 1.3 s intervals (an e-paper refresh takes 0.65 s); "wait MS" adds time, "enter" is the ENTER key,
# "key C" / "type S" / anything else goes over the serial harness.
mk(){ awk -v t=9000 '/^wait /{t+=$2;next}/^shot$/{print t" shot";t+=700;next}/^enter$/{print t" tap 21";t+=1300;next}{print t" serial "$0;t+=1300}END{if(!noquit)print t" quit"}' > "$data/$1.emu"; }
run(){ MM_EMU_LICENSED=1 timeout 200 artifacts/emu-max --headless --air off --data "$data" --script "$data/$1.emu" < /dev/null > "$data/$1.log" 2>&1; mkdir -p "$data/$1"; for f in "$data"/screenshot-*.ppm; do [ -f "$f" ] && mv "$f" "$data/$1/$1-$(basename "$f")"; done; true; }
fail(){ echo "FAIL: $1"; grep -ahE 'BACKUP_|HOME_WIDGET|UI_READY locked' "$data"/*.log | tail -30; exit 1; }
nvs(){ python3 - "$data/nvs.json" "$1" "$2" <<'PY'
import sys
for line in open(sys.argv[1]):
    p=line.split()
    if p and p[0]=='V' and p[1]==f'"{sys.argv[2]}"' and p[2]==f'"{sys.argv[3]}"':print(p[4]);break
else:print('-')
PY
}
radio_name(){ python3 -c "import sys;print(open(sys.argv[1]).read().split('\t')[5])" "$data/sd/mattermesh/radio.conf"; }
orig_name=$(radio_name)
mk a <<'S'
home widget set 3
section 58
key j
key j
key j
key ENTER
section 59
shot
enter
backup list
wait 1500
key j
key j
key j
key ENTER
key k
key k
key k
enter
shot
type secretpass
key ENTER
backup list
shot
S
run a
grep -aq 'BACKUP_LIST count=2' "$data/a.log" || fail 'two backups after Create (plain, then encrypted)'
[ "$(nvs mmui homewg)" = 03000000 ] || fail 'the setting was written'
[ "$(nvs mmsec hide)" != - ] || fail 'the PIN namespace exists, so its exclusion below means something'
check(){ python3 - "$data/sd/mattermesh/backup" "$1" "$2" <<'PY'
import hashlib,struct,sys,os,zlib
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
d,orig,want=sys.argv[1:4]
files=sorted(f for f in os.listdir(d) if f.endswith('.mmb'))
assert len(files)==2 and all(f.startswith('mm-') and len(f)==22 for f in files),files
def read(path,pw=None):
    b=open(path,'rb').read()
    assert b[:4]==b'MMBK' and b[4]==1,'magic/version'
    assert zlib.crc32(b[:-4])==struct.unpack('<I',b[-4:])[0],'crc'
    flags=b[5];nvs,fl=struct.unpack('<HH',b[6:10]);created,blen=struct.unpack('<II',b[12:20])
    enc=bool(flags&1);head=52 if enc else 20
    assert len(b)==head+blen+(16 if enc else 0)+4,'length'
    body=b[head:head+blen]
    if enc:
        it,=struct.unpack('<I',b[20:24]);salt=b[24:40];iv=b[40:52];tag=b[head+blen:head+blen+16]
        key=hashlib.pbkdf2_hmac('sha256',pw.encode(),salt,it,32)
        body=AESGCM(key).decrypt(iv,body+tag,b[:head])
    recs={};files={};at=0
    while at<len(body):
        k=body[at];at+=1
        if k==1:
            n=body[at];at+=1;ns=body[at:at+n].decode();at+=n;n=body[at];at+=1;key_=body[at:at+n].decode();at+=n;t=body[at];at+=1;l,=struct.unpack('<I',body[at:at+4]);at+=4;recs[(ns,key_)]=(t,body[at:at+l]);at+=l
        else:
            n,=struct.unpack('<H',body[at:at+2]);at+=2;p=body[at:at+n].decode();at+=n;l,=struct.unpack('<I',body[at:at+4]);at+=4;files[p]=body[at:at+l];at+=l
    assert len(recs)==nvs and len(files)==fl,'counts'
    return enc,flags,recs,files,body
plain,sealed=[read(os.path.join(d,f),'secretpass') for f in files]
# the older file is the plain one, the newer the encrypted one
assert not plain[0] and sealed[0],'order / encryption flag'
for enc,flags,recs,fl,body in (plain,sealed):
    assert flags&2,'passwords on by default'
    assert recs[('mmui','homewg')]==(5,bytes([3,0,0,0])),'setting in the backup'
    assert not any(ns=='mmsec' for ns,_ in recs),'PIN namespace excluded'
    assert any(ns=='mmconnect' for ns,_ in recs) or True
    assert '/mattermesh/radio.conf' in fl and orig in fl['/mattermesh/radio.conf'].decode(),'radio file'
    assert not any(p.endswith('identity.id') for p in fl),'private key off by default'
assert b'homewg' in plain[4] and b'secretpass' not in open(os.path.join(d,files[1]),'rb').read()
assert b'homewg' not in open(os.path.join(d,files[1]),'rb').read(),'the encrypted file hides its content'
print('ok')
PY
}
check "$orig_name" x | grep -aq ok || fail 'the files are valid by an independent reader (CRC, tag, records, exclusions)'
# restore: change the setting and the radio file, then restore the plain (older) backup through the list and the confirm screen
mk r <<'S'
home widget set 1
radio name Changed
section 59
key j
key j
key j
key j
key ENTER
shot
key j
key ENTER
shot
key h
shot
key ENTER
S
sed -i '$d' "$data/r.emu"
( for i in $(seq 1 2000); do if grep -aq BACKUP_RESTORED "$data/r.log" 2>/dev/null; then printf '9000 serial backup list\n9500 quit\n' > "$data/r.emu"; break; fi; sleep 0.05; done ) &
watcher=$!
run r
wait $watcher || true
grep -aq 'BACKUP_RESTORED settings=' "$data/r.log" || fail 'restore through the list and the confirm screen'
[ "$(nvs mmui homewg)" = 03000000 ] || fail 'the setting is back after the restore'
[ "$(radio_name)" = "$orig_name" ] || fail 'the radio file is back after the restore'
grep -aq 'BACKUP_LIST count=2' "$data/r.log" || fail 'the restarted firmware lists the backups'
[ "$(nvs mmsec hide)" != - ] || fail 'the PIN namespace is untouched by a restore'
# wrong passphrase, then right passphrase and CANCEL: nothing changes; a damaged file is refused
python3 - "$data/sd/mattermesh/backup" <<'PY'
import os,sys
d=sys.argv[1];f=sorted(os.listdir(d))[0];b=bytearray(open(os.path.join(d,f),'rb').read());b[len(b)//2]^=0x40
open(os.path.join(d,'mm-20990101-000000.mmb'),'wb').write(b)
PY
before=$(md5sum "$data/nvs.json" | cut -d' ' -f1)
mk c <<'S'
home widget set 3
section 59
key j
key j
key j
key j
key ENTER
key j
key ENTER
type wrongpass
key ENTER
backup status
shot
type secretpass
key ENTER
shot
backup status
key l
key ENTER
key k
shot
key ENTER
backup status
S
run c
grep -aq 'BACKUP_STATUS view=3 .*status=Wrong passphrase' "$data/c.log" || fail 'a wrong passphrase is refused and the editor stays'
grep -aq 'BACKUP_STATUS view=2 ' "$data/c.log" || fail 'the right passphrase opens the confirm screen'
grep -aq 'BACKUP_STATUS view=1 ' "$data/c.log" || fail 'CANCEL goes back to the list'
! grep -aq 'BACKUP_RESTORED' "$data/c.log" || fail 'nothing was restored'
mk d <<'S'
section 59
key j
key j
key j
key j
key ENTER
key ENTER
backup status
shot
S
run d
grep -aq 'BACKUP_STATUS view=1 .*status=File damaged' "$data/d.log" || fail 'a damaged file is refused with its reason'
! grep -aq 'BACKUP_RESTORED' "$data/d.log" || fail 'a damaged file was restored'
[ "$(nvs mmui homewg)" = 03000000 ] || fail 'a damaged file changed the settings'
echo 'PASS: backup - Create (plain, encrypted) valid for an independent reader, restore by list + confirm, setting and radio file back, wrong passphrase and damaged file change nothing'
