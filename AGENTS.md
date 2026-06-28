# AGENTS.md

Instructions for any AI coding agent (Claude Code, Cursor, etc.) working in this
repository. Read this first. For full architecture and scope, see [PLAN.md](PLAN.md).

This project is an **open-source macOS game porting kit**: it runs Windows games on
Apple Silicon Macs by stacking Wine + DXMT/DXVK/vkd3d + MoltenVK + Rosetta 2.
It does **not** emulate, does **not** use Apple's proprietary D3DMetal, and does
**not** bypass anti-cheat.

---

## 0. Golden rules

1. **Converse in the user's language.** If they write in Korean, respond in Korean.
2. **Never bypass, patch, or defeat anti-cheat.** If a requested game needs kernel/
   competitive anti-cheat not sanctioned on Mac (see §4), say so and stop. Do not help.
3. **The user runs commands that touch their home dir or create Wine prefixes**, not you.
   You compose and explain the commands; they execute. Wine prefixes are large,
   machine-specific, and must never be committed (already in `.gitignore`).
4. **Don't promise frame rates.** Give ranges and confidence, grounded in §3 tiers.
5. **Verify, don't assume.** Detect specs with the commands in §1 — never guess the
   user's chip, RAM, or macOS version.

---

## 1. Onboarding flow (run this when a user first forks / opens the repo)

Walk the user through these steps in order.

### Step 1 — Detect the Mac

Run (read-only, safe):

```bash
sw_vers                                   # macOS name + version
sysctl -n machdep.cpu.brand_string        # e.g. "Apple M1 Pro"
sysctl -n hw.model                         # e.g. "MacBookPro18,3"
echo "$(( $(sysctl -n hw.memsize)/1024/1024/1024 )) GB"   # unified RAM
/usr/bin/pgrep -q oahd && echo "Rosetta: on" || echo "Rosetta: NOT installed"
which brew || echo "Homebrew: missing"
```

Hard requirements — if any fail, fix before continuing:
- **Apple Silicon required** (M-series). Intel Macs are out of scope.
- **macOS 15 (Sequoia) or newer strongly recommended**; 14 (Sonoma) is the floor.
  Older → tell the user to update or expect breakage.
- **Rosetta 2 installed** — most game `.exe`s are x86_64. If missing:
  `softwareupdate --install-rosetta --agree-to-license`
- **Homebrew installed** — used to fetch Wine builds. If missing, point to https://brew.sh

### Step 2 — Ask which game

Ask the user **which specific game(s)** they want to run. Get the exact title and,
if possible, the store (Steam / Epic / GOG / Rockstar). You need this to determine:
- DirectX version (→ backend, §3)
- Whether it has blocking anti-cheat (§4)
- Approximate GPU/RAM demand

### Step 3 — Classify the game

Determine, via the user's knowledge + a quick web lookup if available:
- **Graphics API**: DX9 / DX10 / DX11 / DX12 / Vulkan / OpenGL
- **Anti-cheat**: none / EAC / BattlEye / publisher-custom (§4)
- **Rough demand tier**: light (indie/2D/old) / medium (AA, ~2015-2020 AAA) /
  heavy (modern AAA, ray tracing, 2022+)

### Step 4 — Pick the backend (§3) and judge feasibility (§3 tables)

Tell the user, honestly:
- Which backend the stack will use and why
- A **feasibility verdict**: ✅ should run well / 🟡 playable with compromises /
  🔴 not realistic on this machine / ⛔ blocked (anti-cheat / Intel / etc.)
- Expected compromises (resolution, texture quality, target FPS range)

### Step 5 — Set up

Only after the user understands the verdict, proceed to install the stack and create
a per-game profile (see [PLAN.md](PLAN.md) §6 for the profile schema). The user runs
the prefix-creating commands.

---

## 2. Backend selection rules

Map the game's graphics API to a backend.

| Game API | Backend | Path |
|---|---|---|
| **D3D12** | **D3DMetal** (best perf) / vkd3d-proton + MoltenVK (open fallback) | DX12 → Metal / DX12 → Vulkan → Metal |
| **D3D11** | **DXMT** (preferred) | DX11 → Metal (direct) |
| D3D11 (fallback) | DXVK + MoltenVK / D3DMetal | DX11 → Vulkan → Metal |
| **D3D9 / D3D10** | DXVK + MoltenVK | DX → Vulkan → Metal |
| OpenGL | Wine native GL → MoltenVK/Metal | varies |
| Old Win32 / GDI | Wine alone | no graphics translation needed |

Prefer **DXMT for DX11** on lower-tier chips (less CPU/memory overhead than the
Vulkan path). Use the Vulkan path when DXMT has known issues for that title.

