#!/usr/bin/env python3
"""
WebSocket-to-TCP Proxy for MapleStory WASM Client

This proxy bridges WebSocket connections from the browser to TCP connections
to the actual MapleStory server. It's necessary because browsers cannot make
direct TCP connections.

Usage:
    python ws_proxy.py [--ws-port WS_PORT] [--tcp-host TCP_HOST] [--tcp-port TCP_PORT]

Default configuration:
    - WebSocket listens on: ws://0.0.0.0:8080
    - TCP connects to: 127.0.0.1:8484

Target allowlist:
    The browser names the host:port it wants, so without a limit the proxy
    relays to anything this machine can reach. Set WS_PROXY_ALLOWED_TARGETS to
    a comma-separated list of host:port or host:lo-hi entries, e.g.
        WS_PROXY_ALLOWED_TARGETS=100.91.238.34:8484,100.91.238.34:7575-7577
    and every other target is refused. Unset keeps the open behavior for local
    development and prints a warning at startup.
"""

import asyncio
import websockets
import argparse
import sys
import os


def parse_allowed_targets(raw):
    """Parse WS_PROXY_ALLOWED_TARGETS into {host: [(lo, hi), ...]}, or None when unset."""
    if raw is None or not raw.strip():
        return None
    allowed = {}
    for entry in raw.split(","):
        entry = entry.strip()
        if not entry:
            continue
        # rpartition so the port split is on the last colon only.
        host, _, ports = entry.rpartition(":")
        if not host or not ports:
            raise ValueError(f"Bad allowlist entry '{entry}', expected host:port or host:lo-hi")
        lo, _, hi = ports.partition("-")
        lo = int(lo)
        hi = int(hi) if hi else lo
        if lo > hi:
            raise ValueError(f"Bad port range in allowlist entry '{entry}'")
        allowed.setdefault(host.strip().lower(), []).append((lo, hi))
    # A set-but-empty list would silently allow nothing; treat it as a config error instead.
    if not allowed:
        raise ValueError("WS_PROXY_ALLOWED_TARGETS is set but has no entries")
    return allowed


def is_target_allowed(allowed, host, port):
    """True when no allowlist is configured or host:port matches an entry."""
    if allowed is None:
        return True
    return any(lo <= port <= hi for lo, hi in allowed.get(host.lower(), []))


