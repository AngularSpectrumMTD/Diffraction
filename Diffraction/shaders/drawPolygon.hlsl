struct polygonCB
{
    uint N;
    float ratio;
    float2 reserved;
};

#define PI 3.141592

ConstantBuffer<polygonCB> constantBuffer : register(b0);

RWTexture2D<float> polygon : register(u0);

[numthreads(8, 8, 1)]
void drawPolygon(uint3 dtid : SV_DispatchThreadID)
{
    float2 ID = dtid.xy;
    float2 size;
    polygon.GetDimensions(size.x, size.y);
    
    float2 uv = ID / size - float2(0.5, 0.5);
        
    float rad = atan2(uv.x, uv.y) + 2.0 * PI;
    rad = rad % (2.0 * PI / constantBuffer.N);
        
    float r_polygon = constantBuffer.ratio * cos(PI / constantBuffer.N) / cos(PI / constantBuffer.N - rad);

    polygon[ID] = step(length(uv), r_polygon);
}