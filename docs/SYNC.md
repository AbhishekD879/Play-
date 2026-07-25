# Upstream Sync Runbook

How to pull new Play! releases into our fork **without losing multitap**.
Written to be executed by an AI agent or a human; every step has a check.

---

## 1. Repository model

```
upstream  jpd002/Play-  ──►  master   (mirror only — NEVER commit here)
                                │
                                └──►  multitap   (our patch stack, rebased)
```

- `master` tracks upstream exactly. Its only job is to be a clean rebase target.
- `multitap` is a **rebased stack of small commits**, one per patch in `PATCHES.md`.
- **Rebase, never merge.** A merge history turns the patch set into an unreadable
  tangle within a few syncs; a rebased stack stays reviewable forever and each
  conflict surfaces against the one patch that caused it.
- Built artefacts are **not** committed to the fork. CI (or a local build) produces
  `Play.js`/`Play.wasm`, which are copied into this repo's `public/play-mt/`.

---

## 2. How risky is syncing, really? (measured 2026-07-25)

**Repo velocity — active:**

| Year | Commits |
|---|---|
| 2022 | 581 |
| 2023 | 632 |
| 2024 | 279 |
| 2025 | 266 |
| 2026 | 164 (to July) |

≈ **270 commits/year**, ~5/week. Last push 2026-07-20. Not abandoned.

**Our patch surface — dormant:**

| File | Commits (all time) | Since 2024 | Last touched |
|---|---|---|---|
| `Source/iop/Iop_MtapMan.cpp` | 6 | 1 | 2025-03-11 |
| `Source/iop/Iop_PadMan.cpp` | 31 | 1 | 2025-03-11 |
| `Source/iop/Iop_PadMan.h` | 26 | **0** | 2023-10-31 |
| `Source/input/InputBindingManager.cpp` | 28 | 1 | 2025-02-14 |
| `Source/input/InputBindingManager.h` | 18 | 1 | 2025-02-14 |
| `Source/PadHandler.h` | 8 | 1 | 2024-06-18 |
| `Source/PadInterface.h` | 2 | **0** | 2023-10-31 |
| `Source/ui_js/Main.cpp` | 16 | 4 | 2025-08-25 |
| `Source/PS2VM.cpp` | 100 | **12** | 2026-03-13 |

**Verdict: this is close to the ideal fork profile.** ~270 commits/year land
upstream, but roughly **4 per year** touch anything we own — and every one of those
since 2024 was mechanical (`app_config` refactor, `clang-format`, `CZipArchiveWriter`),
not functional. `Iop_MtapMan`'s last real change was **2016**.

Upstream's current focus is GS/OpenGL, IPU, VIF, CI and per-game patches — the
graphics and core-accuracy end of the emulator. **Input is not being worked on.**

Practical expectation: **most syncs rebase clean.** Budget ~1 conflict/year, and the
likely file is `PS2VM.cpp` (P5) — which is exactly why `PATCHES.md` insists that edit
stays a single line.

---

## 3. Sync procedure

### 3.1 Fetch and inspect

```bash
git fetch upstream
git log --oneline master..upstream/master | wc -l          # how much is new
```

**Triage — does anything we own move?**

```bash
git diff --stat master upstream/master -- \
  Source/iop/Iop_MtapMan.cpp Source/iop/Iop_MtapMan.h \
  Source/iop/Iop_PadMan.cpp  Source/iop/Iop_PadMan.h \
  Source/input/InputBindingManager.cpp Source/input/InputBindingManager.h \
  Source/PadHandler.h Source/PadInterface.h \
  Source/PS2VM.cpp Source/ui_js/Main.cpp CMakeLists.txt
```

Empty output → the rebase is mechanical. Non-empty → read those diffs **before**
rebasing; know what is coming.

### 3.2 Rebase

```bash
git checkout master && git merge --ff-only upstream/master
git checkout multitap && git rebase master
```

### 3.3 On conflict

Resolve **one patch at a time**, in `PATCHES.md` order. For each:

1. Read the upstream change. Is it cosmetic (formatting, a renamed config helper) or
   functional (logic change)?
