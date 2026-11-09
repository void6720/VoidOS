import socket, time, sys, os

SOCK = '/tmp/vltel'
OUT = '/home/hatch/workspace/voidos/distro/shots'

class Mon:
    def __init__(self):
        self.s = socket.socket(socket.AF_UNIX)
        self.s.connect(SOCK)
        self.s.settimeout(3)
        try: self.s.recv(65536)
        except Exception: pass
    def cmd(self, c, w=0.5):
        self.s.sendall(c.encode() + b'\n')
        time.sleep(w)
        try:
            self.s.setblocking(False); self.s.recv(65536)
        except Exception: pass
        self.s.setblocking(True); self.s.settimeout(3)
    def dump(self, name):
        os.makedirs(OUT, exist_ok=True)
        p = f'{OUT}/{name}.ppm'
        self.cmd(f'screendump {p}', 1.2)
        from PIL import Image
        Image.open(p).save(f'{OUT}/{name}.png')
        return f'{OUT}/{name}.png'
    def key(self, k, w=0.4):
        self.cmd(f'sendkey {k}', w)
    def type(self, text, w=0.15):
        km = {' ': 'spc', '-': 'minus', '.': 'dot', ',': 'comma', '/': 'slash',
              '\\': 'backslash', ':': 'shift-semicolon', '_': 'shift-minus',
              '=': 'equal', '>': 'shift-dot', '<': 'shift-comma', '|': 'shift-backslash',
              '$': 'shift-4', '~': 'shift-grave', '&': 'shift-7', '"': 'shift-apostrophe',
              "'": 'apostrophe', '(': 'shift-9', ')': 'shift-0', '+': 'shift-equal',
              '?': 'shift-slash', '*': 'shift-8', '!': 'shift-1', '@': 'shift-2',
              '#': 'shift-3', '%': 'shift-5', '^': 'shift-6', ';': 'semicolon'}
        for ch in text:
            if ch.isupper():
                self.cmd(f'sendkey shift-{ch.lower()}', w)
            elif ch in km:
                self.cmd(f'sendkey {km[ch]}', w)
            elif ch.islower() or ch.isdigit():
                self.cmd(f'sendkey {ch}', w)
            else:
                raise ValueError(f'unmapped char {ch!r}')
    def close(self):
        self.s.close()

if __name__ == '__main__':
    m = Mon()
    action = sys.argv[1]
    if action == 'dump':
        print(m.dump(sys.argv[2]))
    elif action == 'vnc-password':
        m.cmd('change vnc password voidos', 0.8)
        print('vnc password set')
    elif action == 'settings':
        m.key('shift', 0.3)  # wake
        m.cmd('sendkey alt-f2', 1.2)
        m.type('xfce4-settings-manager')
        time.sleep(1.0)
        print(m.dump('appfinder'))
        m.key('ret', 2.5)
        print(m.dump('settings'))
    m.close()
