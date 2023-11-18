static float3x3 XYZtoRGB =
{
    +2.3706743, -0.51388850, +0.0052982,
    -0.9000405, +1.4253036, -0.0146949,
    -0.470638, +0.0885814, +1.0093968
};

float3 convertXYZtoRGB(in float3 XYZ)
{
    return mul(XYZ, XYZtoRGB);
}

Texture2D<float> sourceField : register(t0);
Texture2D<float4> intensity : register(t1);
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
        float3 rgb = convertXYZtoRGB(intensity[dtid - uint2(DELTA, 0)].xyz);
        resultF4[dtid] = float4(rgb.r, rgb.g, rgb.b, 0);
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