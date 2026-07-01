# DX12 → Metal — 코어 PoC 실험 노트

> 브랜치: `experiment/dx12-metal-poc`
> 근거: [PLAN.md](../../PLAN.md) §14 (파괴적 옵션 검토)
> ⚠️ **이것은 풀 번역 레이어를 만들겠다는 약속이 아니다.** "오픈 DX12→Metal이 솔로
> 스코프로 어디까지 가능한가"를 직접 확인하는 **타당성 스파이크**다. 꼬리(게임별 정합성·
> 성능)는 의도적으로 제외한다.

## 스코프 (의도적으로 작게 — §14.5)

- ✅ 목표: 최소 DX12 호출 → Metal로 화면에 **삼각형** 띄우기
- ✅ 그다음: 텍스처 + 간단 셰이더가 있는 DX12 데모 1개
- ❌ 비포함: 테셀레이션 / 지오메트리 셰이더 (= 에뮬 꼬리, §14.2)
- ❌ 비포함: 게임별 정합성·성능 튜닝 (= 영원한 꼬리, §14.3)

## 마일스톤

- [x] **M0 — 환경**: metal-cpp(`third_party/metal-cpp`) + device_probe 빌드·실행. **PASS**
      (Apple M4 / Argument Buffers Tier2 / Apple7 ✅).
- [x] **M1 — clear**: 오프스크린 텍스처에 clear → PPM. 검증 center=26 102 204(=0.1/0.4/0.8). ✅
- [x] **M2 — 셰이더 번역 (진짜)** ✅ **PASS.** HLSL→`dxc`→DXIL→**Metal Shader Converter**
      →metallib→`device->newLibrary(dispatch_data)`. `m2_translated`가 M3-prime과
      **바이트 단위 동일한 PPM**을 생성 (colored_px=16200, center=127 63 65). 셰이더 번역이
      MSL 인라인과 무손실 등가임을 증명. 발견 → **MSC 규약(정점 attribute index)**:
      HLSL user semantics는 Metal `[[attribute(N)]]`에 매핑될 때 N이 **11부터** HLSL 선언
      순서대로 배치됨 (POSITION→11, COLOR→12; 0~10은 예약). RESEARCH §7의 argument-buffer
      바인딩 규약과는 별개의 규칙 — PSO 에러메시지로 발견해 소스에 문서화.
      툴: `metal-shaderconverter 4.0.0` (`/usr/local/bin`, GPTK 동봉),
      `dxc 1.9.2602.24` (Windows 바이너리 via Sikarugir wine — DXIL은 호스트 OS 무관,
      HLSL→DXIL은 순수 컴파일 타임).
- [x] **M3-prime — 삼각형 하버스트**: 정점버퍼+PSO+draw, 오프스크린, **MSL 셰이더로** 검증
      (colored_px=16200). ✅ M2로 실제 셰이더 번역까지 증명 완료.
- [ ] **M4 — 텍스처**: 텍스처 샘플 + MSC argument-buffer 바인딩 규약(RESEARCH §7).

> 빌드(수동, cmake 불필요):
> `clang++ -std=c++17 -I third_party/metal-cpp -I src src/samples/<m>/main.cpp -framework Metal -framework Foundation -framework QuartzCore -o build/<m>`

## 매핑 메모 (DX12 ↔ Metal, §14.1)

| DX12 | Metal |
|---|---|
| CommandQueue / CommandList | CommandQueue / CommandBuffer + Encoder |
| Pipeline State Object (PSO) | MTLRenderPipelineState |
| Descriptor Heap | Argument Buffer |
| Root Signature | Argument Buffer 레이아웃 |
| DXIL 셰이더 | MSL (via Metal Shader Converter) |

## 참고 자료 — 라이선스 안전 (PLAN §12)

- ✅ **Apple GPTK 샘플** (Apache 2.0) — 참고 가능
- ✅ **metal-cpp** (Apple) — 사용
- ✅ **Metal Shader Converter** — DXIL→MSL 변환
- ✅ **DXMT 소스** (오픈, DX11) — 직통 매핑 패턴 레퍼런스
- ❌ **D3DMetal 바이너리 역분석 금지** (비공개·라이선스)

## 결정 로그

- 2026-06-30 — 브랜치 생성, PoC 스코프 확정 (삼각형까지를 1차 성공 기준으로). RDR2
  베이스라인 측정과 병행/이후 진행.
- 2026-06-30 — 리서치 종합(RESEARCH.md) + 설계(DESIGN.md) 완료.
- 2026-06-30 — **M0 PASS**(M4/Tier2/Apple7). **M1·M3-prime 구현·검증**(오프스크린+PPM, D-6).
  코드리뷰(medium) 1건 반영: 공통 헬퍼 `src/poc_common.h` 추출. 다음 = **M2(dxc+MSC 설치 후 셰이더 번역)**.
- 2026-07-01 — **M2 PASS**. dxc는 macOS 네이티브 릴리즈가 없어(공식), Windows 바이너리를
  Sikarugir wine으로 호스팅. HLSL→DXIL은 컴파일 타임이라 결과물(DXIL)은 호스트 OS 무관 —
  네이티브 dxc와 등가로 취급. m2_translated PPM이 M3-prime과 **바이트 단위 동일**하여
  translated pipeline이 무손실임을 증명. `build_shaders.sh`가 native dxc / wine dxc.exe
  둘 다 자동 지원. **다음 = M4 (텍스처 + argument-buffer 실전 규약)**.
