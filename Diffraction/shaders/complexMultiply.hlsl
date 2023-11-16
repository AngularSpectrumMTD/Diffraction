Texture2D<float> real0 : register(t0);
Texture2D<float> imag0 : register(t1);
RWTexture2D<float> real1 : register(u0);
RWTexture2D<float> imag1 : register(u1);

[numthreads(8, 8, 1)]
void complexMultiply(uint2 dtid : SV_DispatchThreadID)
{
    float2 complex0 = float2(real0[dtid], imag0[dtid]);
    float2 complex1 = float2(real1[dtid], imag1[dtid]);
    
    float2 multipled = float2(complex0.x * complex1.x - complex0.y * complex1.y, complex0.y * complex1.x + complex0.x * complex1.y);

    real1[dtid] = multipled.x;
    imag1[dtid] = multipled.y;
}