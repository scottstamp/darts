import os
import sys
import time
import socket

def resolve_host(host):
    try:
        return socket.gethostbyname(host)
    except socket.gaierror:
        return host

def flash_ota(bin_path, target_host):
    if not os.path.exists(bin_path):
        print(f"Error: Binary file '{bin_path}' not found! Build the project first.")
        sys.exit(1)

    ip_or_host = resolve_host(target_host)
    total_size = os.path.getsize(bin_path)

    print(f"\nStarting Fast OTA Upload: {bin_path} ({total_size / (1024 * 1024):.2f} MB) -> {target_host} ({ip_or_host})")

    port = 80
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Disable Nagle's algorithm for instant TCP throughput
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
    sock.settimeout(15.0)

    try:
        sock.connect((ip_or_host, port))
    except Exception as e:
        print(f"Connection failed to {target_host} ({ip_or_host}): {e}")
        sys.exit(1)

    # HTTP POST Header
    header = (
        f"POST /update HTTP/1.1\r\n"
        f"Host: {target_host}\r\n"
        f"User-Agent: Fast-ESP-OTA\r\n"
        f"Content-Type: application/octet-stream\r\n"
        f"Content-Length: {total_size}\r\n"
        f"Connection: close\r\n\r\n"
    )
    sock.sendall(header.encode('ascii'))

    uploaded_bytes = 0
    start_time = time.time()
    chunk_size = 32768  # 32 KB blocks

    if hasattr(sys.stdout, 'reconfigure'):
        try:
            sys.stdout.reconfigure(encoding='utf-8')
        except Exception:
            pass

    with open(bin_path, 'rb') as f:
        while uploaded_bytes < total_size:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            sock.sendall(chunk)
            uploaded_bytes += len(chunk)

            pct = (uploaded_bytes / total_size) * 100
            bar_len = 30
            filled = int(bar_len * uploaded_bytes // total_size)
            bar = '=' * filled + '-' * (bar_len - filled)
            elapsed = time.time() - start_time
            speed = (uploaded_bytes / (1024 * 1024)) / elapsed if elapsed > 0 else 0
            sys.stdout.write(f"\rUploading: [{bar}] {pct:5.1f}% ({uploaded_bytes / (1024 * 1024):.2f}/{total_size / (1024 * 1024):.2f} MB) @ {speed:.2f} MB/s")
            sys.stdout.flush()

    sys.stdout.write("\nFinishing update...\n")
    response = b""
    try:
        while True:
            data = sock.recv(1024)
            if not data:
                break
            response += data
    except socket.timeout:
        pass
    sock.close()

    resp_str = response.decode('utf-8', errors='ignore')
    if "200 OK" in resp_str or "OK" in resp_str or len(resp_str) == 0:
        print("--------------------------------------------------")
        print("OTA Update Successful! Scoreboard is rebooting...")
        print("--------------------------------------------------\n")
    else:
        print(f"OTA Server Response: {resp_str}")

if __name__ == '__main__':
    host = sys.argv[1] if len(sys.argv) > 1 else "darts-scoreboard.local"
    bin_file = sys.argv[2] if len(sys.argv) > 2 else os.path.join("build", "DartsScoreboard.bin")
    flash_ota(bin_file, host)
