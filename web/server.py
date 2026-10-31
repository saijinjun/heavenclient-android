"""
Simple HTTP server for serving MapleStory WASM client files.

This server provides the necessary CORS and COOP/COEP headers for SharedArrayBuffer
support, which is required for the WASM client with threading.

For the WASM client to connect to the MapleStory server, you also need to run
the WebSocket-to-TCP proxy:
    python web/ws_proxy.py

The proxy bridges WebSocket connections from the browser to TCP connections
to the actual MapleStory server.
"""

import http.server
import posixpath
import socketserver
import os
from urllib.parse import unquote, urlsplit

PORT = 8000
# Serve from parent directory (project root) to access assets/
DIRECTORY = os.path.join(os.path.dirname(__file__), "..")
# Only what the client needs is served: the page and its config, the built
# client, and the game files (which the Android app also downloads from here).
# Everything else under the repo root, such as .git/, wz/ or local compose
# overrides, stays private.
SERVED_PREFIXES = ("/web/", "/build/", "/assets/")
ENTRY_PAGE = "/web/index.html"
# Large .nx files are streamed in pieces instead of being read into memory.
COPY_CHUNK = 1024 * 1024

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        super().end_headers()

    def check_path(self):
        """Redirect / to the entry page and refuse anything outside SERVED_PREFIXES."""
        path = posixpath.normpath(unquote(urlsplit(self.path).path))
        if path in ("/", "/web"):
            self.send_response(302)
            self.send_header("Location", ENTRY_PAGE)
            self.end_headers()
            return False
        if not path.startswith(SERVED_PREFIXES):
            self.send_error(404, "File not found")
            return False
        return True

    def list_directory(self, path):
        # No directory listings: they would reveal file names outside the client.
        self.send_error(404, "File not found")
        return None

    def do_GET(self):
        """Serve a GET request."""
        if not self.check_path():
            return
        range_header = self.headers.get("Range")
        if range_header:
            self.send_range_response(range_header)
        else:
            super().do_GET()

    def send_range_response(self, range_header):
        path = self.translate_path(self.path)
        if not os.path.exists(path) or os.path.isdir(path):
            self.send_error(404, "File not found")
            return

        file_size = os.path.getsize(path)
        try:
            start, end = self.parse_range_header(range_header, file_size)
        except ValueError:
            self.send_error(416, "Range Not Satisfiable")
            self.send_header("Content-Range", f"bytes */{file_size}")
            self.end_headers()
            return

        chunk_size = end - start + 1

        self.send_response(206)
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Content-Range", f"bytes {start}-{end}/{file_size}")
        self.send_header("Content-Length", str(chunk_size))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()

        with open(path, "rb") as f:
            f.seek(start)
            remaining = chunk_size
            while remaining > 0:
                data = f.read(min(COPY_CHUNK, remaining))
                if not data:
                    break
                self.wfile.write(data)
                remaining -= len(data)

        # Log Range request for LazyFS debugging
        chunk_mb = chunk_size / (1024 * 1024)
        print(f"[Range] {self.path} - bytes {start}-{end}/{file_size} ({chunk_mb:.2f} MB)")

    def parse_range_header(self, range_header, file_size):
        if not range_header.startswith("bytes="):
            raise ValueError("Invalid Range header")

        ranges = range_header.replace("bytes=", "").split("-")
        if len(ranges) != 2:
             raise ValueError("Invalid Range header format")

        start_str, end_str = ranges

        if start_str and end_str:
            start = int(start_str)
            end = int(end_str)
        elif start_str:
            start = int(start_str)
            end = file_size - 1
        elif end_str:
             # suffix byte range: bytes=-500 means last 500 bytes
            end = file_size - 1
            start = file_size - int(end_str)
        else:
            raise ValueError("Invalid Range header values")

        if start < 0: start = 0
        if end >= file_size: end = file_size - 1
        if start > end:
             raise ValueError("Invalid Range header range")

        return start, end

    def do_HEAD(self):
        """Handle HEAD requests for file metadata."""
        if not self.check_path():
            return
        super().do_HEAD()
        path = self.translate_path(self.path)
        if os.path.exists(path) and not os.path.isdir(path):
            file_size = os.path.getsize(path)
            size_mb = file_size / (1024 * 1024)
            print(f"[HEAD] {self.path} - {file_size} bytes ({size_mb:.2f} MB)")

if __name__ == "__main__":
    # Allow address reuse to avoid "Address already in use" errors on restart
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    # One thread per connection, so a phone downloading game files does not
    # block the page for everyone else.
    socketserver.ThreadingTCPServer.daemon_threads = True
    with socketserver.ThreadingTCPServer(("", PORT), Handler) as httpd:
        print("=" * 60)
        print("MapleStory WASM Server with HTTP Range Request Support")
        print("=" * 60)
        print(f"Serving at http://localhost:{PORT}{ENTRY_PAGE}")
        print(f"Directory: {os.path.abspath(DIRECTORY)}")
        print()
        print("Features:")
        print("  ✓ HTTP Range requests (206 Partial Content)")
        print("  ✓ CORS headers for cross-origin access")
        print("  ✓ COOP/COEP headers for SharedArrayBuffer")
        print("  ✓ LazyFS asset loading support")
        print()
        print("Press Ctrl+C to stop the server.")
        print("=" * 60)
        print()
        httpd.serve_forever()
