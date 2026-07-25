# PS2 Multitap — Specification

**Status:** approved, not yet implemented
**Goal:** play 6-player PS2 games (WWE SmackDown! HCTP, Fire Pro Wrestling Returns, FIFA)
on the console, over the web and locally, **without putting the working 2-player
path at risk**.

---

## 1. Why this is needed

Play!'s web build supports exactly **two** controllers. That is not an arbitrary
cap — `Iop_PadMan::MAX_PADS = 2` models the PS2's two *physical* ports, which is
correct. Extra players on real hardware come from a **multitap**, which multiplexes
up to four pads into one port via a `slot` index (`padPortOpen(port, slot, addr)`;
the PS2SDK sample declares `padBuf[2][4]`).

Play! parses `slot`, logs it, and discards it.

### The five-link chain

| # | Link | Upstream state |
|---|------|----------------|
| 1 | Game asks "is a multitap on this port?" (`XMTAPMAN`) | module registered |
| 2 | Emulator answers **yes** | ✗ `CMtapMan::PortOpen()` returns `0`; SDK contract is *"1 on success; !1 on failure"* |
| 3 | Game opens each `(port, slot)` pad | ✗ `m_padDataAddress[port]` — slot 1 overwrites slot 0 |
| 4 | Emulator writes each slot's state to its own EE RAM address | ✗ indexed by port only |
| 5 | Input + binding layer carries 6–8 pads | ✗ `InputBindingManager::MAX_PADS = 4` |

**Link 2 is the blocker.** Play! actively tells every game *"no multitap here"*, so
games never attempt slots 1–3. Raising any constant alone changes nothing, because
the game is not asking.

### Non-goals

- Changing anything about the current 1–2 player experience.
- 8-player support in v1 (the plumbing allows it; the UI ships 6).
- Upstreaming to jpd002 (desirable later; not a delivery dependency).

---

## 2. Architecture — dual-binary router

We ship **two** wasm builds side by side and route between them. The stock build is
never modified, so the proven path cannot regress.

```
                     ┌─────────────────────────────┐
   player count  →   │   src/ps2/engineRouter.ts   │
   + explicit mode   └──────────────┬──────────────┘
                                    │
                ┌───────────────────┴───────────────────┐
                ▼                                       ▼
     public/play/    (STOCK)                public/play-mt/   (FORK)
     upstream Play.wasm, untouched          multitap Play.wasm
     1–2 players · today's behaviour        3–6 players
     ── the known-good fallback ──          ── the new capability ──
```

### Routing rules

| Situation | Engine |
|---|---|
| Single player | `stock` |
| 2 players (local pads or netplay) | `stock` |
| 3–6 players requested | `multitap` |
| User explicitly picks "Classic (2P)" | `stock` |
| `multitap` build missing / fails to boot | **fall back to `stock`**, surface a note |

Routing is decided **once, before boot** — the engine cannot be swapped mid-session
(the wasm module and its VM are already constructed). The UI must therefore ask for
player count *before* the disc spins, the same moment we already ask for a profile.

### Why dual-binary rather than one patched binary

- The 2-player path keeps byte-identical behaviour to what is verified today.
- A bad fork build degrades to "6-player unavailable", never "PS2 broken".
- Bisecting a regression is trivial: switch engines.
- Cost is ~2.1 MB of extra static assets, well under the Cloudflare Pages 25 MiB
  per-file cap. Both are lazy-loaded; a visitor fetches one.

---

## 3. Functional requirements

**FR-1** `CMtapMan` reports a connected multitap on ports 0 and 1 when the fork is
built with multitap enabled, and implements `PortOpen`, `PortClose`, `GetConnection`,
`GetSlotNumber`.

**FR-2** `CPadMan` tracks pad data addresses per `(port, slot)` — 2 × 4 — and honours
the `slot` argument in `Open`, `SetMainMode` and `Close`.

**FR-3** `SetButtonState` / `SetAxisState` address a pad by `(port, slot)`.

