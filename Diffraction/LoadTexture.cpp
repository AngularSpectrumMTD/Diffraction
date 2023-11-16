#include "Diffraction.h"
#include <DirectXTex.h>
using namespace DirectX;

utility::TextureResource Diffraction::LoadTextureFromFile(const std::wstring& fileName)
{
    DirectX::TexMetadata metadata;
    DirectX::ScratchImage image;

    HRESULT hr = E_FAIL;
    const std::wstring extDDS(L"dds");
    const std::wstring extPNG(L"png");
    if (fileName.length() < 3) {
        throw std::runtime_error("Texture Filename is Invalid.");
    }

    if (std::equal(std::rbegin(extDDS), std::rend(extDDS), std::rbegin(fileName))) {
        hr = LoadFromDDSFile(fileName.c_str(), DDS_FLAGS_NONE, &metadata, image);
    }
    if (std::equal(std::rbegin(extPNG), std::rend(extPNG), std::rbegin(fileName))) {
        hr = LoadFromWICFile(fileName.c_str(), WIC_FLAGS_NONE, &metadata, image);
    }

    ComPtr<ID3D12Resource> texRes;
    ComPtr<ID3D12Device> device;
    mDevice->GetDevice().As(&device);
    CreateTexture(device.Get(), metadata, &texRes);
    texRes->SetName(fileName.c_str());

    ComPtr<ID3D12Resource> srcBuffer;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    PrepareUpload(device.Get(), image.GetImages(), image.GetImageCount(), metadata, subresources);
    const auto totalBytes = GetRequiredIntermediateSize(texRes.Get(), 0, u32(subresources.size()));

    auto staging = mDevice->CreateBuffer(
        totalBytes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
    staging->SetName(L"Tex-Staging");

    auto command = mDevice->CreateCommandList();
    UpdateSubresources(
        command.Get(), texRes.Get(), staging.Get(), 0, 0, u32(subresources.size()), subresources.data());
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texRes.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command->ResourceBarrier(1, &barrier);
    command->Close();

    mDevice->ExecuteCommandList(command);
    mDevice->WaitForCompletePipe();

    utility::TextureResource ret;
    ret.res = texRes;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (metadata.IsCubemap()) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = u32(metadata.mipLevels);
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0;
    }
    else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = u32(metadata.mipLevels);
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0;
    }
    ret.srv = mDevice->CreateShaderResourceView(texRes.Get(), &srvDesc);

    return ret;
}