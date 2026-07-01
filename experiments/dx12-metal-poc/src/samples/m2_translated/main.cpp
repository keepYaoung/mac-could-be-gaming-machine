// M2 — translated shader triangle (proves the shader-translation half).
//
// M3-prime showed the render harness works using hand-written MSL. This sample
// swaps that MSL out for a metallib produced by:
//   triangle.hlsl  --dxc-->  DXIL  --metal-shaderconverter-->  triangle_{vs,ps}.metallib
// If the same triangle appears with the translated shaders, the DX12->Metal
// *shader* path is proven end to end (RESEARCH §3.3, DESIGN D-1/D-7).
//
// Vertex layout matches shaders/triangle.hlsl VSInput exactly:
//   float3 pos : POSITION   (12B, offset 0)
//   float4 color : COLOR    (16B, offset 12)   -> stride 28B, NOT 32B.
//
// Success criterion: colored_px > 1000 AND center pixel is not the background.
// (M3-prime got colored_px=16200; a translated pipeline should be in the same
// ballpark — small diffs are fine because MSC may emit slightly different
// interpolation/precision.)

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "poc_common.h"

// Read whole file into a byte vector. Returns empty on failure.
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

// Wrap a byte buffer in a dispatch_data_t so Metal can adopt it as a library.
// We use the DISPATCH_DATA_DESTRUCTOR_DEFAULT (== NULL): dispatch copies the
// bytes so the source vector can go out of scope.
static dispatch_data_t as_dispatch(const std::vector<uint8_t>& bytes) {
    return dispatch_data_create(bytes.data(), bytes.size(),
                                dispatch_get_main_queue(),
                                DISPATCH_DATA_DESTRUCTOR_DEFAULT);
}

static MTL::Library* load_metallib(MTL::Device* dev, const char* path) {
    auto bytes = read_file(path);
    if (bytes.empty()) {
        std::printf("FAIL: cannot read %s\n", path);
        return nullptr;
    }
    dispatch_data_t dd = as_dispatch(bytes);
    NS::Error* err = nullptr;
    MTL::Library* lib = dev->newLibrary(dd, &err);
    // dispatch_data_t is refcounted; release the local ref (newLibrary retains).
    if (dd) ((NS::Object*)dd)->release();
    if (!lib) {
        std::printf("FAIL: newLibrary(%s): %s\n", path,
                    err ? err->localizedDescription()->utf8String() : "?");
    }
    return lib;
}

