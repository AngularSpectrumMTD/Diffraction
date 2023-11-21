#include "Diffraction.h"
#include <fstream>
#include <wincodec.h>

void Diffraction::GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, u32 pathSize)
{
    if (path == nullptr)
    {
        throw std::exception();
    }

    DWORD size = GetModuleFileName(nullptr, path, pathSize);
    if (size == 0 || size == pathSize)
    {
        // Method failed or path was truncated.
        throw std::exception();
    }

    WCHAR* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash)
    {
        *(lastSlash + 1) = L'\0';
    }
}

std::wstring Diffraction::GetAssetFullPath(LPCWSTR assetName)
{
    return mAssetPath + assetName;
}

void Diffraction::CreateComputeShaderStateObject(const LPCWSTR& compiledComputeShaderName, ComPtr<ID3D12PipelineState>& computePipelineState, ComPtr<ID3D12RootSignature> rootSig)
{
    u32 fileSize = 0;
    UINT8* shaderCodePtr;
    utility::ReadDataFromFile(GetAssetFullPath(compiledComputeShaderName).c_str(), &shaderCodePtr, &fileSize);

    D3D12_COMPUTE_PIPELINE_STATE_DESC copmputePSODesc = {};
    copmputePSODesc.pRootSignature = rootSig.Get();
    copmputePSODesc.CS = CD3DX12_SHADER_BYTECODE((void*)shaderCodePtr, fileSize);

    HRESULT hr = mDevice->GetDevice()->CreateComputePipelineState(&copmputePSODesc, IID_PPV_ARGS(&computePipelineState));

    if (FAILED(hr)) {
        throw std::runtime_error("CreateComputePipelineStateObject failed.");
    }
}

