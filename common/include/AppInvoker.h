#pragma once

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <windows.h>

#include "resource.h"

class AppBase;

class AppInvoker {
public:
    static int Execute(AppBase* appPtr, HINSTANCE hInstance);

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
    static HWND GetHWND() { return mHWnd; }

private:
    static HWND mHWnd;
};

#if defined(APP_INVOKER_IMPLEMENTATION)
#include <exception>
#include <stdexcept>
#include <sstream>

HWND AppInvoker::mHWnd;

int AppInvoker::Execute(AppBase* appPtr, HINSTANCE hInstance)
{
    if (!appPtr) {
        return EXIT_FAILURE;
    }

    try
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
        wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpfnWndProc = AppInvoker::WindowProc;
        wc.lpszClassName = L"DXR_APPLICATION";
        RegisterClassExW(&wc);

        RECT windowRect{};
        windowRect.right = LONG(appPtr->GetWidth());
        windowRect.bottom = LONG(appPtr->GetHeight());

        DWORD dwStyle = WS_OVERLAPPEDWINDOW;
        dwStyle &= ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX);
        AdjustWindowRect(&windowRect, dwStyle, FALSE);

        mHWnd = CreateWindowW(
            wc.lpszClassName,
            appPtr->GetTitle(),
            dwStyle,
            CW_USEDEFAULT, CW_USEDEFAULT,
            windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
            nullptr,
            nullptr,
            hInstance,
            appPtr
        );

        appPtr->Initialize();

        ShowWindow(mHWnd, SW_SHOWNORMAL);

        MSG msg{};
        //Execution Loop
        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        appPtr->Terminate();

        return EXIT_SUCCESS;
    }
    catch (std::exception& e)
    {
        std::ostringstream ss;
        ss << "Exception Occurred.\n";
        ss << e.what() << std::endl;
        OutputDebugStringA(ss.str().c_str());
        appPtr->Terminate();
        return EXIT_FAILURE;
    }
}

LRESULT CALLBACK AppInvoker::WindowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    POINT mousePoint{};
    static POINT lastMousePoint{};
    static bool isMousePressed = false;
    auto* appPtr = (AppBase*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_CREATE:
    {
        { // Set the Application instance with GWLP_USERDATA so that it can be used for other message processing.
            auto lpCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lp);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(lpCreateStruct->lpCreateParams));
        }
        return 0;
    }

    case WM_PAINT:
    {
        if (appPtr) {
            appPtr->Update();
            appPtr->Draw();
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (appPtr && isMousePressed) {
            GetCursorPos(&mousePoint);
            ScreenToClient(hWnd, &mousePoint);
            appPtr->OnMouseMove(mousePoint.x - lastMousePoint.x, mousePoint.y - lastMousePoint.y);
            lastMousePoint = mousePoint;
        }
    }
    break;

    case WM_MOUSEWHEEL:
    {
        appPtr->OnMouseWheel(HIWORD(wp));
    }
    break;

    case WM_KEYDOWN:
    {
        if (wp == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        else
        {
            appPtr->OnKeyDown(static_cast<UINT8>(wp));
        }
    }
    break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    {
        if (appPtr) {
            auto btn = AppBase::MouseButton::LBUTTON;
            if (msg == WM_RBUTTONDOWN) {
                btn = AppBase::MouseButton::RBUTTON;
            }
            if (msg == WM_MBUTTONDOWN) {
                btn = AppBase::MouseButton::MBUTTON;
            }
            SetCapture(hWnd);
            GetCursorPos(&mousePoint);
            ScreenToClient(hWnd, &mousePoint);

            appPtr->OnMouseDown(btn, mousePoint.x, mousePoint.y);
            lastMousePoint = mousePoint;
            isMousePressed = true;
        }
    }
    break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        if (appPtr) {
            ReleaseCapture();

            auto btn = AppBase::MouseButton::LBUTTON;
            if (msg == WM_RBUTTONUP) {
                btn = AppBase::MouseButton::RBUTTON;
            }
            if (msg == WM_MBUTTONUP) {
                btn = AppBase::MouseButton::MBUTTON;
            }
            GetCursorPos(&mousePoint);
            ScreenToClient(hWnd, &mousePoint);

            appPtr->OnMouseUp(btn, mousePoint.x, mousePoint.y);
            lastMousePoint = mousePoint;
            isMousePressed = false;
        }
    }
    break;

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    default:
        break;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

#endif//APP_INVOKER_IMPLEMENTATION