int main() {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (!device) { std::printf("FAIL: no Metal device\n"); return 1; }
    MTL::CommandQueue* queue = device->newCommandQueue();
    if (!queue) { std::printf("FAIL: no command queue\n"); return 1; }

    // Load the TRANSLATED shaders (produced at build time by tools/build_shaders.sh).
    MTL::Library* vlib = load_metallib(device, "build/shaders/triangle_vs.metallib");
    MTL::Library* plib = load_metallib(device, "build/shaders/triangle_ps.metallib");
    if (!vlib || !plib) return 2;

    // Entry names are the HLSL entry-point names — MSC preserves them.
    // (Verified with `strings triangle_vs.metallib | grep Main`.)
    MTL::Function* vfn = vlib->newFunction(
        NS::String::string("VSMain", NS::UTF8StringEncoding));
    MTL::Function* ffn = plib->newFunction(
        NS::String::string("PSMain", NS::UTF8StringEncoding));
    if (!vfn || !ffn) {
        std::printf("FAIL: missing VSMain/PSMain in metallib (found=%d/%d)\n",
                    vfn != nullptr, ffn != nullptr);
        return 3;
    }

    const MTL::PixelFormat fmt = MTL::PixelFormatRGBA8Unorm;
    const uint32_t W = 256, H = 256;

    // Vertex descriptor matches triangle.hlsl VSInput.
    // *** Metal Shader Converter binding convention (verified empirically): ***
    // user-defined HLSL semantics are exposed as Metal [[attribute(N)]] where
    // N counts from 11 in HLSL declaration order (NOT 0). For triangle.hlsl:
    //   position0 (float3 : POSITION) -> attribute 11
    //   color0    (float4 : COLOR)    -> attribute 12
    // Attribute indices 0..10 appear to be reserved by MSC for future/system
    // slots. RESEARCH §7 covers argument-buffer bindings (buffer index 2);
    // this attribute-index convention is a separate rule, documented here
    // because we had to discover it by reading the PSO error messages.
    MTL::VertexDescriptor* vd = MTL::VertexDescriptor::alloc()->init();
    vd->attributes()->object(11)->setFormat(MTL::VertexFormatFloat3);       // position0
    vd->attributes()->object(11)->setOffset(0);
    vd->attributes()->object(11)->setBufferIndex(0);
    vd->attributes()->object(12)->setFormat(MTL::VertexFormatFloat4);       // color0
    vd->attributes()->object(12)->setOffset(12);
    vd->attributes()->object(12)->setBufferIndex(0);
    vd->layouts()->object(0)->setStride(28);                  // 12 + 16
    vd->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);

    // PSO.
    MTL::RenderPipelineDescriptor* pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vfn);
    pd->setFragmentFunction(ffn);
    pd->setVertexDescriptor(vd);
    pd->colorAttachments()->object(0)->setPixelFormat(fmt);
    NS::Error* err = nullptr;
    MTL::RenderPipelineState* pso = device->newRenderPipelineState(pd, &err);
    if (!pso) {
        std::printf("FAIL: pso: %s\n",
                    err ? err->localizedDescription()->utf8String() : "?");
        return 4;
    }

    // Vertex buffer: 3 verts of {x,y,z, r,g,b,a} — same triangle as M3-prime.
    const float verts[] = {
         0.0f,  0.7f, 0.0f,   1.0f, 0.0f, 0.0f, 1.0f,  // top    red
        -0.7f, -0.7f, 0.0f,   0.0f, 1.0f, 0.0f, 1.0f,  // left   green
         0.7f, -0.7f, 0.0f,   0.0f, 0.0f, 1.0f, 1.0f,  // right  blue
    };
    MTL::Buffer* vbuf = device->newBuffer(verts, sizeof(verts),
                                          MTL::ResourceStorageModeShared);
    if (!vbuf) { std::printf("FAIL: no vertex buffer\n"); return 5; }

    // Offscreen render target.
    MTL::TextureDescriptor* td = MTL::TextureDescriptor::texture2DDescriptor(fmt, W, H, false);
    td->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    td->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* tex = device->newTexture(td);
    if (!tex) { std::printf("FAIL: no render-target texture\n"); return 6; }

    MTL::RenderPassDescriptor* rp = MTL::RenderPassDescriptor::alloc()->init();
    auto* c0 = rp->colorAttachments()->object(0);
    c0->setTexture(tex);
    c0->setLoadAction(MTL::LoadActionClear);
    c0->setStoreAction(MTL::StoreActionStore);
    c0->setClearColor(MTL::ClearColor(0.05, 0.05, 0.05, 1.0));

    MTL::CommandBuffer* cb = queue->commandBuffer();
    MTL::RenderCommandEncoder* enc = cb->renderCommandEncoder(rp);
    enc->setRenderPipelineState(pso);
    enc->setVertexBuffer(vbuf, 0, 0);
    enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    enc->endEncoding();
    cb->commit();
    cb->waitUntilCompleted();

    // Readback + verify.
    std::vector<uint8_t> px = poc::read_rgba8(tex, W, H);
    if (!poc::write_ppm("build/m2_translated.ppm", px, W, H)) {
        std::printf("FAIL: write PPM (need build/ dir; run from PoC root)\n");
        return 7;
    }
    size_t colored = 0;
    for (uint32_t i = 0; i < W * H; ++i) {
        const uint8_t* p = &px[i * 4];
        if (p[0] > 30 || p[1] > 30 || p[2] > 30) ++colored;
    }
    const size_t mid = (static_cast<size_t>(H / 2) * W + W / 2) * 4;
    const bool ok = colored > 1000 && (px[mid] > 30 || px[mid + 1] > 30 || px[mid + 2] > 30);
    std::printf("M2 %s: build/m2_translated.ppm  colored_px=%zu  center=%d %d %d  "
                "(M3-prime baseline colored_px=16200)\n",
                ok ? "OK" : "FAIL", colored, px[mid], px[mid + 1], px[mid + 2]);

    pso->release(); pd->release(); vd->release();
    vfn->release(); ffn->release(); vlib->release(); plib->release();
    vbuf->release(); tex->release(); rp->release(); queue->release(); device->release();
    pool->release();
    return ok ? 0 : 8;
}
