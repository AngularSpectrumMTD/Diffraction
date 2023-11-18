RWTexture2D<float4> tex : register(u0);

[numthreads(8, 8, 1)]
void clearFloat4(uint2 dtid : SV_DispatchThreadID)
{
    tex[dtid] = float4(0, 0, 0, 0);
}