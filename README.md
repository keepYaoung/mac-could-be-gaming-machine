# mac-could-be-gaming-machine

> A free, open-source game porting kit for Apple Silicon Macs.
> Run Windows games on macOS — without paying for CrossOver.

**Status:** 🟡 Planning / Pre-PoC

---

## ⚠️ RDR2 트랙 — 여기서 멈춤 (재시작 전 반드시 읽기)

첫 실전 타깃은 **RDR2 (Steam판, M4)**. Rockstar Games Launcher까지 파고들었고,
아키텍처는 검증됐지만 **한 벽에서 막혀 의도적으로 파킹**했다.
과정에서 나온 재사용 가치(런북·프로파일·분석)는 이미 아래 문서에 다 남겨뒀다.

- 벽별 조사 로그: [`docs/2026-07-01-rdr2-sikarugir-cef-investigation.md`](docs/2026-07-01-rdr2-sikarugir-cef-investigation.md)
- 병목 재정리(모순 조정): [`docs/2026-07-02-rdr2-blocker-reconciliation.md`](docs/2026-07-02-rdr2-blocker-reconciliation.md)
- 실행 런북(STEP 0-4): [`docs/rdr2-sikarugir-runbook.md`](docs/rdr2-sikarugir-runbook.md)
- 프로파일 DB 첫 엔트리: [`profiles/red-dead-redemption-2.yaml`](profiles/red-dead-redemption-2.yaml)

**막힌 지점:** ⑤ Rockstar 런처 CEF의 `int3` self-crash (`libcef+0x6b4be47`).
①~④(D3DMetal 런처 크래시 / Error 1002 / CEF init / libcef 로딩)는 다 뚫음.

### 지금 다시 시작하려는 미래의 나에게 — ROI가 떨어진다. 하지 마라. 🛑

이 트랙 재개는 **투입 대비 회수가 낮다.** 이유:

1. **막힌 게 프리픽스 설정이 아니라 엔진(WineCX 소스)이다.** ⑤ CEF 픽스는 CrossOver가
   Wine 바이너리에 넣은 패치이지, 내가 프리픽스에서 켜고 끌 수 있는 값이 아니다.
   → **내 노력으로 뚫는 게 아니라, 업스트림 유입을 "기다리는" 문제.** 앉아서 판다고
   나오지 않는다.
2. **불투명한 30MB stripped `libcef` 블라인드 패치는 엔지니어링이 아니라 로또다.**
   모든 플래그 조합이 같은 주소에서 죽고, 로그도 안 남는다. 시간당 기대수익 ≈ 0.
3. **지금 당장 RDR2를 하고 싶으면 답은 이미 있다** — CrossOver 트라이얼이 동일 파일을
   구동한다(그래서 아키텍처가 검증된 것). "무료로 재현"이라는 자존심 값을 위해
   주말을 태우는 것일 뿐, 플레이 자체는 이미 해결됨.
4. **재사용 가치는 이미 다 뽑았다.** 런북·프로파일 스키마·백엔드 규칙은 커밋됨.
   RGL 디버그 루프를 한 바퀴 더 도는 것의 **한계효용은 사실상 0.**

**대신 이렇게 해라 (저비용):** 재개 조건을 "노력"이 아니라 "이벤트"에 건다.
Sikarugir 엔진(`WS12WineSikarugir`)이 CX의 CEF 패치를 흡수한 버전으로 올라오면,
그때 [런북 STEP 3-0](docs/rdr2-sikarugir-runbook.md)은 **5분짜리 재확인**으로 줄어든다.
→ 업스트림 릴리스만 지켜보고, 그 전까지 이 트랙은 **닫아둔다.**

**요지: 이건 근성으로 뚫는 벽이 아니라 릴리스를 기다리는 벽이다. 파킹이 정답.**

---

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
