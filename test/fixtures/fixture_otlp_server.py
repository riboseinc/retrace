#!/usr/bin/env python3
"""
Fixture HTTP server for otlp-c live streaming tests.

Listens on a port, accepts POST /v1/traces, /v1/metrics, /v1/logs,
records requests to a file. Returns 200 always (no PartialSuccess
by default).

Usage:
    fixture_otlp_server.py <port> <output_file>
"""
import http.server
import json
import socketserver
import sys
import time


class OtlpHandler(http.server.BaseHTTPRequestHandler):
    out_path = None
    request_count = 0

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)
        OtlpHandler.request_count += 1

        # Record the request as a base64-encoded line so we can
        # inspect it later. The fixture doesn't decode protobuf
        # -- otlp-c's correctness is proven by the property tests;
        # this is a presence + path check.
        record = {
            "ts": time.time(),
            "path": self.path,
            "content_type": self.headers.get("Content-Type", ""),
            "len": length,
        }
        with open(OtlpHandler.out_path, "ab") as f:
            f.write((json.dumps(record) + "\n").encode())

        self.send_response(200)
        self.send_header("Content-Type", "application/x-protobuf")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, format, *args):
        # Suppress default access logs.
        pass


class ReusableThreadingHTTPServer(socketserver.ThreadingMixIn,
                                  http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    if len(sys.argv) != 3:
        print("usage: fixture_otlp_server.py <port> <output_file>",
              file=sys.stderr)
        sys.exit(1)

    port = int(sys.argv[1])
    OtlpHandler.out_path = sys.argv[2]

    # Truncate the output file
    open(OtlpHandler.out_path, "wb").close()

    server = ReusableThreadingHTTPServer(("127.0.0.1", port), OtlpHandler)
    # flush=True: the integration test waits for this line as a
    # readiness handshake (piped stdout is block-buffered).
    print(f"fixture_otlp_server: listening on 127.0.0.1:{port} -> {OtlpHandler.out_path}",
          flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
