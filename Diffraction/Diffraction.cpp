#include "Diffraction.h"

#include "AppInvoker.h"

#include <fstream>
#include <DirectXTex.h>

#include <wincodec.h>
#include "utility/Utility.h"

#include <iostream>
#include <string>
#include <sstream>

using namespace DirectX;

#define STATE_SRV D3D12_RESOURCE_STATE_COPY_SOURCE
#define STATE_UAV D3D12_RESOURCE_STATE_UNORDERED_ACCESS

Diffraction::Diffraction(u32 width, u32 height) : AppBase(width, height, L"Diffraction")
{
   WCHAR assetsPath[512];
   GetAssetsPath(assetsPath, _countof(assetsPath));
   mAssetPath = assetsPath;
}

void Diffraction::Dispatch(const u32 x, const u32 y, const std::wstring cmdName)
{
    PIXBeginEvent(mCommandList.Get(), 0, cmdName.c_str());
    mCommandList->Dispatch(x, y, 1);
    PIXEndEvent(mCommandList.Get());
}

void Diffraction::UpdateWorkState(const D3D12_RESOURCE_STATES dst, const u32 idx)
{
    if (idx >= WORK_BUFFER_SIZE)
    {
        throw std::runtime_error("Invalid Index.");
    }

    if (dst != mWorkStateTbl[idx])
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mWorkBufferTbl[idx].Get(), mWorkStateTbl[idx], dst);
        mCommandList->ResourceBarrier(1, &barrier);
        mWorkStateTbl[idx] = dst;
    }
}

void Diffraction::UpdateResultState(const D3D12_RESOURCE_STATES dst)
{
    if (dst != mResultState)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mResultBuffer.Get(), mResultState, dst);
        mCommandList->ResourceBarrier(1, &barrier);
        mResultState = dst;
    }
}

void Diffraction::Initialize()
{
    if (!InitializeRenderDevice(AppInvoker::GetHWND()))
    {
        throw std::runtime_error("Failed Initialize RenderDevice.");
    }
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if ((hr == S_OK) || (hr == S_FALSE))
    {
        CreateRegularBuffer();
        CreateConstantBuffer();
        CreateComputeRootSignatureAndPSO();

        mCommandList = mDevice->CreateCommandList();
        mCommandList->Close();
    }
    else
    {
        throw std::runtime_error("Failed CoInitializeEx.");
    }
}

void Diffraction::Terminate()
{
    TerminateRenderDevice();
}

void Diffraction::UpdateWindowText()
{
    std::wstringstream windowText;
    windowText.str(L"");

    f32 realSizeX = mGenerateFRFParam.samplingIntervalRealX * EXECUTE_SIZE;
    f32 realSizeY = mGenerateFRFParam.samplingIntervalRealY * EXECUTE_SIZE;
    f32 apertureRealSizeX = realSizeX * mDrawPolygonParam.ratio;
    f32 apertureRealSizeY = realSizeY * mDrawPolygonParam.ratio;
    f32 apertureRadius = sqrt(apertureRealSizeX * apertureRealSizeX + apertureRealSizeY * apertureRealSizeY);

    windowText << L"       Mode (SPACE) : " << (mIsReverseMode ? L" ↓"  : L" ↑") << L"       PolygonAngle (A) : " << mDrawPolygonParam.N << L"       AngleX (X) [deg]: " << mRotateInFourierParam.degX << L"       AngleY (Y) [deg]: " << mRotateInFourierParam.degY << L"       AngleZ (Z) [deg]: " << mRotateInFourierParam.degZ << L"       Aperture Radius (R) [mm]: " << apertureRadius * 1000 << L"       Propagation Distance (P) [mm]: " << mGenerateFRFParam.propagateDistance * 1000;

    std::wstring finalWindowText = std::wstring(GetTitle()) + windowText.str().c_str();
    SetWindowText(AppInvoker::GetHWND(), finalWindowText.c_str());
}

void Diffraction::Update()
{
    UpdateWindowText();
}

