# Fork Patch Set — what we add on top of upstream Play!

**Upstream:** https://github.com/jpd002/Play- · BSD-2-Clause · branch `master`
**Fork branch:** `multitap`
**Baseline commit:** `50aedca2639521bc498ace0b2be1ea012801a86a` (2026-07-11)
**Fork commit:** `25dacde` — 380 insertions, 30 deletions, 13 files
**Local clone:** `~/src/Play-` (branch `multitap`, `upstream` remote configured)

This is the register of every deviation from upstream. Keep it accurate — `SYNC.md`
depends on it being the single source of truth for "what is ours".

---

## Ground rules that keep syncing cheap

1. **Add files rather than edit them.** New behaviour goes in new files wherever the
   language allows. Every edited upstream line is a future conflict; every new file
   is not.
2. **Every edit inside an upstream file is fenced** with a sentinel so it is
   greppable and machine-reviewable:
   ```cpp
   //>>> PLAYSTATION-PORTFOLIO MULTITAP — begin
   ...our lines...
   //<<< PLAYSTATION-PORTFOLIO MULTITAP — end
   ```
   Invariant: `git grep -c "PLAYSTATION-PORTFOLIO MULTITAP"` must equal the count in
   the table below. If it drifts, a rebase silently dropped a hunk.
3. **One concern per commit**, in the order of the table. A rebase then fails at the
   smallest possible unit.
4. **Never reformat.** Upstream runs `clang-format` in CI (`check-format.yaml`).
   Touching whitespace on unrelated lines turns a 3-line patch into a 300-line
   conflict.
5. **Feature-flag everything** so the fork can build behaviourally identical to
   stock (SPEC FR-8). Implemented as a *runtime* switch (`Multitap::SetEnabled`)
   rather than a compile-time flag — one binary, testable both ways, and AC-2 is
   checkable in the browser.
6. **★ PRESERVE CRLF.** `Iop_PadMan.{cpp,h}` and `Iop_Sio2.{cpp,h}` are **CRLF**
   upstream; most other files are LF. Editing with a tool that normalises line
   endings rewrites every line and turns a 380-line patch into a 3,000-line one
   that conflicts with everything. This bit us on the first commit. Always check:
   ```bash
   git diff --stat --ignore-cr-at-eol <base>..HEAD   # the TRUE size
   ```
   If that number is much smaller than plain `--stat`, line endings got mangled.

---

## Collision-risk key

| Risk | Meaning |
|---|---|
| 🟢 **None** | New file. Cannot conflict. Rebase always clean. |
| 🟡 **Low** | Edit in a file upstream has not functionally changed in years. |
| 🟠 **Medium** | Edit in a file upstream touches occasionally. |
| 🔴 **High** | Edit in a file upstream touches often. Avoid; isolate hard. |

Risk is assigned from measured upstream churn (see `SYNC.md` §2), **not** from
intuition.

---

## The patch set

### P1 — Multitap presence · `Source/iop/Iop_MtapMan.{cpp,h}` · 🟡 Low

The blocker. `PortOpen` returns `0` (= failure per PS2SDK `libmtap.h`). Implement the
real surface so a game believes a tap is attached.

- `PortOpen(port)` → `1` when the port is enabled
- `PortClose(port)` → `1`
- `GetConnection(port)` → `1` if a tap is present
- `GetSlotNumber(port)` → `4` with a tap, `1` without
- Wire methods into `Invoke902` / `Invoke903` (currently empty `switch`es that only warn)
- Per-port enable flags, defaulted from the build flag

