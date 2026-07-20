#!/usr/bin/env python3
import socket
import sys
import time
from xmodem import XMODEM

if len(sys.argv) < 2:
    print("Usage: ./xmodem_send.py <file>")
    sys.exit(1)

filename = sys.argv[1]

def getc(size, timeout=3):
    sock.settimeout(timeout)
    try:
        data = sock.recv(size)
        print(f"← {data.hex()}", end=' ')
        return data
    except socket.timeout:
        return None

def putc(data, timeout=3):
    sock.settimeout(timeout)
    print(f"→ {data.hex()}", end=' ')
    sock.sendall(data)
    time.sleep(0.1)  # 50ms delay - STM32'nin yetişmesi için

# Renode'a bağlan
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 1235))
print(f"Connected to localhost:1235")

# Xmodem transfer
try:
    with open(filename, 'rb') as f:
        modem = XMODEM(getc, putc)
        print(f"\nSending {filename}...")
        status = modem.send(f, retry=32, timeout=5, quiet=False)
        print(f"\n{'✓ Success' if status else '✗ Failed'}")
except Exception as e:
    print(f"\n✗ Error: {e}")
finally:
    sock.close()