void Diffraction::OnKeyDown(UINT8 wparam)
{
    switch (wparam)
    {
    case 'A':
        mDrawPolygonParam.N = Clamp(3, 200, mDrawPolygonParam.N + (mIsReverseMode ? -1 : 1));
        break;
    case 'R':
        mDrawPolygonParam.ratio = Clamp(0.01, 0.5, mDrawPolygonParam.ratio + (mIsReverseMode ? -0.001 : 0.001));
        break;
    case 'P':
        mGenerateFRFParam.propagateDistance = Clamp(-0.05, 0.05, mGenerateFRFParam.propagateDistance + (mIsReverseMode ? -0.000001 : 0.000001));
        break;
    case 'X':
        mRotateInFourierParam.degX = Clamp(-85, 85, mRotateInFourierParam.degX + (mIsReverseMode ? - 1 : 1));
        break;
    case 'Y':
        mRotateInFourierParam.degY = Clamp(-85, 85, mRotateInFourierParam.degY + (mIsReverseMode ? - 1 : 1));
        break;
    case 'Z':
        mRotateInFourierParam.degZ = Clamp(-85, 85, mRotateInFourierParam.degZ + (mIsReverseMode ? -1 : 1));
        break;
    case VK_SPACE:
        mIsReverseMode = !mIsReverseMode;
        break;
    }
}

void Diffraction::OnMouseDown(MouseButton button, s32 x, s32 y)
{
}

void Diffraction::OnMouseUp(MouseButton button, s32 x, s32 y)
{
}

void Diffraction::OnMouseMove(s32 dx, s32 dy)
{
}

void Diffraction::OnMouseWheel(s32 rotate)
{
}

f32 Diffraction::Clamp(f32 min, f32 max, f32 src)
{
    return std::fmax(min, std::fmin(src, max));
}

void Diffraction::UpdateConstantBufferParams()
{
    auto polygonConstantBuffer = mDrawPolygonCB.Get();
    mDevice->ImmediateBufferUpdateHostVisible(polygonConstantBuffer, &mDrawPolygonParam, sizeof(mDrawPolygonParam));

    auto generateFRFConstantBuffer = mGenerateFRFCB.Get();
    mDevice->ImmediateBufferUpdateHostVisible(generateFRFConstantBuffer, &mGenerateFRFParam, sizeof(mGenerateFRFParam));

    auto rotateInFourierSpaceConstantBuffer = mRotateInFourierCB.Get();
    mDevice->ImmediateBufferUpdateHostVisible(rotateInFourierSpaceConstantBuffer, &mRotateInFourierParam, sizeof(mRotateInFourierParam));
}

void Diffraction::PreProcess()
{
    QueryPerformanceFrequency(&mCpuFreq);

    auto allocator = mDevice->GetCurrentCommandAllocator();
    allocator->Reset();
    mCommandList->Reset(allocator.Get(), nullptr);

    UpdateConstantBufferParams();

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        mDevice->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Get(),
    };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void Diffraction::PostProcess()
{
    auto renderTarget = mDevice->GetRenderTarget();

    auto PresentToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    mCommandList->ResourceBarrier(1, &PresentToCopyDest);

    UpdateResultState(STATE_SRV);
    mCommandList->CopyResource(renderTarget.Get(), mResultBuffer.Get());

    auto CopyDestToTarget = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &CopyDestToTarget);

    auto TargetToPresent = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &TargetToPresent);

    mCommandList->Close();

    QueryPerformanceCounter(&mStartTime);
    mDevice->ExecuteCommandList(mCommandList);
    mDevice->Present(1);
    QueryPerformanceCounter(&mEndTime);
}

void Diffraction::Draw()
{
    PreProcess();

    BandLimitedASMProp();
    
    PostProcess();
}

f32 Diffraction::getFrameRate()
{
    return 1000.0f * (mEndTime.QuadPart - mStartTime.QuadPart) / mCpuFreq.QuadPart;
}

