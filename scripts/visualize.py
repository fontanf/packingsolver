"""Visualize a solution certificate written by the solvers ('--certificate').

Serves the solution viewer of the web page ('wasm/web/viewer.html') and the
certificate on a local web server, and opens the viewer in the browser. The
problem type is found from the certificate.

Usage:

    python3 scripts/visualize.py solution.csv [--port 0] [--no-browser]

The server runs until it is stopped (Ctrl+C). The browsers don't load the
modules of the viewer from 'file://' URLs, hence the server.
"""

import argparse
import functools
import http.server
import os
import urllib.parse
import webbrowser

WEB_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "wasm", "web")


class Handler(http.server.SimpleHTTPRequestHandler):

    # The URL path of the certificate, and its file.
    certificate_path = None
    certificate_file = None

    def translate_path(self, path):
        name = urllib.parse.unquote(path.split("?", 1)[0].split("#", 1)[0])
        if name == self.certificate_path:
            return self.certificate_file
        return super().translate_path(path)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, format, *args):
        pass


Handler.extensions_map = {
    **http.server.SimpleHTTPRequestHandler.extensions_map,
    ".js": "text/javascript",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("certificate", help="path to the certificate (CSV, or JSON for irregular)")
    parser.add_argument("--port", type=int, default=0, help="port of the server (default: a free port)")
    parser.add_argument("--no-browser", action="store_true", help="don't open the browser")
    args = parser.parse_args()

    certificate = os.path.abspath(args.certificate)
    if not os.path.isfile(certificate):
        parser.error(f"no file {args.certificate}")
    name = os.path.basename(certificate)
    Handler.certificate_path = "/certificate/" + name
    Handler.certificate_file = certificate

    handler = functools.partial(Handler, directory=os.path.normpath(WEB_DIR))
    with http.server.ThreadingHTTPServer(("localhost", args.port), handler) as server:
        url = "http://localhost:{}/viewer.html?certificate={}".format(
                server.server_address[1],
                urllib.parse.quote("/certificate/" + urllib.parse.quote(name)))
        print(f"Viewer of {args.certificate}: {url}", flush=True)
        print("Stop it with Ctrl+C.", flush=True)
        if not args.no_browser:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
