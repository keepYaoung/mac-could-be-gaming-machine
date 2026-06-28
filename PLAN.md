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
  - ⚠️ **정정**: SwiftUI 아님. 실제 앱 소스는
    [`Sikarugir-foss-sources`](https://github.com/Sikarugir-App/Sikarugir-foss-sources)로
    **Objective-C / AppKit** (Wineskin 혈통, 클래스명 `Wineskin*` 그대로). UI는 단일
    `MainMenu.xib` 한 개 — 레거시 단일 창.
  - 메인 `Sikarugir` 레포는 배포용(D3DMetal 바이너리 + 문서). 번역 레이어들(dxmt, d9mt,
    dxvk, MoltenVK, wine)은 org 내 개별 레포로 관리됨.
  - DXVK/DXMT/D3DMetal/WineD3D 백엔드 토글 내장 → 우리는 D3DMetal 토글만 비활성/제거
  - 활발히 유지보수 중 (Whisky는 2025-04 중단됨 → 베이스 부적합)
- **재활용 가능한 기존 자산**: `NSComputerInformation`(사양 감지), `Download`/`NSWebUtilities`
  (엔진 다운로드), `NSDropIconView`·`NSProgressView`(드래그앤드롭·진행 UI),
  `NSPortManager`(래퍼 데이터 모델).
- **포크 전략**: GPLv3 호환 확인 후 vendoring/포크. 상류(upstream) 변경은 수동 추적.
- **우리가 추가하는 차별화 레이어**: §6 프로파일 시스템 + §11 UX 재설계.

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
- **P5 — 생태계·애드보커시** (장기): 유저 수요·호환성 데이터를 쌓아 안티치트 벤더/
  퍼블리셔에 **공식 허용(sanctioned enablement)** 요청. §9 참고.

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

## 9. 안티치트 정책 — 현재 + 장기 전략

### 현재 (2026-06 기준)
- CrossOver 26(2026-02)부터 EAC/BattlEye/nProtect 일부 지원 → **게임사 허용분 한정**.
- **오버워치 2**: 블리자드 자체 안티치트(Defense Matrix) → 미지원 확정. 현재 범위 밖.
- **RDR 온라인**: 안티치트 → 미지원. 단 **스토리모드는 지원 가능**.
- 본 프로젝트는 안티치트 우회를 **절대 하지 않는다**.

### 장기 전략 — "우회"가 아니라 "허용 요청" (P5)
리눅스가 게이밍에서 이긴 방식이 모델이다. Valve가 안티치트 벤더와 협력해
**퍼블리셔가 서버 토글 하나로 Wine/Proton을 허용**하는 구조(EAC는 원클릭)를 만들었고,
그 덕에 EAC/BattlEye 게임이 리눅스에서 정식으로 돌아간다 — 우회가 아니라 **허용**이다.

**핵심 원칙: "절대 우회 안 함" 하드룰이 곧 미래 협상 자격이다.**
- 우회 도구로 인식되면 벤더는 절대 허용하지 않는다.
- 깨끗한 sanctioned-path 생태계 + 충분한 유저 수요가 쌓이면, 퍼블리셔에게
  "맥 유저 허용 = 매출 상승"이라는 ROI가 생겨 토글을 켤 명분이 된다.
- 따라서 우리의 무기는 두 가지: **(a) 청정한 평판**, **(b) 호환성·수요 데이터**.

**실행 항목(P5)**:
- [ ] 익명 호환성/플레이 통계 수집(옵트인) → 수요 근거 데이터화
- [ ] 안티치트 벤더(Epic EAC, BattlEye)·주요 퍼블리셔에 Mac 허용 요청 채널 정리
- [ ] Proton의 안티치트 허용 선례·문서를 레퍼런스로 정리

---

## 10. 미해결/결정 대기

- [ ] PoC 첫 게임 확정 (RDR2 설치본 유무 / 대체 DX11 타이틀)
- [ ] Sikarugir 포크 방식: 전체 vendoring vs submodule vs 정식 GitHub fork
- [ ] 프로파일 레포 분리 시점 (이 레포 내 폴더 vs 별도 레포)
- [ ] 앱 이름/번들 ID 확정

### 확정된 결정
- [x] **UI 전략 = 하이브리드 (장기 방향)**: UI 셸은 **SwiftUI 신규**, 기존
  **Objective-C 엔진은 유지**하고 브리징 헤더로 호출.
  - 버림: 레거시 UI 셸(`MainMenu.xib`). 우리 UI는 "게임 중심"이라 사실상 새 프런트라
    xib 점진 개선은 실익이 적음.
  - 유지: `NSPortManager`·`NSWineskinPortDataWriter`·`Download`·`NSComputerInformation`
    등 어려운 플러밍(Wine prefix 관리·다운로드·사양 감지)은 안 건드림.
  - 근거: 동적 게임 그리드/실시간 판정 카드는 SwiftUI 바인딩에 적합 / xib는 머지·리뷰
    불가에 가까워 오픈소스 부적합 / SwiftUI가 기여 진입장벽↓ → P5 커뮤니티 전략과 정합.
  - 전환 방식: 점진적. 엔진 안정성 유지하며 프런트만 단계적으로 SwiftUI화.

---

## 11. UX 재설계 — 차별화의 본체

번역 플러밍은 빌려 쓴다. **우리 제품 가치는 전부 경험 레이어에 있다.** UX에서 차별화
못 하면 Sikarugir 재포장일 뿐이다.

### Sikarugir(래퍼 빌더) UX 갭 → 우리 개선

| Sikarugir의 한계 (코드로 확인됨) | 우리 개선 |
|---|---|
| **"Port/래퍼" 중심** 데이터 모델(`NSPortManager`) — 먼저 래퍼 만들고 exe 연결 | **게임 중심** 라이브러리 그리드. "뭐 만들까"가 아니라 "뭐 하고 싶어?" |
| 백엔드를 **유저가 직접** 선택 (DX 버전 알아야 함) | **자동 선택 + 배지** ("DXMT 사용 — 이 게임에 최적"), 고급에서 override |
| 될지 안 될지 **안 알려줌** (`NSComputerInformation`은 있으나 판정 UX 없음) | **설치 전 "내 맥에서 돼?" 판정 카드** — ✅/🟡/🔴/⛔ + 칩×RAM 근거 |
| prefix·engine·winetricks·DLL override **전문용어 범벅** | **무전문용어 위저드** — 프로파일이 기본값 자동, 고급은 숨김 |
| 첫 실행 **스피너만** | **정직한 진행 표시** + 예상 FPS/화질 사전 고지 |
| 에러 = **raw Wine 로그** | **사람 말로 번역된 에러** + 추천 조치 |
| 게임별 사회적 증거 없음 | **커뮤니티 호환성 카드** (프로파일 DB 연동) |

### 플래그십 화면 (목업 완료, P1 타깃)
1. **자동 감지된 "내 맥" 스트립** — 칩·RAM·macOS·Rosetta 상태 (`NSComputerInformation` 재활용)
2. **게임 중심 라이브러리** — 카드마다 DX 버전 · 백엔드 · 판정 배지(✅🟡🔴⛔)
3. **"이 게임, 내 맥에서 돼?" 상세 카드** — GPU/RAM/백엔드/안티치트 항목별 판정 + 예상 성능

### 우선순위
- **임팩트 최상**: ① 게임 중심 라이브러리 + ② 설치 전 판정 카드
  → "Sikarugir엔 없고 우리한텐 있는" 첫인상을 만드는 두 가지.
- 백엔드(화면 뒤): per-game 프로파일 DB(§6), 사양 판정 로직(AGENTS.md §3 루브릭 보정),
  에러 번역 파이프라인.
