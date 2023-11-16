#define PI 3.14159265359f

Texture2D<float> real : register(t0);
Texture2D<float> imag : register(t1);
RWTexture2D<float> phase : register(u0);

[numthreads(8, 8, 1)]
void complexPhase(uint2 dtid : SV_DispatchThreadID)
{
    float2 complex = float2(real[dtid], imag[dtid]);
    float p = atan2(complex.y, complex.x);
    if (length(complex) < 3e-2)
    {
        p = 0;
    }
    phase[dtid] = p / (2.0f * PI) + 0.5f;
}