**D3DMetal caveat (license):** D3DMetal gives the best DX12 performance, but its license
forbids redistribution. The app must **never bundle it** — it is loaded as a
**user-provided external component** the user obtains on their own machine. If D3DMetal
isn't present, fall back to the open Vulkan path (vkd3d/DXVK + MoltenVK). Treat D3DMetal
as an optional accelerator, never a hard dependency. See [PLAN.md](PLAN.md) §5 / §9.

---

## 3. Feasibility heuristics

These are **starting estimates**, not guarantees. Always hedge. Translation overhead
typically costs 10–30% vs native Windows on the same silicon.

### GPU tier (by chip)

| Chip class | GPU cores (approx) | Realistic target |
|---|---|---|
| M1 / M2 / M3 / M4 (base) | 7–10 | Light games 1080p; medium AAA at low/medium 1080p |
| **M_ Pro** | 14–20 | Medium AAA 1080p–1440p medium/high; heavy AAA at low/medium |
| M_ Max | 30–40 | Heavy AAA 1440p high |
| M_ Ultra | 60–80 | Heavy AAA 1440p–4K high |
| Newer gens (M3/M4) | — | Bump one row better, esp. for ray tracing |

### Unified RAM (shared CPU+GPU — this matters a lot)

| RAM | Guidance |
|---|---|
| 8 GB | Light/older games only. Modern AAA will swap and stutter. |
| 16 GB | Most games at medium. Close other apps for heavy titles. |
| 32 GB+ | Comfortable for AAA, higher texture settings. |

### Verdict rubric (combine chip × RAM × game tier)

- ✅ **Good**: chip tier ≥ game demand AND RAM comfortable → runs well, minor tuning.
- 🟡 **Playable**: one tier short OR RAM tight → works with lowered settings; set
  expectations on resolution/FPS.
- 🔴 **Not realistic**: two+ tiers short → discourage; suggest lighter alternatives.
- ⛔ **Blocked**: anti-cheat (§4), Intel Mac, or macOS too old → cannot proceed.

Always state the **assumptions** behind a verdict (e.g. "1080p, medium textures,
~40–50 FPS, expect dips in busy scenes").

---

## 4. Anti-cheat policy (hard stop)

Do not attempt to run, bypass, spoof, or work around anti-cheat. Classify the game:

- **No anti-cheat** → proceed normally.
- **EAC / BattlEye** → *may* work **only if the publisher has enabled Mac/Proton
  support** for that title. If unconfirmed, treat as 🔴 and tell the user to verify;
  do not attempt bypass.
- **Publisher-custom / kernel anti-cheat** (e.g. Blizzard Defense Matrix on
  Overwatch 2, Riot Vanguard on Valorant) → ⛔ **blocked, out of scope.** Say so plainly.
- **Single-player modes** of games whose *online* mode has anti-cheat (e.g. RDR2
  story mode) → usually fine. Target the single-player executable only.

This project's scope is single-player and publisher-sanctioned titles. Competitive
multiplayer with enforced anti-cheat is **currently** out of scope.

**Long-term framing (when a user asks "why not Overwatch?"):** the path forward is
*advocacy, not bypass* — exactly how Linux/Proton got EAC and BattlEye games sanctioned
(publishers flip a server-side toggle to allow Wine). Our strict no-bypass rule is what
earns the credibility to eventually make that ask. So the honest answer is: "Blocked
today, and we will never bypass it — but a clean ecosystem + user demand is how titles
get officially enabled over time." See [PLAN.md](PLAN.md) §9 / P5.

---

## 5. Repo conventions

- **Plan of record**: [PLAN.md](PLAN.md). Keep it in sync if scope changes.
- **License**: GPLv3. Any vendored upstream (Wine, DXMT, Sikarugir) must keep its
  attribution and be license-compatible. **Do not vendor CC BY-NC-SA content** (e.g.
  AppleGamingWiki data) — link only; it is GPL-incompatible.
- **GUI base**: built on [Sikarugir](https://github.com/Sikarugir-App/Sikarugir) — actually
  **Objective-C / AppKit** (source: `Sikarugir-foss-sources`), not SwiftUI. Long-term UI
  direction is a **new SwiftUI shell over the retained Objective-C engine** (PLAN.md §10).
- **D3DMetal**: keep it as a selectable backend, but **never commit/bundle the binary** —
  load it as a user-provided external component (license forbids redistribution).
- **Never commit**: D3DMetal binaries, Wine prefixes, downloaded runtimes, `.dmg`/`.pkg`,
  logs (see `.gitignore`).
- **Per-game profiles**: JSON/YAML following the schema in PLAN.md §6.
