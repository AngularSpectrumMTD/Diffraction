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
#define STATE_SRC D3D12_RESOURCE_STATE_COPY_SOURCE
#define STATE_DST D3D12_RESOURCE_STATE_COPY_DEST

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

dx12::Descriptor Diffraction::getFullColorIntensitySRV()
{
    if (STATE_SRV != mResultState)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mFullColorIntensityBuffer.Get(), mFullColorIntensityState, STATE_SRV);
        mCommandList->ResourceBarrier(1, &barrier);
        mFullColorIntensityState = STATE_SRV;
    }
    return mFullColorIntensitySRV;
}

dx12::Descriptor Diffraction::getFullColorIntensityUAV()
{
    if (STATE_UAV != mFullColorIntensityState)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mFullColorIntensityBuffer.Get(), mFullColorIntensityState, STATE_UAV);
        mCommandList->ResourceBarrier(1, &barrier);
        mFullColorIntensityState = STATE_UAV;
    }
    return mFullColorIntensityUAV;
}

dx12::Descriptor Diffraction::getWorkBufferSRV(const u32 idx)
{
    UpdateWorkState(STATE_SRV, idx);
    return mWorkSRVTbl[idx];
}

dx12::Descriptor Diffraction::getWorkBufferUAV(const u32 idx)
{
    UpdateWorkState(STATE_UAV, idx);
    return mWorkUAVTbl[idx];
}

ComPtr<ID3D12Resource> Diffraction::getWorkBufferSRC(const u32 idx)
{
    UpdateWorkState(STATE_SRC, idx);
    return mWorkBufferTbl[idx];
}

ComPtr<ID3D12Resource> Diffraction::getWorkBufferDST(const u32 idx)
{
    UpdateWorkState(STATE_DST, idx);
    return mWorkBufferTbl[idx];
}

dx12::Descriptor Diffraction::getInputSpectrumSRV(const u32 idx)
{
    if (STATE_SRV != mInputSpectrumStateTbl[idx])
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mInputSpectrumBufferTbl[idx].Get(), mInputSpectrumStateTbl[idx], STATE_SRV);
        mCommandList->ResourceBarrier(1, &barrier);
        mInputSpectrumStateTbl[idx] = STATE_SRV;
    }
    return mInputSpectrumSRVTbl[idx];
}

dx12::Descriptor Diffraction::getInputSpectrumUAV(const u32 idx)
{
    if (STATE_UAV != mInputSpectrumStateTbl[idx])
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mInputSpectrumBufferTbl[idx].Get(), mInputSpectrumStateTbl[idx], STATE_UAV);
        mCommandList->ResourceBarrier(1, &barrier);
        mInputSpectrumStateTbl[idx] = STATE_UAV;
    }
    return mInputSpectrumUAVTbl[idx];
}

ComPtr<ID3D12Resource> Diffraction::getInputSpectrumDST(const u32 idx)
{
    if (STATE_DST != mInputSpectrumStateTbl[idx])
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mInputSpectrumBufferTbl[idx].Get(), mInputSpectrumStateTbl[idx], STATE_DST);
        mCommandList->ResourceBarrier(1, &barrier);
        mInputSpectrumStateTbl[idx] = STATE_DST;
    }
    return mInputSpectrumBufferTbl[idx];
}

dx12::Descriptor Diffraction::getResultBufferSRV()
{
    UpdateResultState(STATE_SRV);
    return mResultSRV;
}

