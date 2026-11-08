import os
import sys
import time
import socket

def resolve_host(host):
    try:
        return socket.gethostbyname(host)
    except socket.gaierror:
        return host

def load_token(cli_token):
    """OTA token from --token, the DARTS_OTA_TOKEN env var, or a .ota_token file
    in the repo root (git-ignored). Set the same token on the device with
    'ota token <secret>' over serial."""
    if cli_token:
        return cli_token
    env = os.environ.get("DARTS_OTA_TOKEN")
    if env:
        return env.strip()
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    path = os.path.join(repo_root, ".ota_token")
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            return f.read().strip()
    return None

def flash_ota(bin_path, target_host, token):
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
        f"X-OTA-Token: {token}\r\n"
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
    status_line = resp_str.split("\r\n", 1)[0]
    if " 403 " in status_line:
        print("Upload rejected: OTA token missing or wrong (see 'ota' serial command).")
        sys.exit(1)
    if " 200 " in status_line or len(resp_str) == 0:
        print("--------------------------------------------------")
        print("OTA Update Successful! Scoreboard is rebooting...")
        print("--------------------------------------------------\n")
    else:
        print(f"OTA Server Response: {resp_str}")

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description="Upload firmware to the scoreboard over Wi-Fi")
    parser.add_argument("host", nargs="?", default="darts-scoreboard.local")
    parser.add_argument("bin", nargs="?", default=os.path.join("build", "DartsScoreboard.bin"))
    parser.add_argument("--token", help="OTA token (else DARTS_OTA_TOKEN or .ota_token)")
    args = parser.parse_args()

    token = load_token(args.token)
    if not token:
        print("Error: no OTA token. Pass --token, set DARTS_OTA_TOKEN, or create .ota_token "
              "in the repo root. Set it on the device with 'ota token <secret>'.")
        sys.exit(1)
    flash_ota(args.bin, args.host, token)
