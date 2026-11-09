#!/usr/bin/env python3
"""Companion protocol v14 dispatch check against the emulator: every MeshCore opcode must reach its handler in
src/mesh/companion.inc (an unknown opcode answers {1,1} ERR_UNSUPPORTED), the values that are state (tuning, flood
scope) must round-trip, and mode "reset" sends the factory reset as its only command.
Usage: companion_v14_client.py HOST PORT dispatch|reset"""
import struct, sys
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from companion_client import Companion

UNSUPPORTED = bytes([1, 1])

def main():
    c = Companion(sys.argv[1], int(sys.argv[2]))
    def rq(*payload):
        c.send(bytes(payload))
        while True:
            f = c.recv(5)
            assert f is not None, "no answer to opcode %d" % payload[0]
            if f[0] < 0x80:
                return f
    assert rq(22, 3)[0] == 13
    if sys.argv[3] == "reset":
        c.send(bytes([51]) + b"reset")
        return
    assert rq(1, 3, 0, 0, 0, 0, 0, 0, 0)[0] == 5
    # negative control: the check must be able to see an unhandled opcode
    assert rq(67) == UNSUPPORTED, "opcode 67 is outside v14 and must stay unsupported"
    r = rq(43); assert r[0] == 23 and struct.unpack("<II", r[1:9]) == (0, 9000), r.hex()
    assert rq(21, *struct.pack("<II", 500, 4000))[0] == 0
    r = rq(43); assert struct.unpack("<II", r[1:9]) == (500, 4000)
    rq(21, *struct.pack("<II", 0, 9000))
    for t in (0, 1, 2):
        r = rq(56, t); assert r[0] == 24 and r[1] == t, r.hex()
    assert rq(40) == b"\x15gps:1"  # the emulated Max has its GPS, on by default
    assert rq(41, *b"gps:0") == bytes([0]) and rq(40) == b"\x15gps:0"
    assert rq(41, *b"gps:1") == bytes([0]) and rq(40) == b"\x15gps:1"
    assert rq(41, *b"foo:1") == bytes([1, 6])
    assert b"Unknown command" in rq(66, *b"xyzzy") and rq(66, *b"get name")[0] == 29
    def cli(text):
        r = rq(66, *text.encode()); assert r[0] == 29, r.hex()
        return r[1:].decode()
    # the app's "xx|" prefix is reflected first; the Wi-Fi CLI answers like stock (the emulator is joined to EMU-WiFi, app wifi on)
    assert cli("01|get wifi.enabled") == "01|> 1"
    assert cli("02|get wifi.status") == "02|> connected"
    assert cli("03|get wifi.ssid") == "03|> EMU-WiFi"
    ip = cli("04|get wifi.ip"); assert ip.startswith("04|> ") and "not connected" not in ip and ip.count(".") == 3, ip
    assert cli("01|get buzzer") == "01|Unknown command"
    assert cli("05|set wifi.enabled 1") == "05|> wifi.enabled is now 1"
    assert cli("06|set wifi.ssid Foo Net") == "06|> wifi.ssid is now Foo Net (set wifi.pwd too)"
    assert cli("07|get wifi.ssid") == "07|> Foo Net"
    assert cli("08|set wifi.clear") == "08|> wifi config cleared"
    assert cli("09|get wifi.ssid") == "09|> EMU-WiFi"
    assert cli("10|set wifi.pwd secretpw1") == "10|Error, set wifi.ssid first"
    assert cli("get wifi.status") == "> connected"
    assert rq(64) == bytes([28])
    key = bytes(range(1, 32)) + b"\x07"
    assert rq(63, *b"IT-TOS".ljust(31, b"\0"), *key)[0] == 0
    r = rq(64); assert r[0] == 28 and r[1:7] == b"IT-TOS", r.hex()
    rq(63); assert rq(64) == bytes([28])
    assert rq(28, *[9] * 32) == bytes([1, 2]), "no connection to an unknown contact"
    c.send(bytes([39, 0, 0, 0])); f = c.recv(5); assert f is not None and f[0] == 0x8B, "self telemetry push"
    r = rq(33); assert r[0] == 19
    rq(34, *b"hello"); r = rq(35); assert r[0] == 20 and len(r) == 65
    assert rq(23) == bytes([15]) and rq(24, *[0] * 64) == bytes([15]), "private key commands are off by default"
    assert rq(54, 1)[0] == 0 and rq(54, 0)[0] == 0
    # transmitting / remote commands: the contact is unknown, so they answer anything but "unsupported"
    for op, pl in ((26, [9] * 32 + list(b"pw")), (27, [9] * 32), (29, [9] * 32), (36, [1, 0, 0, 0, 2, 0, 0, 0, 0, 5]),
                   (50, [9] * 32 + [3]), (52, [0] + [9] * 32), (25, [0, 1, 2, 3, 4]), (62, [0, 255, 2, 0, 1]),
                   (42, [0] * 33), (65, [0, 1, 2, 3, 4]), (60, []), (11, list(struct.pack("<II", 869618, 62500)) + [8, 8, 0])):
        assert rq(op, *pl) != UNSUPPORTED, "opcode %d is not wired" % op
    # CLI text to an unknown contact: the type must be accepted (not found), not rejected as unsupported
    for typ in (1, 3):
        assert rq(2, typ, 0, 1, 0, 0, 0, *[9] * 6, *b"ver") == bytes([1, 2]), "txt type %d" % typ
    print("PASS: companion v14 dispatch - every opcode reaches its handler, tuning and flood scope round-trip, key commands off, CLI text types accepted")

if __name__ == "__main__":
    main()
