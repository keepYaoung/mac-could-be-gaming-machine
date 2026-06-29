# DX12 → Metal — 리서치 종합 (정합성 패스 v1)

> 브랜치: `experiment/dx12-metal-poc` · 근거: [PLAN.md](../../PLAN.md) §14
> 방법: 6개 영역 병렬 리서치(DXMT / vkd3d-proton / Metal Shader Converter+metal-cpp /
> D3DMetal·GPTK / DX12↔Metal 기능매핑 / 선행연구·자료) → 교차검증 → 본 종합.
> 신뢰도 태그: **[H]** 1차/벤더 직접 또는 다중 교차확인 · **[M]** 단일 신뢰 2차 · **[L]** 추론/단일 약함.
> 원자료(verbatim 브리프)는 세션 scratchpad의 `raw-facet*.md`에 보관.

---

## 0. 한 줄 결론

**DX12→Metal은 가능하다(D3DMetal·vkd3d가 입증). 진짜 난점은 "변환 가능성"이 아니라
①바인딩 모델 ②배리어를 *빠르게* ③DXIL 셰이더 ④TBDR 정합성 ⑤긴 꼬리(GS/RT/ExecuteIndirect)다.
그리고 우리 PoC(삼각형→간단 텍스처)는 이 다섯 중 거의 아무것도 필요 없다 → 솔로 스코프로 충분히 도달.**

---

## 1. 지형도 — 누가 무엇을 하는가

| 프로젝트 | 경로 | 오픈? | 셰이더 | 우리 관련성 |
|---|---|---|---|---|
| **D3DMetal** (Apple GPTK) | DX11/12 → Metal **직통** | ❌ 독점(.framework ~134MiB) | Metal Shader Converter (DXIL→metallib) | 벤치마크·존재증명. 동봉 불가(평가 라이선스) [H] |
| **DXMT** (3Shain) | **D3D11/10** → Metal 직통 | ✅ LGPL-2.1, CodeWeavers backed | 자체 `airconv` (DXBC/SM5→AIR, LLVM15) | **가장 가까운 오픈 코드베이스. 단 DX12 미지원** [H] |
| **vkd3d-proton** | DX12 → Vulkan | ✅ | `dxil-spirv` (DXIL→SPIR-V) | **DX12 시맨틱 레퍼런스. 프론트엔드 재사용 가능** [H] |
| **MoltenVK** | Vulkan → Metal | ✅ | SPIR-V→MSL | Vk-on-Metal 정석. 체인의 Metal 백엔드 [H] |
| **MS D3D12TranslationLayer / D3D11On12** | (DDI→D3D12) | ✅ MIT | — | **번역기 아키텍처 청사진**(상태추적·디스크립터/힙 관리) [H] |

**오픈 DIRECT DX12→Metal은 현재 존재하지 않는다.** 두 개의 오픈 경로뿐:
- **(A) DXMT(D3D11→Metal)를 DX12로 확장** + Metal Shader Converter 또는 자체 DXIL 프론트엔드
- **(B) vkd3d-proton → MoltenVK → Metal** 체인 (오늘 특정 갭으로 막힘, §6)

---

## 2. 마스터 매핑표 (DX12 → Metal)

