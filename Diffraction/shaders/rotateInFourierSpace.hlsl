#define PI 3.14159265359f

struct rotationCB
{
    float degX;
    float degY;
    float degZ;
    float wavelength;
};

SamplerState imageSampler : register(s0);

ConstantBuffer<rotationCB> constantBuffer : register(b0);

Texture2D<float> realSrc : register(t0);
Texture2D<float> imagSrc : register(t1);
RWTexture2D<float> realDst : register(u0);
RWTexture2D<float> imagDst : register(u1);

float3x3 rotationMatrixAxisX(in float radX)
{
    float s, c;
    sincos(radX, s, c);
    
    return float3x3(
        1, 0, 0,
        0, c, s,
        0, -s, c
    );
}

float3x3 rotationMatrixAxisY(in float radY)
{
    float s, c;
    sincos(radY, s, c);
    
    return float3x3(
        c, 0, -s,
        0, 1, 0,
        s, 0, c
    );
}

float3x3 rotationMatrixAxisZ(in float radZ)
{
    float s, c;
    sincos(radZ, s, c);
    
    return float3x3(
        c, s, 0,
        -s, c, 0,
        0, 0, 1
    );
}

float getFrequencyW(in float wavelength, float2 freqUV)
{
    return (1 / wavelength / wavelength) - freqUV.x * freqUV.x - freqUV.y * freqUV.y;
}

bool isInRange(in int2 id, in float2 size)
{
    return 0 <= id.x && id.x < size.x && 0 <= id.y && id.y < size.y;
}

[numthreads(8, 8, 1)]
void rotateInFourierSpace(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 size;
    realSrc.GetDimensions(size.x, size.y);

    const float invWavelength = 1 / constantBuffer.wavelength;

    float samplingIntervalFourierU = invWavelength / size.x;
    float samplingIntervalFourierV = invWavelength / size.y;
    
    float frequencyU = (dispatchThreadID.x - size.x * 0.5f) * samplingIntervalFourierU;
    float frequencyV = (dispatchThreadID.y - size.y * 0.5f) * samplingIntervalFourierV;

    float degX = constantBuffer.degX * (PI / 180.f);
    float degY = constantBuffer.degY * (PI / 180.f);
    float degZ = constantBuffer.degZ * (PI / 180.f);

    float3x3 rotationMatrix = mul(mul(rotationMatrixAxisX(degX), rotationMatrixAxisY(degY)), rotationMatrixAxisZ(degZ));
    float3x3 invRotationMatrix = transpose(rotationMatrix);

    float3 refFrequency = float3(frequencyU, frequencyV, invWavelength);
    float refW = getFrequencyW(constantBuffer.wavelength, refFrequency.xy);
    
    bool isEvanescent = (refW < 0);
    if (isEvanescent)
    {
        realDst[dispatchThreadID] = 0;
        imagDst[dispatchThreadID] = 0;
        return;
    }
    
    float3 carrierFrequency = mul(float3(0, 0, invWavelength), rotationMatrix);
    float3 shiftFrequency = refFrequency + carrierFrequency;
    float shiftW = getFrequencyW(constantBuffer.wavelength, shiftFrequency.xy);

    isEvanescent = (shiftW < 0);
    if (isEvanescent)
    {
        realDst[dispatchThreadID] = 0;
        imagDst[dispatchThreadID] = 0;
        return;
    }

    shiftFrequency.z = sqrt(shiftW);
    float3 srcFrequency = mul(shiftFrequency, invRotationMatrix);
    float2 srcID = srcFrequency.xy * float2(1 / samplingIntervalFourierU, 1 / samplingIntervalFourierV) + size / 2;

    if (isInRange(srcID, size))
    {
        realDst[dispatchThreadID] = realSrc[srcID];
        imagDst[dispatchThreadID] = imagSrc[srcID];
    }
    else
    {
        realDst[dispatchThreadID] = 0;
        imagDst[dispatchThreadID] = 0;
    }
}