#define LANBDA_INF_NM 770
#define LAMBDA_VIO_NM 380
#define LAMBDA_NUM 40
#define LAMBDA_STEP 10

static float3 XYZ380to770_10nmTbl[LAMBDA_NUM] =
{
    float3(0.0014, 0, 0.0065),
    float3(0.0042, 0.0001, 0.0201),
    float3(0.0143, 0.0004, 0.0679),
    float3(0.0435, 0.0012, 0.2074),
    float3(0.1344, 0.004, 0.6456),
    float3(0.2839, 0.0116, 1.3856),
    float3(0.3483, 0.023, 1.7471),
    float3(0.3362, 0.038, 1.7721),
    float3(0.2908, 0.06, 1.6692),
    float3(0.1954, 0.091, 1.2876),
    float3(0.0956, 0.139, 0.813),
    float3(0.032, 0.208, 0.4652),
    float3(0.0049, 0.323, 0.272),
    float3(0.0093, 0.503, 0.1582),
    float3(0.0633, 0.71, 0.0782),
    float3(0.1655, 0.862, 0.0422),
    float3(0.294, 0.954, 0.0203),
    float3(0.4334, 0.995, 0.0087),
    float3(0.5945, 0.995, 0.0039),
    float3(0.7621, 0.952, 0.0021),
    float3(0.9163, 0.87, 0.0017),
    float3(1.0263, 0.757, 0.0011),
    float3(1.0622, 0.631, 0.0008),
    float3(1.0026, 0.503, 0.0003),
    float3(0.8544, 0.381, 0.0002),
    float3(0.6424, 0.265, 0),
    float3(0.4479, 0.175, 0),
    float3(0.2835, 0.107, 0),
    float3(0.1649, 0.061, 0),
    float3(0.0874, 0.032, 0),
    float3(0.0468, 0.017, 0),
    float3(0.0227, 0.0082, 0),
    float3(0.0114, 0.0041, 0),
    float3(0.0058, 0.0021, 0),
    float3(0.0029, 0.001, 0),
    float3(0.0014, 0.0005, 0),
    float3(0.0007, 0.0003, 0),
    float3(0.0003, 0.0001, 0),
    float3(0.0002, 0.0001, 0),
    float3(0.0001, 0, 0)
};

#define X_integrated 5.48
#define Y_integrated 5.52
#define Z_integrated 5.45

float3 lambdatoXYZ(float lambdaNM)
{
    float fid = clamp((lambdaNM - LAMBDA_VIO_NM) / LAMBDA_STEP, 0, LAMBDA_NUM);
    int baseID = int(fid + 0.5);
    float t = fid - baseID;
    float3 XYZ0 = XYZ380to770_10nmTbl[baseID];
    float3 XYZ1 = XYZ380to770_10nmTbl[baseID + 1];
    return lerp(XYZ0, XYZ1, t);
}

struct CompositeIntensityCB
{
    float wavelengthNM;
    float wavelengthNum;
    float reserved0;
    float reserved1;
};

ConstantBuffer<CompositeIntensityCB> constantBuffer : register(b0);
Texture2D<float> intensitySrc : register(t0);
RWTexture2D<float4> intensityDst : register(u0);

[numthreads(8, 8, 1)]
void compositeIntensity(uint2 dtid : SV_DispatchThreadID)
{
    float intensity = intensitySrc[dtid];
    float3 XYZ = lambdatoXYZ(constantBuffer.wavelengthNM);
    float3 result = XYZ * intensity;
    intensityDst[dtid].rgb += result / float3(X_integrated, Y_integrated, Z_integrated);
}