| DX12 | Metal 대응 | 난이도 | 핵심 갭 |
|---|---|---|---|
| Command queue / list | `MTLCommandQueue` / `MTLCommandBuffer`+encoder | 중 | 타입별 인코더, 커맨드버퍼는 단일제출 [H] |
| Bundle | Indirect Command Buffer (느슨) | 상 | ICB는 GPU-driven, 16384 cmd 캡; 보통 인라인 replay [M] |
| Graphics/Compute PSO | `MTLRender/ComputePipelineState` | 하~중 | depth/stencil은 별도 `MTLDepthStencilState`; 포맷이 PSO에 baked [H] |
| Root constants | `setBytes` | 중 | 부분 갱신 불가, <4KB, 인코더당 단일 [M] |
| Descriptor heap/table | **Argument Buffer (Tier2)** + `MTLArgumentEncoder` | 상 | 타입소거 vs 타입드; **수동 residency** [H] |
| Root signature(전체) | 단일 객체 없음 — AB+직접바인드+setBytes 조합 | 상 | 바인딩 모델 전면 재배치 [H] |
| Bindless / SM6.6 dynamic | Argument Buffer(Tier2) + `useResource`/`MTLResidencySet` | 중 | 간접참조 자동 residency 없음 [H] |
| Resource barrier | 자동 hazard tracking; `MTLFence`/`MTLEvent` | 하~중(정확) / 상(고속) | enhanced-barrier **layout 전이 대응 없음**; tracked는 over-sync [H] |
| Heap / placed resource | `MTLHeap`(placement/automatic), storage modes, `makeAliasable` | 중~상 | byte-exact placement는 perf 손해; `memoryless`/`managed` 비대칭 [H] |
| **Tessellation** (HS+DS) | compute커널 → 고정 tessellator → post-tess vertex fn | 상 | hull 단계 없음→compute로; **factor 16 vs 64**; isoline 없음 [H] |
| **Geometry shader** | 네이티브 없음 — compute/mesh/vertex-amp 에뮬 | **최상** | 데이터의존 확장의 일반 매핑 없음 [H] |
| Mesh + amplification | mesh fn + object fn (`MTLMeshRenderPipelineDescriptor`) | 중 | object→mesh 팬아웃 1024 vs ~4.19M; Metal3/Apple Silicon 전용 [H] |
| **Ray tracing (DXR1.0 full)** | inline `intersector` + intersection/visible fn table | **최상** | RT 파이프라인·SBT·재귀·hit/miss 스테이지 없음 [H] |
| Ray tracing (DXR1.1 inline RayQuery) | `intersection_query` inline | 중 | 실행모델 정렬, 가장 깔끔 [H] |
| **TBDR mid-pass framebuffer read** (SSR/굴절) | 패스 분할+resolve / programmable blending(동일픽셀만) | 상 | **#1 정합성 함정 — 에러 아닌 아티팩트** [H] |

---

## 3. 핵심 난점 심층 (정합성 주의점 포함)

### 3.1 바인딩 모델 = THE 크럭스 (단, Metal이 Vulkan보다 유리)
- vkd3d 교훈: **D3D12 디스크립터 힙은 Metal argument buffer에 거의 1:1로 떨어진다** — Metal은
  본래 타입소거·bindless라, Vulkan에서 겪던 mutable-descriptor-type·VOLATILE 고통이 대부분 사라진다.
  힙을 `MTLBuffer` of `MTLResourceID`로 모델링. [H]
- ⚠ **정합성 주의:** 개념 매핑은 자연스럽지만(매핑표 난이도 "상"의 이유는 **residency 관리**다).
  Metal은 간접참조 리소스를 자동으로 resident 유지하지 않음 → `useResource`/`useHeap`/`MTLResidencySet`을
  정확히 구동해야 함. "비-resident 접근 = GPU 재시작의 흔한 원인". [H]
- **결론:** *개념*은 Metal이 더 쉽고, *노동*은 residency에 있다.

### 3.2 배리어 — "정확하게는 쉽다, 빠르게는 어렵다" (모순 해소)
- 표면 모순: 매핑표는 배리어 "하~중"(대부분 D3D12 transition이 tracked 모드에서 **no-op**) [H].
  반면 DXMT/K0bin은 "D3D12 배리어 모델은 Metal 위에 잘 구현 못 한다"며 D3DMetal이 **over-sync**한다고 함 [H].
- **해소:** 둘 다 참. `MTLHazardTrackingModeTracked`로 두면 **정확하게 동작하지만 과동기화(perf 손해)**.
  D3D12 수준 성능을 내려면 리소스를 **untracked로 두고 수동 `MTLFence`** 관리 — 이게 어려운 쪽.
  → DXMT가 D3D11에서 성공한 비결(자동 tracking에 기댐)이 D3D12에선 perf 한계가 됨. [H/M]
- enhanced-barrier의 **layout 전이는 Metal에 대응 없음**(Metal이 내부 관리) → 번역 시 layout 필드 폐기. [H]

