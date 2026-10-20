#!/usr/bin/env python3
"""
RapidDB Chaos & Protocol Reliability Testing Suite
Validates that RapidDB gracefully handles malformed frames, partial TCP packets,
burst connections, huge payloads, and byte fuzzing without crashing.
"""

import socket
import time
import os
import sys
import subprocess
import random

SERVER_PORT = 6389  # Use a dedicated port for testing
SERVER_HOST = "127.0.0.1"

def send_raw(payload, timeout=2.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((SERVER_HOST, SERVER_PORT))
    s.sendall(payload)
    response = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            response += chunk
            if b"\r\n" in response:
                break
    except (socket.timeout, ConnectionResetError, BrokenPipeError):
        pass
    finally:
        s.close()
    return response

def test_ping():
    print("[TEST] Basic PING/PONG connectivity...")
    resp = send_raw(b"*1\r\n$4\r\nPING\r\n")
    assert b"+PONG" in resp, f"Expected +PONG, got {resp}"
    print("  -> PASSED")

def test_malformed_multibulk():
    print("[TEST] Malformed Multi-Bulk Lengths (*invalid, *-5, *99999999999999999999)...")
    payloads = [
        b"*abc\r\n$4\r\nPING\r\n",
        b"*-5\r\n$4\r\nPING\r\n",
        b"*99999999999999999999999999999999\r\n$4\r\nPING\r\n",
        b"*\r\n",
        b"*0\r\n",
    ]
    for p in payloads:
        resp = send_raw(p)
        print(f"     Sent payload {p[:20]!r}: response len = {len(resp)}")
    test_ping()
    print("  -> PASSED (Server survived all malformed multi-bulk frames)")

def test_malformed_bulk_string():
    print("[TEST] Malformed Bulk String Lengths ($xyz, $-2, giant length without data)...")
    payloads = [
        b"*2\r\n$xyz\r\nSET\r\n$1\r\nx\r\n",
        b"*2\r\n$-2\r\nSET\r\n$1\r\nx\r\n",
        b"*2\r\n$999999999999999999999999\r\nSET\r\n$1\r\nx\r\n",
        b"*1\r\n$5\r\nABC\r\n",  # length says 5, only 3 provided
    ]
    for p in payloads:
        send_raw(p, timeout=0.3)
    test_ping()
    print("  -> PASSED (Server survived all malformed bulk strings)")

def test_rapid_disconnects():
    print("[TEST] Rapid Client Connect & Abrupt Disconnect (100 sockets)...")
    for _ in range(100):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((SERVER_HOST, SERVER_PORT))
            if random.random() > 0.5:
                s.send(b"*1\r\n")  # partial bytes before abrupt drop
            s.close()
        except (ConnectionResetError, BrokenPipeError):
            pass
    # Confirm server still responds immediately
    test_ping()
    print("  -> PASSED (100 rapid disconnects handled gracefully)")

def test_pipelined_commands():
    print("[TEST] Pipelined Commands in a Single TCP Packet...")
    pipeline = b"*1\r\n$4\r\nPING\r\n" * 5
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((SERVER_HOST, SERVER_PORT))
    s.sendall(pipeline)
    response = b""
    while response.count(b"+PONG\r\n") < 5:
        chunk = s.recv(1024)
        if not chunk:
            break
        response += chunk
    s.close()
    assert response.count(b"+PONG\r\n") == 5, f"Expected 5 PONGs, got {response}"
    print("  -> PASSED (All 5 pipelined commands returned responses)")

def test_chunked_streaming():
    print("[TEST] Slowloris / Fragmented TCP stream (1 byte at a time)...")
    cmd = b"*3\r\n$3\r\nSET\r\n$8\r\nslow_key\r\n$10\r\nslow_value\r\n"
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((SERVER_HOST, SERVER_PORT))
    for byte in cmd:
        s.send(bytes([byte]))
        time.sleep(0.001)
    resp = s.recv(1024)
    s.close()
    assert b"+OK" in resp, f"Expected +OK, got {resp}"
    print("  -> PASSED (Fragmented multi-byte stream assembled and executed)")

def test_large_payload():
    print("[TEST] Large Payload Handling (1MB string SET & GET)...")
    key = "big_payload_test"
    val = "X" * (1024 * 1024)  # 1MB string
    
    # Send SET
    set_frame = f"*3\r\n$3\r\nSET\r\n${len(key)}\r\n{key}\r\n${len(val)}\r\n{val}\r\n".encode()
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((SERVER_HOST, SERVER_PORT))
    s.sendall(set_frame)
    resp = s.recv(1024)
    assert b"+OK" in resp, f"SET failed: {resp}"
    
    # Send GET
    get_frame = f"*2\r\n$3\r\nGET\r\n${len(key)}\r\n{key}\r\n".encode()
    s.sendall(get_frame)
    received = b""
    while len(received) < len(val):
        chunk = s.recv(65536)
        if not chunk:
            break
        received += chunk
    s.close()
    assert val.encode() in received, "Retrieved 1MB value does not match sent data!"
    print(f"  -> PASSED (1MB payload successfully stored and retrieved)")

def test_random_byte_fuzzing():
    print("[TEST] Random Byte Fuzzing (10 iterations of garbage bytes)...")
    for i in range(10):
        garbage = bytes(random.getrandbits(8) for _ in range(random.randint(16, 2048)))
        try:
            send_raw(garbage, timeout=0.2)
        except Exception:
            pass
    test_ping()
    print("  -> PASSED (Server survived all fuzzing iterations)")

def main():
    print("==================================================")
    print("      RapidDB Chaos & Reliability Test Suite      ")
    print("==================================================")
    
    # Ensure server is running or start it
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
        test_ping()
        test_malformed_multibulk()
        test_malformed_bulk_string()
        test_rapid_disconnects()
        test_pipelined_commands()
        test_chunked_streaming()
        test_large_payload()
        test_random_byte_fuzzing()
        print("\nALL CHAOS AND RELIABILITY TESTS PASSED!")
    finally:
        if server_proc:
            print("Terminating test server...")
            server_proc.terminate()
            server_proc.wait()

if __name__ == "__main__":
    main()
