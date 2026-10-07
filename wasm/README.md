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
  - `viewer/`: the visualizer of the solutions (rectangleguillotine, rectangle, onedimensional, irregular): an overview of all the bins, with their numbers of copies, and a view of one bin below it.
  - `viewer.html`: the solution viewer, a page which shows a certificate of the solvers (see below).
  - `visualize/`: JavaScript ports of the plotly visualizers of `python/packingsolver/visualize/` (used for box and boxstacks).
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

## Solution viewer

`web/viewer.html` shows a certificate written by the solvers (`--certificate`): opened or dropped on the page, or given by its URL (`viewer.html?certificate=<URL>`). The problem type is found from the certificate. To open a certificate from the command line:

```shell
python3 scripts/visualize.py solution.csv
```

It serves the page and the certificate on a local server (the browsers don't load the modules of the page from `file://` URLs) and opens the browser.

## Deployment

The workflow `.github/workflows/wasm.yml` builds the module and the page and runs the tests. Every time the Build workflow succeeds on `master`, it also deploys the page to [Cloudflare Pages](https://pages.cloudflare.com/), which serves `web/_headers`. GitHub Pages can't set these headers.

The deployment requires:
- a Cloudflare Pages project created for direct upload, named `packingsolver`, or set the repository variable `CLOUDFLARE_PAGES_PROJECT`;
- the repository secrets `CLOUDFLARE_API_TOKEN` (an API token with the "Cloudflare Pages: Edit" permission) and `CLOUDFLARE_ACCOUNT_ID`.
