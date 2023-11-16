#include "RenderDeviceDX12.h"

#include <stdexcept>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

//VRAM ALLOCATOR
namespace dx12
{
    void VRAMAllocator::Allocate(Descriptor* desc) {
        auto it = mAllocationMap.find(1);
        if (it != mAllocationMap.end()) {
            if (!it->second.empty()) {
                *desc = it->second.front();
                it->second.pop_front();
                return;
            }
        }
        auto hCpu = mHeap->GetCPUDescriptorHandleForHeapStart();
        auto hGpu = mHeap->GetGPUDescriptorHandleForHeapStart();

        *desc = { };
        auto heapDesc = mHeap->GetDesc();
        if (mAllocateIndex < heapDesc.NumDescriptors) {
            auto offset = mIncrementSize * mAllocateIndex;
            hCpu.ptr += offset;
            hGpu.ptr += offset;
            (*desc).heapBaseOffset = offset;
            (*desc).hCpu = hCpu;
            (*desc).hGpu = hGpu;
            (*desc).type = heapDesc.Type;
            mAllocateIndex++;
        }
    }

    void VRAMAllocator::Deallocate(Descriptor* desc) {
        auto it = mAllocationMap.find(1);
        if (it == mAllocationMap.end()) {
            mAllocationMap.insert(std::make_pair(1, DescriptorList()));
            it = mAllocationMap.find(1);
        }
        it->second.push_front(*desc);
    }
}//VRAM ALLOCATOR




//RENDER DEVICE DX12
namespace dx12
{
    RenderDeviceDX12::RenderDeviceDX12() : 
        mWidth(0), mHeight(0), mFrameFenceTbl(), mFenceValueTbl(), mViewport(), mScissorRect()
    {
        mDefaultHeapProps = D3D12_HEAP_PROPERTIES{
            D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1
        };
        mUploadHeapProps = D3D12_HEAP_PROPERTIES{
            D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1
        };
    }
    RenderDeviceDX12::~RenderDeviceDX12() {
    }

    bool RenderDeviceDX12::Initialize()
    {
        u32 dxgiFlags = 0;

#if _DEBUG 
        {
            ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
                debug->EnableDebugLayer();
                dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }

        {
            ComPtr<ID3D12Debug1> debug1;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug1))))
            {
                debug1->EnableDebugLayer();
                debug1->SetEnableGPUBasedValidation(true);
            }
        }
