#include "Diffraction.h"

using namespace DirectX;

void Diffraction::CreateResultBuffer()
{
    auto width = GetWidth();
    auto height = GetHeight();

    mResultBuffer = mDevice->CreateTexture2D(
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_HEAP_TYPE_DEFAULT,
        L"ResultBuffer"
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mResultSRV = mDevice->CreateShaderResourceView(mResultBuffer.Get(), &srvDesc);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    mResultUAV = mDevice->CreateUnorderedAccessView(mResultBuffer.Get(), &uavDesc);

    mResultState = D3D12_RESOURCE_STATE_COPY_SOURCE;
}

void Diffraction::CreateWorkBuffer()
{
    for (u32 i = 0; i < WORK_BUFFER_SIZE; i++)
    {
        mWorkBufferTbl[i] = mDevice->CreateTexture2D(
            EXECUTE_SIZE, EXECUTE_SIZE, DXGI_FORMAT_R32_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_HEAP_TYPE_DEFAULT
        );

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        mWorkSRVTbl[i] = mDevice->CreateShaderResourceView(mWorkBufferTbl[i].Get(), &srvDesc);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        mWorkUAVTbl[i] = mDevice->CreateUnorderedAccessView(mWorkBufferTbl[i].Get(), &uavDesc);

        mWorkStateTbl[i] = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }

    for (u32 i = 0; i < 2; i++)
    {
        mInputSpectrumBufferTbl[i] = mDevice->CreateTexture2D(
            EXECUTE_SIZE, EXECUTE_SIZE, DXGI_FORMAT_R32_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_HEAP_TYPE_DEFAULT
        );

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        mInputSpectrumSRVTbl[i] = mDevice->CreateShaderResourceView(mInputSpectrumBufferTbl[i].Get(), &srvDesc);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        mInputSpectrumUAVTbl[i] = mDevice->CreateUnorderedAccessView(mInputSpectrumBufferTbl[i].Get(), &uavDesc);

        mInputSpectrumStateTbl[i] = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }

    mFullColorIntensityBuffer = mDevice->CreateTexture2D(
        EXECUTE_SIZE, EXECUTE_SIZE, DXGI_FORMAT_R32G32B32A32_FLOAT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_HEAP_TYPE_DEFAULT
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mFullColorIntensitySRV = mDevice->CreateShaderResourceView(mFullColorIntensityBuffer.Get(), &srvDesc);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    mFullColorIntensityUAV = mDevice->CreateUnorderedAccessView(mFullColorIntensityBuffer.Get(), &uavDesc);

    mFullColorIntensityState = D3D12_RESOURCE_STATE_COPY_SOURCE;
}

void Diffraction::CreateRegularBuffer()
{
    CreateResultBuffer();
    CreateWorkBuffer();
}

void Diffraction::CreateConstantBuffer()
{
    mDrawPolygonCB = mDevice->CreateConstantBuffer(sizeof(DrawPolygonParam), nullptr, L"DrawPolygonCB");
    mQuadraticCB = mDevice->CreateConstantBuffer(sizeof(QuadraticParam), nullptr, L"QuadraticCB");
  
    for (u32 i = 0; i < LAMBDA_NUM; i++)
    {
        mGenerateFRFCBTbl[i] = mDevice->CreateConstantBuffer(sizeof(GenerateFRFParam), nullptr, L"GenerateFRFCB");
        mRotateInFourierCBTbl[i] = mDevice->CreateConstantBuffer(sizeof(RotationInFourierSpaceParam), nullptr, L"RotateInFourierCB");
        mCompositeIntensityCBTbl[i] = mDevice->CreateConstantBuffer(sizeof(CompositeIntensityParam), nullptr, L"CompositeIntensityCB");
    }
}