# mac-could-be-gaming-machine — 아키텍처 플랜 (v1.0)

> 무료·오픈소스 macOS 게임 포팅킷. Windows 게임을 Apple Silicon 맥에서 돌리기 위한
> GUI 런처 + 게임별 자동 설정 시스템.

최종 갱신: 2026-06-30

---

## 0. 한 줄 요약

기존 오픈소스 번역 레이어(Wine + DXMT/DXVK/vkd3d + MoltenVK)를 **갖다 쓰고**,
그 위에 **게임별 자동 설정(프로파일) + 깔끔한 GUI**를 얹는다.
통역 레이어 자체는 새로 만들지 않는다. 앱 자체는 **GPLv3 오픈소스**.
애플 D3DMetal은 **백엔드로 지원하되**, 라이선스상 **레포·배포물에 동봉하지 않고**
사용자 맥에서 외부 컴포넌트로 받아 로드한다(§5 참고).

---

## 1. 스코프

### Goals
- 무료·오픈소스 GUI 포팅킷
- .exe 추가 → 백엔드 자동 선택 → 원클릭 실행
- 커뮤니티 기여형 per-game 프로파일 DB
- 1차 타깃: **싱글플레이어 게임** (RDR2 스토리모드 등)

### Non-goals
- ❌ Wine/DXVK/DXMT 등 번역 레이어 신규 개발 (기존 것 사용)
- ❌ D3DMetal 바이너리를 **우리 레포/배포물에 동봉** (NC 전용 → GPL과 충돌. 백엔드
  지원은 하되 외부 컴포넌트로만. Apple 정식 허가 확보는 별도 애드보커시 트랙, §9 참고)
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

## 3. 그래픽 백엔드 전략 (자동 선택)

게임의 DirectX 버전에 따라 프로파일이 백엔드를 지정한다. **D3DMetal 포함 지원** —
단 라이선스상 동봉 불가라 외부 컴포넌트로 받아 쓴다(§5).

| 게임 API | 1순위 백엔드 | 경로 | 비고 |
|---|---|---|---|
| D3D12 | **D3DMetal** | DX12 → Metal 직통 | 최고 성능. 외부 컴포넌트(비동봉). 대안: vkd3d+MoltenVK |
| D3D11 | **DXMT** | DX11 → Metal 직통 | 오픈소스, M1 Pro에 유리. 대안: D3DMetal |
| D3D9/10 | **DXVK + MoltenVK** | DX → Vulkan → Metal | 구형 게임 |

> **오픈소스 vs 성능**: vkd3d/DXVK 경로는 전 스택 오픈소스(동봉 가능), D3DMetal은
> 성능이 더 좋으나 비동봉 외부 컴포넌트. 프로파일이 게임별로 최적 백엔드를 고르고,
> 사용자가 override 가능. 앱 코어는 어느 쪽이든 GPLv3 유지.

---

## 4. GUI 베이스 결정 — Sikarugir 포크

