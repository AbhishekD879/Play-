# Play! Web performance lab

This branch is an isolated measurement surface for the Play! WebAssembly runtime. It is not the Game Hub production client, and no experiment should move into Game Hub without a repeatable before/after result.

## What the lab measures

- Delivered frames per second and estimated emulation speed
- Browser long-frame percentage
- WebAssembly heap size
- JIT blocks compiled and currently live
- JIT code size and compile time
- The active EE clock experiment

The **Export JSON** action records the live metrics, browser user agent, selected mode, and cross-origin-isolation state. Game files remain local and are never included in the export.

## Local setup

The Play! JavaScript and WebAssembly artifacts are generated build outputs and are intentionally not committed.

1. Build Play! with Emscripten, or copy a known-good local `Play.js` and `Play.wasm` pair into `public/`.
2. Install the browser-app dependencies with `npm ci`.
3. Run `npm start` from `js/play_browser`.
4. Verify that the status pill says **Runtime ready** and **Cross-origin isolated** before loading a legal local test image.

The development server sets COOP and COEP headers because the threaded WebAssembly runtime depends on cross-origin isolation.

## Benchmark protocol

Use the same browser build, hardware power mode, test image, save point, renderer, resolution, controller state, and 60-second gameplay segment for every comparison.

1. Restart the runtime and let the scene settle for 15 seconds.
2. Record a 60-second baseline in **Full clock** mode.
3. Repeat twice and use the median result.
4. Apply one optimization only.
5. Repeat the same three runs.
6. Export the JSON result for every run.
7. Reject changes that improve headline FPS while increasing stutter, breaking timing, corrupting graphics/audio, or changing game behavior.

## Experiment lanes

| Lane | Candidate | Required evidence |
|---|---|---|
| Scheduling | Main-thread yield points, audio-buffer sizing, frame pacing | FPS, long frames, audio underruns, input latency |
| WebAssembly | Memory growth policy, LTO, SIMD, exception mode | FPS, heap, startup time, browser compatibility |
| JIT | Block-cache limits, invalidation strategy, compilation batching | JIT metrics, long frames, correctness suite |
| Rendering | WebGL state reduction, texture uploads, resolution scaling | GPU time, FPS, visual parity |
| Recovery | Runtime watchdog and resumable local session | Recovery time, save integrity, repeated-failure behavior |

Automatic EE clock selection is a playability fallback, not a free performance win: lowering the emulated clock can change timing and must be validated per game.

## Promotion gate

An experiment can be proposed for Game Hub only when:

- It wins against the median baseline on at least one low-tier and one high-tier device.
- Legal homebrew and representative locally supplied titles pass boot, gameplay, save/load, audio, controller, and fullscreen checks.
- Cross-origin-isolated and unsupported-browser paths both fail safely.
- The optimization is behind a runtime capability or feature flag and can be rolled back independently.
