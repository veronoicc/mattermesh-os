"""Helpers for emulator tests that drive two firmware nodes through their companion servers (TCP over the virtual Wi-Fi)."""
import struct, sys, time
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from companion_client import Companion

def rq(c, *payload):
    """One command, its answer (pushes are skipped)."""
    c.send(bytes(payload))
    while True:
        f = c.recv(10)
        assert f is not None, "no answer to opcode %d" % payload[0]
        if f[0] < 0x80:
            return f

def connect(port):
    c = Companion("127.0.0.1", port)
    assert rq(c, 22, 3)[0] == 13
    return c

def own_key(c):
    r = rq(c, 1, 3, 0, 0, 0, 0, 0, 0, 0)
    assert r[0] == 5
    return r[4:36]

def add_contact(c, key, name, typ, flags):
    """CMD_ADD_UPDATE_CONTACT: adds the contact, or changes its type and flags when it is known."""
    f = bytes([9]) + key + bytes([typ, flags, 255]) + bytes(64) + name.encode().ljust(32, b"\0") + struct.pack("<I", int(time.time()))
    assert rq(c, *f) == bytes([0])

def contact_flags(c, key):
    r = rq(c, 30, *key)
    assert r[0] == 3, r.hex()
    return r[34]

def send_text(c, key, txt_type, text):
    r = rq(c, 2, txt_type, 0, *struct.pack("<I", int(time.time())), *key[:6], *text.encode())
    assert r[0] == 6, r.hex()

def next_message(c, wait=40):
    """The next queued message of the app (RESP_CONTACT_MSG_RECV_V3) as (sender key prefix, txt_type, text), or None."""
    end = time.time() + wait
    while time.time() < end:
        r = rq(c, 10)
        if r[0] == 16:
            return r[4:10], r[11], r[16:].decode()
        time.sleep(1)
    return None