2. **Cosmetic** → take upstream's version, re-apply our sentinel-fenced lines inside it.
3. **Functional** → understand the new logic first, then re-apply our change *on top
   of the new behaviour*. Do not blind-resolve to "ours" — that silently reverts an
   upstream fix.
4. Never resolve by deleting a sentinel block.

```bash
git rebase --continue
```

### 3.4 Verify the patch set survived

```bash
# must equal the sentinel count in PATCHES.md
git grep -c "PLAYSTATION-PORTFOLIO MULTITAP" | awk -F: '{s+=$2} END {print s}'

# every patch commit still present
git log --oneline master..multitap
```

A missing sentinel means a hunk was dropped during conflict resolution. **Stop and
find it** — this is the failure mode that ships a build where multitap silently does
nothing.

### 3.5 Rebuild and re-verify

```bash
emcmake cmake --preset wasm-ninja
cmake --build --preset wasm-ninja-release
cp build_cmake/build/wasm-ninja/Source/ui_js/Release/Play.{js,wasm} \
   <portfolio>/public/play-mt/
```

Then walk SPEC §4 acceptance criteria. **Minimum gate before shipping a synced
build:**

- AC-2 — multitap **disabled** behaves exactly like stock
- AC-3 — a 6-player game's own controller screen shows 6 pads
- AC-5 — a stock-written savestate still loads

### 3.6 Record it

Append to the `PATCHES.md` change log: date, new upstream baseline SHA, conflicts hit
and how they were resolved. Future syncs read this.

---

## 4. Agent instructions

If you are an AI agent performing this sync:

**Do**
- Read `PATCHES.md` first. It is the authoritative list of what is ours.
- Run the §3.1 triage before touching anything, and say what it found.
- Resolve conflicts one patch at a time, smallest unit first.
- Run the §3.4 sentinel check and **report the number**.
- State plainly which acceptance criteria you verified and which you could not
  (AC-3 and AC-4 need a real 6-player disc and six inputs — if you cannot run them,
  say so rather than implying the sync is proven).

**Do not**
- Merge instead of rebase.
- Resolve a conflict with `--ours` on a file where upstream made a *functional*
  change. Read the diff.
- Reformat, tidy, or "improve" upstream code. `check-format.yaml` is upstream's CI;
  gratuitous formatting turns tiny patches into huge conflicts.
- **Mangle line endings.** `Iop_PadMan.{cpp,h}` and `Iop_Sio2.{cpp,h}` are CRLF
  upstream; most other files are LF. Editors and scripts that normalise on save
  rewrite every line. After any edit, confirm the true patch size:
  `git diff --stat --ignore-cr-at-eol <base>..HEAD` — if it is far smaller than
  plain `--stat`, endings were flattened and must be restored before committing.
- Widen `MAX_PORTS` beyond 2. The PS2 has two physical ports; capacity comes from
  `slot`. Anyone "fixing" this has misread the hardware — see SPEC §1.
- Commit built `.wasm`/`.js` into the fork.
- Touch `public/play/` (stock). If multitap is broken, the fix is the router falling
  back, never editing the known-good engine.

**Escalate to a human when**
- `Iop_PadMan.cpp` or `Iop_MtapMan.cpp` gain functional upstream changes — that is
  either a collision with our design or upstream implementing multitap themselves
  (in which case we may be able to **delete** patches, which is the best outcome).
- The sentinel count does not match after a rebase.
- AC-2 fails — the fork has diverged from stock in the disabled path, which means a
  patch leaked outside its feature flag.

---

## 5. Exit strategies

Worth stating up front, because a fork you cannot leave is a liability.

1. **Upstream implements multitap.** Best case. Drop P1–P5, keep the router, retire
   `public/play-mt/`. Watch `Iop_MtapMan.cpp` for activity.
2. **Upstream accepts our patches.** Submit P1–P4 (the clean, hardware-correct parts).
   BSD-2-Clause and `CONTRIBUTING.md` allow it. Same end state as (1).
3. **We abandon the fork.** Delete `public/play-mt/`, router falls back to stock
   everywhere, console returns to today's 2-player behaviour. **Nothing else breaks** —
   which is the whole point of the dual-binary architecture.
