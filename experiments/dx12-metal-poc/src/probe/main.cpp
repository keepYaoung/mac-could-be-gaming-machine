// M0 — headless Metal device probe.
// Goal: validate the metal-cpp toolchain end-to-end (compile + link + run) and
// confirm this machine meets the PoC's hard requirement: Argument Buffers Tier 2
// + Apple-family GPU (RESEARCH.md §7). No windowing yet — that's M1.
//
// Build: see CMakeLists.txt (needs third_party/metal-cpp from M0).
//
// NOTE: unverified until metal-cpp headers are present and this is built on the
// target Mac. API names follow Apple's metal-cpp (developer.apple.com/metal/cpp).

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cstdio>

int main() {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (device == nullptr) {
        std::printf("FAIL: no Metal device\n");
        pool->release();
        return 1;
    }

    std::printf("device              : %s\n", device->name()->utf8String());

    // Argument Buffers Tier 2 is the hard gate for Metal Shader Converter output
    // and for the bindless/descriptor-heap mapping (RESEARCH.md §7).
    MTL::ArgumentBuffersTier tier = device->argumentBuffersSupport();
    const bool tier2 = (tier == MTL::ArgumentBuffersTier2);
    std::printf("argument buffers    : Tier %d  (%s)\n",
                tier2 ? 2 : 1, tier2 ? "OK for PoC" : "INSUFFICIENT — need Tier 2");

    // Apple-family GPU (mesh/bindless features assume Apple7+; M4 is Apple9).
    const bool apple7 = device->supportsFamily(MTL::GPUFamilyApple7);
    std::printf("supports Apple7     : %s\n", apple7 ? "yes" : "no");

    const bool ok = tier2 && apple7;
    std::printf("\nM0 verdict          : %s\n", ok ? "PASS — proceed to M1" : "BLOCKED");

    device->release();
    pool->release();
    return ok ? 0 : 2;
}
