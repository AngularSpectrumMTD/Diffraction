RWTexture2D<float> tex : register(u0);

[numthreads(8, 8, 1)]
void clearFloat(uint2 dtid : SV_DispatchThreadID)
{
    tex[dtid] = 0.0;
}