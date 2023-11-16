#include "utility/Utility.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <dxcapi.h>
#include "d3dx12.h"
#include <fstream>
#include <wincodec.h>

#pragma comment(lib, "dxcompiler.lib")

namespace utility {
    void RootSignatureCreater::Push(const RootType type, const u32 shaderRegister, const u32 registerSpace)
    {
        D3D12_ROOT_PARAMETER rootParam{};
        rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParam.ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(type);
        rootParam.Descriptor.ShaderRegister = shaderRegister;
        rootParam.Descriptor.RegisterSpace = registerSpace;
        mParams.push_back(rootParam);
    }

    void RootSignatureCreater::Push(const RangeType type, const u32 shaderRegister, const u32 registerSpace, const u32 descriptorCount)
    {
        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(type);
        range.NumDescriptors = descriptorCount;
        range.BaseShaderRegister = shaderRegister;
        range.RegisterSpace = registerSpace;
        range.OffsetInDescriptorsFromTableStart = 0;
        mRanges.push_back(range);

        // Range is resolved on "Create()"
        u32 rangeIndex = u32(mParams.size());
        mRangeLocation.push_back(rangeIndex);

        D3D12_ROOT_PARAMETER rootParam{};
        rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParam.DescriptorTable.NumDescriptorRanges = 1;
        rootParam.DescriptorTable.pDescriptorRanges = nullptr;
        mParams.push_back(rootParam);
    }

    void RootSignatureCreater::PushStaticSampler(const u32 shaderRegister, const u32 registerSpace, const D3D12_FILTER filter, const AddressMode addressingModeU, const AddressMode addressingModeV, const AddressMode addressingModeW)
    {
        CD3DX12_STATIC_SAMPLER_DESC desc;
        desc.Init(shaderRegister,
            filter,
            static_cast<D3D12_TEXTURE_ADDRESS_MODE>(addressingModeU),
            static_cast<D3D12_TEXTURE_ADDRESS_MODE>(addressingModeV),
            static_cast<D3D12_TEXTURE_ADDRESS_MODE>(addressingModeW)
        );
        desc.RegisterSpace = registerSpace;
        mSamplers.push_back(desc);
    }

    void RootSignatureCreater::Clear() {
        mParams.clear();
        mRanges.clear();
        mRangeLocation.clear();
        mSamplers.clear();
    }

    ComPtr<ID3D12RootSignature> RootSignatureCreater::Create(std::unique_ptr<dx12::RenderDeviceDX12>& device, bool isLocalRoot, const wchar_t* name)
    {
        for (u32 i = 0; i < u32(mRanges.size()); ++i) {
            auto index = mRangeLocation.at(i);
            mParams.at(index).DescriptorTable.pDescriptorRanges = &mRanges.at(i);
        }
        D3D12_ROOT_SIGNATURE_DESC desc{};
        if (!mParams.empty()) {
            desc.pParameters = mParams.data();
            desc.NumParameters = u32(mParams.size());
        }
        if (!mSamplers.empty()) {
            desc.pStaticSamplers = mSamplers.data();
            desc.NumStaticSamplers = u32(mSamplers.size());
        }
        if (isLocalRoot) {
            desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        }

        ComPtr<ID3DBlob> blob, errBlob;
        HRESULT hr = D3D12SerializeRootSignature(
            &desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &errBlob);
        if (FAILED(hr)) {
            return nullptr;
        }

        ComPtr<ID3D12RootSignature> rootSignature;
        hr = device->GetDevice()->CreateRootSignature(
            0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        if (FAILED(hr)) {
            return nullptr;
        }

        return rootSignature;
    }
}


namespace utility {
    HRESULT ReadDataFromFile(LPCWSTR filename, byte** dataPtr, u32* sizePtr)
    {
        using namespace Microsoft::WRL;

        CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {};
        extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
        extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
        extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
        extendedParams.lpSecurityAttributes = nullptr;
        extendedParams.hTemplateFile = nullptr;

        Wrappers::FileHandle file(CreateFile2(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, &extendedParams));
        if (file.Get() == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("file not found");
        }

        FILE_STANDARD_INFO fileInfo = {};
        if (!GetFileInformationByHandleEx(file.Get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
        {
            throw std::runtime_error("file not found");
        }

        if (fileInfo.EndOfFile.HighPart != 0)
        {
            throw std::runtime_error("file not found");
        }

        *dataPtr = reinterpret_cast<byte*>(malloc(fileInfo.EndOfFile.LowPart));
        *sizePtr = fileInfo.EndOfFile.LowPart;

        if (!ReadFile(file.Get(), *dataPtr, fileInfo.EndOfFile.LowPart, nullptr, nullptr))
        {
            throw std::exception();
        }

        return S_OK;
    }
}
