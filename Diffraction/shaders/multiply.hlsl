Texture2D<float> tex0 : register(t0);
RWTexture2D<float> tex1 : register(u0);

[numthreads(8, 8, 1)]
void multiply(uint2 dtid : SV_DispatchThreadID)
{
    float v0 = tex0[dtid];
    float v1 = tex1[dtid];
    tex1[dtid] = v0 * v1;
}