RWTexture2D<float2> tex : register(u0);

[numthreads(8, 8, 1)]
void clearFloat2(uint2 dtid : SV_DispatchThreadID)
{
    tex[dtid] = float2(0.0, 0.0);
}