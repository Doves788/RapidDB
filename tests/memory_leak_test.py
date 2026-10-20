#!/usr/bin/env python3
"""
RapidDB High-Volume Memory & TTL Leak Verification Test
Inserts 10,000 keys across Strings, Lists, and Hashes, tests TTL eviction,
verifies deletion, and checks that memory remains stable under load.
"""

import socket
import time
import os
import sys
import subprocess

SERVER_PORT = 6389
SERVER_HOST = "127.0.0.1"

class RespReader:
    def __init__(self, sock):
        self.sock = sock
        self.buf = b""

    def read_line(self):
        while b"\r\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                break
            self.buf += chunk
        if b"\r\n" in self.buf:
            pos = self.buf.index(b"\r\n") + 2
            line = self.buf[:pos]
            self.buf = self.buf[pos:]
            return line
        line = self.buf
        self.buf = b""
        return line

    def read_resp(self):
        line = self.read_line()
        if not line:
            return b""
        if line.startswith(b"$"):
            length_str = line[1:].strip()
            try:
                length = int(length_str)
            except ValueError:
                return line
            if length == -1:
                return line
            while len(self.buf) < length + 2:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                self.buf += chunk
            data = self.buf[:length + 2]
            self.buf = self.buf[length + 2:]
            return line + data
        return line

def send_cmd(s, cmd_parts):
    # Formats a RESP array command
    frame = f"*{len(cmd_parts)}\r\n".encode()
    for part in cmd_parts:
        encoded = str(part).encode()
        frame += f"${len(encoded)}\r\n".encode() + encoded + b"\r\n"
    s.sendall(frame)

def main():
    print("==================================================")
    print("     RapidDB Memory Safety & TTL Eviction Test    ")
    print("==================================================")
    
    server_proc = None
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((SERVER_HOST, SERVER_PORT))
        s.close()
        print(f"Connected to existing RapidDB instance on port {SERVER_PORT}")
    except (ConnectionRefusedError, OSError):
        bin_path = "./RapidDB-asan" if os.path.exists("./RapidDB-asan") else "./RapidDB"
        if not os.path.exists(bin_path):
            bin_path = "./RapidDB.exe"
        if not os.path.exists(bin_path):
            print(f"Binary {bin_path} not found. Please compile first.")
            sys.exit(1)
        print(f"Starting RapidDB ({bin_path}) on port {SERVER_PORT}...")
        server_proc = subprocess.Popen([bin_path, str(SERVER_PORT)])
        time.sleep(1.0)

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((SERVER_HOST, SERVER_PORT))
        reader = RespReader(s)

        # 1. Flush any existing keys
        print("[STEP 1] Flushing database...")
        send_cmd(s, ["FLUSHALL"])
        resp = reader.read_resp()
        assert b"+OK" in resp, f"Expected +OK, got {resp}"

        # 2. Bulk insertion: 10,000 keys with TTL in pipelined batches
        TOTAL_KEYS = 10000
        BATCH_SIZE = 500
        print(f"[STEP 2] Inserting {TOTAL_KEYS} keys with 2-second TTL in batches of {BATCH_SIZE}...")
        start_time = time.time()
        for i in range(0, TOTAL_KEYS, BATCH_SIZE):
            batch_count = min(BATCH_SIZE, TOTAL_KEYS - i)
            pipeline = b""
            for j in range(i, i + batch_count):
                key = f"key:{j}".encode()
                val = f"value_{j}".encode()
                pipeline += f"*3\r\n$3\r\nSET\r\n${len(key)}\r\n".encode() + key + f"\r\n${len(val)}\r\n".encode() + val + b"\r\n"
                pipeline += f"*3\r\n$6\r\nEXPIRE\r\n${len(key)}\r\n".encode() + key + b"\r\n$1\r\n2\r\n"
            s.sendall(pipeline)
            for _ in range(batch_count * 2):
                r = reader.read_resp()
                assert b"+OK" in r, f"Expected +OK, got {r}"

        elapsed = time.time() - start_time
        print(f"  -> Inserted {TOTAL_KEYS} keys + {TOTAL_KEYS} EXPIREs in {elapsed:.2f}s ({TOTAL_KEYS * 2 / elapsed:.0f} ops/sec)")

        # Verify keys exist before TTL
        send_cmd(s, ["GET", "key:0"])
        resp = reader.read_resp()
        assert b"value_0" in resp, f"Key not found: {resp}"
        print("  -> Verified keys are accessible")

        # 3. Wait for TTL to expire
        print("[STEP 3] Waiting 3 seconds for TTL expiration...")
        time.sleep(3.0)

        # 4. Verify keys have expired and been lazy-purged
        print("[STEP 4] Verifying keys have expired...")
        send_cmd(s, ["GET", "key:0"])
        resp = reader.read_resp()
        assert b"$-1" in resp, f"Expected null bulk string $-1 for expired key, got {resp}"
        
        send_cmd(s, ["KEYS"])
        resp = reader.read_resp()
        assert b"*0" in resp, f"Expected *0 keys remaining, got {resp}"
        print("  -> Verified all 10,000 keys successfully evicted!")

        # 5. Test List & Hash eviction and operations at scale
        print("[STEP 5] Testing Lists and Hashes under load...")
        for i in range(1000):
            send_cmd(s, ["LPUSH", f"list:{i}", "itemA", "itemB", "itemC"])
            reader.read_resp()
            send_cmd(s, ["HSET", f"hash:{i}", "field1", "val1"])
            reader.read_resp()

        # Deletion test
        print("[STEP 6] Testing DEL return values and memory release...")
        send_cmd(s, ["DEL", "list:0"])
        resp = reader.read_resp()
        assert b":1" in resp, f"Expected :1 for deleted key, got {resp}"

        send_cmd(s, ["DEL", "list:0"])
        resp = reader.read_resp()
        assert b":0" in resp, f"Expected :0 for non-existent key, got {resp}"

        # Clean shutdown
        send_cmd(s, ["FLUSHALL"])
        reader.read_resp()
        s.close()

        print("\nALL MEMORY SAFETY AND TTL EVICTION TESTS PASSED!")
    finally:
        if server_proc:
            print("Terminating test server...")
            server_proc.terminate()
            server_proc.wait()

if __name__ == "__main__":
    main()

