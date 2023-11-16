RWTexture2D<float> real : register(u0);
RWTexture2D<float> imag : register(u1);

[numthreads(8, 8, 1)]
void fftAdjust(uint2 dtid : SV_DispatchThreadID)
{
    float2 complex = float2(real[dtid], imag[dtid]);

    float2 size;
    real.GetDimensions(size.x, size.y);
    
    real[dtid] = complex.x / size.x / size.y;
    imag[dtid] = complex.y / size.x / size.y;
}