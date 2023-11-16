#pragma once

#include <d3d12.h>
#include <dxgi1_5.h>

#include <wrl.h>
#include <string>
#include <memory>
#include <vector>
#include <list>
#include <array>
#include <unordered_map>

typedef float f32;
typedef int s32;
typedef unsigned int u32;
typedef double f64;

namespace dx12
{
    struct Descriptor
    {
        u32 heapBaseOffset;
        D3D12_CPU_DESCRIPTOR_HANDLE hCpu;
        D3D12_GPU_DESCRIPTOR_HANDLE hGpu;
        D3D12_DESCRIPTOR_HEAP_TYPE type;
        Descriptor() : heapBaseOffset(0), hCpu(), hGpu(), type(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES) { }
        bool IsInvalid() const { 
            return type == D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
        }
    };

    struct AccelerationStructureBuffers {
        Microsoft::WRL::ComPtr<ID3D12Resource> scratchBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> ASBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> updateBuffer;
    };

    class VRAMAllocator {
    public:
        template<class T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;

        VRAMAllocator() = default;
        VRAMAllocator(const VRAMAllocator&) = delete;
        VRAMAllocator& operator=(const VRAMAllocator&) = delete;
        void Initialize(ComPtr<ID3D12DescriptorHeap> heap, u32 incSize) { mHeap = heap; mIncrementSize = incSize; }
        
        void Allocate(Descriptor* desc);
        void Deallocate(Descriptor* desc);

        ComPtr<ID3D12DescriptorHeap> GetHeap() { return mHeap; }
    private:
        u32 mAllocateIndex = 0;
        u32 mIncrementSize = 0;
        ComPtr<ID3D12DescriptorHeap> mHeap;

        using DescriptorList = std::list<Descriptor>;
        std::unordered_map<u32, DescriptorList> mAllocationMap;
    };

    class RenderDeviceDX12 {
    public:
        template<class T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;

        static const u32 BackBufferCount = 3;
        static const u32 RenderTargetViewMax = 64;
        static const u32 DepthStencilViewMax = 64;
        static const u32 ShaderResourceViewMax = 1024;

        RenderDeviceDX12();
        RenderDeviceDX12(const RenderDeviceDX12&) = delete;
        RenderDeviceDX12& operator=(const RenderDeviceDX12&) = delete;

        ~RenderDeviceDX12();

        bool Initialize();
        void Terminate();

        bool CreateSwapchain(u32 width, u32 height, HWND hwnd);

        u32 RoundUp(size_t size, u32 align) {
            return u32(size + align - 1) & ~(align - 1);
        }

        D3D12_HEAP_PROPERTIES GetDefaultHeapProps() const {
            return mDefaultHeapProps;
        }

        D3D12_HEAP_PROPERTIES GetUploadHeapProps() const {
            return mUploadHeapProps;
        }

        ComPtr<ID3D12CommandAllocator> GetCurrentCommandAllocator() {
            return mCommandAllocatorTbl[mFrameIndex];
        }
        u32 GetCurrentFrameIndex() const { return mFrameIndex; }
        
        ComPtr<ID3D12GraphicsCommandList4> CreateCommandList();
        ComPtr<ID3D12Fence1> CreateFence();

        D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView();
        D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView();
        ComPtr<ID3D12Resource> GetRenderTarget();
        ComPtr<ID3D12Resource> GetDepthStencil();
        const D3D12_VIEWPORT& GetDefaultViewport() const { return mViewport; }

        ComPtr<ID3D12Device5> GetDevice() { return mD3D12Device; }


        void ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList4> command);
        void Present(u32 interval);

        void WaitForCompletePipe();

        ComPtr<ID3D12Resource> CreateConstantBuffer(size_t size, const void* initDataPtr = nullptr, const wchar_t* namePtr = nullptr);
        ComPtr<ID3D12Resource> CreateBuffer(size_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, const void* initDataPtr = nullptr, const wchar_t* namePtr = nullptr);
        ComPtr<ID3D12Resource> CreateTexture2D(u32 width, u32 height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, const wchar_t* namePtr = nullptr);
        
        AccelerationStructureBuffers CreateAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& asDesc);
        
        dx12::Descriptor CreateShaderResourceView(ComPtr<ID3D12Resource> resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc);
        dx12::Descriptor CreateUnorderedAccessView(ComPtr<ID3D12Resource> resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc);

        // map/unmap
        void ImmediateBufferUpdateHostVisible(ComPtr<ID3D12Resource> resource, const void* dataPtr, size_t dataSize);

        // using staging bufferd copy
        void ImmediateBufferUpdateStagingCopy(ComPtr<ID3D12Resource> resource, const void* dataPtr, size_t dataSize);

        Descriptor AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        void DeallocateDescriptor(Descriptor& descriptor);

        ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);

        std::string GetAdapterName() const {
            return mAdapterName;
        }
        ComPtr<ID3D12CommandQueue> GetDefaultQueue() const {
            return mCommandQueue;
        }
    private:
        void WaitAvailableFrame();

        ComPtr<ID3D12Device5> mD3D12Device;
        ComPtr<ID3D12CommandQueue> mCommandQueue;
        ComPtr<IDXGISwapChain3> mSwapchain;

        VRAMAllocator mRTVHeap;
        VRAMAllocator mDSVHeap;
        VRAMAllocator mHeap;

        std::array<ComPtr<ID3D12CommandAllocator>, BackBufferCount> mCommandAllocatorTbl;
        std::array<ComPtr<ID3D12Fence1>, BackBufferCount> mFrameFenceTbl;
        std::array<UINT64, BackBufferCount> mFenceValueTbl;
        
        HANDLE mFenceEvent = 0;
        HANDLE mWaitEvent = 0;

        u32 mFrameIndex = 0;

        DXGI_FORMAT mBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT mDepthFormat = DXGI_FORMAT_D32_FLOAT;

        std::array<ComPtr<ID3D12Resource>, BackBufferCount> mRenderTargetTbl;
        std::array<Descriptor, BackBufferCount> mRenderTargetDescriptorTbl;
        ComPtr<ID3D12Resource> mDepthBuffer;
        Descriptor mDSVDescriptor;

        u32 mWidth;
        u32 mHeight;
        D3D12_VIEWPORT	mViewport;
        D3D12_RECT	mScissorRect;

        D3D12_HEAP_PROPERTIES mDefaultHeapProps;
        D3D12_HEAP_PROPERTIES mUploadHeapProps;

        std::string mAdapterName;
    };
} // dx12
