#!/usr/bin/env bash
# Builds and runs a PC companion emulator (sim/<name>.cpp, default companion_emulator) on the real MeshCore core.
# Usage: tools/test_companion_emulator.sh [name ...]   (no name: every sim/companion*_emulator.cpp)
set -euo pipefail
cd "$(dirname "$0")/.."
C=.pio/libdeps/mattermesh-free/Crypto;M=meshcore-base;O=artifacts/companion-emulator-ed25519;mkdir -p $O
for f in $M/lib/ed25519/*.c;do gcc -c -O1 -w "$f" -o $O/$(basename "$f" .c).o;done
names=("$@");if [ ${#names[@]} -eq 0 ];then for f in sim/companion*_emulator.cpp;do n=$(basename "$f" .cpp);names+=("$n");done;fi
for n in "${names[@]}";do
g++ -std=c++17 -O1 -w -Isim/meshcore-host -Isrc -I$M/src -I$M/lib/ed25519 -I$C -I.pio/libdeps/mattermesh-free/base64/src \
 sim/$n.cpp $M/src/Mesh.cpp $M/src/Dispatcher.cpp $M/src/Packet.cpp $M/src/Identity.cpp $M/src/Utils.cpp \
 $M/src/helpers/BaseChatMesh.cpp $M/src/helpers/StaticPoolPacketManager.cpp $M/src/helpers/AdvertDataHelpers.cpp $M/src/helpers/TxtDataHelpers.cpp \
 $C/AES128.cpp $C/AESCommon.cpp $C/BlockCipher.cpp $C/Crypto.cpp $C/SHA256.cpp $C/SHA512.cpp $C/Hash.cpp $C/Ed25519.cpp $C/Curve25519.cpp $C/BigNumberUtil.cpp $C/RNG.cpp \
 $C/ChaCha.cpp $C/Cipher.cpp $O/*.o -o artifacts/$n
artifacts/$n
done
