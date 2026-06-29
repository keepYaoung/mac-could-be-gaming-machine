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
      (Apple M4 / Argument Buffers Tier2 / Apple7 ✅). *MSC·dxc는 M2에서.*
- [x] **M1 — clear**: 오프스크린 텍스처에 clear → PPM. 검증 center=26 102 204(=0.1/0.4/0.8). ✅
- [ ] **M2 — 셰이더 번역 (진짜)**: HLSL→`dxc`→DXIL→**Metal Shader Converter**→metallib 로드.
      ⛔ **dxc + metal-shaderconverter 설치 필요** (다음 블로커).
- [~] **M3-prime — 삼각형 하버스트**: 정점버퍼+PSO+draw, 오프스크린, **MSL 셰이더로** 검증
      (colored_px=16200). ✅ *단 셰이더 번역은 미증명 — M2에서 metallib로 swap.*
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
