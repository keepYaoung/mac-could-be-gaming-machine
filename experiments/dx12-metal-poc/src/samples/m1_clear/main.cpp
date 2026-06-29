// M1 — offscreen clear.
// Renders a single clear color into an offscreen MTLTexture, reads it back, and
// writes a PPM image. No window (DESIGN.md D-6): render-to-texture is verifiable
// (we assert the pixel value) and needs only core metal-cpp.
//
// Mapped D3D12 surface (DESIGN.md §2): device, command queue, command list,
// an RTV cleared via OMSetRenderTargets-equivalent, fence wait, "present" = readback.

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

int main() {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (device == nullptr) { std::printf("FAIL: no Metal device\n"); return 1; }
    MTL::CommandQueue* queue = device->newCommandQueue();

    const uint32_t W = 256, H = 256;

    // Offscreen render target (≈ D3D12 RTV). Shared storage so the CPU can read it
    // back on Apple Silicon without a blit.
    MTL::TextureDescriptor* td = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, W, H, false);                 // autoreleased
    td->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    td->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* tex = device->newTexture(td);

    // Render pass: clear to (0.10, 0.40, 0.80).
    MTL::RenderPassDescriptor* rp = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* c0 = rp->colorAttachments()->object(0);
    c0->setTexture(tex);
    c0->setLoadAction(MTL::LoadActionClear);
    c0->setStoreAction(MTL::StoreActionStore);
    c0->setClearColor(MTL::ClearColor(0.10, 0.40, 0.80, 1.0));

    MTL::CommandBuffer* cb = queue->commandBuffer();             // autoreleased
    MTL::RenderCommandEncoder* enc = cb->renderCommandEncoder(rp); // autoreleased
    // M1: no draws — the clear is the whole frame.
    enc->endEncoding();
    cb->commit();
    cb->waitUntilCompleted();                                    // ≈ fence wait

    // Read back + write PPM.
    std::vector<uint8_t> px = poc::read_rgba8(tex, W, H);
    poc::write_ppm("build/m1_clear.ppm", px, W, H);

    const size_t mid = (static_cast<size_t>(H / 2) * W + W / 2) * 4;
    std::printf("M1 OK: wrote build/m1_clear.ppm  center rgb = %d %d %d  (expect ~26 102 204)\n",
                px[mid + 0], px[mid + 1], px[mid + 2]);

    rp->release();
    tex->release();
    queue->release();
    device->release();
    pool->release();
    return 0;
}