#endif

        // DXGIFactory
        ComPtr<IDXGIFactory3> factory;
        HRESULT hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            return false;
        }

        // search adaper
        ComPtr<IDXGIAdapter1> useAdapter;
        {
            u32 adapterIndex = 0;
            ComPtr<IDXGIAdapter1> adapter;
            while (DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter)) {
                DXGI_ADAPTER_DESC1 desc1{};
                adapter->GetDesc1(&desc1);
                ++adapterIndex;
                if (desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                    continue;
                }

                hr = D3D12CreateDevice(
                    adapter.Get(),
                    D3D_FEATURE_LEVEL_12_0,
                    __uuidof(ID3D12Device), nullptr);
                if (SUCCEEDED(hr)) {
                    break;
                }
            }
            adapter.As(&useAdapter);
        }

        //create device instance
        hr = D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mD3D12Device));
        if (FAILED(hr)) {
            return false;
        }

        DXGI_ADAPTER_DESC1 adapterDesc{};
        useAdapter->GetDesc1(&adapterDesc);
        {
            std::vector<char> buf;
            auto ret = WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, nullptr, 0, nullptr, nullptr);
            if (ret > 0) {
                buf.resize(ret + 1);
                ret = WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, buf.data(), s32(buf.size()) - 1, nullptr, nullptr);
                mAdapterName = std::string(buf.data());
            }
        }

        // check support DXR 
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};
        hr = mD3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, u32(sizeof(options)));
        if (FAILED(hr) || options.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
            throw std::runtime_error("DirectX Raytracing NOT supported.");
        }

        // create command queue
        D3D12_COMMAND_QUEUE_DESC queueDesc{
            D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0
        };
        hr = mD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));
        if (FAILED(hr)) {
            return false;
        }

        // create descriptor heap for RTV
        ComPtr<ID3D12DescriptorHeap> heap;
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RenderTargetViewMax, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0
        };
        hr = mD3D12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            return false;
        }
        mRTVHeap.Initialize(heap, mD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

        // create descriptor heap for DSV
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DepthStencilViewMax, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0
        };
        hr = mD3D12Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            return false;
        }
        mDSVHeap.Initialize(heap, mD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

        // create descriptor heap for general purpose
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, ShaderResourceViewMax, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0
        };
        hr = mD3D12Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            return false;
        }
        mHeap.Initialize(heap, mD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

        // create command allocator
        for (u32 i = 0; i < BackBufferCount; ++i) {
            hr = mD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocatorTbl.at(i).ReleaseAndGetAddressOf()));
            if (FAILED(hr)) {
                return false;
            }
        }

        mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        mWaitEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);;
        return true;
    }
    void RenderDeviceDX12::Terminate()
    {
        WaitForCompletePipe();

        CloseHandle(mFenceEvent); 
        mFenceEvent = 0;
        CloseHandle(mWaitEvent); 
        mWaitEvent = 0;

        for (u32 i = 0; i < BackBufferCount; ++i) {
            mRTVHeap.Deallocate(&mRenderTargetDescriptorTbl.at(i));
        }
        mDSVHeap.Deallocate(&mDSVDescriptor);
    }

    bool RenderDeviceDX12::CreateSwapchain(u32 width, u32 height, HWND hwnd)
    {
        HRESULT hr;
        mWidth = width;
        mHeight = height;

        if (!mSwapchain)
        {
            ComPtr<IDXGIFactory3> factory;
            hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
            if (FAILED(hr)) {
                return false;
            }

            DXGI_SWAP_CHAIN_DESC1 scDesc{};
            scDesc.BufferCount = BackBufferCount;
            scDesc.Width = width;
            scDesc.Height = height;
            scDesc.Format = mBackbufferFormat;
            scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            scDesc.SampleDesc.Count = 1;

            ComPtr<IDXGISwapChain1> swapchain;
            hr = factory->CreateSwapChainForHwnd(
                mCommandQueue.Get(),
                hwnd,
                &scDesc,
                nullptr,
                nullptr,
                &swapchain
            );
            if (FAILED(hr)) {
                return false;
            }
            swapchain.As(&mSwapchain);

            for (u32 i = 0; i < BackBufferCount; ++i) {
                mFenceValueTbl.at(i) = 0;
                hr = mD3D12Device->CreateFence(mFenceValueTbl.at(i), D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFrameFenceTbl.at(i).ReleaseAndGetAddressOf()));
                if (FAILED(hr)) {
                    return false;
                }
            }
        }
        else
        {
            // if u want support resizable window, do impl
        }

        for (u32 i = 0; i < BackBufferCount; ++i) {
            mSwapchain->GetBuffer(i, IID_PPV_ARGS(mRenderTargetTbl.at(i).ReleaseAndGetAddressOf()));

            wchar_t name[30];
            swprintf(name, 30, L"RenderTargetTbl[%d]", i);

            mRenderTargetTbl.at(i)->SetName(name);

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format = mBackbufferFormat;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

            mRTVHeap.Allocate(&mRenderTargetDescriptorTbl.at(i));
            auto rtvHandle = mRenderTargetDescriptorTbl.at(i).hCpu;
            mD3D12Device->CreateRenderTargetView(mRenderTargetTbl.at(i).Get(), &rtvDesc, rtvHandle);
        }

        {
            D3D12_RESOURCE_DESC depthDesc{};
            depthDesc.Format = mDepthFormat;
            depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.DepthOrArraySize = 1;
            depthDesc.MipLevels = 1;
            depthDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            depthDesc.SampleDesc.Count = 1;

            D3D12_CLEAR_VALUE clearValue{};
            clearValue.Format = mDepthFormat;
            clearValue.DepthStencil.Depth = 1.0f;

            hr = mD3D12Device->CreateCommittedResource(
                &mDefaultHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &depthDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &clearValue,
                IID_PPV_ARGS(mDepthBuffer.ReleaseAndGetAddressOf()));
            if (FAILED(hr)) {
                return false;
            }
            mDepthBuffer->SetName(L"DepthBuffer");
        }
        mFrameIndex = mSwapchain->GetCurrentBackBufferIndex();

        mViewport.TopLeftX = 0.0f;
        mViewport.TopLeftY = 0.0f;
        mViewport.Width = f32(width);
        mViewport.Height = f32(height);
        mViewport.MinDepth = 0.0f;
        mViewport.MaxDepth = 1.0f;

        mScissorRect.left = 0;
        mScissorRect.right = width;
        mScissorRect.top = 0;
        mScissorRect.bottom = height;

        return true;
    }

    RenderDeviceDX12::ComPtr<ID3D12GraphicsCommandList4> RenderDeviceDX12::CreateCommandList() {
        ComPtr<ID3D12GraphicsCommandList4> commandList;
        auto allocator = GetCurrentCommandAllocator();
        mD3D12Device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf())
        );
        return commandList;
    }
    RenderDeviceDX12::ComPtr<ID3D12Fence1> RenderDeviceDX12::CreateFence() {
        ComPtr<ID3D12Fence1> fence;
        mD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()));
        return fence;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE RenderDeviceDX12::GetRenderTargetView() {
        return mRenderTargetDescriptorTbl[mFrameIndex].hCpu;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE RenderDeviceDX12::GetDepthStencilView() {
        return mDSVDescriptor.hCpu;
    }

    RenderDeviceDX12::ComPtr<ID3D12Resource> RenderDeviceDX12::GetRenderTarget() {
        return mRenderTargetTbl[mFrameIndex];
    }

    RenderDeviceDX12::ComPtr<ID3D12Resource> RenderDeviceDX12::GetDepthStencil() {
        return mDepthBuffer;
    }

    void RenderDeviceDX12::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList4> command)
    {
        ID3D12CommandList* commandLists[] = {
            command.Get(),
        };
        mCommandQueue->ExecuteCommandLists(1, commandLists);
    }

    void RenderDeviceDX12::Present(u32 interval) {
        if (mSwapchain) {
            mSwapchain->Present(interval, 0);
            WaitAvailableFrame();
        }
    }

    void RenderDeviceDX12::WaitForCompletePipe() {
        if (mCommandQueue) {
            auto commandList = CreateCommandList();
            auto waitFence = CreateFence();
            UINT64 fenceValue = 1;
            waitFence->SetEventOnCompletion(fenceValue, mWaitEvent);
            mCommandQueue->Signal(waitFence.Get(), fenceValue);
            WaitForSingleObject(mWaitEvent, INFINITE);
        }
    }

    RenderDeviceDX12::ComPtr<ID3D12Resource> RenderDeviceDX12::CreateConstantBuffer(size_t size, const void* initDataPtr, const wchar_t* namePtr)
    {
        size = RoundUp(size, 255);
        return CreateBuffer(
            size,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_HEAP_TYPE_UPLOAD,
            initDataPtr,
            namePtr);
    }

    RenderDeviceDX12::ComPtr<ID3D12Resource> RenderDeviceDX12::CreateBuffer(size_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, const void* initDataPtr, const wchar_t* namePtr) {
        D3D12_HEAP_PROPERTIES heapProps{};

        if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
            initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        if (heapType == D3D12_HEAP_TYPE_DEFAULT) {
            heapProps = GetDefaultHeapProps();
        }
        if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
            heapProps = GetUploadHeapProps();
        }
        HRESULT hr;
        ComPtr<ID3D12Resource> resource;
        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Alignment = 0;
        resDesc.Width = size;
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.Format = DXGI_FORMAT_UNKNOWN;
        resDesc.SampleDesc = { 1, 0 };
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resDesc.Flags = flags;

        hr = mD3D12Device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) {
            OutputDebugStringA("CreateBuffer Failed.\n");
        }
        if (resource != nullptr && namePtr != nullptr) {
            resource->SetName(namePtr);
        }

        if (initDataPtr != nullptr) {
            if (heapType == D3D12_HEAP_TYPE_DEFAULT) {
                ImmediateBufferUpdateStagingCopy(resource, initDataPtr, size);
            }
            if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
                ImmediateBufferUpdateHostVisible(resource, initDataPtr, size);
            }
        }

        return resource;
    }

    RenderDeviceDX12::ComPtr<ID3D12Resource> RenderDeviceDX12::CreateTexture2D(u32 width, u32 height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, const wchar_t* namePtr) {
        D3D12_HEAP_PROPERTIES heapProps{};
        if (heapType == D3D12_HEAP_TYPE_DEFAULT) {
            heapProps = GetDefaultHeapProps();
        }
        if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
            heapProps = GetUploadHeapProps();
        }
        HRESULT hr;
        ComPtr<ID3D12Resource> resource;
        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resDesc.Alignment = 0;
        resDesc.Width = width;
        resDesc.Height = height;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.Format = format;
        resDesc.SampleDesc = { 1, 0 };
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resDesc.Flags = flags;

        hr = mD3D12Device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) {
            OutputDebugStringA("CreateTexture2D Failed.\n");
        }

        if (resource != nullptr && namePtr != nullptr)
        {
            resource->SetName(namePtr);
        }

        return resource;
    }

    dx12::Descriptor dx12::RenderDeviceDX12::CreateShaderResourceView(ComPtr<ID3D12Resource> resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc)
    {
        auto descriptor = AllocateDescriptor();
        mD3D12Device->CreateShaderResourceView(
            resource.Get(),
            srvDesc,
            descriptor.hCpu);
        return descriptor;
    }
    dx12::Descriptor dx12::RenderDeviceDX12::CreateUnorderedAccessView(ComPtr<ID3D12Resource> resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc)
    {
        auto descriptor = AllocateDescriptor();
        mD3D12Device->CreateUnorderedAccessView(
            resource.Get(),
            nullptr,
            uavDesc,
            descriptor.hCpu);
        return descriptor;
    }

    dx12::AccelerationStructureBuffers dx12::RenderDeviceDX12::CreateAccelerationStructure(
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& asDesc)
    {
        AccelerationStructureBuffers asb{};
        // calc memory footprint
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO asPrebuild{};
        GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(
            &asDesc.Inputs, &asPrebuild
        );

        // alloc scratchBuffer buffer
        asb.scratchBuffer = CreateBuffer(
            asPrebuild.ScratchDataSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_HEAP_TYPE_DEFAULT
        );

        // alloc AS buffer
        asb.ASBuffer = CreateBuffer(
            asPrebuild.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            D3D12_HEAP_TYPE_DEFAULT
        );

        if (asb.scratchBuffer == nullptr || asb.ASBuffer == nullptr) {
            throw std::runtime_error("CreateAccelerationStructure Failed.");
        }

        // alloc update buffer
        if (asDesc.Inputs.Flags & D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE) {
            asb.updateBuffer = CreateBuffer(
                asPrebuild.UpdateScratchDataSizeInBytes,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_HEAP_TYPE_DEFAULT
            );
            if (asb.updateBuffer == nullptr) {
                throw std::runtime_error("CreateAccelerationStructure Failed.");
            }
        }

        return asb;
    }

    void RenderDeviceDX12::ImmediateBufferUpdateHostVisible(ComPtr<ID3D12Resource> resource, const void* dataPtr, size_t dataSize) {
        if (resource == nullptr) {
            return;
        }
        void* mappedResPtr = nullptr;
        D3D12_RANGE range{ 0, dataSize };
        HRESULT hr = resource->Map(0, &range, &mappedResPtr);
        if (SUCCEEDED(hr)) {
            memcpy(mappedResPtr, dataPtr, dataSize);
            resource->Unmap(0, &range);
        }
    }

    void RenderDeviceDX12::ImmediateBufferUpdateStagingCopy(ComPtr<ID3D12Resource> resource, const void* dataPtr, size_t dataSize) {
        if (resource == nullptr) {
            return;
        }
        ComPtr<ID3D12Resource> staging;
        HRESULT hr;
        auto heapProps = GetUploadHeapProps();
        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = dataSize;
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc = { 1, 0 };
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hr = mD3D12Device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(staging.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) {
            return;
        }
        void* mappedResPtr = nullptr;
        D3D12_RANGE range{ 0, dataSize };
        hr = staging->Map(0, &range, &mappedResPtr);
        if (SUCCEEDED(hr)) {
            memcpy(mappedResPtr, dataPtr, dataSize);
            staging->Unmap(0, &range);
        }

        auto command = CreateCommandList();
        ComPtr<ID3D12Fence> fence = CreateFence();
        command->CopyResource(resource.Get(), staging.Get());
        command->Close();

        ExecuteCommandList(command);

        WaitForCompletePipe();
    }


    Descriptor RenderDeviceDX12::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type) {
        dx12::Descriptor descriptor;
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
            mHeap.Allocate(&descriptor);
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
            mRTVHeap.Allocate(&descriptor);
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
            mDSVHeap.Allocate(&descriptor);
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
        }
        return descriptor;
    }
    void RenderDeviceDX12::DeallocateDescriptor(Descriptor& descriptor) {
        auto type = descriptor.type;
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
            mHeap.Deallocate(&descriptor);
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
            mRTVHeap.Deallocate(&descriptor);
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
            mDSVHeap.Deallocate(&descriptor);
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
        }
    }
    RenderDeviceDX12::ComPtr<ID3D12DescriptorHeap> RenderDeviceDX12::GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) {
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
            return mHeap.GetHeap();
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
            return mRTVHeap.GetHeap();
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
            return mDSVHeap.GetHeap();
        }
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
        }
        return nullptr;
    }

    void RenderDeviceDX12::WaitAvailableFrame() {
        auto fence = mFrameFenceTbl[mFrameIndex];
        auto value = ++mFenceValueTbl[mFrameIndex];
        mCommandQueue->Signal(fence.Get(), value);

        mFrameIndex = mSwapchain->GetCurrentBackBufferIndex();
        fence = mFrameFenceTbl.at(mFrameIndex);
        auto finishValue = mFenceValueTbl[mFrameIndex];
        if (fence->GetCompletedValue() < finishValue) {
            fence->SetEventOnCompletion(finishValue, mFenceEvent);
            WaitForSingleObject(mFenceEvent, INFINITE);
        }
    }
}//RENDER DEVICE DX12