### 3.3 셰이더 전략 — 세 갈래, 오픈성과 충돌 (중요 결정점)
입력은 **DXIL/SM6**(DX12 표준). 선택지:
1. **Apple Metal Shader Converter** (DXIL→metallib 직접). 가장 빠른 길, tess/geom/mesh/RT 스테이지 지원.
   ⚠ **그러나 독점 컴포넌트** — 오픈·동봉 가능 레이어라는 우리 동기(§5/§14)와 *다시* 충돌. DXMT가 의도적으로
   거부한 이유가 이것("또 하나의 독점 조각 회피"). [H]
2. **DXMT `airconv`** (DXBC/SM5→AIR). 오픈이지만 **SM5/DXBC 입력** — DX12의 SM6/DXIL 아님. 프론트엔드 교체 필요. [H]
3. **vkd3d `dxil-spirv` 프론트엔드 재사용** (DXIL→LLVM bitcode 파서 + **CFGStructurizer** + ResourceRemappingInterface).
   백엔드만 SPIR-V→AIR/MSL로 교체. **CFG 구조화기가 가장 가치 있는 재사용 자산**(둘 다 structured CF 요구). [H]
- **결정(코히런트):** *PoC 단계*는 **Metal Shader Converter로 실용 우선**(삼각형 빨리 띄움). *장기 오픈 레이어*는
  **dxil-spirv 프론트엔드 + 자체 AIR/MSL 백엔드**로 독점 의존 제거. (= PLAN §14 "PoC는 학습, 풀레이어는 별개" 그대로)

### 3.4 TBDR 정합성 — RDR2 물버그와 직결
- Apple GPU는 **타일기반 지연렌더(TBDR)**. 렌더타깃은 패스 중 **타일 메모리**에 있고 패스 끝에만 시스템 메모리로 flush.
  → 즉시모드 가정으로 **패스 도중 프레임버퍼를 SRV로 샘플(SSR·굴절·왜곡)** 하면 **stale/미초기화 메모리**를 읽어
  **아티팩트(에러 아님)** 발생. [H]
- 해법: (동일픽셀) programmable blending + memoryless / (교차픽셀) 인코더 종료→resolve→새 패스에서 샘플. [H]
- **RDR2 물버그 연결(정합성 명시):** D3DMetal의 RDR2 물 반사/파도 버그에 대한 **공개 root-cause는 없음** [H].
  단 (a) CodeWeavers 문서: Metal은 테셀레이션을 다르게 처리하고 **지오메트리 셰이더·transform feedback 없음** [H],
  (b) TBDR mid-pass read 함정 [H] → **테셀레이션 갭 + TBDR read-back이 근거 있는 *가설*** (RDR2 특정 확증 아님 [L]).
  AppleGamingWiki가 실제 문서화한 RDR2 Apple Silicon 아티팩트는 "물"이 아니라 **그림자(눈 덮인 산)** [H].

### 3.5 긴 꼬리 (PoC 제외, 풀레이어의 진짜 비용)
가장 어려운 5개(매핑+vkd3d 교차): **① 지오메트리 셰이더(네이티브 없음) ② DXR1.0 풀 파이프라인(SBT/재귀 없음)
③ 디스크립터-힙 바인딩+residency ④ 테셀레이션(factor 16 vs 64) ⑤ TBDR mid-pass read.**
- ExecuteIndirect → `MTLIndirectCommandBuffer`(per-dispatch root 변경이 최난; ICB가 Vk DGC보다 1급이나 캡 16384). [H]
- D3DMetal은 **GS·테셀까지 번역함**(WWDC23 *The Medium* "pixel-perfect" 시연) → "가능, 단 엔지니어링 비용". [H]

---

## 4. 하드웨어/플랫폼 천장 (못 넘는 선)
- **리소스 캡: Metal ~500,000/argument buffer vs D3D12 Tier-2 ~1,000,000** → 풀 DX12 일부 게임 차단. PoC·다수 게임엔 무관. [H]
- **GPU VA / Buffer Device Address: Apple 미지원** → DXR류·VA기반 root descriptor 곤란. vkd3d식 **VA→리소스 맵** 우회 필요. [H]
- **mesh/GS-via-mesh·일부 기능: Metal3 + Apple Silicon(Apple7/8+) 전용** (Intel/구형 불가). [H]
- DXMT 사례상 **Rosetta(Intel) 경로는 사양길** — Apple Silicon 1급. [H]

