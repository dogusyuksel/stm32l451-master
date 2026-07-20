#!/usr/bin/env python3
import socket
import sys
import time
from xmodem import XMODEM1k  # ✅ 1024 byte paketler

if len(sys.argv) < 2:
    print("Usage: ./xmodem_send.py <file>")
    sys.exit(1)

filename = sys.argv[1]

def getc(size, timeout=3):
    sock.settimeout(timeout)
    try:
        data = sock.recv(size)
        return data
    except socket.timeout:
        return None

def putc(data, timeout=3):
    sock.settimeout(timeout)
    sock.sendall(data)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 1235))
print(f"Connected to localhost:1235")

try:
    with open(filename, 'rb') as f:
        # ✅ XMODEM1k = 1024 byte paketler
        modem = XMODEM1k(getc, putc)
        
        file_size = f.seek(0, 2)
        f.seek(0)
        print(f"Sending {filename} ({file_size} bytes)...")
        
        start = time.time()
        status = modem.send(f, retry=16, timeout=5)
        elapsed = time.time() - start
        
        if status:
            print(f"✓ Success! {file_size} bytes in {elapsed:.1f}s")
        else:
            print(f"✗ Failed")
except Exception as e:
    print(f"✗ Error: {e}")
finally:
    sock.close()
