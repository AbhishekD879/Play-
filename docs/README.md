# Multitap fork of Play!

This branch (`multitap`) adds PS2 multitap emulation to jpd002/Play-, enabling up
to 8 controllers instead of 2.

* **SPEC.md** — what the feature is and why `MAX_PADS = 2` was correct to begin with
* **PATCHES.md** — every deviation from upstream, with collision risk per file
* **SYNC.md** — runbook for rebasing onto new upstream releases without losing multitap

The consumer is the AbhishekStation console portfolio, which ships the built
`Play.js`/`Play.wasm` in `public/play-mt/` and routes 3-6 player sessions to it,
leaving 1-2 player sessions on the unmodified upstream build.

Built with the upstream wasm recipe (`.github/workflows/build-js.yaml`):

    emcmake cmake --preset wasm-ninja
    cmake --build --preset wasm-ninja-release