---

## 5. 재사용 가능한 오픈 자산 (바퀴 재발명 금지)
1. **vkd3d `dxil-spirv` 프론트엔드** — LLVM-bitcode 파서(자체 경량 LLVM 서브셋) + **CFGStructurizer** +
   ResourceRemappingInterface. DXIL 파싱·구조화는 백엔드 불문 100% 재사용. **가장 큰 자산.** [H]
2. **MS `D3D12TranslationLayer` / `D3D11On12` / `D3D9On12`** — 상태추적·디스크립터/힙 관리·DDI 커맨드 매핑의
   *동작하는* 아키텍처 청사진(MIT). 호스트 런타임 설계 직접 참고. [H]
3. **DXMT의 PE/Unix 패턴** — `winemetal.so` + `obj_handle_t` 핸들 마샬링(PE↔Unix 경계로 Metal 호출). Wine 통합 시 재사용(PoC엔 불필요). [H]
4. **DXMT 메시셰이더 테셀레이션 에뮬**(PR #90, hull/domain→단일 mesh 파이프, temp버퍼 제거). 테셀 꼬리용. [H]
5. **Metal Shader Converter** — DXIL→metallib 실용 경로(독점 트레이드오프 인지). PoC 가속. [H]
6. **vkd3d 패턴들**: timeline-semaphore 펜스→`MTLSharedEvent`; 2-tier 캐시(DXIL→IR + `MTLBinaryArchive`); GPU-side 디스크립터 검증 레이어. [H]

---

## 6. 두 오픈 경로의 막힘 (왜 아무도 아직 안 했나)
- **(B) vkd3d→MoltenVK→Metal 갭**(CodeWeavers 2021 + vkd3d #379):
  Metal ~500k 리소스(Tier2 1M 미달), **GPU VA/BDA 미지원**(RT), **dynamic vertex stride 없음**(`VK_EXT_extended_dynamic_state` 부재). [H]
- **(A) DXMT 확장 갭**: D3D11→Metal은 Metal 자동 tracking에 기대 성립. D3D12의 명시적 배리어/디스크립터힙/DXIL은
  설계 전면 변경 필요(§3.1~3.3). DXMT 스코프는 명시적으로 D3D10/11(1.0 계획에 D3D12 없음). [H]
- **결론:** 오픈 DX12→Metal은 "비밀"이라 못 한 게 아니라 **공수+소수 하드웨어 천장**이 막은 것 (PLAN §14.4와 정합).

---

## 7. 우리 PoC에 대한 함의 (← "그래서 뭘 하면 되나")

**삼각형 → 간단 텍스처 DX12 앱** 목표엔 §3의 난점이 거의 불필요:

| PoC에 필요 | 난이도 | 비고 |
|---|---|---|
| MTLCommandQueue/Buffer + RenderCommandEncoder | 하 | metal-cpp로 직행 |
| Graphics PSO (vtx+frag) | 하~중 | depth/stencil 분리만 주의 |
| Vertex buffer | 하 | |
| 셰이더: HLSL→DXC→DXIL→**Metal Shader Converter**→metallib | 중 | PoC는 MSC 실용 우선 |
| (M4) 텍스처 1장: 최소 argument buffer + `useResource` | 중 | TLAB 규약(아래) 준수 |
| 배리어/테셀/GS/RT/bindless/ExecuteIndirect | — | **PoC 제외** (auto tracking으로 충분) |

**MSC 바인딩 규약(코딩 시 필수, scratchpad facet3에 상세):**
- 변환 셰이더는 **top-level argument buffer(TLAB)** 를 **buffer index 2**(`kIRArgumentBufferBindPoint`)에서 기대.
- 디스크립터 테이블 엔트리 = `IRDescriptorTableEntry { uint64 gpuVA; uint64 textureViewID; uint64 metadata; }` = **24B 고정 stride**.
- `IRDescriptorTableSetTexture/Buffer/Sampler` 헬퍼로 채움(직접 X). 샘플러는 `supportArgumentBuffers=YES`.
- 예약 슬롯 0·2 + vertex(6)/stage-in(11) 회피. `metal_irconverter_runtime.h` 직접 확인 필요.
- **HW 게이트: Argument Buffers Tier2 + macOS14+** — 우리 M4/macOS26 충족 ✅.

**metal-cpp 주의:** C++17; ONE .cpp에 `*_PRIVATE_IMPLEMENTATION` 정의; **ARC 없음**(alloc/new/copy/Create는 `->release()`);
`NS::AutoreleasePool` 프레임당 수동; `NS::SharedPtr`+`TransferPtr/RetainPtr`로 RAII.

→ **연구 결론: PoC는 "알려진 길"의 80% 코어. M3(삼각형)까지는 솔로로 도달 가능. 꼬리는 손대지 않는다.**

---

## 8. 영역별 신뢰도 / 검증 필요 항목
- **[H] 견고:** MS DirectX-Specs/Learn, Apple WWDC/헤더, vkd3d themaister 블로그, MSC 바인딩 규약, 라이선스/천장.
- **[M] 보강 필요:** bundle↔ICB 등가, split-barrier↔fence, D3DMetal PE-vs-native DLL 분할(공개 미확정), GPTK 3.0 날짜(brew Dec-2024 신뢰).
- **[L] 가설:** RDR2 물버그의 테셀/TBDR 원인(확증 없음); DeepWiki발 DXMT 내부 심볼/스레딩(소스 직접 확인 권장); "D3D12 = 배리어모델 때문에 막힘"은 전문가 추론(공식 진술 아님).
- **다음 검증(코딩 전):** `metal_irconverter_runtime.h` 실제 헤더 정독 / WWDC23 10124·10125 시청 / dxil-spirv `CFGStructurizer` 소스 일독.

---

## 9. 주석 자료 인덱스 (핵심 URL)
**프로젝트:** DXMT github.com/3Shain/dxmt (disc #19/#15/#7, PR #90, issue #151) · vkd3d-proton github.com/HansKristian-Work/vkd3d-proton · dxil-spirv(동 저자) · MoltenVK github.com/KhronosGroup/MoltenVK · DXVK github.com/doitsujin/dxvk
**MS 번역레이어:** github.com/microsoft/D3D12TranslationLayer · D3D11On12 · D3D9On12 · DirectX-Specs(microsoft.github.io/DirectX-Specs: ResourceBinding, D3D12EnhancedBarriers, MeshShader, Raytracing) · DirectX-Graphics-Samples · DirectXShaderCompiler(DXIL.rst)
**Apple:** developer.apple.com/metal/shader-converter/ · /metal/cpp/ · MSL Spec PDF · Feature Set Tables PDF · github.com/apple/metal-cpp
**WWDC:** 23-10123/10124/10125(Bring game to Mac) · 21-10286/22-10101(bindless) · 22-10162(mesh) · 23-10128(RT) · 20-10602(TBDR) · 22-10160(metal-cpp) · 25-205/209/211/254(Metal4) · 26-356(Cyberpunk)/357(agentic MiniEngine port)
**딥다이브:** themaister.net/blog/2021/11/ (디스크립터) + DXIL→SPIR-V hell 1-5+finale · asawicki.info(memory_management_vulkan_direct3d_12, news_1754) · carette.xyz/posts/deep_dive_into_crossover/ · medium.com/@batuhanbozyel(MSC 바인딩) · CodeWeavers blog 2021/12/22(DX12 갭)
**천장 증거:** vkd3d issue #379(500k cap) · enfusion-dxgi-fix(DXGI 갭) · AppleGamingWiki RDR2/GPTK
**학습:** metalbyexample.com · Kodeco Metal by Tutorials · gzorin/sdl-metal-cpp-example(삼각형) · saxaboom(MSC Rust 바인딩)
**커뮤니티:** r/macgaming · Apple Dev Forums(metal/game-porting-toolkit/metal-cpp tags) · wine-devel(Hyperkitty) · Khronos/Vulkan Discord · Phoronix(Wine-VKD3D-Mac-MoltenVK)
