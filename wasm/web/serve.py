"""Development server of the PackingSolver web page.

Serves 'wasm/web/', the Web Worker ('wasm/js/packingsolver_worker.js'), the
WebAssembly module ('<build directory>/wasm/packingsolver.js' and '.wasm')
and the examples of solutions ('img/<problem type>.png'), with the headers
that enable 'SharedArrayBuffer' (required by the threads of the module):

    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp

Usage:

    python3 wasm/web/serve.py [--port 8000] [--build-dir build_wasm]
"""

import argparse
import functools
import http.server
import os

WEB_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(WEB_DIR, "..", "..")


class Handler(http.server.SimpleHTTPRequestHandler):

    # Files served from outside 'wasm/web/'.
    extra_files = {}

    def translate_path(self, path):
        name = path.split("?", 1)[0].split("#", 1)[0].lstrip("/")
        if name in self.extra_files:
            return self.extra_files[name]
        return super().translate_path(path)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


Handler.extensions_map = {
    **http.server.SimpleHTTPRequestHandler.extensions_map,
    ".js": "text/javascript",
    ".wasm": "application/wasm",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument(
            "--build-dir",
            default=os.path.join(ROOT_DIR, "build_wasm"),
            help="build directory of the WebAssembly module")
    args = parser.parse_args()

    module_dir = os.path.join(args.build_dir, "wasm")
    Handler.extra_files = {
        "packingsolver.js": os.path.join(module_dir, "packingsolver.js"),
        "packingsolver.wasm": os.path.join(module_dir, "packingsolver.wasm"),
        "packingsolver_worker.js": os.path.join(
            ROOT_DIR, "wasm", "js", "packingsolver_worker.js"),
    }
    # The examples of solutions of the README.
    for problem_type in [
            "rectangleguillotine", "rectangle", "box", "boxstacks",
            "onedimensional", "irregular"]:
        Handler.extra_files["img/" + problem_type + ".png"] = os.path.join(
                ROOT_DIR, "img", problem_type + ".png")
    for path in Handler.extra_files.values():
        if not os.path.exists(path):
            print("warning: missing " + os.path.normpath(path))

    handler = functools.partial(Handler, directory=WEB_DIR)
    with http.server.ThreadingHTTPServer(("localhost", args.port), handler) as server:
        print(f"Serving on http://localhost:{args.port}/")
        server.serve_forever()


if __name__ == "__main__":
    main()
