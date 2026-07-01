# RDR2 on Sikarugir — Launcher CEF Investigation (2026-07-01)

Session log. Goal: run **Red Dead Redemption 2 (Steam)** on Apple **M4** via
**Sikarugir** (Wineskin/Kegworks lineage + Apple GPTK / MoltenVK / D3DMetal).

## Result at a glance — 4 walls fixed, 1 terminal

| # | Wall | Status |
|---|------|--------|
| ① | D3DMetal crash on 2D launcher | ✅ fixed — turn D3DMetal **off**, use Vulkan→MoltenVK path |
| ② | Social Club SDK init = **Error 1002** | ✅ fixed — **created missing `Documents` tree** (see below) |
| ③ | CEF **initialization** failure | ✅ bypassed — CEF flags (below) |
| ④ | CEF module loading (`libcef.dll`) | ✅ loads |
| ⑤ | **CEF browser `int3` self-crash** | ❌ **blocked — free-Wine ceiling** |

## Root causes & fixes found

### ② Error 1002 — Documents folder missing (PERMANENT FIX, kept)
The prefix user dir had **no `Documents` folder**, but the registry `Personal`
shell folder pointed at `C:\users\brio\Documents`. Social Club writes its
machine config to `Documents\Rockstar Games\Social Club\`; the missing folder
made that write fail → 1002. Created:

```
drive_c/users/Sikarugir/Documents/Rockstar Games/Social Club
drive_c/users/Sikarugir/Documents/Rockstar Games/Red Dead Redemption 2/Settings
```

→ **1002 gone.** This folder is left in place (real, permanent improvement).

### ③ CEF init — flags that got furthest
Injected via the Sikarugir `Info.plist` **`Program Flags`** while temporarily
pointing **`Program Name and Path`** at `Launcher.exe` (auto-restored after):

```
--no-sandbox --disable-gpu --in-process-gpu --disable-gpu-compositing
```

With these the launcher reached the **"Connecting to Rockstar Games Services"**
splash (CEF actually rendered).

### ⑤ CEF browser crash — the terminal wall
Deterministic **`int3` / STATUS_BREAKPOINT (0x80000003)** at
**`libcef+0x6b4be47`** inside Rockstar's Social Club `libcef.dll`
(a Chromium intentional `CHECK`/`ImmediateCrash`, not an access violation).

Evidence it is **not** touchable from our side:
- **Every** flag combo crashes at the *same* address: tried
  `--single-process`, `--use-angle=swiftshader`, `--disable-gpu`,
  `--disable-gpu-rasterization`, `--disable-accelerated-video-decode`,
  `--disable-accelerated-2d-canvas`. GPU path is **not** the cause.
- CEF writes **no log** before dying (`--enable-logging --log-file=` produced
  nothing) → the failing `CHECK` message is never emitted.
- **winedbg** (`AeDebug` → `winedbg --auto`) attaches but yields only the bare
  crash frame `libcef+0x6b4be47: int3`; no symbols for Rockstar's custom CEF.
- Crash hits `SocialClubHelper.exe` **and** the content renderer → the main
  launcher then shows **"timed out loading both online and offline content"**
  (a downstream symptom, not a network fault).

Ruled out: **network / TLS** — `www.rockstargames.com` and
`socialclub.rockstargames.com` return HTTP 200/307 with TLS verify OK from the
Mac even on the 2026 clock. Not a cert/date problem.

**Verdict:** this is the genuine free-Wine ceiling for the Rockstar Games
Launcher — the CEF-under-Wine class that **CrossOver 23+ specifically patched**
(they have CEF symbols/expertise). Blind-patching Wine against an opaque,
un-recompilable 30 MB stripped `libcef` is a lottery, not engineering.

## Environment changes (state after session)
- **Sikarugir prefix:** `Documents` tree **kept** (fixes 1002 permanently).
  CEF caches cleared. `Info.plist` and `SocialClubHelper.exe` **restored to
  normal**. One clean session (Steam) left running; all stray wine sessions
  killed.
- **System (Homebrew), kept, harmless:** `samba` (provides `ntlm_auth`),
  `mingw-w64` (installed for a PE shim, see below).
- **Prepared, NOT applied:** a transparent CEF-flag **PE shim**
  (`launcher_shim.c`, in the session scratchpad) that would rename
  `Launcher.exe → Launcher_real.exe` and re-exec with flags + inherited CEF IPC
  handles. Unneeded once ⑤ proved terminal.

## Next steps / open decisions
1. **To play RDR2 now:** CrossOver 23+ trial (bundles the CEF fix) or Heroic.
   Most reliable path; the free-Sikarugir path is capped at wall ⑤.
2. **To engineer:** spend the "expensive" effort on the **DX12→Metal PoC
   moonshot** (branch `experiment/dx12-metal-poc`). Wall ⑤ is itself the
   argument for a properly rebuilt translation layer — RGL-on-Wine is
   CrossOver's already-solved problem.

## Related: PoC track (separate branch)
`experiment/dx12-metal-poc` — code-review #2 landed (`59dffd8`): `write_ppm()`
return checks + null-guards on `newCommandQueue/newTexture/newBuffer`; verified
on M4 (m1 centre rgb 26/102/204; m3 `colored_px=16200`; negative write test
now exits 1 instead of falsely reporting success).
