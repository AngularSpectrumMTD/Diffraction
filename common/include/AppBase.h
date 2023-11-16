#pragma once

#include <exception>
#include <stdexcept>
#include <functional>
#include <fstream>
#include <vector>

#include "RenderDeviceDX12.h"

#include <DirectXMath.h>

class AppBase {
public:
    template<class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    using XMFLOAT2 = DirectX::XMFLOAT2;
    using XMFLOAT3 = DirectX::XMFLOAT3;
    using XMFLOAT4 = DirectX::XMFLOAT4;
    using XMFLOAT3X4 = DirectX::XMFLOAT3X4;
    using XMFLOAT4X4 = DirectX::XMFLOAT4X4;
    using XMVECTOR = DirectX::XMVECTOR;
    using XMMATRIX = DirectX::XMMATRIX;
    using XMUINT3 = DirectX::XMUINT3;
    using XMUINT4 = DirectX::XMUINT4;

    virtual void Initialize() = 0;
    virtual void Terminate() = 0;

    virtual void Update() = 0;
    virtual void Draw() = 0;

    enum class MouseButton
    {
        LBUTTON,
        RBUTTON,
        MBUTTON,
    };

    virtual void OnMouseDown(MouseButton button, s32 x, s32 y) { }
    virtual void OnMouseUp(MouseButton button, s32 x, s32 y) { }
    virtual void OnMouseMove(s32 x, s32 y) {}
    virtual void OnKeyDown(UINT8 wparam){}
    virtual void OnMouseWheel(s32 rotate){}

    AppBase(u32 width, u32 height, const std::wstring& title) : mWidth(width), mHeight(height), mTitle(title) { }

    u32 GetWidth() const { return mWidth; }
    u32 GetHeight() const { return mHeight; }
    f32 GetAspect() const { return f32(mWidth) / f32(mHeight); }
    const wchar_t* GetTitle() const { return mTitle.c_str(); }

protected:
    bool InitializeRenderDevice(HWND hwnd) {
        mDevice = std::make_unique<dx12::RenderDeviceDX12>();
        if (!mDevice->Initialize()) {
            return false;
        }
        if (!mDevice->CreateSwapchain(GetWidth(), GetHeight(), hwnd)) {
            return false;
        }
        return true;
    }
    void TerminateRenderDevice() {
        if (mDevice) {
            mDevice->Terminate();
        }
        mDevice.reset();
    }

    std::unique_ptr<dx12::RenderDeviceDX12> mDevice;

private:
    u32 mWidth;
    u32 mHeight;
    std::wstring mTitle;
};