class MapleStoryProxy:
    """Proxy that forwards data between WebSocket and TCP connections."""

    def __init__(self, allowed_targets=None):
        self.allowed_targets = allowed_targets

    async def handle_client(self, websocket):
        """Handle a WebSocket client connection."""
        client_addr = websocket.remote_address
        print(f"[WebSocket] Client connected from {client_addr}")

        tcp_reader = None
        tcp_writer = None

        try:
            # Read first message to get target TCP server address
            print(f"[WebSocket] Waiting for target address...")
            target_msg = await websocket.recv()
            
            if isinstance(target_msg, bytes):
                target_str = target_msg.decode('utf-8')
            else:
                target_str = target_msg
            
            # Parse target address (format: "host:port")
            try:
                tcp_host, tcp_port = target_str.strip().split(':')
                tcp_port = int(tcp_port)

                # If target is localhost/127.0.0.1 and we're in Docker, remap it to a routable host.
                # Browser "localhost" is the user's machine, but container "localhost" is the container itself.
                is_docker = os.environ.get("IS_DOCKER", "false").lower() == "true"
                if is_docker and tcp_host in ["localhost", "127.0.0.1", "0.0.0.0"]:
                    mapped_host = os.environ.get("WS_PROXY_LOCALHOST_TARGET", "host.docker.internal")
                    print(f"[Proxy] Redirecting {tcp_host} -> {mapped_host} (Docker localhost mapping)")
                    tcp_host = mapped_host

            except ValueError:
                print(f"[Error] Invalid target format '{target_str}', expected 'host:port'")
                await websocket.close()
                return
            
            print(f"[WebSocket] Client requested connection to {tcp_host}:{tcp_port}")

            # Checked after the Docker localhost remap so the allowlist names the
            # address the proxy actually dials, not what the browser typed.
            if not is_target_allowed(self.allowed_targets, tcp_host, tcp_port):
                print(f"[Proxy] Refused {tcp_host}:{tcp_port} (not in WS_PROXY_ALLOWED_TARGETS)")
                await websocket.close(code=1008, reason="target not allowed")
                return

            # Connect to target TCP server
            print(f"[TCP] Connecting to {tcp_host}:{tcp_port}")
            tcp_reader, tcp_writer = await asyncio.open_connection(tcp_host, tcp_port)
            print(f"[TCP] Connected to target server")

            # Create tasks for bidirectional forwarding
            ws_to_tcp_task = asyncio.create_task(
                self.forward_ws_to_tcp(websocket, tcp_writer)
            )
            tcp_to_ws_task = asyncio.create_task(
                self.forward_tcp_to_ws(tcp_reader, websocket)
            )

            # Wait for either direction to complete (usually means disconnect)
            done, pending = await asyncio.wait(
                [ws_to_tcp_task, tcp_to_ws_task],
                return_when=asyncio.FIRST_COMPLETED
            )

            # Cancel pending tasks
            for task in pending:
                task.cancel()
                try:
                    await task
                except asyncio.CancelledError:
                    pass

        except ConnectionRefusedError:
            print(f"[TCP] Connection refused to {tcp_host}:{tcp_port}")
            print("[TCP] Is the target server running?")
        except Exception as e:
            print(f"[Error] Exception in client handler: {e}")
        finally:
            # Clean up connections
            if tcp_writer:
                tcp_writer.close()
                await tcp_writer.wait_closed()
            print(f"[WebSocket] Client {client_addr} disconnected")

    async def forward_ws_to_tcp(self, websocket, tcp_writer):
        """Forward data from WebSocket to TCP."""
        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    # Binary message - forward to TCP
                    tcp_writer.write(message)
                    await tcp_writer.drain()
                    print(f"[WS→TCP] Forwarded {len(message)} bytes")
                else:
                    print(f"[WS→TCP] Warning: Received text message, expected binary")
        except websockets.exceptions.ConnectionClosed:
            print("[WS→TCP] WebSocket connection closed")
        except Exception as e:
            print(f"[WS→TCP] Error: {e}")

    async def forward_tcp_to_ws(self, tcp_reader, websocket):
        """Forward data from TCP to WebSocket."""
        try:
            while True:
                # Read data from TCP server
                data = await tcp_reader.read(8192 * 8)
                if not data:
                    print("[TCP→WS] TCP connection closed by server")
                    break

                # Send to WebSocket as binary
                await websocket.send(data)
                print(f"[TCP→WS] Forwarded {len(data)} bytes")

        except websockets.exceptions.ConnectionClosed:
            print("[TCP→WS] WebSocket connection closed")
        except Exception as e:
            print(f"[TCP→WS] Error: {e}")


async def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="WebSocket-to-TCP proxy for MapleStory WASM client"
    )
    parser.add_argument(
        "--ws-port",
        type=int,
        default=8080,
        help="WebSocket port to listen on (default: 8080)"
    )

    args = parser.parse_args()

    try:
        allowed_targets = parse_allowed_targets(os.environ.get("WS_PROXY_ALLOWED_TARGETS"))
    except ValueError as e:
        # Fail closed: a typo in the allowlist must not fall back to an open relay.
        print(f"[Error] {e}")
        sys.exit(1)

    # Create proxy instance
    proxy = MapleStoryProxy(allowed_targets)

    # Start WebSocket server
    print("=" * 60)
    print("Generic WebSocket-to-TCP Proxy")
    print("=" * 60)
    print(f"WebSocket server: ws://0.0.0.0:{args.ws_port}")
    print("Clients will specify target TCP server on connection")
    if allowed_targets is None:
        print("WARNING: WS_PROXY_ALLOWED_TARGETS is unset; any host:port is relayed")
    else:
        for host, ranges in allowed_targets.items():
            ports = ", ".join(str(lo) if lo == hi else f"{lo}-{hi}" for lo, hi in ranges)
            print(f"Allowed target: {host} ports {ports}")
    print("=" * 60)
    print("Waiting for connections...")

    async with websockets.serve(proxy.handle_client, "0.0.0.0", args.ws_port):
        await asyncio.Future()  # Run forever


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[Server] Shutting down...")
        sys.exit(0)
