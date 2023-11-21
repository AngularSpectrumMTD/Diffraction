#define PI 3.14159265359f

struct quadraticCB
{
    float focalLensgth;
    float isBiConcave;
    float wavelength;
    float samplingIntervalRealX;
    float samplingIntervalRealY;
    float reserved0;
    float reserved1;
    float reserved2;
};

ConstantBuffer<quadraticCB> constantBuffer : register(b0);

RWTexture2D<float> real : register(u0);
RWTexture2D<float> imag : register(u1);

bool isInRange(in int2 id, in float2 size)
{
    return 0 <= id.x && id.x < size.x && 0 <= id.y && id.y < size.y;
}

[numthreads(8, 8, 1)]
void multiplyQuadraticPhase(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 size;
    imag.GetDimensions(size.x, size.y);

    const float k = constantBuffer.isBiConcave ? -2 * PI / constantBuffer.wavelength : 2 * PI / constantBuffer.wavelength;
    
    const float X = (dispatchThreadID.x - size.x * 0.5f) * constantBuffer.samplingIntervalRealX;
    const float Y = (dispatchThreadID.y - size.y * 0.5f) * constantBuffer.samplingIntervalRealY;

    const float phase = -k * (X * X + Y * Y) / (2 * constantBuffer.focalLensgth);
    
    const float srcReal = real[dispatchThreadID] ;
    const float srcImag = imag[dispatchThreadID];
    
    const float amp = sqrt(srcReal * srcReal + srcImag * srcImag);
    
    real[dispatchThreadID] = amp * cos(phase);
    imag[dispatchThreadID] = amp * sin(phase);
}