void Diffraction::BandLimitedASMProp()
{
    for (u32 i = 0; i < WORK_BUFFER_SIZE; i++)
    {
        UpdateWorkState(STATE_UAV, i);
        mCommandList->SetComputeRootSignature(mRsClearFloat.Get());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapClearFloat["tex"], mWorkUAVTbl[i].hGpu);
        mCommandList->SetPipelineState(mClearFloatPSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ClearFloat");
    }

    std::vector<CD3DX12_RESOURCE_BARRIER> uavBarrier0;
    uavBarrier0.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[0].Get()));
    mCommandList->ResourceBarrier(u32(uavBarrier0.size()), uavBarrier0.data());

    UpdateWorkState(STATE_UAV, 0);
    mCommandList->SetComputeRootSignature(mRsDrawPolygon.Get());
    mCommandList->SetComputeRootConstantBufferView(mRegisterMapDrawPolygon["constantBuffer"], mDrawPolygonCB.Get()->GetGPUVirtualAddress());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapDrawPolygon["polygon"], mWorkUAVTbl[0].hGpu);
    mCommandList->SetPipelineState(mDrawPolygonPSO.Get());
    Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"DrawPolygon");

    UpdateWorkState(STATE_SRV, 0);
    UpdateWorkState(STATE_SRV, 1);
    UpdateWorkState(STATE_UAV, 2);
    UpdateWorkState(STATE_UAV, 3);
    mCommandList->SetComputeRootSignature(mRsFFT.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realSrc"], mWorkSRVTbl[0].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagSrc"], mWorkSRVTbl[1].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realDst"], mWorkUAVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagDst"], mWorkUAVTbl[3].hGpu);
    mCommandList->SetPipelineState(mFFT_rowPSO.Get());
    Dispatch(1, EXECUTE_SIZE, L"FFT_row");

    UpdateWorkState(STATE_SRV, 2);
    UpdateWorkState(STATE_SRV, 3);
    UpdateWorkState(STATE_UAV, 0);
    UpdateWorkState(STATE_UAV, 1);
    mCommandList->SetComputeRootSignature(mRsFFT.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realSrc"], mWorkSRVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagSrc"], mWorkSRVTbl[3].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realDst"], mWorkUAVTbl[0].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagDst"], mWorkUAVTbl[1].hGpu);
    mCommandList->SetPipelineState(mFFT_colPSO.Get());
    Dispatch(1, EXECUTE_SIZE, L"FFT_col");

    UpdateWorkState(STATE_UAV, 2);
    UpdateWorkState(STATE_UAV, 3);
    mCommandList->SetComputeRootSignature(mRsGenerateFRF.Get());
    mCommandList->SetComputeRootConstantBufferView(mRegisterMapGenerateFRF["constantBuffer"], mGenerateFRFCB.Get()->GetGPUVirtualAddress());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapGenerateFRF["real"], mWorkUAVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapGenerateFRF["imag"], mWorkUAVTbl[3].hGpu);
    mCommandList->SetPipelineState(mGenerateFRFPSO.Get());
    Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"GenerateFRF");

    std::vector<CD3DX12_RESOURCE_BARRIER> uavBarrier1;
    uavBarrier1.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[0].Get()));
    uavBarrier1.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[1].Get()));
    mCommandList->ResourceBarrier(u32(uavBarrier1.size()), uavBarrier1.data());

    UpdateWorkState(STATE_SRV, 2);
    UpdateWorkState(STATE_SRV, 3);
    UpdateWorkState(STATE_UAV, 0);
    UpdateWorkState(STATE_UAV, 1);
    mCommandList->SetComputeRootSignature(mRsComplexMultiply.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexMultiply["real0"], mWorkSRVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexMultiply["imag0"], mWorkSRVTbl[3].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexMultiply["real1"], mWorkUAVTbl[0].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexMultiply["imag1"], mWorkUAVTbl[1].hGpu);
    mCommandList->SetPipelineState(mComplexMultiplyPSO.Get());
    Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ComplexMultiply");

    UpdateWorkState(STATE_SRV, 0);
    UpdateWorkState(STATE_SRV, 1);
    UpdateWorkState(STATE_UAV, 2);
    UpdateWorkState(STATE_UAV, 3);
    mCommandList->SetComputeRootSignature(mRsRotateInFourierSpace.Get());
    mCommandList->SetComputeRootConstantBufferView(mRegisterMapRotateInFourierSpace["constantBuffer"], mRotateInFourierCB.Get()->GetGPUVirtualAddress());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapRotateInFourierSpace["realSrc"], mWorkSRVTbl[0].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapRotateInFourierSpace["imagSrc"], mWorkSRVTbl[1].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapRotateInFourierSpace["realDst"], mWorkUAVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapRotateInFourierSpace["imagDst"], mWorkUAVTbl[3].hGpu);
    mCommandList->SetPipelineState(mRotateInFourierSpacePSO.Get());
    Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"RotateInFourierSpace");
    
    UpdateWorkState(STATE_SRV, 2);
    UpdateWorkState(STATE_SRV, 3);
    UpdateWorkState(STATE_UAV, 0);
    UpdateWorkState(STATE_UAV, 1);
    mCommandList->SetComputeRootSignature(mRsFFT.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realSrc"], mWorkSRVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagSrc"], mWorkSRVTbl[3].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realDst"], mWorkUAVTbl[0].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagDst"], mWorkUAVTbl[1].hGpu);
    mCommandList->SetPipelineState(mInvFFT_rowPSO.Get());
    Dispatch(1, EXECUTE_SIZE, L"invFFT_row");
   
    UpdateWorkState(STATE_SRV, 0);
    UpdateWorkState(STATE_SRV, 1);
    UpdateWorkState(STATE_UAV, 2);
    UpdateWorkState(STATE_UAV, 3);
    mCommandList->SetComputeRootSignature(mRsFFT.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realSrc"], mWorkSRVTbl[0].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagSrc"], mWorkSRVTbl[1].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realDst"], mWorkUAVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagDst"], mWorkUAVTbl[3].hGpu);
    mCommandList->SetPipelineState(mInvFFT_colPSO.Get());
    Dispatch(1, EXECUTE_SIZE, L"invFFT_col");

    std::vector<CD3DX12_RESOURCE_BARRIER> uavBarrier2;
    uavBarrier2.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[2].Get()));
    uavBarrier2.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[3].Get()));
    mCommandList->ResourceBarrier(u32(uavBarrier2.size()), uavBarrier2.data());

    mCommandList->SetComputeRootSignature(mRsFFT_Adjust.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT_Adjust["real"], mWorkUAVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT_Adjust["imag"], mWorkUAVTbl[3].hGpu);
    mCommandList->SetPipelineState(mFFT_AdjustPSO.Get());
    Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"FFT_Adjust");
   
    //Result 2: Intensity 3: Phase 0: SourceField
    {
        UpdateWorkState(STATE_SRV, 2);
        UpdateWorkState(STATE_SRV, 3);
        UpdateWorkState(STATE_UAV, 0);
        mCommandList->SetComputeRootSignature(mRsComplexIntensity.Get());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexIntensity["real"], mWorkSRVTbl[2].hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexIntensity["imag"], mWorkSRVTbl[3].hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexIntensity["intensity"], mWorkUAVTbl[0].hGpu);
        mCommandList->SetPipelineState(mComplexIntensityPSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ComplexIntensity");

        UpdateWorkState(STATE_SRV, 2);
        UpdateWorkState(STATE_SRV, 3);
        UpdateWorkState(STATE_UAV, 1);
        mCommandList->SetComputeRootSignature(mRsComplexPhase.Get());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexPhase["real"], mWorkSRVTbl[2].hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexPhase["imag"], mWorkSRVTbl[3].hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexPhase["phase"], mWorkUAVTbl[1].hGpu);
        mCommandList->SetPipelineState(mComplexPhasePSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ComplexPhase");

        UpdateWorkState(STATE_UAV, 2);
        mCommandList->SetComputeRootSignature(mRsDrawPolygon.Get());
        mCommandList->SetComputeRootConstantBufferView(mRegisterMapDrawPolygon["constantBuffer"], mDrawPolygonCB.Get()->GetGPUVirtualAddress());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapDrawPolygon["polygon"], mWorkUAVTbl[2].hGpu);
        mCommandList->SetPipelineState(mDrawPolygonPSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"DrawPolygon");
    }

    UpdateWorkState(STATE_SRV, 0);
    UpdateWorkState(STATE_SRV, 1);
    UpdateWorkState(STATE_SRV, 2);
    UpdateResultState(STATE_UAV);
    mCommandList->SetComputeRootSignature(mRsConstructResult.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapConstructResult["sourceField"], mWorkSRVTbl[2].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapConstructResult["intensity"], mWorkSRVTbl[0].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapConstructResult["phase"], mWorkSRVTbl[1].hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapConstructResult["resultF4"], mResultUAV.hGpu);
    mCommandList->SetPipelineState(mConstructResultPSO.Get());
    Dispatch(GetWidth() / NORMAL_THREAD_SIZE, GetHeight() / NORMAL_THREAD_SIZE, L"ConstructResult");
}