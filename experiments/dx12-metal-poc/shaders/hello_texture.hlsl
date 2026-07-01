// Minimal HLSL for the M4 HelloTexture PoC.
// Fullscreen quad sampled from Texture2D bound via a descriptor table (SRV).
// The sampler is a static sampler baked into the root signature (no runtime
// sampler binding required).
//
// Pipeline: hello_texture.hlsl --dxc--> DXIL (root sig embedded)
//           --metal-shaderconverter--> triangle_{vs,ps}.metallib
// See tools/build_shaders.sh and DESIGN.md §M4.

#define ROOTSIG \
    "DescriptorTable(SRV(t0)), " \
    "StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, " \
    "addressU=TEXTURE_ADDRESS_CLAMP, addressV=TEXTURE_ADDRESS_CLAMP)"

Texture2D<float4> gImage : register(t0);
SamplerState     gSamp  : register(s0);

struct VSInput {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

[RootSignature(ROOTSIG)]
PSInput VSMain(VSInput input) {
    PSInput o;
    o.pos = float4(input.pos, 0.0, 1.0);
    o.uv  = input.uv;
    return o;
}

[RootSignature(ROOTSIG)]
float4 PSMain(PSInput input) : SV_TARGET {
    return gImage.Sample(gSamp, input.uv);
}
