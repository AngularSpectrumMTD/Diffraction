Texture2D<float> sourceField : register(t0);
Texture2D<float> intensity : register(t1);
Texture2D<float> phase : register(t2);
RWTexture2D<float4> resultF4 : register(u0);

[numthreads(8, 8, 1)]
void constructResult(uint2 dtid : SV_DispatchThreadID)
{
    float2 resultSize;
    resultF4.GetDimensions(resultSize.x, resultSize.y);

    const float DELTA = resultSize.x / 3;
    
    if (dtid.x < DELTA)
    {
        resultF4[dtid] = sourceField[dtid];
    }
    else if (dtid.x < 2 * DELTA)
    {
        resultF4[dtid] = intensity[dtid - uint2(DELTA, 0)];
    }
    else
    {
        resultF4[dtid] = phase[dtid - uint2(2 * DELTA, 0)];
    }
    
    //draw line
    if (dtid.x == DELTA || dtid.x == 2 * DELTA)
    {
        resultF4[dtid] = float4(0., 1, 0, 0);
    }

}