# DX12 → Metal — 코어 PoC 설계 (v1)

> 근거: [RESEARCH.md](RESEARCH.md) · 스코프: [NOTES.md](NOTES.md) · [PLAN.md](../../PLAN.md) §14
> 목표: "DX12 호출 → Metal로 삼각형/간단 텍스처" 를 **솔로 스코프**로 증명. 긴 꼬리 제외.

---

## 1. 접근 — Wine 없이, 최소 D3D12 콜 표면만 번역

PoC는 **Wine를 끼우지 않는다**(RESEARCH §5: PE/Unix 통합은 풀레이어용, PoC 밖).
대신 **standalone macOS C++ 앱**으로:

```
HelloTriangle 드라이버 (D3D12 스타일 호출)
   │   ID3D12Device / CommandQueue / CommandList / PSO / RootSignature / VertexBuffer / Draw
   ▼
d3d12_to_metal 코어 (얇은 번역 façade)   ← 우리가 구현하는 부분
   │   metal-cpp (MTL::)
   ▼
Metal (Apple GPU)
```

- **입력 기준점**: Microsoft `DirectX-Graphics-Samples`의 **HelloTriangle / HelloTexture**가
  쓰는 최소 D3D12 호출 집합만 대상으로 한다(그 이상은 PoC 밖).
- **셰이더 경로**: HLSL → `dxc`(DXIL/SM6) → **Metal Shader Converter** → `.metallib` → `MTL::Library`.
- **DXMT식 분리 차용**: `Device`(리소스 생성) / `Context`(인코딩) 분리, 인코딩은 argument buffer 기반
  (RESEARCH §1). 단 PoC는 단일스레드 — DXMT의 3-스레드 청크 모델은 **미도입**(꼬리).

### 왜 이 형태인가
- 번역의 **두 불확실성(커맨드/PSO/바인딩 매핑 + DXIL 셰이더 파이프)** 을 둘 다 실증한다.
- 실제 게임·Wine·배리어·디스크립터힙 없이도 "DX12 의미론 → Metal" 의 코어가 도는지 본다.

---

## 2. 최소 콜 매핑 (HelloTriangle 기준)

| HelloTriangle의 D3D12 | d3d12_to_metal 구현 | Metal |
|---|---|---|
| `D3D12CreateDevice` | `Device` 생성 | `MTL::CreateSystemDefaultDevice()` |
| `CreateCommandQueue` | | `device->newCommandQueue()` |
| `CreateSwapChain` (DXGI) | 창/표면 | `CA::MetalLayer` (AppKit/SDL 창) |
| `CreateCommandAllocator`+`CreateCommandList` | | `queue->commandBuffer()` + `MTL::RenderCommandEncoder` |
| RTV heap + `OMSetRenderTargets` | | `MTL::RenderPassDescriptor` colorAttachment = drawable.texture |
| `CreateRootSignature` | 루트시그 보관 | (M3) MSC explicit layout 입력 / (M1·M2) 미사용 |
| `CreateGraphicsPipelineState` | PSO 번역 | `MTL::RenderPipelineState` (vtx/frag fn + color format) |
| `CreateCommittedResource`(vertex buffer) | | `device->newBuffer(...)` (StorageModeShared) |
| `IASetVertexBuffers` | | `encoder->setVertexBuffer(buf, 0, idx)` |
| `RSSetViewports`/`Scissor` | | `encoder->setViewport/ScissorRect` (인코더 동적) |
| `DrawInstanced` | | `encoder->drawPrimitives(Triangle, 0, 3)` |
| Fence/`Signal`/`Wait` | | `MTL::SharedEvent` (또는 PoC는 commandBuffer->waitUntilCompleted) |
| Present | | `commandBuffer->presentDrawable(); commit()` |

> depth/stencil은 삼각형에 불필요 → 생략. PSO에 color format만.

---

## 3. 마일스톤별 구현 계획

### M0 — 환경 (수동 의존성)
- [ ] `metal-cpp` 헤더 번들 다운로드 (developer.apple.com/metal/cpp) → `third_party/metal-cpp/`
- [ ] **Metal Shader Converter** 설치 (developer.apple.com/download, "Metal Shader Converter") → `metal-shaderconverter` CLI 확인
- [ ] `dxc` 확보 (DirectXShaderCompiler; macOS 빌드 또는 Metal Dev Tools 동봉분)
- [ ] CMake + 빌드 (Xcode 16 toolchain 보유 ✅)
- 검증: `MTL::CreateSystemDefaultDevice()` 가 non-null (M4/macOS26, Tier2 ✅).

