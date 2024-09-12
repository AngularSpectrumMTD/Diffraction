Texture2D<float3> texSRV : register(t0);
RWTexture2D<float> texUAV : register(u0);

[numthreads(8, 8, 1)]
void averagedOneElemCopy(uint2 dtid : SV_DispatchThreadID)
{
    float2 sizeDst = 0.xx;
    texUAV.GetDimensions(sizeDst.x, sizeDst.y);
    float2 sizeSrc = 0.xx;
    texSRV.GetDimensions(sizeSrc.x, sizeSrc.y);

    uint2 readID = dtid * sizeSrc / sizeDst;
    const float3 inColor = texSRV[readID];

    if (readID.x < sizeSrc.x && readID.y < sizeSrc.y)
    {
        texUAV[dtid] = (inColor.x + inColor.y + inColor.z) / 3.0f;
    }
}