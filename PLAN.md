# mac-could-be-gaming-machine — 아키텍처 플랜 (v1.0)

> 무료·오픈소스 macOS 게임 포팅킷. Windows 게임을 Apple Silicon 맥에서 돌리기 위한
> GUI 런처 + 게임별 자동 설정 시스템.

최종 갱신: 2026-06-28

---

## 0. 한 줄 요약

기존 오픈소스 번역 레이어(Wine + DXMT/DXVK/vkd3d + MoltenVK)를 **갖다 쓰고**,
그 위에 **게임별 자동 설정(프로파일) + 깔끔한 GUI**를 얹는다.
통역 레이어 자체는 새로 만들지 않는다. 애플 독점 D3DMetal도 쓰지 않는다(라이선스 청정).

---

## 1. 스코프

### Goals
- 무료·오픈소스 GUI 포팅킷
- .exe 추가 → 백엔드 자동 선택 → 원클릭 실행
- 커뮤니티 기여형 per-game 프로파일 DB
- 1차 타깃: **싱글플레이어 게임** (RDR2 스토리모드 등)

### Non-goals
- ❌ Wine/DXVK/DXMT 등 번역 레이어 신규 개발 (기존 것 사용)
- ❌ 애플 D3DMetal 사용 (독점 라이선스 → 재배포 불가)
- ❌ 안티치트 우회 (법적·윤리적 금지선. 오버워치 등 경쟁 멀티 미지원 확정)

---

## 2. 핵심 원리

포팅킷은 **에뮬레이션이 아니라 실시간 번역**이다. OS를 통째로 흉내내는 VM과 달리,
Windows 프로그램의 호출을 맥이 알아듣는 호출로 통역한다 → 빠르고 가볍다.

Apple Silicon에서 넘어야 할 3개의 벽:

| 층 | 문제 | 해결 (전부 오픈소스) |
|---|---|---|
| OS API | 게임이 Win32 API 호출 | **Wine** (Gcenx 빌드 베이스) |
| 그래픽 | 게임이 DirectX 사용 | **DXMT / DXVK / vkd3d-proton + MoltenVK** |
| CPU | 게임이 x86_64 바이너리 | **Rosetta 2** (OS 내장) |

---

## 3. 그래픽 백엔드 전략 (3-way, 자동 선택)

게임의 DirectX 버전에 따라 프로파일이 백엔드를 지정한다.
**애플 D3DMetal은 의도적으로 제외** → 전 스택 오픈소스 유지.

| 게임 API | 1순위 백엔드 | 경로 | 비고 |
|---|---|---|---|
| D3D11 | **DXMT** | DX11 → Metal 직통 | 저~중사양에 최적, M1 Pro에 유리 |
| D3D12 | **vkd3d-proton + MoltenVK** | DX12 → Vulkan → Metal | RDR2가 여기 |
| D3D9/10 | **DXVK + MoltenVK** | DX → Vulkan → Metal | 구형 게임 |

---

## 4. GUI 베이스 결정 — Sikarugir 포크

- **베이스**: [Sikarugir](https://github.com/Sikarugir-App/Sikarugir) (구 Kegworks, 구 Wineskin)
  - Swift/SwiftUI, 맥 네이티브
  - DXVK/DXMT/D3DMetal 백엔드 토글 이미 내장 → 우리는 D3DMetal 토글만 비활성/제거
  - 활발히 유지보수 중 (Whisky는 2025-04 중단됨 → 베이스 부적합)
- **포크 전략**: GPLv3 호환 확인 후 vendoring/포크. 상류(upstream) 변경은 수동 추적.
- **우리가 추가하는 차별화 레이어**: §6 프로파일 시스템.

---

## 5. 라이선스 — GPLv3

- Wine: LGPL / DXVK·DXMT: zlib·MIT / Sikarugir: 오픈 계열 → GPLv3 채택에 충돌 없음
- 카피레프트: 누가 우리 코드로 상업화하면 소스 공개 의무 → 오픈소스 생태계 보호
- `LICENSE` 파일 포함 완료.

---

## 6. 진짜 가치 = per-game 프로파일 시스템

포팅킷의 본질은 통역기가 아니라 **"이 게임엔 이 설정"의 DB**다 (ProtonDB가 증명).

```
GameProfile {
  id              # 게임 식별자 (steam appid / 해시)
  title
  backend         # dxmt | vkd3d | dxvk
  dllOverrides     # Wine DLL override 목록
  envVars          # WINEESYNC, WINEMSYNC, MTL_HUD 등
  winePrefixHints  # 권장 prefix 설정
  graphics         # 해상도, DXVK 옵션 등
  notes            # 알려진 이슈 / 팁
  tested { chip, macos, status }
}
```

- 포맷: JSON 또는 YAML
- 커뮤니티 기여: 별도 `profiles/` 레포에 PR로 수집
- 앱이 실행 시 프로파일 자동 적용

---

## 7. 로드맵 (단계)

- **P0 — PoC** (다음 단계)
  - Sikarugir 레포 구조 분석 (포크 전 수정 지점 파악)
  - 로컬에 Gcenx Wine + DXMT 수동 설치
  - DX11 게임 1개 띄워 M1 Pro 성능/메모리 실측
- **P1 — MVP**: GUI에서 .exe 추가 → 백엔드 선택 → 실행. 프로파일 1개 하드코딩.
- **P2 — 프로파일 시스템** + 커뮤니티 레포 연동
- **P3 — UX**: 자동 백엔드 감지, FPS 오버레이, 로그 수집
- **P4 — 실험**: 안티치트 허용 게임(EAC/BattlEye) 지원 (게임사 허용분 한정)

---

## 8. 타깃 환경 (개발 머신)

| 항목 | 값 |
|---|---|
| 기기 | MacBook Pro 16" (MacBookPro18,3) |
| 칩 | Apple M1 Pro |
| RAM | 32 GB |
| macOS | 15.6 (Sequoia) — Wine/DXMT 요구사항 충족 ✅ |
| Rosetta 2 | 설치·작동 중 ✅ |
| Homebrew | /opt/homebrew ✅ |

첫 PoC 타깃 게임: RDR2 스토리모드(DX12) + 가벼운 DX11 게임 1개(스택 검증용).

---

## 9. 안티치트 현실 (2026-06 기준)

- CrossOver 26(2026-02)부터 EAC/BattlEye/nProtect 일부 지원 → **게임사 허용분 한정**.
- **오버워치 2**: 블리자드 자체 안티치트(Defense Matrix) → 미지원 확정. 본 프로젝트 범위 밖.
- **RDR 온라인**: 안티치트 → 미지원. 단 **스토리모드는 지원 가능**.
- 본 프로젝트는 안티치트 우회를 **하지 않는다**.

---

## 10. 미해결/결정 대기

- [ ] PoC 첫 게임 확정 (RDR2 설치본 유무 / 대체 DX11 타이틀)
- [ ] Sikarugir 포크 방식: 전체 vendoring vs submodule vs 정식 GitHub fork
- [ ] 프로파일 레포 분리 시점 (이 레포 내 폴더 vs 별도 레포)
- [ ] 앱 이름/번들 ID 확정