void Diffraction::CreateComputeRootSignatureAndPSO()
{
    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRegisterMapClearFloat["tex"] = 0;
        mRsClearFloat = rsCreater.Create(mDevice, false, L"rsClearFloat");
        CreateComputeShaderStateObject(ComputeShaders::ClearFloat, mClearFloatPSO, mRsClearFloat);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRegisterMapClearFloat["tex"] = 0;
        mRsClearFloat4 = rsCreater.Create(mDevice, false, L"rsClearFloat4");
        CreateComputeShaderStateObject(ComputeShaders::ClearFloat4, mClearFloat4PSO, mRsClearFloat4);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RootType::CBV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRegisterMapDrawPolygon["constantBuffer"] = 0;
        mRegisterMapDrawPolygon["polygon"] = 1;
        mRsDrawPolygon = rsCreater.Create(mDevice, false, L"rsDrawPolygon");
        CreateComputeShaderStateObject(ComputeShaders::DrawPolygon, mDrawPolygonPSO, mRsDrawPolygon);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RootType::CBV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 1);
        mRegisterMapGenerateFRF["constantBuffer"] = 0;
        mRegisterMapGenerateFRF["real"] = 1;
        mRegisterMapGenerateFRF["imag"] = 2;
        mRsGenerateFRF = rsCreater.Create(mDevice, false, L"rsGenerateFRF");
        CreateComputeShaderStateObject(ComputeShaders::GenerateFRF, mGenerateFRFPSO, mRsGenerateFRF);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRsMultiply = rsCreater.Create(mDevice, false, L"rsMultiply");
        mRegisterMapMultiply["tex0"] = 0;
        mRegisterMapMultiply["tex1"] = 1;
        CreateComputeShaderStateObject(ComputeShaders::Multiply, mMultiplyPSO, mRsMultiply);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 1);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 1);
        mRegisterMapComplexMultiply["real0"] = 0;
        mRegisterMapComplexMultiply["imag0"] = 1;
        mRegisterMapComplexMultiply["real1"] = 2;
        mRegisterMapComplexMultiply["imag1"] = 3;
        mRsComplexMultiply = rsCreater.Create(mDevice, false, L"rsComplexMultiply");
        CreateComputeShaderStateObject(ComputeShaders::ComplexMultiply, mComplexMultiplyPSO, mRsComplexMultiply);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 1);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRegisterMapComplexIntensity["real"] = 0;
        mRegisterMapComplexIntensity["imag"] = 1;
        mRegisterMapComplexIntensity["intensity"] = 2;
        mRsComplexIntensity = rsCreater.Create(mDevice, false, L"rsComplexIntensity");
        CreateComputeShaderStateObject(ComputeShaders::ComplexIntensity, mComplexIntensityPSO, mRsComplexIntensity);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 1);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRegisterMapComplexPhase["real"] = 0;
        mRegisterMapComplexPhase["imag"] = 1;
        mRegisterMapComplexPhase["phase"] = 2;
        mRsComplexPhase = rsCreater.Create(mDevice, false, L"reComplexPhase");
        CreateComputeShaderStateObject(ComputeShaders::ComplexPhase, mComplexPhasePSO, mRsComplexPhase);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 1);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 1);
        mRegisterMapFFT["realSrc"] = 0;
        mRegisterMapFFT["imagSrc"] = 1;
        mRegisterMapFFT["realDst"] = 2;
        mRegisterMapFFT["imagDst"] = 3;
        mRsFFT = rsCreater.Create(mDevice, false, L"rsFFT");
        CreateComputeShaderStateObject(ComputeShaders::FFT_row, mFFT_rowPSO, mRsFFT);
        CreateComputeShaderStateObject(ComputeShaders::FFT_col, mFFT_colPSO, mRsFFT);
        CreateComputeShaderStateObject(ComputeShaders::invFFT_row, mInvFFT_rowPSO, mRsFFT);
        CreateComputeShaderStateObject(ComputeShaders::invFFT_col, mInvFFT_colPSO, mRsFFT);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 1);
        mRegisterMapFFT_Adjust["real"] = 0;
        mRegisterMapFFT_Adjust["imag"] = 1;
        mRsFFT_Adjust = rsCreater.Create(mDevice, false, L"rsFFT_Adjust");
        CreateComputeShaderStateObject(ComputeShaders::FFT_Adjust, mFFT_AdjustPSO, mRsFFT_Adjust);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRegisterMapFloat1toFloat4["f1"] = 0;
        mRegisterMapFloat1toFloat4["f4"] = 1;
        mRsFloat1toFloat4 = rsCreater.Create(mDevice, false, L"rsFloat1toFloat4");
        CreateComputeShaderStateObject(ComputeShaders::Float1toFloat4, mFloat1toFloat4PSO, mRsFloat1toFloat4);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 1);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 2);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRegisterMapConstructResult["sourceField"] = 0;
        mRegisterMapConstructResult["intensity"] = 1;
        mRegisterMapConstructResult["phase"] = 2;
        mRegisterMapConstructResult["resultF4"] = 3;
        mRsConstructResult = rsCreater.Create(mDevice, false, L"rsConstructResult");
        CreateComputeShaderStateObject(ComputeShaders::ConstructResult, mConstructResultPSO, mRsConstructResult);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.PushStaticSampler(0, 0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, utility::RootSignatureCreater::AddressMode::Clamp, utility::RootSignatureCreater::AddressMode::Clamp, utility::RootSignatureCreater::AddressMode::Clamp);
        rsCreater.Push(utility::RootSignatureCreater::RootType::CBV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 1);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 1);
        mRegisterMapRotateInFourierSpace["constantBuffer"] = 0;
        mRegisterMapRotateInFourierSpace["realSrc"] = 1;
        mRegisterMapRotateInFourierSpace["imagSrc"] = 2;
        mRegisterMapRotateInFourierSpace["realDst"] = 3;
        mRegisterMapRotateInFourierSpace["imagDst"] = 4;
        mRsRotateInFourierSpace = rsCreater.Create(mDevice, false, L"rsRotateInFourierSpace");
        CreateComputeShaderStateObject(ComputeShaders::RotateInFourierSpace, mRotateInFourierSpacePSO, mRsRotateInFourierSpace);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RootType::CBV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::SRV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        mRegisterMapCompositeIntensity["constantBuffer"] = 0;
        mRegisterMapCompositeIntensity["intensitySrc"] = 1;
        mRegisterMapCompositeIntensity["intensityDst"] = 2;
        mRsCompositeIntensity = rsCreater.Create(mDevice, false, L"rsCompositeIntensity");
        CreateComputeShaderStateObject(ComputeShaders::CompositeIntensity, mCompositeIntensityPSO, mRsCompositeIntensity);
    }

    {
        utility::RootSignatureCreater rsCreater;
        rsCreater.Push(utility::RootSignatureCreater::RootType::CBV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 0);
        rsCreater.Push(utility::RootSignatureCreater::RangeType::UAV, 1);
        mRegisterMapMultiplyQuadraticPhase["constantBuffer"] = 0;
        mRegisterMapMultiplyQuadraticPhase["real"] = 1;
        mRegisterMapMultiplyQuadraticPhase["imag"] = 2;
        mRsMultiplyQuadraticPhase = rsCreater.Create(mDevice, false, L"rsMultiplyQuadraticPhase");
        CreateComputeShaderStateObject(ComputeShaders::MultiplyQuadraticPhase, mMultiplyQuadraticPhasePSO, mRsMultiplyQuadraticPhase);
    }
}