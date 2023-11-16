#pragma once

#include "AppBase.h"
#include "utility/Utility.h"
#include <DirectXMath.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define POISSON_LOOP 30

#define EXECUTE_SIZE 512
#define NORMAL_THREAD_SIZE 8

#define WORK_BUFFER_SIZE 5

#include <d3d12.h>
#include <dxgi1_6.h>

#include "d3dx12.h"
#include <pix3.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace ComputeShaders {
    const LPCWSTR ClearFloat = L"clearFloat.cso";
    const LPCWSTR DrawPolygon = L"drawPolygon.cso";
    const LPCWSTR GenerateFRF = L"frequencyResponseFunction.cso";
    const LPCWSTR Multiply = L"multiply.cso";
    const LPCWSTR ComplexMultiply = L"complexMultiply.cso";
    const LPCWSTR ComplexIntensity = L"complexIntensity.cso";
    const LPCWSTR ComplexPhase = L"complexPhase.cso";
    const LPCWSTR FFT_row = L"FFT_512_ROW_execute.cso";
    const LPCWSTR FFT_col = L"FFT_512_COL_execute.cso";
    const LPCWSTR invFFT_row = L"FFT_512_ROW_INVERSE_execute.cso";
    const LPCWSTR invFFT_col = L"FFT_512_COL_INVERSE_execute.cso";
    const LPCWSTR FFT_Adjust = L"fftAdjust.cso";
    const LPCWSTR Float1toFloat4 = L"float1tofloat4.cso";
    const LPCWSTR ConstructResult = L"constructResult.cso";
    const LPCWSTR RotateInFourierSpace = L"rotateInFourierSpace.cso";
}

