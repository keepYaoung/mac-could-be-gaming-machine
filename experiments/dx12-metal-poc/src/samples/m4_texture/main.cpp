// M4 — textured fullscreen quad via Metal Shader Converter argument buffers.
//
// This proves the DX12 *resource-binding* half of the translation. HLSL root
// sig here is minimal but exercises the full MSC argument-buffer scaffolding:
//   DescriptorTable(SRV(t0)), StaticSampler(s0, linear/clamp)
//
// MSC lays out three top-level argument buffers, all needed by the emitted MSL
// (verified via `strings htex_{vs,ps}.metallib`):
//   • TLAB           — top-level global argument buffer, one IRDescriptorTable-
//                      Entry per *root parameter*. Our sole root param (the
//                      descriptor table) points to the resource heap's gpuVA.
//                      Bound at kIRArgumentBufferBindPoint = 2, BOTH stages.
//   • res_desc_heap  — flat array of IRDescriptorTableEntry, one per SRV/UAV/
//                      CBV in the descriptor table. One entry here (the texture).
//                      Bound at kIRDescriptorHeapBindPoint  = 0, fragment stage.
//   • smp_desc_heap  — same shape, for samplers referenced by the shader.
//                      MSC's smp_desc_heap_ab symbol appears even when the root
//                      sig only has a StaticSampler — the safe thing is to
//                      provide a matching sampler at heap[0].
//                      Bound at kIRSamplerHeapBindPoint    = 1, fragment stage.
//
// The vertex layout follows the M2 finding: HLSL user semantics map to Metal
// attribute(N) starting at N = kIRStageInAttributeStartIndex = 11.
//
// Success criterion: the render target shows a bright red/blue 8×8 checker
// pattern (crisp, not blurred to grey) covering the whole viewport, and the
// center pixel matches the expected cell color (red for a (4,4) cell in an
// 8×8 grid). Cross-check: sampling in the fragment shader worked AND the
// argument-buffer bindings landed on the slots MSC expects.

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

// Must come after Metal.hpp. Pulls in the inline helper bodies
// (IRDescriptorTableSet*, kIR* constants) — no libmetalirconverter link needed.
#define IR_RUNTIME_METALCPP
#define IR_PRIVATE_IMPLEMENTATION
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "poc_common.h"

static std::vector<uint8_t> read_file(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(n > 0 ? static_cast<size_t>(n) : 0);
    if (n > 0) std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    return buf;
}

