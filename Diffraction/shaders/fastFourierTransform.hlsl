#define DOUBLE_BUFFER
#define PI 3.14159265359f

Texture2D<float> realSrc : register(t0);
Texture2D<float> imagSrc : register(t1);

RWTexture2D<float> realDst : register(u0);
RWTexture2D<float> imagDst : register(u1);

void computeSrcID(uint passIndex, uint x, out uint2 indices)
{
    uint regionWidth = 2u << passIndex;
    indices.x = (x - x % regionWidth) + (x % (regionWidth / 2));
    indices.y = indices.x + regionWidth / 2;
    
    //bit reverse input
    if (passIndex == 0)
    {
        indices = reversebits(indices) >> (32 - BUTTERFLY_COUNT) & (EXECUTE_LENGTH - 1);
    }
}

void computeTwiddleFactor(uint passIndex, uint x, out float2 weights)
{
    uint regionWidth = 2u << passIndex;
    sincos(-2.0 * PI * float(x % regionWidth) / float(regionWidth), weights.y, weights.x);
}

#define REAL 0
#define IMAGE 1

#ifdef DOUBLE_BUFFER
groupshared float ButterflyArray[2][2][EXECUTE_LENGTH];
#define SharedArray(tmpID, x, realImage) (ButterflyArray[(tmpID)][(realImage)][(x)])
#else
groupshared float ButterflyArray[2][EXECUTE_LENGTH];
#define SharedArray(tmpID, x, realImage) (ButterflyArray[(realImage)][(x)])
#endif

void twiddleWeighting(uint passIndex, uint x, uint tmp, out float resultR, out float resultI)
{
    uint2 srcIndices;
    float2 Weights;
    
    computeSrcID(passIndex, x, srcIndices);
    
    float inputR1 = SharedArray(tmp, srcIndices.x, REAL);
    float inputI1 = SharedArray(tmp, srcIndices.x, IMAGE);

    float inputR2 = SharedArray(tmp, srcIndices.y, REAL);
    float inputI2 = SharedArray(tmp, srcIndices.y, IMAGE);
    
    computeTwiddleFactor(passIndex, x, Weights);
    
#ifndef DOUBLE_BUFFER
    GroupMemoryBarrierWithGroupSync();
#endif

#if INVERSE_TRANSFORM
	resultR = (inputR1 + Weights.x * inputR2 + Weights.y * inputI2) * 0.5;
	resultI = (inputI1 - Weights.y * inputR2 + Weights.x * inputI2) * 0.5;
#else
    resultR = inputR1 + Weights.x * inputR2 - Weights.y * inputI2;
    resultI = inputI1 + Weights.y * inputR2 + Weights.x * inputI2;
#endif
}

void prepareButterfly(uint bufferID, inout uint2 texPos)
{
    texPos = (texPos + EXECUTE_LENGTH / 2) % EXECUTE_LENGTH; //centering spectrum
    SharedArray(0, bufferID, REAL) = realSrc[texPos];
    SharedArray(0, bufferID, IMAGE) = imagSrc[texPos];
}

void executeButterfly(in uint bufferID, out float real, out float image)
{
    for (int butterFlyID = 0; butterFlyID < BUTTERFLY_COUNT - 1; ++butterFlyID)
    {
        GroupMemoryBarrierWithGroupSync();
        twiddleWeighting(butterFlyID, bufferID, butterFlyID % 2,
        SharedArray((butterFlyID + 1) % 2, bufferID, REAL),
        SharedArray((butterFlyID + 1) % 2, bufferID, IMAGE));
    }

    GroupMemoryBarrierWithGroupSync();
    twiddleWeighting(BUTTERFLY_COUNT - 1, bufferID, (BUTTERFLY_COUNT - 1) % 2, real, image);
}

[numthreads(EXECUTE_LENGTH, 1, 1)]
void FFT1D(uint3 dtid : SV_DispatchThreadID)
{
    const uint bufferID = dtid.x;

#if ROW
	uint2 texPos = dtid.xy;
#else
    uint2 texPos = dtid.yx;
#endif

    float r = 0, i = 0;
    prepareButterfly(bufferID, texPos);
    executeButterfly(bufferID, r, i);
    
    realDst[texPos] = r;
    imagDst[texPos] = i;
}