### M1 — clear color
- 창(`CA::MetalLayer`) + command queue + 매 프레임 render pass(clearColor) → present.
- D3D12측: device/queue/swapchain/commandlist/RTV clear 만.
- **성공 기준**: 단색 화면이 뜬다.

### M2 — 셰이더 파이프 (가장 불확실한 절반)
- `triangle.hlsl`(VSMain/PSMain, SM6.0) → `dxc -T vs_6_0 -Fo triangle_vs.dxil` → `metal-shaderconverter triangle_vs.dxil -o triangle_vs.metallib`.
- 런타임: metallib 바이트 → `device->newLibrary(data)` → `MTL::Function`.
- **성공 기준**: 두 스테이지 metallib 로드 + PSO 생성 성공(아직 미드로우).

### M3 — 삼각형 (1차 성공 기준)
- vertex buffer(3 정점, pos+color) → PSO(vtx/frag + drawable format) → drawPrimitives.
- **성공 기준**: 컬러 삼각형 렌더. ← PoC 1차 목표 달성.

### M4 — 텍스처 (스트레치)
- HelloTexture 경로. **MSC 바인딩 규약 필수**(RESEARCH §7):
  - 변환 셰이더가 기대하는 **TLAB를 buffer index 2**(`kIRArgumentBufferBindPoint`)에 바인드.
  - 디스크립터 테이블 엔트리 = `IRDescriptorTableEntry{uint64 gpuVA; uint64 textureViewID; uint64 metadata}` 24B stride.
  - `IRDescriptorTableSetTexture/Sampler` 헬퍼 사용. 샘플러 `supportArgumentBuffers=YES`.
  - 셰이더 컴파일 시 explicit root signature 전달(`IRCompilerSetGlobalRootSignature`)로 TLAB 레이아웃 1:1.
- **성공 기준**: 텍스처 입힌 사각형 렌더.

---

## 4. 디렉터리 구조 (예정)

```
experiments/dx12-metal-poc/
  CMakeLists.txt
  third_party/metal-cpp/        # 사용자가 드롭(.gitignore)
  src/
    d3d12_to_metal/             # 번역 코어
      device.{h,cpp}            # Device: 리소스/PSO 생성
      context.{h,cpp}           # Context: 인코딩
      shader.{h,cpp}            # DXIL→metallib 로드 (MSC 산출물)
    samples/
      hello_triangle/main.cpp   # M1~M3 드라이버
  shaders/
    triangle.hlsl
  tools/build_shaders.sh        # hlsl→dxil→metallib 파이프
```

---

## 5. 결정 로그 (코히런트)

- **D-1 셰이더: PoC=Metal Shader Converter(실용·빠름), 풀레이어=dxil-spirv 프론트엔드 재사용(오픈).**
  MSC는 독점이라 동봉 가능 오픈레이어 동기와 충돌(RESEARCH §3.3) → PoC 학습용으로만, 장기엔 교체.
- **D-2 Wine 미도입** — PoC는 standalone. PE/Unix(winemetal.so) 패턴은 풀레이어용.
- **D-3 배리어 미구현** — Metal 자동 hazard tracking에 의존(삼각형/텍스처엔 충분). D3D12 명시 배리어·untracked 고속화는 꼬리.
- **D-4 단일스레드** — DXMT 3-스레드 청크 모델 미도입.
- **D-5 입력 표면 = HelloTriangle/HelloTexture 최소집합** — 그 이상 D3D12 API는 PoC 밖.

## 6. 리스크 / 검증 선행
- ⚠ MSC 바인딩 규약은 `metal_irconverter_runtime.h` **실물 헤더로 재확인** 후 M4 착수(RESEARCH §8).
- ⚠ `dxc`의 macOS/ARM 빌드 가용성 확인(M0). 대안: Metal Dev Tools 동봉 dxc.
- ⚠ metallib 버전/GPU family 타깃을 M4/macOS26에 맞춤(`IRCompilerSetMinimumGPUFamily/DeploymentTarget`).
