# mac-could-be-gaming-machine

> A free, open-source game porting kit for Apple Silicon Macs.
> Run Windows games on macOS — without paying for CrossOver.

**Status:** 🟡 Planning / Pre-PoC

## What is this?

A GUI launcher that runs Windows games on macOS by stacking existing open-source
translation layers — no emulation, no proprietary Apple D3DMetal, no anti-cheat bypass.

```
Game (.exe)
  → Wine            (Win32 API → macOS)
  → DXMT / DXVK / vkd3d-proton + MoltenVK   (DirectX → Metal)
  → Rosetta 2       (x86_64 → ARM64)
  → macOS Metal
```

The real value isn't the translation layer (that already exists) — it's the
**per-game profile database** that auto-applies the right settings for each title.

## Scope

- ✅ Single-player Windows games (e.g. RDR2 story mode)
- ✅ Open-source app (GPLv3). Open backends (DXMT/DXVK/vkd3d + MoltenVK) by default
- ✅ D3DMetal supported as an optional, **user-provided** backend (not bundled — license
  forbids redistribution; the app loads it from your own machine). Bundling rights are a
  long-term advocacy goal with Apple
- ❌ **Never** an anti-cheat bypass — competitive multiplayer (Overwatch, etc.) is out of scope *today*
- 🎯 **Long-term:** we will keep petitioning anti-cheat vendors and publishers to grant
  *official* Mac/Wine support (the path Linux/Proton took — sanctioned enablement, not bypass).
  Our strict no-bypass stance is exactly what earns the credibility to make that ask.
  See [PLAN.md](PLAN.md) §9 / P5.

## License

GPLv3 — see [LICENSE](LICENSE).

## Plan

See [PLAN.md](PLAN.md) for the full architecture and roadmap.

## Built on

- [Wine](https://www.winehq.org/) (Gcenx builds) · [DXMT](https://github.com/3Shain/dxmt) ·
  [DXVK](https://github.com/doitsujin/dxvk) · [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton) ·
  [MoltenVK](https://github.com/KhronosGroup/MoltenVK) · GUI base: [Sikarugir](https://github.com/Sikarugir-App/Sikarugir)
