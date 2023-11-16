Texture2D<float> real : register(t0);
Texture2D<float> imag : register(t1);
RWTexture2D<float> intensity : register(u0);

[numthreads(8, 8, 1)]
void complexIntensity(uint2 dtid : SV_DispatchThreadID)
{
    float2 complex = float2(real[dtid], imag[dtid]);
    intensity[dtid] = length(complex);
}