dx12::Descriptor Diffraction::getResultBufferUAV()
{
    UpdateResultState(STATE_UAV);
    return mResultUAV;
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
        mCommandList->SetComputeRootSignature(mRsClearFloat.Get());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapClearFloat["tex"], getWorkBufferUAV(i).hGpu);
        mCommandList->SetPipelineState(mClearFloatPSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ClearFloat");
    }

    mCommandList->SetComputeRootSignature(mRsClearFloat4.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapClearFloat4["tex"], getFullColorIntensityUAV().hGpu);
    mCommandList->SetPipelineState(mClearFloat4PSO.Get());
    Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ClearFloat4");

    std::vector<CD3DX12_RESOURCE_BARRIER> uavBarrier0;
    uavBarrier0.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[0].Get()));
    mCommandList->ResourceBarrier(u32(uavBarrier0.size()), uavBarrier0.data());

    mCommandList->SetComputeRootSignature(mRsDrawPolygon.Get());
    mCommandList->SetComputeRootConstantBufferView(mRegisterMapDrawPolygon["constantBuffer"], mDrawPolygonCB.Get()->GetGPUVirtualAddress());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapDrawPolygon["polygon"], getWorkBufferUAV(0).hGpu);
    mCommandList->SetPipelineState(mDrawPolygonPSO.Get());
    Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"DrawPolygon");

    mCommandList->SetComputeRootSignature(mRsFFT.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realSrc"], getWorkBufferSRV(0).hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagSrc"], getWorkBufferSRV(1).hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realDst"], getWorkBufferUAV(2).hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagDst"], getWorkBufferUAV(3).hGpu);
    mCommandList->SetPipelineState(mFFT_rowPSO.Get());
    Dispatch(1, EXECUTE_SIZE, L"FFT_row");

    mCommandList->SetComputeRootSignature(mRsFFT.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realSrc"], getWorkBufferSRV(2).hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagSrc"], getWorkBufferSRV(3).hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realDst"], getWorkBufferUAV(0).hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagDst"], getWorkBufferUAV(1).hGpu);
    mCommandList->SetPipelineState(mFFT_colPSO.Get());
    Dispatch(1, EXECUTE_SIZE, L"FFT_col");

    //この時点の周波数スペクトルは波長に依存しないので保持
    mCommandList->CopyResource(getInputSpectrumDST(0).Get(), getWorkBufferSRC(0).Get());
    mCommandList->CopyResource(getInputSpectrumDST(1).Get(), getWorkBufferSRC(1).Get());

    //波長ループ開始
    for (u32 i = 0; i < LAMBDA_NUM; i++)
    {
        const f32 lambdaNM = LAMBDA_VIO_NM + i * LAMBDA_STEP;

        mCompositeIntensityParam.wavelengthNM = lambdaNM;
        mGenerateFRFParam.wavelength = lambdaNM * 1e-9;
        mRotateInFourierParam.wavelength = lambdaNM * 1e-9;

        auto compositeIntensityConstantBuffer = mCompositeIntensityCBTbl[i].Get();
        mDevice->ImmediateBufferUpdateHostVisible(compositeIntensityConstantBuffer, &mCompositeIntensityParam, sizeof(mCompositeIntensityParam));
        auto generateFRFConstantBuffer = mGenerateFRFCBTbl[i].Get();
        mDevice->ImmediateBufferUpdateHostVisible(generateFRFConstantBuffer, &mGenerateFRFParam, sizeof(mGenerateFRFParam));
        auto rotateInFourierSpaceCconstantBuffer = mRotateInFourierCBTbl[i].Get();
        mDevice->ImmediateBufferUpdateHostVisible(rotateInFourierSpaceCconstantBuffer, &mRotateInFourierParam, sizeof(mRotateInFourierParam));

        mCommandList->SetComputeRootSignature(mRsGenerateFRF.Get());
        mCommandList->SetComputeRootConstantBufferView(mRegisterMapGenerateFRF["constantBuffer"], mGenerateFRFCBTbl[i].Get()->GetGPUVirtualAddress());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapGenerateFRF["real"], getWorkBufferUAV(0).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapGenerateFRF["imag"], getWorkBufferUAV(1).hGpu);
        mCommandList->SetPipelineState(mGenerateFRFPSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"GenerateFRF");

        std::vector<CD3DX12_RESOURCE_BARRIER> uavBarrier1;
        uavBarrier1.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[0].Get()));
        uavBarrier1.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[1].Get()));
        mCommandList->ResourceBarrier(u32(uavBarrier1.size()), uavBarrier1.data());

        mCommandList->SetComputeRootSignature(mRsComplexMultiply.Get());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexMultiply["real0"], getInputSpectrumSRV(0).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexMultiply["imag0"], getInputSpectrumSRV(1).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexMultiply["real1"], getWorkBufferUAV(0).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexMultiply["imag1"], getWorkBufferUAV(1).hGpu);
        mCommandList->SetPipelineState(mComplexMultiplyPSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ComplexMultiply");

        mCommandList->SetComputeRootSignature(mRsRotateInFourierSpace.Get());
        mCommandList->SetComputeRootConstantBufferView(mRegisterMapRotateInFourierSpace["constantBuffer"], mRotateInFourierCBTbl[i].Get()->GetGPUVirtualAddress());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapRotateInFourierSpace["realSrc"], getWorkBufferSRV(0).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapRotateInFourierSpace["imagSrc"], getWorkBufferSRV(1).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapRotateInFourierSpace["realDst"], getWorkBufferUAV(2).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapRotateInFourierSpace["imagDst"], getWorkBufferUAV(3).hGpu);
        mCommandList->SetPipelineState(mRotateInFourierSpacePSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"RotateInFourierSpace");

        mCommandList->SetComputeRootSignature(mRsFFT.Get());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realSrc"], getWorkBufferSRV(2).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagSrc"], getWorkBufferSRV(3).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realDst"], getWorkBufferUAV(0).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagDst"], getWorkBufferUAV(1).hGpu);
        mCommandList->SetPipelineState(mInvFFT_rowPSO.Get());
        Dispatch(1, EXECUTE_SIZE, L"invFFT_row");

        mCommandList->SetComputeRootSignature(mRsFFT.Get());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realSrc"], getWorkBufferSRV(0).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagSrc"], getWorkBufferSRV(1).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["realDst"], getWorkBufferUAV(2).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT["imagDst"], getWorkBufferUAV(3).hGpu);
        mCommandList->SetPipelineState(mInvFFT_colPSO.Get());
        Dispatch(1, EXECUTE_SIZE, L"invFFT_col");

        std::vector<CD3DX12_RESOURCE_BARRIER> uavBarrier2;
        uavBarrier2.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[2].Get()));
        uavBarrier2.emplace_back(CD3DX12_RESOURCE_BARRIER::UAV(mWorkBufferTbl[3].Get()));
        mCommandList->ResourceBarrier(u32(uavBarrier2.size()), uavBarrier2.data());

        mCommandList->SetComputeRootSignature(mRsFFT_Adjust.Get());
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT_Adjust["real"], getWorkBufferUAV(2).hGpu);
        mCommandList->SetComputeRootDescriptorTable(mRegisterMapFFT_Adjust["imag"], getWorkBufferUAV(3).hGpu);
        mCommandList->SetPipelineState(mFFT_AdjustPSO.Get());
        Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"FFT_Adjust");

        //Result 2: Intensity 3: Phase 0: SourceField
        {
            mCommandList->SetComputeRootSignature(mRsComplexIntensity.Get());
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexIntensity["real"], getWorkBufferSRV(2).hGpu);
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexIntensity["imag"], getWorkBufferSRV(3).hGpu);
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexIntensity["intensity"], getWorkBufferUAV(0).hGpu);
            mCommandList->SetPipelineState(mComplexIntensityPSO.Get());
            Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ComplexIntensity");

            mCommandList->SetComputeRootSignature(mRsCompositeIntensity.Get());
            mCommandList->SetComputeRootConstantBufferView(mRegisterMapCompositeIntensity["constantBuffer"], mCompositeIntensityCBTbl[i].Get()->GetGPUVirtualAddress());
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapCompositeIntensity["intensitySrc"], getWorkBufferSRV(0).hGpu);
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapCompositeIntensity["intensityDst"], getFullColorIntensityUAV().hGpu);
            mCommandList->SetPipelineState(mCompositeIntensityPSO.Get());
            Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"CompositeIntensity");

            mCommandList->SetComputeRootSignature(mRsComplexPhase.Get());
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexPhase["real"], getWorkBufferSRV(2).hGpu);
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexPhase["imag"], getWorkBufferSRV(3).hGpu);
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapComplexPhase["phase"], getWorkBufferUAV(1).hGpu);
            mCommandList->SetPipelineState(mComplexPhasePSO.Get());
            Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"ComplexPhase");

            mCommandList->SetComputeRootSignature(mRsDrawPolygon.Get());
            mCommandList->SetComputeRootConstantBufferView(mRegisterMapDrawPolygon["constantBuffer"], mDrawPolygonCB.Get()->GetGPUVirtualAddress());
            mCommandList->SetComputeRootDescriptorTable(mRegisterMapDrawPolygon["polygon"], getWorkBufferUAV(2).hGpu);
            mCommandList->SetPipelineState(mDrawPolygonPSO.Get());
            Dispatch(EXECUTE_SIZE / NORMAL_THREAD_SIZE, EXECUTE_SIZE / NORMAL_THREAD_SIZE, L"DrawPolygon");
        }
    }//波長ループ終了

    mCommandList->SetComputeRootSignature(mRsConstructResult.Get());
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapConstructResult["sourceField"], getWorkBufferSRV(2).hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapConstructResult["intensity"], getFullColorIntensitySRV().hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapConstructResult["phase"], getWorkBufferSRV(1).hGpu);
    mCommandList->SetComputeRootDescriptorTable(mRegisterMapConstructResult["resultF4"], getResultBufferUAV().hGpu);
    mCommandList->SetPipelineState(mConstructResultPSO.Get());
    Dispatch(GetWidth() / NORMAL_THREAD_SIZE, GetHeight() / NORMAL_THREAD_SIZE, L"ConstructResult");
}