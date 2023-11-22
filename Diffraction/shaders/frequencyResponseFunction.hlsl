#define PI 3.14159265359f

struct frfCB
{
    float propagateDistance;
    float samplingIntervalRealX;
    float samplingIntervalRealY;
    float wavelength;
};

ConstantBuffer<frfCB> constantBuffer : register(b0);

RWTexture2D<float> real : register(u0);
RWTexture2D<float> imag : register(u1);

[numthreads(8, 8, 1)]
void generateFRF(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 size;
    imag.GetDimensions(size.x, size.y);

    float samplingIntervalFourierU = 1.0f / constantBuffer.samplingIntervalRealX / size.x;
    float samplingIntervalFourierV = 1.0f / constantBuffer.samplingIntervalRealY / size.y;

    float U_limit_helper = 2 * samplingIntervalFourierU * constantBuffer.propagateDistance;
    float U_limit = 1.0f / (constantBuffer.wavelength * sqrt(U_limit_helper * U_limit_helper + 1));
    
    float V_limit_helper = 2 * samplingIntervalFourierV * constantBuffer.propagateDistance;
    float V_limit = 1.0f / (constantBuffer.wavelength * sqrt(V_limit_helper * V_limit_helper + 1));
    
    float frequencyU = (dispatchThreadID.x - size.x * 0.5f) * samplingIntervalFourierU;
    float frequencyV = (dispatchThreadID.y - size.y * 0.5f) * samplingIntervalFourierV;
    float w_uv = 1.0f / constantBuffer.wavelength / constantBuffer.wavelength - frequencyU * frequencyU - frequencyV * frequencyV;
    
    bool isEvanescent = (w_uv < 0);
    float r = 0;
    float i = 0;
    
    //Band Limiting
    if (!isEvanescent && abs(frequencyU) < U_limit && abs(frequencyV) < V_limit)
    {
        float phase = 2 * PI * sqrt(w_uv) * constantBuffer.propagateDistance;
        sincos(phase, r, i);
    }
    
    real[dispatchThreadID] = r;
    imag[dispatchThreadID] = i;
}