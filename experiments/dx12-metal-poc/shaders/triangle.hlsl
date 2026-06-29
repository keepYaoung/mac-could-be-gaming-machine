// Minimal HLSL (SM 6.0) for the HelloTriangle PoC.
// Pipeline: triangle.hlsl --dxc--> DXIL --metal-shaderconverter--> .metallib
// See tools/build_shaders.sh and DESIGN.md M2/M3.

struct VSInput {
    float3 pos   : POSITION;
    float4 color : COLOR;
};

struct PSInput {
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input) {
    PSInput o;
    o.pos   = float4(input.pos, 1.0);
    o.color = input.color;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
