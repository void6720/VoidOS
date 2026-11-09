#!/usr/bin/env python3
"""Minimal RFB (VNC) input client for the VoidOS VM on 127.0.0.1:5901 (no auth).
Usage: vnc.py click X Y | key HEX | type TEXT | enter"""
import socket, struct, sys, time

HOST, PORT = '127.0.0.1', 5901

def recv_exact(s, n):
    buf = b''
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError('server closed')
        buf += chunk
    return buf

SHIFTED = {'!': '1', '@': '2', '#': '3', '$': '4', '%': '5', '^': '6',
           '&': '7', '*': '8', '(': '9', ')': '0', '_': '-', '+': '=',
           '|': '\\', ':': ';', '"': "'", '<': ',', '>': '.', '?': '/',
           '~': '`', '{': '[', '}': ']'}

class VNC:
    def __init__(self):
        self.s = socket.create_connection((HOST, PORT), timeout=10)
        ver = recv_exact(self.s, 12)
        assert ver.startswith(b'RFB'), ver
        self.s.sendall(b'RFB 003.008\n')
        n = recv_exact(self.s, 1)[0]
        types = recv_exact(self.s, n)
        if 1 not in types:
            raise RuntimeError(f'no none-auth offered, types={list(types)}')
        self.s.sendall(bytes([1]))
        self.s.sendall(bytes([1]))  # ClientInit: shared
        init = recv_exact(self.s, 24)
        self.w, self.h = struct.unpack('>HH', init[:4])
        name_len = struct.unpack('>I', init[20:24])[0]
        self.name = recv_exact(self.s, name_len).decode('utf-8', 'replace')

    def pointer(self, x, y, mask):
        self.s.sendall(struct.pack('>BBHH', 5, mask, x, y))

    def click(self, x, y):
        self.pointer(x, y, 0); time.sleep(0.4)
        self.pointer(x, y, 1); time.sleep(0.25)
        self.pointer(x, y, 0); time.sleep(0.4)

    def key_event(self, keysym, down):
        self.s.sendall(struct.pack('>BBHI', 4, 1 if down else 0, 0, keysym))

    def key(self, keysym):
        self.key_event(keysym, True); time.sleep(0.12)
        self.key_event(keysym, False); time.sleep(0.12)

    def type(self, text):
        for ch in text:
            if ch == '\n':
                self.key(0xff0d)
            elif ch.isupper():
                self.key_event(0xffe1, True)
                self.key(ord(ch.lower()))
                self.key_event(0xffe1, False)
            elif ch in SHIFTED:
                self.key_event(0xffe1, True)
                self.key(ord(SHIFTED[ch]))
                self.key_event(0xffe1, False)
            else:
                self.key(ord(ch))
            time.sleep(0.03)

    def close(self):
        self.s.close()

if __name__ == '__main__':
    v = VNC()
    cmd = sys.argv[1]
    if cmd == 'click':
        v.click(int(sys.argv[2]), int(sys.argv[3]))
    elif cmd == 'key':
        v.key(int(sys.argv[2], 16))
    elif cmd == 'type':
        v.type(sys.argv[2])
    elif cmd == 'enter':
        v.key(0xff0d)
    v.close()
    print('sent')