- **베이스**: [Sikarugir](https://github.com/Sikarugir-App/Sikarugir) (구 Kegworks, 구 Wineskin)
  - ⚠️ **정정**: SwiftUI 아님. 실제 앱 소스는
    [`Sikarugir-foss-sources`](https://github.com/Sikarugir-App/Sikarugir-foss-sources)로
    **Objective-C / AppKit** (Wineskin 혈통, 클래스명 `Wineskin*` 그대로). UI는 단일
    `MainMenu.xib` 한 개 — 레거시 단일 창.
  - 메인 `Sikarugir` 레포는 배포용(D3DMetal 바이너리 + 문서). 번역 레이어들(dxmt, d9mt,
    dxvk, MoltenVK, wine)은 org 내 개별 레포로 관리됨.
  - DXVK/DXMT/D3DMetal/WineD3D 백엔드 토글 내장 → **그대로 활용**(D3DMetal은
    비동봉·외부 컴포넌트 방식으로 연결). 참고로 Sikarugir도 D3DMetal을 별도 배포
    레포로 분리해 동봉 = 우리가 따를 패턴
  - 활발히 유지보수 중 (Whisky는 2025-04 중단됨 → 베이스 부적합)
- **재활용 가능한 기존 자산**: `NSComputerInformation`(사양 감지), `Download`/`NSWebUtilities`
  (엔진 다운로드), `NSDropIconView`·`NSProgressView`(드래그앤드롭·진행 UI),
  `NSPortManager`(래퍼 데이터 모델).
- **포크 전략**: GPLv3 호환 확인 후 vendoring/포크. 상류(upstream) 변경은 수동 추적.
- **우리가 추가하는 차별화 레이어**: §6 프로파일 시스템 + §11 UX 재설계.

---

## 5. 라이선스 — GPLv3 (확정)

- Wine: LGPL / DXVK·DXMT: zlib·MIT / Sikarugir: 오픈 계열 → GPLv3 채택에 충돌 없음
- 카피레프트: 누가 우리 코드로 상업화하면 소스 공개 의무 → 오픈소스 생태계 보호
- `LICENSE` 파일 포함 완료.

### CC BY-NC-SA 검토 → 폐기
- CC 라이선스는 **콘텐츠·데이터용**. Creative Commons도 소프트웨어엔 쓰지 말라고 명시
  (소스/바이너리 구분·특허·링킹 미처리).
- 더 결정적: 우리는 **Wine(LGPL)** 위에 서 있어 GPL/LGPL이 **"추가 제약 금지"**. NC 족쇄를
  덧씌우면 GPL 위반 → 프로젝트 전체 NC화는 **법적으로 불가**.
- "상업적 사유화 방지"는 **GPLv3가 이미 해결**(상업 이용은 가능하나 소스공개 강제 →
  독점 캡처 불가). 별도 NC 불필요.

### D3DMetal 처리 — 지원하되 비동봉 (GPL 준수의 핵심)
- D3DMetal 라이선스 = **평가/비상업 배포만 허용, 상업 재배포 금지**. NC 전용이라
  GPLv3 배포물에 **동봉하면 충돌**.
- 해법(= Sikarugir 패턴): D3DMetal 바이너리를 **우리 레포/배포물에 넣지 않고**, 앱이
  **사용자 맥에서 외부 컴포넌트로 받아** Wine DLL 플러그인으로 로드. 별도 배포 =
  "단순 결합", 설치 주체 = 사용자 본인(Apple 라이선스 범위). → 앱 코어 GPLv3 청정 유지.
- **장기 트랙**: Apple에 **정식 번들 허가**를 요청(오너 직접 추진). 허가 확보 시
  외부 다운로드 없이 직접 동봉 가능. 안티치트 애드보커시(§9)와 같은 결의 "정식 허용" 전략.

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

## 9. 정식 허용 애드보커시 (안티치트 + D3DMetal)

> 공통 원리: **우회·해킹이 아니라 "정식 허용 요청".** 깨끗한 오픈소스 평판 +
> 수요 데이터가 협상 자격이다. 두 개의 트랙이 같은 전략을 공유한다.

### 트랙 A — 안티치트

#### 현재 (2026-06 기준)
- CrossOver 26(2026-02)부터 EAC/BattlEye/nProtect 일부 지원 → **게임사 허용분 한정**.
- **오버워치 2**: 블리자드 자체 안티치트(Defense Matrix) → 미지원 확정. 현재 범위 밖.
- **RDR 온라인**: 안티치트 → 미지원. 단 **스토리모드는 지원 가능**.
- 본 프로젝트는 안티치트 우회를 **절대 하지 않는다**.

#### 선례 — 오픈소스가 안티치트 정식 허용을 받은 실제 사례 (입증됨, 2026-06 확인)

질문: "이런 오픈소스 컨버팅 프로젝트에 허가가 난 용례가 있나?"
→ **있다. 그리고 그 선례 자체가 오픈소스(Proton)다.**

- **2021**: Epic이 **EAC(Easy Anti-Cheat)의 Wine/Proton 공식 지원** 발표 → 익일
  **BattlEye도 Proton 지원** 확인. Proton = Wine 기반 오픈소스.
  (출처: [Phoronix](https://www.phoronix.com/news/BattlEye-Proton-Steam-Deck),
  [itsfoss](https://itsfoss.com/news/easy-anti-cheat-linux/))
- **2026-02**: **CrossOver 26**이 Mac에서 안티치트 일부 돌파 — Helldivers 2 /
  Starfield / God of War Ragnarök 등 동작. EAC가 Mac용 개발자 옵트인으로 확장.
  (출처: [wineformac.org](https://wineformac.org/news/blog-crossover-26-anti-cheat-2026.html),
  [CodeWeavers](https://support.codeweavers.com/anti-cheat))

**허가 메커니즘 (핵심 — 우리는 "수혜자"지 "수령자"가 아니다):**
1. 안티치트 **벤더**(EAC/BattlEye)가 SDK에 "Wine/Proton 호환 모드"를 추가하고,
2. 각 게임 **퍼블리셔**가 개발자 포털에서 게임별로 옵트인(몇 클릭)하면,
3. 그 공인 환경에서 그 게임만 허용된다.

→ 벤더가 "이 도구"를 축복하는 게 아니라 **환경 + 게임 단위 허용**. 우리 앱은 이게
켜지면 **자동 수혜**할 뿐, 스스로 권한을 따낼 수는 없다.

**냉정한 함의 (전략 보정):**
- 그 토글을 켜게 만든 건 **Valve/Steam Deck 규모의 시장 압력**이었다 ("Valve의 노력이
  없었으면 EAC의 Linux 지원 가능성은 낮았다" — itsfoss). → **소규모 OSS 단독
  레버리지는 미미.** 그래서 우리 무기는 "직접 요청"이 아니라 **수요·호환 데이터 축적
  + 청정 평판**으로 *수혜 조건*을 만드는 것.
- **천장**: 커널 레벨 안티치트(Riot Vanguard 등)는 Wine이 커널 심층 접근을 못 해
  **여전히 하드 차단**. 발로란트·오버워치2(Defense Matrix)는 애드보커시로도 단기 불가.
  (출처: [IVSOFTE 2026](https://ivsofte.biz/en/blog/antichity-2026-vanguard-eac-battleye-faceit/))

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
- [x] Proton의 안티치트 허용 선례·문서를 레퍼런스로 정리 (→ 위 "선례" 블록, 2026-06)

### 트랙 B — D3DMetal 정식 번들 허가

- **현재**: D3DMetal은 NC 전용이라 우리 GPLv3 배포물에 동봉 불가 → 비동봉 외부
  컴포넌트로만 사용(§5).
- **목표**: Apple에 **정식 번들/배포 허가**를 요청해, 외부 다운로드 없이 직접 동봉.
  근거: 이미 Apple이 D3DMetal을 배포 중이고 Sikarugir 등이 NC로 동봉 중 → 오픈소스
  포팅킷에 대한 허용은 자연스러운 확장.
- **추진**: 프로젝트 오너가 직접 Apple과 커뮤니케이션(향후).
- [ ] Apple Developer 채널로 GPTK/D3DMetal 라이선스 담당 접점 파악
- [ ] 오픈소스·비상업 배포용 D3DMetal 예외 허가 요청서 초안

---

## 10. 미해결/결정 대기

- [ ] PoC 첫 게임 확정 (RDR2 설치본 유무 / 대체 DX11 타이틀)
- [ ] Sikarugir 포크 방식: 전체 vendoring vs submodule vs 정식 GitHub fork
- [ ] 프로파일 레포 분리 시점 (이 레포 내 폴더 vs 별도 레포)
- [ ] 앱 이름/번들 ID 확정
- [ ] 엔진 아키텍처: x86-under-Rosetta(현행) 유지 vs ARM 네이티브 하이브리드 채택 — RDR2 실측 후 결정. §13

### 확정된 결정
- [x] **라이선스 = GPLv3** (확정). CC BY-NC-SA는 검토 후 폐기(소프트웨어 부적합 +
  Wine LGPL과 충돌). 상업 사유화 방지는 GPL 카피레프트로 충족. (§5)
- [x] **D3DMetal = 백엔드로 지원 + 비동봉 외부 컴포넌트**. 앱 코어는 GPLv3 유지.
  Apple 정식 번들 허가는 별도 애드보커시 트랙(§9 트랙 B). (§3, §5)
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

---

## 12. 외부 리소스 & 라이선스 (함정 방지용)

라이선스 실수는 까먹기 쉬워 한 곳에 모은다. **원칙: "참고·링크 ✅ / 통째 복사 ❌".**

| 리소스 | 라이선스 | 우리한테 | 사용 방침 |
|---|---|---|---|
| [apple/game-porting-toolkit](https://github.com/apple/game-porting-toolkit) | **Apache 2.0** | ✅ GPLv3 호환 | docs·samples·metal-cpp·**에이전트 스킬** 참고·활용 가능. AGENTS.md 레퍼런스 |
| GPTK 문서·노하우 (Wine 설정·env·이슈) | 지식 | ✅ | 프로파일 DB·판정 로직에 반영 |
| **D3DMetal** | 평가/**비상업 배포만**, 상업 재배포 금지 | 🟡 조건부 | **비동봉 외부 컴포넌트로만** 사용(§5). 동봉은 Apple 허가 후(§9 트랙 B) |
| Metal Shader Converter | 독점 도구, 생성 metallib 출력은 동봉 허용 | 🟡 한계적 | 지금 불필요(오픈 스택이 셰이더 처리). 장기 사전컴파일 최적화 후보 |
| [AppleGamingWiki](https://www.applegamingwiki.com/wiki/M1_compatible_games_master_list) 호환성 리스트 | **CC BY-NC-SA** | 🟡 링크만 | 표 스크랩 금지(NC+SA → GPL 충돌). **링크·참고**만. 우리 오픈 스택 결과는 자체 검증·역기여 |
| GameKit | 독점 프레임워크 | ❌ 범위 밖 | 네이티브 Game Center용. Wine 위 Windows 게임과 무관. 사용 안 함 |
| GPTK 설치 번들 전체 | 평가 전용 | ❌ | 배포 불가. 로컬 벤치마크 기준선으로만(내부 평가 범위) |

**프로파일 DB 데이터**: 외부 표 복사 대신 **자체 생산**(커뮤니티 PR), GPL 친화
라이선스(CC0/MIT 등)로 둔다. AppleGamingWiki는 교차 링크·검증용으로만.

---

## 13. 엔진 아키텍처 전략 — x86-under-Rosetta vs ARM 네이티브 (토론 기록)

> 출처: 오너 설계 토론(2026-06-30). "변환 횟수를 줄일 수 있나?"에서 출발.
> **RDR2 P0 실측 후 재논의 예정.**

### 13.1 변환은 3개의 독립 축 — CPU 변환은 1회뿐 (횟수 오해 정정)

스택은 "여러 번 변환"처럼 보이지만 서로 다른 축이고, CPU 변환은 한 번이다.

| 축 | 변환 | 담당 | 비고 |
|---|---|---|---|
| CPU ISA | x86_64 → ARM64 | **Rosetta 2** | **CPU 명령어만**. 게임이 x86이라 불가피 |
| OS API | Win32 → macOS | Wine | 프로젝트 목적 자체 |
| 그래픽 API | DirectX → Metal | D3DMetal/DXMT/vkd3d | API 통역 — CPU 재변환 아님 |

**GPU엔 x86이 없다** → Metal 셰이더/커맨드는 어떤 모델이든 Apple GPU에서 네이티브
실행. Rosetta가 닿는 건 D3DMetal의 *CPU측 변환 코드*뿐. → D3DMetal(DX12 직통)을
고른 순간 **그래픽 축은 이미 최소 홉**. "횟수"로는 더 줄일 게 없음(네이티브 포팅 외).

### 13.2 진짜 레버 = "Rosetta가 무엇을 번역하느냐"

- **현행 (Sikarugir 기본)**: 게임 + Wine + 백엔드 **전부 x86 → 통째로 Rosetta**.
  단순·견고. (macOS "Intel 앱 지원 종료" 경고의 출처)
- **이상 (WoW64 하이브리드 = ARM 네이티브 Wine)**: **게임 x86 코드만 Rosetta**,
  Wine 호스트 + 백엔드는 ARM64 네이티브. CrossOver 최신이 가는 길.

### 13.3 핵심 난점 = x86↔ARM 경계 위치 (thunk 빈도)

ARM이 무조건 빠른 게 아니라 **경계 thunk 빈도**가 좌우한다.

| 경계 위치 | 장점 | 단점 |
|---|---|---|
| 백엔드를 ARM 네이티브로 (경계 낮게) | 무거운 변환이 네이티브로 빠름 | D3D 콜 프레임당 수천 → **thunk 폭증** |
| 백엔드까지 x86, Metal 핸드오프만 ARM (높게) | thunk 적음(Metal 제출은 굵음) | 변환 로직이 Rosetta로 |

DirectX = 잘고 잦은 API → 양쪽 동일 아키텍처가 유리. 최적 경계 튜닝 = CrossOver가
돈 들여 푸는 영역.

### 13.4 오픈 vs 기업비밀 (스코프 핵심)

"ARM 네이티브 엔진을 골라 쓰자"의 현실 — **비밀인 부분과 아닌 부분을 가른다**:

- **CrossOver의 하이브리드 튜닝** = 독점·비공개. 크립 불가.
- **그러나 오픈 경로는 비밀이 아니다**:
  - **Wine mainline**: wow64 + ARM64 PE 지원이 이미 본류(LGPL, 공개). CrossOver보다
    덜 최적화됐을 뿐 *존재*한다.
  - **오픈 백엔드(DXMT/DXVK/vkd3d + MoltenVK)**: 전부 오픈소스 → ARM 네이티브 빌드는
    **"비밀"이 아니라 "공수"의 문제**.
  - **D3DMetal**: Apple 독점 → **아키텍처는 Apple 소관**, 우리가 못 바꾼다. 주는
    대로(현재 x86) 소비.
- **결론**: D3DMetal 경로의 ARM화 = **Apple 대기**. 오픈 경로의 ARM화 = **우리가
  상류와 함께 추진 가능**(단 CrossOver 대비 성능 랙 감수).

### 13.5 결정 포지션

- ❌ **직접 엔진 개발 안 함** — PLAN §1 Non-goal(번역 레이어 신규 개발 금지)과 충돌,
  CrossOver급 공수.
- ✅ **상류 ARM 네이티브 엔진이 성숙하면 채택** (오픈 경로 우선).
- ✅ **가치는 측정으로 증명** — 동일 게임 x86 vs ARM 엔진 전후 비교 → §6 프로파일에
  "이 게임, ARM 엔진 N% 우위" 기록. 이게 우리 차별점.
- ⏳ **상태: 미결. RDR2 베이스라인(P0) 후 재논의.**

---

## 14. 파괴적 옵션 검토 — "오픈 DX12→Metal을 새로 쓴다?" (moonshot 분석)

> 출처: 오너 토론(2026-06-30). "물 버그를 우회만 말고, 아예 D3DMetal을 새로 쓰는
> 파괴적 해결은 없나?"에서 출발.
> **결론: 풀 레이어 *소유*는 비현실. 단 "코어 PoC"는 솔로 스코프로 시도 가치 있음
> → `experiment/dx12-metal-poc` 브랜치에서 스파이크.**

### 14.1 가능한가? — YES, 이미 출하 중

- **D3DMetal**(Apple) = 작동하는 DX12→Metal. **vkd3d-proton + MoltenVK** =
  DX12→Vulkan→Metal. → 두 경로로 입증. "가능성"은 닫힌 문제.
- DX12와 Metal은 **둘 다 현대적 explicit 저수준 API** → DX11→Metal보다 궁합이 좋음.
  CommandList→CommandBuffer, PSO→PipelineState, **DescriptorHeap→ArgumentBuffer** 등 거의 1:1.
- 셰이더: **DXIL→MSL** 변환은 Apple **Metal Shader Converter**가 이미 해결(공식 도구).

### 14.2 깨끗하진 않다 — "에뮬레이션 꼬리"

Metal에 대응 단계가 없는 소수 기능은 우회 구현 필요(여기가 버그·성능손실의 소굴):
- **테셀레이션**(hull/domain) → compute 에뮬 (RDR2 *파도* 의심 지점)
- **지오메트리 셰이더** → compute 에뮬, 느림
- **배리어/메모리·TBDR read-back** → 세심한 매핑 (RDR2 *반사* 의심 지점)

### 14.3 난이도 calibration — "머리"가 아니라 "꼬리"가 어렵다

| 구간 | 난이도 | 기간(숙련자) |
|---|---|---|
| 코어 80% (API 매핑·셰이더 변환) | 🟢 알려진 길 | **수개월** |
| 에뮬 꼬리 20% (테셀·GS를 *빠르게*) | 🟡 미니 연구과제 | 수개월~수년 |
| 게임별 정합성·성능 (수천 타이틀) | 🔴 사실상 안 끝남 | **영원** |

증거: vkd3d=Valve 펀딩 수년 / DXMT=DX11에 ~수년(이제 CodeWeavers 합류) /
**D3DMetal=Apple 팀 수년인데도 RDR2 물 버그 잔존.** → 꼬리는 팀·시간·돈을 영구 소모.

### 14.4 파괴력 vs 현실성 사다리

| 옵션 | 파괴력 | 현실성 | 판정 |
|---|---|---|---|
| DX12→Metal **greenfield 완전 작성** | 🌋 최대 | ❌ 수년·Apple과 경쟁·§1 위반 | 단독 불가 |
| **오픈 DXMT를 DX12로 확장** | 🔥 매우 큼 | 🟡 0에서 시작 아님 | 장기 생태계 기여 |
| **스트리밍 우회** (PC/클라우드→Mac) | ⚡ 번역 제거 | ✅ 이미 존재(GFN) | 다른 제품, 정체성 밖 |
| **지능·데이터 레이어 차별화** | 💡 제품 차원 | ✅ 소팀이 이기는 곳 | **진짜 레버 (§11)** |

### 14.5 결론 + 스파이크 결정

- ❌ **풀 DX12→Metal 레이어 "소유"는 안 한다** — §1 Non-goal 위반 + 꼬리 비용 영구.
- ✅ **방향 1**: DXMT(오픈·직통·테셀 막 해결)에 **기여/가속**. DXMT가 DX12를 뚫으면
  우리 앱은 자동 수혜.
- ✅ **방향 2 (본체)**: 번역기로 싸우지 말고 **지능 레이어(auto-backend·프로파일·판정
  UX)를 소유**(§11). Apple·CrossOver도 여기선 못 이김.
- 🧪 **단, "코어 PoC"는 솔로 스코프 스파이크로 시도 가치 있음**:
  - 목표: DX12 호출 → Metal로 **삼각형/간단한 DX12 앱 1개** 띄우기 (꼬리 미포함).
  - 도구: Metal Shader Converter(DXIL→MSL), metal-cpp, GPTK 샘플 참고(Apache 2.0, §12).
  - 위치: **`experiment/dx12-metal-poc`** 브랜치. **풀 레이어 약속 아님** — 학습·타당성 스파이크.
