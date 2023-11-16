Texture2D<float> f1 : register(t0);
RWTexture2D<float4> f4 : register(u0);

[numthreads(8, 8, 1)]
void float1tofloat4(uint2 dtid : SV_DispatchThreadID)
{
    float v = f1[dtid];
    f4[dtid] = float4(v, v, v, 1);
}