**FR-4** The binding layer carries at least 8 pads (`MAX_PADS` 4 → 8).

**FR-5** The web build exposes bindings for pads 3–6 through the same input-profile
mechanism already used for pad 2, so the host-side injector needs no new concept.

**FR-6** Savestates round-trip all 8 pad addresses, and **load pre-existing 2-pad
states without error** (schema versioning, see FR-7).

**FR-7** The savestate register file gains `pad_address{port}_{slot}` keys. A state
written by stock Play! (`pad_address0`, `pad_address1`) must still load: absent keys
default to 0.

**FR-8** With multitap disabled at runtime, the fork behaves exactly like stock —
`GetConnection` returns 0, pads 3+ are inert. This gives us an in-binary A/B.

---

## 4. Acceptance criteria

1. Unmodified upstream builds to wasm locally and boots a disc in the console.
   *(Milestone 0 — proves the toolchain before any patch is written.)*
2. Fork with multitap **disabled** is behaviourally identical to stock: same boot,
   same 2-player netplay, existing savestates load.
3. A 6-player game reports 6 connected pads in its own controller-select screen.
4. Six distinct inputs drive six distinct in-game characters.
5. Savestate written by the fork loads in the fork; savestate written by stock loads
   in the fork.
6. Router picks the correct engine for 1/2/3/6 players and falls back cleanly when
   `public/play-mt/` is absent.

Criterion **3** is the real gate. Until a game's own UI shows 6 pads, the handshake
is not working, regardless of what our code does.

---

## 5. Build

Upstream ships an official wasm CI recipe (`.github/workflows/build-js.yaml`) — we
mirror it exactly:

```bash
# emsdk 4.0.1 (pinned by upstream CI)
emcmake cmake --preset wasm-ninja
cmake --build --preset wasm-ninja-release
# → build_cmake/build/wasm-ninja/Source/ui_js/Release/{Play.js,Play.wasm}
```

Submodules are required (`--recurse-submodules`). Output is copied to
`public/play-mt/`. The stock files in `public/play/` are never overwritten by this
pipeline.

---

## 6. Licence

Play! is **BSD-2-Clause** (`License.txt`, © 2006–2026 Jean-Philip Desjardins).
Modification and binary redistribution are permitted provided the copyright notice
and disclaimer are retained. `public/play-mt/` must ship a `LICENSE` file carrying
upstream's notice plus a statement that the binary is modified.

---

## 7. Risk register

| Risk | Severity | Mitigation |
|---|---|---|
| HLE multitap insufficient — game probes SIO2 directly | **High** | Milestone gate at FR/AC-3; if a game bypasses `XMTAPMAN`, stop and reassess rather than escalating into SIO2 bus emulation |
| Target game runs poorly on Play! regardless | **High** | SmackDown HCTP is `state-ingame`, not `state-playable`, on upstream's tracker. Validate playability at 1 player **before** building multitap |
| Fork drifts from upstream | Low | Patch surface is dormant — see `SYNC.md` |
| Emscripten toolchain drift | Medium | Pin emsdk 4.0.1 to match upstream CI |
| Savestate corruption | Medium | FR-7 versioning + AC-5 |
| Bundle size | Low | ~2.1 MB extra, lazy-loaded |

---

## 8. Milestones

| # | Deliverable | Gate |
|---|---|---|
| **M0** | Unmodified upstream → wasm → boots in console | toolchain proven |
| **M1** | Fork + branch structure + `public/play-mt/` plumbing + router | stock path untouched, verified |
| **M2** | `CMtapMan` reports connected | game's controller screen sees a tap |
| **M3** | `(port, slot)` pad addressing + savestate versioning | 4 pads on one port |
| **M4** | Binding layer to 8, web profile for pads 3–6 | 6 pads driven from JS |
| **M5** | Netplay + local seating to 6 | AC 1–6 all pass |

**M0 and M2 are the honest go/no-go gates.** If M2 does not land, the remaining work
has no payoff and we stop with the stock path untouched.
