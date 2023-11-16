#pragma once

#include <memory>
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

#include "RenderDeviceDX12.h"

namespace dx12 {
    class RenderDeviceDX12;
}

namespace utility {
    using Microsoft::WRL::ComPtr;
    using namespace DirectX;

    inline u32 roundUpPow2(s32 n)
    {
        if (n <= 0) return 0;
        if ((n & (n - 1)) == 0) return (u32)n;
        u32 ret = 1;
        while (n > 0) { ret <<= 1; n >>= 1; }
        return ret;
    }

    inline u32 RoundUp(size_t size, u32 align) {
        return u32(size + align - 1) & ~(align-1);
    }

    struct TextureResource
    {
        ComPtr<ID3D12Resource> res;
        dx12::Descriptor srv;
    };

    class RootSignatureCreater {
    public:
        enum class RootType {
            CBV = D3D12_ROOT_PARAMETER_TYPE_CBV,
            SRV = D3D12_ROOT_PARAMETER_TYPE_SRV,
            UAV = D3D12_ROOT_PARAMETER_TYPE_UAV,
        };
        enum class RangeType {
            SRV = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            UAV = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            CBV = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            Sampler = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
        };
        enum class AddressMode {
            Wrap = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            Mirror = D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
            Clamp = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            Border = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            MirrorOnce = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE
        };

        void Push(
            const RootType type,
            const u32 shaderRegister,
            const u32 registerSpace = 0
        );

        void Push(
            const RangeType type,
            const u32 shaderRegister,
            const u32 registerSpace = 0,
            const u32 descriptorCount = 1
        );

        void PushStaticSampler(
            const u32 shaderRegister,
            const u32 registerSpace = 0,
            const D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            const AddressMode addressingModeU = AddressMode::Wrap,
            const AddressMode addressingModeV = AddressMode::Wrap,
            const AddressMode addressingModeW = AddressMode::Wrap
        );

        void Clear();

        ComPtr<ID3D12RootSignature> Create(
            std::unique_ptr<dx12::RenderDeviceDX12>& device,
            bool isLocalRoot, const wchar_t* name);
    private:
        std::vector<D3D12_ROOT_PARAMETER> mParams;
        std::vector<D3D12_DESCRIPTOR_RANGE> mRanges;
        std::vector<u32> mRangeLocation;
        std::vector<D3D12_STATIC_SAMPLER_DESC> mSamplers;
    };

    HRESULT ReadDataFromFile(LPCWSTR filename, byte** dataPtr, u32* sizePtr);
}