template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class Diffraction : public AppBase {
public:
    Diffraction(u32 width, u32 height);

    void Initialize() override;
    void Terminate() override;

    void Update() override;
    void Draw() override;

    void OnMouseDown(MouseButton button, s32 x, s32 y) override;
    void OnMouseUp(MouseButton button, s32 x, s32 y) override;
    void OnMouseMove(s32 dx, s32 dy) override;
    void OnKeyDown(UINT8 wparam) override;
    void OnMouseWheel(s32 rotate) override;

private:

    struct DrawPolygonParam
    {
        u32 N = 8;
        f32 ratio = 0.1;
        XMFLOAT2 reserved;
    };

    struct GenerateFRFParam
    {
        f32 propagateDistance = 0;
        f32 samplingIntervalRealX = 1e-6f;
        f32 samplingIntervalRealY = 1e-6f;
        f32 wavelength = 633e-9f;
    };

    struct RotationInFourierSpaceParam
    {
        f32 degX = 0;
        f32 degY = 0;
        f32 degZ = 0;
        f32 wavelength = 633e-9f;
    };

    void Dispatch(const u32 x, const u32 y, const std::wstring cmdName);

    void CreateComputeRootSignatureAndPSO();
    void CreateComputeShaderStateObject(const LPCWSTR& compiledComputeShaderName, ComPtr<ID3D12PipelineState>& computePipelineState, ComPtr<ID3D12RootSignature> rootSig);
    void CreateResultBuffer();
    void CreateWorkBuffer();
    void CreateRegularBuffer();
    void CreateConstantBuffer();

    void UpdateConstantBufferParams();
    void UpdateWindowText();

    void UpdateWorkState(const D3D12_RESOURCE_STATES dst, const u32 idx = 0);
    void UpdateResultState(const D3D12_RESOURCE_STATES dst);

    void BandLimitedASMProp();
    void PreProcess();
    void PostProcess();

    void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, u32 pathSize);
    std::wstring GetAssetFullPath(LPCWSTR assetName);
    f32 Clamp(f32 min, f32 max, f32 src);
    f32 getFrameRate();
    utility::TextureResource LoadTextureFromFile(const std::wstring& fileName);

    ComPtr<ID3D12GraphicsCommandList4> mCommandList;
    static const u32 BackBufferCount = dx12::RenderDeviceDX12::BackBufferCount;

    //ConstantBuffers
    DrawPolygonParam mDrawPolygonParam;
    ComPtr<ID3D12Resource> mDrawPolygonCB;

    GenerateFRFParam mGenerateFRFParam;
    ComPtr<ID3D12Resource> mGenerateFRFCB;

    RotationInFourierSpaceParam mRotateInFourierParam;
    ComPtr<ID3D12Resource> mRotateInFourierCB;

    //workbuffer
    ComPtr <ID3D12Resource> mWorkBufferTbl[WORK_BUFFER_SIZE];
    dx12::Descriptor mWorkUAVTbl[WORK_BUFFER_SIZE];
    dx12::Descriptor mWorkSRVTbl[WORK_BUFFER_SIZE];
    D3D12_RESOURCE_STATES mWorkStateTbl[WORK_BUFFER_SIZE];

    //result
    ComPtr <ID3D12Resource> mResultBuffer;
    dx12::Descriptor mResultUAV;
    dx12::Descriptor mResultSRV;
    D3D12_RESOURCE_STATES mResultState;

    //Pipeline State
    ComPtr<ID3D12RootSignature> mRsClearFloat;
    ComPtr<ID3D12PipelineState> mClearFloatPSO;
    std::unordered_map < std::string, u32> mRegisterMapClearFloat;

    ComPtr<ID3D12RootSignature> mRsDrawPolygon;
    ComPtr<ID3D12PipelineState> mDrawPolygonPSO;
    std::unordered_map < std::string, u32> mRegisterMapDrawPolygon;

    ComPtr<ID3D12RootSignature> mRsGenerateFRF;
    ComPtr<ID3D12PipelineState> mGenerateFRFPSO;
    std::unordered_map < std::string, u32> mRegisterMapGenerateFRF;

    ComPtr<ID3D12RootSignature> mRsMultiply;
    ComPtr<ID3D12PipelineState> mMultiplyPSO;
    std::unordered_map < std::string, u32> mRegisterMapMultiply;

    ComPtr<ID3D12RootSignature> mRsComplexMultiply;
    ComPtr<ID3D12PipelineState> mComplexMultiplyPSO;
    std::unordered_map < std::string, u32> mRegisterMapComplexMultiply;

    ComPtr<ID3D12RootSignature> mRsComplexIntensity;
    ComPtr<ID3D12PipelineState> mComplexIntensityPSO;
    std::unordered_map < std::string, u32> mRegisterMapComplexIntensity;

    ComPtr<ID3D12RootSignature> mRsComplexPhase;
    ComPtr<ID3D12PipelineState> mComplexPhasePSO;
    std::unordered_map < std::string, u32> mRegisterMapComplexPhase;

    ComPtr<ID3D12RootSignature> mRsFFT;
    ComPtr<ID3D12PipelineState> mFFT_rowPSO;
    ComPtr<ID3D12PipelineState> mFFT_colPSO;
    ComPtr<ID3D12PipelineState> mInvFFT_rowPSO;
    ComPtr<ID3D12PipelineState> mInvFFT_colPSO;
    std::unordered_map < std::string, u32> mRegisterMapFFT;

    ComPtr<ID3D12RootSignature> mRsFFT_Adjust;
    ComPtr<ID3D12PipelineState> mFFT_AdjustPSO;
    std::unordered_map < std::string, u32> mRegisterMapFFT_Adjust;

    ComPtr<ID3D12RootSignature> mRsFloat1toFloat4;
    ComPtr<ID3D12PipelineState> mFloat1toFloat4PSO;
    std::unordered_map < std::string, u32> mRegisterMapFloat1toFloat4;

    ComPtr<ID3D12RootSignature> mRsConstructResult;
    ComPtr<ID3D12PipelineState> mConstructResultPSO;
    std::unordered_map < std::string, u32> mRegisterMapConstructResult;

    ComPtr<ID3D12RootSignature> mRsRotateInFourierSpace;
    ComPtr<ID3D12PipelineState> mRotateInFourierSpacePSO;
    std::unordered_map < std::string, u32> mRegisterMapRotateInFourierSpace;

    std::wstring mAssetPath;

    LARGE_INTEGER mCpuFreq;
    LARGE_INTEGER mStartTime;
    LARGE_INTEGER mEndTime;

    bool mIsReverseMode = false;
};
