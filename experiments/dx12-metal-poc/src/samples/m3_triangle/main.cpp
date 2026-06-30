// M3-prime — offscreen triangle (render harness proof).
//
// Proves the command/PSO/vertex-buffer/draw mapping (DESIGN.md §2) end to end by
// rendering a colored triangle to an offscreen texture and writing a PPM.
//
// IMPORTANT (DESIGN.md D-7): the shader here is hand-written MSL, compiled at
// runtime. That deliberately does NOT exercise DX12 shader translation — it
// de-risks the render harness first. The real M2 swaps this MSL for a metallib
// produced by HLSL -> dxc -> DXIL -> Metal Shader Converter (needs those tools
// installed), proving the *translated* shader yields the same triangle.

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "poc_common.h"

static const char* kMSL = R"(
#include <metal_stdlib>
using namespace metal;
struct VIn  { float4 pos; float4 color; };
struct VOut { float4 pos [[position]]; float4 color; };
vertex VOut v_main(uint vid [[vertex_id]], const device VIn* v [[buffer(0)]]) {
    VOut o;
    o.pos   = float4(v[vid].pos.xyz, 1.0);
    o.color = v[vid].color;
    return o;
}
fragment float4 f_main(VOut in [[stage_in]]) { return in.color; }
)";

int main() {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (device == nullptr) { std::printf("FAIL: no Metal device\n"); return 1; }
    MTL::CommandQueue* queue = device->newCommandQueue();
    if (queue == nullptr) { std::printf("FAIL: no command queue\n"); return 1; }

    // Compile the (MSL) shader library at runtime.
    NS::Error* err = nullptr;
    NS::String* src = NS::String::string(kMSL, NS::UTF8StringEncoding);
    MTL::Library* lib = device->newLibrary(src, nullptr, &err);
    if (lib == nullptr) {
        std::printf("FAIL: shader compile: %s\n",
                    err ? err->localizedDescription()->utf8String() : "?");
        return 1;
    }
    MTL::Function* vfn = lib->newFunction(NS::String::string("v_main", NS::UTF8StringEncoding));
    MTL::Function* ffn = lib->newFunction(NS::String::string("f_main", NS::UTF8StringEncoding));

    const MTL::PixelFormat fmt = MTL::PixelFormatRGBA8Unorm;
    const uint32_t W = 256, H = 256;

    // Pipeline state (≈ D3D12 PSO).
    MTL::RenderPipelineDescriptor* pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vfn);
    pd->setFragmentFunction(ffn);
    pd->colorAttachments()->object(0)->setPixelFormat(fmt);
    MTL::RenderPipelineState* pso = device->newRenderPipelineState(pd, &err);
    if (pso == nullptr) {
        std::printf("FAIL: pso: %s\n", err ? err->localizedDescription()->utf8String() : "?");
        return 1;
    }

    // Vertex buffer: 3 verts, each {x,y,z,w, r,g,b,a} (float4 pos + float4 color).
    const float verts[] = {
         0.0f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,  // top    — red
        -0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,  // left   — green
         0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,  // right  — blue
    };
    MTL::Buffer* vbuf = device->newBuffer(verts, sizeof(verts), MTL::ResourceStorageModeShared);
    if (vbuf == nullptr) { std::printf("FAIL: no vertex buffer\n"); return 1; }

    // Offscreen render target.
    MTL::TextureDescriptor* td = MTL::TextureDescriptor::texture2DDescriptor(fmt, W, H, false);
    td->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    td->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* tex = device->newTexture(td);
    if (tex == nullptr) { std::printf("FAIL: no render-target texture\n"); return 1; }

    MTL::RenderPassDescriptor* rp = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* c0 = rp->colorAttachments()->object(0);
    c0->setTexture(tex);
    c0->setLoadAction(MTL::LoadActionClear);
    c0->setStoreAction(MTL::StoreActionStore);
    c0->setClearColor(MTL::ClearColor(0.05, 0.05, 0.05, 1.0));   // dark background

    MTL::CommandBuffer* cb = queue->commandBuffer();
    MTL::RenderCommandEncoder* enc = cb->renderCommandEncoder(rp);
    enc->setRenderPipelineState(pso);
    enc->setVertexBuffer(vbuf, 0, 0);
    enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    enc->endEncoding();
    cb->commit();
    cb->waitUntilCompleted();

    // Read back + write PPM, and verify a triangle actually rendered.
    std::vector<uint8_t> px = poc::read_rgba8(tex, W, H);
    if (!poc::write_ppm("build/m3_triangle.ppm", px, W, H)) {
        std::printf("FAIL: could not write build/m3_triangle.ppm (run from PoC root; build/ must exist)\n");
        return 1;
    }

    // Verification: count pixels that aren't the (13,13,13) background.
    size_t colored = 0;
    for (uint32_t i = 0; i < W * H; ++i) {
        const uint8_t* p = &px[i * 4];
        if (p[0] > 30 || p[1] > 30 || p[2] > 30) ++colored;
    }
    const size_t mid = (static_cast<size_t>(H / 2) * W + W / 2) * 4;
    const bool ok = colored > 1000 && (px[mid] > 30 || px[mid + 1] > 30 || px[mid + 2] > 30);
    std::printf("M3 %s: wrote build/m3_triangle.ppm  colored_px=%zu  center rgb=%d %d %d\n",
                ok ? "OK" : "FAIL", colored, px[mid], px[mid + 1], px[mid + 2]);

    pso->release(); pd->release(); vfn->release(); ffn->release(); lib->release();
    vbuf->release(); tex->release(); rp->release(); queue->release(); device->release();
    pool->release();
    return ok ? 0 : 1;
}
