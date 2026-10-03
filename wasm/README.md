# PackingSolver in the browser

PackingSolver compiled to WebAssembly with [Emscripten](https://emscripten.org/), and a web page to solve instances in the browser. The computation runs on the user's machine.

## Build

```shell
emcmake cmake -S . -B build_wasm -DCMAKE_BUILD_TYPE=Release -DPACKINGSOLVER_BUILD_WASM=ON
cmake --build build_wasm --target PackingSolver_wasm
```

This builds `build_wasm/wasm/packingsolver.js` and `packingsolver.wasm`.

## Contents

- `src/packingsolver_wasm.cpp`: the JavaScript API of the module: `solve(problemType, instanceJson, parametersJson)`, which blocks, and `Session`, which doesn't. See the comment at the top of the file.
- `js/packingsolver_worker.js`: a Web Worker running sessions and posting the solutions to the page.
- `web/`: the web page.
  - `visualize/`: JavaScript ports of the plotly visualizers of `python/packingsolver/visualize/`.
- `tests/`: the tests.

## Tests

```shell
node --test wasm/tests/*.test.js wasm/tests/*.test.mjs
```

The visualizers are compared with the figures of the Python ones. These are stored in `tests/fixtures/visualize/`, and generated with plotly installed:

```shell
python3 wasm/tests/generate_visualize_fixtures.py [type ...]
```

## Web page

The threads of the module need `SharedArrayBuffer`. So the page must be served with the headers `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`. The development server sets them:

```shell
python3 wasm/web/serve.py
```

Then open http://localhost:8000/.

## Deployment

The workflow `.github/workflows/wasm.yml` builds the module and the page and runs the tests. Every time the Build workflow succeeds on `master`, it also deploys the page to [Cloudflare Pages](https://pages.cloudflare.com/), which serves `web/_headers`. GitHub Pages can't set these headers.

The deployment requires:
- a Cloudflare Pages project created for direct upload, named `packingsolver`, or set the repository variable `CLOUDFLARE_PAGES_PROJECT`;
- the repository secrets `CLOUDFLARE_API_TOKEN` (an API token with the "Cloudflare Pages: Edit" permission) and `CLOUDFLARE_ACCOUNT_ID`.
