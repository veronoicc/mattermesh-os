# MatterMesh OS

An alternative firmware for the LilyGO **T-Deck Max**, built on [MeshCore](https://github.com/meshcore-dev/MeshCore):
off-grid LoRa chat with a real keyboard, designed for use in the field.

**Website and browser installer: [mattermeshos.com](https://mattermeshos.com)** — install it on your T-Deck from Chrome or Edge, no tools needed.

<table>
<tr>
<td align="center"><img src="docs/images/max-lock.png" width="280" alt="T-Deck Max lock screen"><br><b>T-Deck Max</b> · lock screen</td>
<td align="center"><img src="docs/images/max-chat.png" width="280" alt="T-Deck Max chat"><br>Chat in two languages</td>
</tr>
</table>

This repository holds the source of the **Free edition**, released under the GNU General Public License v3.0.

| Folder | Device | Display |
|---|---|---|
| [`tdeck-max/`](tdeck-max) | T-Deck Max (ESP32-S3, SX1262) | 240×320 e-paper, touch |

It speaks the MeshCore companion protocol, so it works with the MeshCore apps and with every other MeshCore node.

## What's in the Free edition

- Channel and direct chats, contacts, emoji, mentions and quick replies; wallpapers
- **Keyboard languages**: Italian, English, German, French, Spanish, Portuguese, Russian and Ukrainian (Settings → Keyboard languages, with search). Alt + Space switches between the enabled languages and the one in use shows in the status bar; the accent key, tapped before a letter, types the letters without a key of their own
- **Motion / compass** (BHI260AP, Settings → Motion / compass): raise to wake (three sensitivities, wakes the lights after the display timeout or the lock hint on the lock screen), heading arrow on the Maps position marker with an optional heading-up view and Home radar, accelerometer and heading in the node's telemetry (off by default). The board has no magnetometer: the heading comes from the gyro and is anchored to the GPS course while you walk, so it needs a moving fix first. The sensor firmware (100 KB) loads in the background after boot
- MeshCore companion over Bluetooth, USB and Wi-Fi
- Full MeshCore companion protocol v14: repeater login and admin console, status, telemetry, trace path (ping), path discovery, signing, flood scopes, stats and tuning, and client repeat mode (Settings → LoRa → Device). Private key import and export stay off unless built with `-D ENABLE_PRIVATE_KEY_EXPORT=1 -D ENABLE_PRIVATE_KEY_IMPORT=1`, as in MeshCore
- Apps: **Maps** (OpenStreetMap tiles with GPS), **Weather** (Open-Meteo), **Files**, **CLI**
- **GPS tracks** (Tracks app, Settings → GPS tracks): a breadcrumb recorded to `/mattermesh/tracks/` on the SD card (a point per interval and distance, crash-safe rows), distance / time / climb / top speed, GPX 1.1 export next to it (listed in Files), the track drawn in Maps, optional auto-start and GPS kept on while recording
- Offline dictation in English, German and Italian (hold MIC; the speech model of the language chosen in Settings → Sound / dictation is loaded from the SD card and is distributed separately, see [Offline dictation models](#offline-dictation-models))

## Build

Requirements: [PlatformIO Core](https://platformio.org/install/cli), git, a C++17 host compiler for the tests.

```sh
scripts/fetch-deps.sh                 # MeshCore companion-v1.18.0 and the LilyGO T-Deck-MAX libraries, pinned and patched
cd tdeck-max && pio run -e mattermesh-free          # T-Deck Max
```

Flash the app image at `0x10000` (for example `esptool.py write-flash 0x10000 .pio/build/<env>/firmware.bin`).
Host tests: `bash tools/test_release.sh` in `tdeck-max/`. The folder also contains a PC emulator (`emu/`, see its README).

The emoji fonts live once in `shared/fonts/` and are symlinked into the tree; they are only needed to regenerate
the emoji bitmaps (`tools/make_color_emoji.py`, `tools/build_emoji_assets.py`), not to build. On Windows clone with
`git clone -c core.symlinks=true` (or build on Linux/macOS).

Builds made from this repository have no update server configured: over-the-air updates are only offered by the
official images.

## Offline dictation models

Dictation runs on the device with a QuartzNet-15x5 CTC model per language, read from the SD card as `/mattermesh/asr-en.mmq`,
`/mattermesh/asr-de.mmq` or `/mattermesh/asr-it.mmq`. Settings → Sound / dictation → Dictation picks the language
(`< English | Deutsch | Italiano >`, tap the card or press `A` / `D`) and shows whether its file is on the card and how large it is.
The recording limit stays 6 s; one dictation needs about 2.9 MB of PSRAM (1.9 MB work area + two 0.5 MB model buffers) whatever the language.

Make the English and German models from NVIDIA's public NeMo checkpoints (about 70 MB download each, needs Python 3 on the host; the
export uses plain PyTorch, not the NeMo toolkit):

```sh
cd tdeck-max/tools/offline_asr
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt     # project-local environment
mkdir -p ../../artifacts/offline-asr && cd ../../artifacts/offline-asr
for l in en de; do curl -fSL -o stt_${l}_quartznet15x5.nemo \
  https://api.ngc.nvidia.com/v2/models/nvidia/nemo/stt_${l}_quartznet15x5/versions/1.0.0rc1/files/stt_${l}_quartznet15x5.nemo; done
for l in en de; do ../../tools/offline_asr/.venv/bin/python ../../tools/offline_asr/export_nemo.py stt_${l}_quartznet15x5.nemo asr-$l.mmq; done
```

Copy `asr-en.mmq` and `asr-de.mmq` to `/mattermesh/` on the SD card (about 15.3 MB each; `--bits 8` makes a 19.5 MB file that is
closer to the float model). The Italian model is installed the same way as before. Check the result on the host with
`tools/offline_asr/fetch_samples.py` (public speech clips) and `tools/offline_asr/compare.py` (float reference against the
firmware engine); `tools/test_release.sh` runs the engine on every `asr-*.mmq` it finds under `artifacts/offline-asr/`.
The checkpoints are NVIDIA's (NGC `nvidia/nemo`, [terms of use](https://ngc.nvidia.com/legal/terms)); trained on LibriSpeech,
Mozilla Common Voice and others, they are not re-distributed here. An optional flash copy of the Italian model (`asr_it` partition)
is ignored for the other languages.

## Support

MatterMesh OS is free and has no ads. If you find it useful, you can support it with a donation of any amount at
[ko-fi.com/mattermeshos](https://ko-fi.com/mattermeshos) — on the device: Settings → Donate.

## License

MatterMesh OS Free edition — GNU General Public License v3.0, see [LICENSE](LICENSE).
Third-party components keep their own licenses: see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

MatterMesh OS is an independent project, not affiliated with LilyGO or the MeshCore project.