*Churn:* 6 commits all-time; last **functional** change 2016 ("Added MTAPMAN HLE
module"). Everything since is cosmetic (clang-format, `Warn`, `app_config`). This is
effectively abandoned code — the safest file in the repo to own.

---

### P2 — Slot-aware pad addressing · `Source/iop/Iop_PadMan.{cpp,h}` · 🟡 Low

Core of the feature.

- `uint32 m_padDataAddress[MAX_PADS]` → `[MAX_PORTS][MAX_SLOTS]` (2 × 4)
- `Open` / `SetMainMode` / `Close`: stop discarding `slot`; index `[port][slot]`
- `SetButtonState` / `SetAxisState`: take `(port, slot)` instead of a flat `padNumber`
- Keep `MAX_PADS = 2` **as `MAX_PORTS`** — renaming makes the hardware meaning explicit
  and makes any stray upstream use fail loudly at compile time rather than silently
  mis-index

⚠️ Do **not** simply raise `MAX_PADS` to 6. There is no third port; the axis is
`slot`. See SPEC §1.

*Churn:* `.cpp` 31 all-time / 1 since 2024 (mechanical). `.h` 26 all-time / **0 since
2024** (last touched 2023-10-31).

---

### P3 — Savestate schema · `Source/iop/Iop_PadMan.cpp` · 🟡 Low

`SaveState`/`LoadState` persist exactly two hardcoded keys, `pad_address0` and
`pad_address1`. Widening the array without this is **silent save corruption**.

- Emit `pad_address{port}_{slot}` for all 8
- On load, fall back to the legacy keys when the new ones are absent → **stock states
  still load** (SPEC FR-7)
- Bump a schema version register

Same file as P2; keep it a **separate commit** so a conflict in one does not drag in
the other.

---

### P4 — Binding capacity · `Source/input/InputBindingManager.{cpp,h}` · 🟡 Low

`MAX_PADS = 4` → `8`. Mechanical, but touches `m_bindings[MAX_PADS][...]`,
`m_analogSensitivity`, `m_motorBindings` and `m_padPreferenceName[]` — the last needs
four more `"pad5".."pad8"` strings.

*Churn:* 1 commit each since 2024 (2025-02-14, `app_config`).

---

### P5 — Pad plumbing · `Source/PadHandler.h`, `Source/PadInterface.h`, `Source/PS2VM.cpp` · 🟠 Medium

Thread `(port, slot)` through the listener interface between input source and
`CPadMan`.

⚠️ `PS2VM.cpp` is the **only hot file we touch**: 100 commits all-time, 12 since
2024, last 2026-03-13. Keep the edit to the smallest possible hunk — ideally a single
changed line in `CreatePadHandler`. If it grows, move the logic into a new file and
call into it.

*Churn:* `PadInterface.h` 2 commits all-time. `PadHandler.h` 8 all-time.

---

### P6 — Web build bindings · `Source/ui_js/Main.cpp` · 🟠 Medium

`Main.cpp` hardcodes `SetSimpleBinding(0, ...)` for pad 0 only. This is *why* the
existing pad-2 config-profile trick works (the binding manager loads all pads at
`initVm`; `Main.cpp` overrides only pad 0 afterwards).

- Extend the same override loop to pads 3–6 **or**, preferably, leave `Main.cpp`
  alone entirely and rely on the input-profile mechanism the console already writes
  (`public/play/index.html` → `writeInputProfile`). **Prefer the second** — a 🟢 None
  change on our side of the boundary instead of a 🟠 Medium one upstream.

*Churn:* 16 all-time, 4 since 2024 (last 2025-08-25).

---

### P7 — Build flag · `CMakeLists.txt` (or a new `.cmake` include) · 🟡 Low

`PORTFOLIO_MULTITAP` option, default `ON` for our build. Prefer a **new**
`cmake/portfolio-multitap.cmake` included by one added line — keeps the root
`CMakeLists.txt` edit to a single line.

---

### P8 — Attribution · `public/play-mt/LICENSE` · 🟢 None

Upstream BSD-2-Clause notice + a "this binary is modified" statement. New file,
lives in *our* repo, not the fork.

---

## Summary

| Patch | Files | Risk | Sentinels |
|---|---|---|---|
| P0 Shared config | `iop/Iop_MultitapConfig.{h,cpp}` **(new)** | 🟢 | 2 |
| P1 Multitap presence (HLE) | `Iop_MtapMan.{cpp,h}` | 🟡 | 8 |
| P1b Multitap presence (SIO2) | `Iop_Sio2.{cpp,h}` | 🟡 | 12 |
| P2 Slot addressing | `Iop_PadMan.{cpp,h}` | 🟡 | 20 |
| P3 Savestate schema | `Iop_PadMan.cpp` | 🟡 | *(in P2 count)* |
| P4 Binding capacity | `InputBindingManager.{cpp,h}` | 🟡 | 4 |
| ~~P5 Pad plumbing~~ | ~~`PadHandler.h`, `PadInterface.h`, `PS2VM.cpp`~~ | — | **0 — not needed** |
| P6 Web bindings | `ui_js/MultitapBindings.cpp` **(new)** | 🟢 | 2 |
| P7 Build wiring | `Source/CMakeLists.txt`, `ui_js/CMakeLists.txt` | 🟡 | 4 |
| P8 Attribution | `public/play-mt/LICENSE` | 🟢 | 0 |

**Actual: 380 insertions / 30 deletions across 13 files. Sentinel total: 52.**

**We never touched `PS2VM.cpp`, `PadHandler.h`, `PadInterface.h` or `ui_js/Main.cpp`** —
the four riskiest files in the original plan. The flat pad index already flowed
through `CPH_GenericInput`, and a new file with its own `EMSCRIPTEN_BINDINGS` block
replaced the planned `Main.cpp` edit. **Every 🟠 Medium-risk patch was eliminated.**

---

## Our-side changes (this repo — zero upstream conflict risk)

These are ordinary app changes and carry no fork burden:

| File | Change |
|---|---|
| `public/play-mt/` | new — forked `Play.js` + `Play.wasm` + `index.html` + `LICENSE` |
| `src/ps2/engineRouter.ts` | new — picks `stock` vs `multitap`, handles fallback |
| `public/play/index.html` | `writeInputProfile` extended to pads 3–6 (guarded: harmless on stock, which ignores unknown pads) |
| `src/ps2mp/input.ts` | already generalised — `claimGamepadPress` takes an index array |
| `src/xmb/Ps2.tsx` | player-count picker before boot; seat management to 6 |
| `mp-worker/src/index.ts` | room cap already 7; no change needed |

---

## Change log

Append one row per landed change. Keep the baseline SHA current after every rebase.

| Date | Patch | Upstream baseline | Notes |
|---|---|---|---|
| 2026-07-25 | M0 toolchain proof | `50aedca` | emsdk 4.0.1 + `wasm-ninja` preset. Unmodified upstream built (Play.js 202K / Play.wasm 2.1M — matches shipped) and booted in the console: module up, `initVm` ran, canvas live. |
| 2026-07-25 | P1–P6 multitap | `50aedca` | Full patch set landed as one commit `25dacde`. **380 insertions, 30 deletions** across 13 files. Builds clean; AC-2 verified. |

### What the implementation changed vs the original plan

Reading the source turned up something the spec had wrong, and it made the job
*easier*:

**`CSio2::ProcessMultitap` already existed.** Upstream had written the multitap
SIO2 command handler (`GetSlotNumber` 0x12/0x13, `ChangeSlot` 0x21/0x22) and then
**deliberately disabled it**, with a comment naming the reason — a *compatibility*
workaround, not a missing capability:

> *"Mark command as error/invalid to prevent MTAPMAN from reporting that there's a
> multitap in that slot. Time Crisis 3 refuses to check for memory card if a
> multitap is connected in slot 0. We don't properly support multitap at the
> moment, so, no point in giving the impression we have one."*

So P1 became "make an existing suppression conditional" rather than "write a
protocol". That comment also tells us the exact regression to watch for, which is
why the tap is **opt-in per port**: a port with no tap behaves precisely as
upstream, so Time Crisis 3 is unaffected.

**There are TWO pad paths, not one.** The spec only accounted for the HLE
`CPadMan`. Games that talk to the hardware go through `CSio2` instead. Both are
`CPadInterface` listeners fed by `CPH_GenericInput::Update`, and **both** needed
slot awareness. `Iop_MultitapConfig.h` exists so they cannot disagree.

**The flat pad index already flowed end-to-end.** `CPH_GenericInput` was already
iterating pads `0..MAX_PADS` and calling `SetButtonState(pad, ...)`; `CPadMan` was
simply dropping everything above 1. Widening the binding manager and teaching the
two sinks about slots was enough — no new plumbing through `PadHandler`/`PS2VM`
was needed, so **planned patch P5 was not required at all**.