static MTL::Library* load_metallib(MTL::Device* dev, const char* path) {
    auto bytes = read_file(path);
    if (bytes.empty()) { std::printf("FAIL: cannot read %s\n", path); return nullptr; }
    dispatch_data_t dd = dispatch_data_create(bytes.data(), bytes.size(),
                                              dispatch_get_main_queue(),
                                              DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NS::Error* err = nullptr;
    MTL::Library* lib = dev->newLibrary(dd, &err);
    if (dd) ((NS::Object*)dd)->release();
    if (!lib) std::printf("FAIL: newLibrary(%s): %s\n", path,
                          err ? err->localizedDescription()->utf8String() : "?");
    return lib;
}

int main() {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (!device) { std::printf("FAIL: no Metal device\n"); return 1; }
    MTL::CommandQueue* queue = device->newCommandQueue();
    if (!queue) { std::printf("FAIL: no command queue\n"); return 1; }

    // --- Translated shaders (VSMain/PSMain preserved by MSC) ---
    MTL::Library* vlib = load_metallib(device, "build/shaders/htex_vs.metallib");
    MTL::Library* plib = load_metallib(device, "build/shaders/htex_ps.metallib");
    if (!vlib || !plib) return 2;
    MTL::Function* vfn = vlib->newFunction(NS::String::string("VSMain", NS::UTF8StringEncoding));
    MTL::Function* ffn = plib->newFunction(NS::String::string("PSMain", NS::UTF8StringEncoding));
    if (!vfn || !ffn) { std::printf("FAIL: missing VSMain/PSMain\n"); return 3; }

    const MTL::PixelFormat fmt = MTL::PixelFormatRGBA8Unorm;
    const uint32_t W = 256, H = 256;

    // --- Vertex descriptor (MSC N=11 convention, HLSL declaration order) ---
    //   float2 pos : POSITION  -> attribute(11), offset 0
    //   float2 uv  : TEXCOORD0 -> attribute(12), offset 8   → stride 16
    MTL::VertexDescriptor* vd = MTL::VertexDescriptor::alloc()->init();
    vd->attributes()->object(11)->setFormat(MTL::VertexFormatFloat2);
    vd->attributes()->object(11)->setOffset(0);
    vd->attributes()->object(11)->setBufferIndex(0);
    vd->attributes()->object(12)->setFormat(MTL::VertexFormatFloat2);
    vd->attributes()->object(12)->setOffset(8);
    vd->attributes()->object(12)->setBufferIndex(0);
    vd->layouts()->object(0)->setStride(16);
    vd->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);

    // --- PSO ---
    MTL::RenderPipelineDescriptor* pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vfn);
    pd->setFragmentFunction(ffn);
    pd->setVertexDescriptor(vd);
    pd->colorAttachments()->object(0)->setPixelFormat(fmt);
    NS::Error* err = nullptr;
    MTL::RenderPipelineState* pso = device->newRenderPipelineState(pd, &err);
    if (!pso) { std::printf("FAIL: pso: %s\n",
                            err ? err->localizedDescription()->utf8String() : "?"); return 4; }

    // --- Fullscreen quad (6 verts, triangle list). uv (0,0) top-left. ---
    const float verts[] = {
        -1.f, -1.f,   0.f, 1.f,
         1.f, -1.f,   1.f, 1.f,
        -1.f,  1.f,   0.f, 0.f,
        -1.f,  1.f,   0.f, 0.f,
         1.f, -1.f,   1.f, 1.f,
         1.f,  1.f,   1.f, 0.f,
    };
    MTL::Buffer* vbuf = device->newBuffer(verts, sizeof(verts), MTL::ResourceStorageModeShared);

    // --- Source texture: 8x8 RGBA8 red/blue checker (CPU-filled) ---
    const uint32_t TW = 8, TH = 8;
    uint8_t src[TW * TH * 4];
    for (uint32_t y = 0; y < TH; ++y) {
        for (uint32_t x = 0; x < TW; ++x) {
            uint8_t* p = &src[(y * TW + x) * 4];
            const bool red = ((x + y) & 1) == 0;
            p[0] = red ? 255 : 0;
            p[1] = 0;
            p[2] = red ? 0 : 255;
            p[3] = 255;
        }
    }
    MTL::TextureDescriptor* std_td = MTL::TextureDescriptor::texture2DDescriptor(fmt, TW, TH, false);
    std_td->setUsage(MTL::TextureUsageShaderRead);
    std_td->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* srcTex = device->newTexture(std_td);
    if (!srcTex) { std::printf("FAIL: no source texture\n"); return 5; }
    srcTex->replaceRegion(MTL::Region::Make2D(0, 0, TW, TH), 0, src, TW * 4);

    // --- Sampler (matches HLSL StaticSampler(linear/clamp)) ---
    MTL::SamplerDescriptor* sd = MTL::SamplerDescriptor::alloc()->init();
    sd->setMinFilter(MTL::SamplerMinMagFilterLinear);
    sd->setMagFilter(MTL::SamplerMinMagFilterLinear);
    sd->setMipFilter(MTL::SamplerMipFilterLinear);
    sd->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sd->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    sd->setSupportArgumentBuffers(true);   // required for gpuResourceID access
    MTL::SamplerState* samp = device->newSamplerState(sd);

    // --- Argument buffers: TLAB + resource heap + sampler heap ---
    // Each is one IRDescriptorTableEntry (24B). We hold one entry each because
    // there's exactly one root param, one SRV, and one sampler.
    const size_t E = sizeof(IRDescriptorTableEntry);
    MTL::Buffer* resHeap = device->newBuffer(E, MTL::ResourceStorageModeShared);
    MTL::Buffer* smpHeap = device->newBuffer(E, MTL::ResourceStorageModeShared);
    MTL::Buffer* tlab    = device->newBuffer(E, MTL::ResourceStorageModeShared);

    auto* resEntries = static_cast<IRDescriptorTableEntry*>(resHeap->contents());
    auto* smpEntries = static_cast<IRDescriptorTableEntry*>(smpHeap->contents());
    auto* tlabEntries = static_cast<IRDescriptorTableEntry*>(tlab->contents());
    IRDescriptorTableSetTexture(&resEntries[0], srcTex, 0.0f, 0);
    IRDescriptorTableSetSampler(&smpEntries[0], samp, 0.0f);
    // Root param 0 (the descriptor table) points at the resource heap's base.
    IRDescriptorTableSetBuffer(&tlabEntries[0], resHeap->gpuAddress(), 0);

    // --- Render target ---
    MTL::TextureDescriptor* rt_td = MTL::TextureDescriptor::texture2DDescriptor(fmt, W, H, false);
    rt_td->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    rt_td->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* rt = device->newTexture(rt_td);
    if (!rt) { std::printf("FAIL: no render-target texture\n"); return 6; }

    MTL::RenderPassDescriptor* rp = MTL::RenderPassDescriptor::alloc()->init();
    auto* c0 = rp->colorAttachments()->object(0);
    c0->setTexture(rt);
    c0->setLoadAction(MTL::LoadActionClear);
    c0->setStoreAction(MTL::StoreActionStore);
    c0->setClearColor(MTL::ClearColor(0.05, 0.05, 0.05, 1.0));

    MTL::CommandBuffer* cb = queue->commandBuffer();
    MTL::RenderCommandEncoder* enc = cb->renderCommandEncoder(rp);
    enc->setRenderPipelineState(pso);

    // Vertex-stage: stage_in buffer at 0, TLAB at 2.
    enc->setVertexBuffer(vbuf, 0, 0);
    enc->setVertexBuffer(tlab, 0, kIRArgumentBufferBindPoint);   // 2

    // Fragment-stage: resource heap 0, sampler heap 1, TLAB 2.
    enc->setFragmentBuffer(resHeap, 0, kIRDescriptorHeapBindPoint);   // 0
    enc->setFragmentBuffer(smpHeap, 0, kIRSamplerHeapBindPoint);      // 1
    enc->setFragmentBuffer(tlab,    0, kIRArgumentBufferBindPoint);   // 2

    // The texture is referenced *through* the descriptor heap, so the encoder
    // otherwise has no idea it's used. Explicitly declare residency + read.
    enc->useResource(srcTex, MTL::ResourceUsageRead, MTL::RenderStageFragment);

    enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));
    enc->endEncoding();
    cb->commit();
    cb->waitUntilCompleted();

    // --- Readback + verify ---
    std::vector<uint8_t> px = poc::read_rgba8(rt, W, H);
    if (!poc::write_ppm("build/m4_texture.ppm", px, W, H)) {
        std::printf("FAIL: write PPM (need build/ dir; run from PoC root)\n"); return 7;
    }
    size_t red_px = 0, blue_px = 0, clear_px = 0;
    for (uint32_t i = 0; i < W * H; ++i) {
        const uint8_t* p = &px[i * 4];
        if (p[0] > 200 && p[1] < 50 && p[2] < 50) ++red_px;
        else if (p[2] > 200 && p[0] < 50 && p[1] < 50) ++blue_px;
        else if (p[0] < 30 && p[1] < 30 && p[2] < 30) ++clear_px;
    }
    const size_t mid = (static_cast<size_t>(H / 2) * W + W / 2) * 4;
    // Cell (4,4) with 8×8 grid on 256×256 → (x+y)=8 even → red.
    const bool center_red = px[mid] > 200 && px[mid + 1] < 50 && px[mid + 2] < 50;
    // Both colors should occupy roughly half the image (~32k each).
    const bool balanced = red_px > 20000 && blue_px > 20000;
    const bool ok = center_red && balanced;
    std::printf("M4 %s: build/m4_texture.ppm  red=%zu blue=%zu clear=%zu  "
                "center=%d %d %d (expect ~255 0 0)\n",
                ok ? "OK" : "FAIL", red_px, blue_px, clear_px,
                px[mid], px[mid + 1], px[mid + 2]);

    // texture2DDescriptor() returns an autoreleased object — do NOT release
    // std_td / rt_td explicitly (matches m2_translated cleanup pattern).
    pso->release(); pd->release(); vd->release();
    vfn->release(); ffn->release(); vlib->release(); plib->release();
    vbuf->release(); srcTex->release();
    samp->release(); sd->release();
    resHeap->release(); smpHeap->release(); tlab->release();
    rt->release(); rp->release();
    queue->release(); device->release();
    pool->release();
    return ok ? 0 : 8;
}
