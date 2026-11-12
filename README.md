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
- Apps: **Maps** (OpenStreetMap tiles with GPS), **Weather** (Open-Meteo), **Files**, **CLI**, and the field tools **RPT Scanner** (repeaters by SNR / last heard / distance, scan, ping, status, login and console), **RX Log** (every received packet with type, route, hops, RSSI/SNR, filter and CSV export) and **Noise Floor** (live RSSI chart against the calibrated floor, interference warning, CSV export)
- **Cellular** (A7682E or A7670G, Settings → Cellular): SIM PIN and APN, network routing (Wi-Fi only / Wi-Fi then cellular / cellular only) for Weather and map tiles, an MQTT uplink of received packets in the meshcoretomqtt format (broker, port, credentials, region, TLS) with an optional [downlink](#mqtt-downlink) (read and send chats remotely), and the **SMS** app (inbox, read, reply, compose, new-message banner)
- **GPS tracks** (Tracks app, Settings → GPS tracks): a breadcrumb recorded to `/mattermesh/tracks/` on the SD card (a point per interval and distance, crash-safe rows), distance / time / climb / top speed, GPX 1.1 export next to it (listed in Files), the track drawn in Maps, optional auto-start and GPS kept on while recording
- **Quick messages** (Settings → Quick messages): up to eight canned texts (edit, reorder, delete, reset to defaults) that a chat sends in one tap from Chat options → Quick messages, or by the Quick key (Alt + Q / Y / U on an empty composer, set in the same page, off by default). A message can add your GPS position (`GPS: lat, lon`, as Share position); without a fresh fix, or when text and position exceed the message limit, nothing is sent. Mode: send now or insert into the composer
- **Scheduled messages** (Chat options → Send later, and the Scheduled app): write the message in the composer, choose Send later and pick when: at a date and time (local time zone), when a repeater is heard, when the radio is idle, or when the contact is heard (direct chats). The Scheduled app lists what is waiting (edit, send now, delete) and keeps a History of the last eight sent or failed ones. Items are stored in `/mattermesh/scheduled.bin` and survive a reboot (a timed message more than 24 h overdue is reported as missed); they go through the normal send path, wait while the companion link or a full TX queue holds the radio, retry three times 30 s apart, and fail safely if the chat was removed or changed
- **Delivery history** (tap one of your messages → DELIVERY): a timestamped timeline of what happened to it (queued, transmitted, each repeater heard with its name, ACK with route and hops, no ACK, retry by flood, failure) with a one-line summary, and a RESEND button. Kept in the SD journal as extra `E1` rows, so older messages open with a history made from their stored status and heard list; the timeline of a message sent through the companion link only has what the companion reports
- **Security** (Settings → Security): a lock PIN of 4 to 8 digits (salted PBKDF2-SHA256 hash in NVS, never the PIN itself), asked after the two-tap unlock gesture on an on-screen 3 × 4 pad or the keyboard (digits, or the letters under them: `w e r` `s d f` `z x c` `$`). Five wrong tries cost 30 s, doubling with every further wrong PIN up to an hour; the count survives a restart and a restart always asks for the PIN. Choose how long after locking it is asked (immediately, 1, 5 or 15 min), hide message previews on the lock screen, and block USB data commands (`sd`, `ls`, `read`, `shot`, `cli`, the MeshCore app over USB, input injection) while locked. Changing or removing the PIN asks for the old one. With a PIN set the lock screen's Quick Reply is off. The MeshCore app's Bluetooth PIN is separate
- **Backup / restore** (Settings → Backup / restore): Create backup writes `/mattermesh/backup/mm-YYYYMMDD-HHMMSS.mmb` (versioned, CRC-32) with every NVS setting of the firmware and the contact, channel, route, radio and identity files. Options: include Wi-Fi / cellular / link passwords (on), include the private key (off; only in builds with `-D ENABLE_PRIVATE_KEY_EXPORT=1`), encrypt with a passphrase (AES-256-GCM, key from PBKDF2-SHA256). The lock PIN is never saved. Restore lists the files with date, size and contents, checks one completely (CRC, authentication tag, every record) before anything is written, shows what it replaces and restarts; a failed write puts everything back
- Offline dictation in English, German and Italian (hold MIC; the speech model of the language chosen in Settings → Sound / dictation is loaded from the SD card and is distributed separately, see [Offline dictation models](#offline-dictation-models))

## MQTT downlink

Settings → Cellular → MQTT DOWNLINK turns the uplink's broker session into a remote for the chats (Wi-Fi or cellular, same broker, region and TLS settings). Topics are under `meshcore/<REGION>/<PUBKEY>/` as for the packet uplink (`<PUBKEY>` upper-case hex):

| Topic | Direction | Content |
|---|---|---|
| `messages` | device → broker | every chat message the node can read (channels it has joined, DMs to it) and every message sent from the device, plus delivery updates (**Publish** switch) |
| `cmd` | broker → device | JSON commands (QoS 0) |
| `resp` | device → broker | one reply per command: `{"id":…,"ok":true,"result":…}` or `{"id":…,"ok":false,"error":"…"}` |

Settings: **Downlink** (master switch; off = nothing is subscribed or published), **Cmd token** (shared secret, 8–64 characters, shown masked), **Generate** (a random 24-character token, saved at once and shown only until you move off that row), **Allow send** (off: `send_*` and `advert` are refused, the read-only commands still work), **Publish** (the `messages` stream). Without a token every command is refused.

A command is one JSON object with a string `id` (up to 64 characters, echoed back), the `token` and a `cmd`:

```json
{"id":"1","token":"<your token>","cmd":"send_channel","channel":"Public","text":"hello"}
```

| `cmd` | Fields | Result |
|---|---|---|
| `send_channel` | `channel` (name, or slot number), `text` | `{"message_id","state"}` |
| `send_dm` | `to` (contact name, or at least 6 hex characters of its public key), `text` | `{"message_id","state"}` |
| `advert` | `position` (optional bool) | – |
| `ping` | – | name, pubkey, uptime, radio, counters |
| `list_channels`, `list_contacts` | – | `{"items":[…],"count","truncated"}` (contacts: first 40, 1.5 kB at most) |

Sends use the same path as the keyboard: the text limit is MeshCore's (160 bytes, less the sender name on channels), the message lands in the device's chat, journal and delivery tracking, and appears on `messages` with the `message_id` from the reply. At most 10 payloads per minute are accepted (good or bad); the token is compared in constant time.

A `messages` entry: `{"id":"0000002a","event":"message","ts":…,"kind":"channel"|"dm","channel","channel_index"|"contact","pubkey" (12 hex),"sender","text","direction":"in"|"out","hops","route","snr","rssi","state"}`; later `{"id":"0000002a","event":"delivery","ts":…,"state":"Delivered","repeaters","heard","first_repeater"}` entries follow the same `id`. SNR/RSSI are those of the last packet the radio decoded.

Anyone who can publish to `cmd` and knows the token can send as you: use a broker login and TLS, and keep Allow send off if you only want to read.

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
