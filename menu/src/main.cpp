// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
// Astral menu host: Win32 window + Direct3D 11 flip-model swap chain, per-monitor DPI, occlusion and resize handling.
//
// Debug switches (used for pixel comparisons against the reference captures):
//   --shot <file.bmp>  capture the back buffer after --frames N (default 90) and exit
//   --tab <0..6>       initial tab        --sub <n>    category / page inside the tab
//   --scale <s>        force UI scale     --size WxH   client size
//   --mouse x,y        park the cursor at window-relative x,y
//   --search text      prefill the footer search
//   --accent n         accent preset (0 Salmon, 1 Crimson, 2 Sunset, 3 Custom)
//   --custom a,b       custom accent start/end as RRGGBB,RRGGBB
//   --click x,y@f      left click at window-relative x,y on frame f (repeatable), --rclick for the right button
#ifndef UNICODE
#define UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d11.h>
#include <dxgi1_5.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

#include "astral/menu.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "shell32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

using Microsoft::WRL::ComPtr;

constexpr wchar_t kWindowClass[] = L"AstralMenuHost";

enum class PresentResult { Ok, Occluded, DeviceLost };

class D3D11Renderer {
public:
    D3D11Renderer() = default;
    ~D3D11Renderer()
    {
        if (latency_wait_ != nullptr) CloseHandle(latency_wait_);
    }
    D3D11Renderer(const D3D11Renderer&) = delete;
    D3D11Renderer& operator=(const D3D11Renderer&) = delete;

    [[nodiscard]] bool Create(HWND hwnd)
    {
        constexpr D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        HRESULT hr = E_FAIL;
        for (const D3D_DRIVER_TYPE type : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP }) {
            D3D_FEATURE_LEVEL got{};
            hr = D3D11CreateDevice(nullptr, type, nullptr, 0, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                   device_.ReleaseAndGetAddressOf(), &got, context_.ReleaseAndGetAddressOf());
            if (SUCCEEDED(hr)) break;
        }
        if (FAILED(hr)) return false;

        ComPtr<IDXGIDevice> dxgi_device;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory;
        if (FAILED(device_.As(&dxgi_device)) || FAILED(dxgi_device->GetAdapter(&adapter)) || FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
            return false;

        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory.As(&factory5))) {
            BOOL allow = FALSE;
            if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow)))) tearing_ = allow == TRUE;
        }
        swap_flags_ = static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) |
                      (tearing_ ? static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) : 0u);

        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Flags = swap_flags_;

        ComPtr<IDXGISwapChain1> swap1;
        if (FAILED(factory->CreateSwapChainForHwnd(device_.Get(), hwnd, &sd, nullptr, nullptr, &swap1))) return false;
        if (FAILED(swap1.As(&swap_))) return false;
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        swap_->SetMaximumFrameLatency(1);
        latency_wait_ = swap_->GetFrameLatencyWaitableObject();
        CreateTarget();
        return rtv_ != nullptr;
    }

    [[nodiscard]] bool Resize(UINT width, UINT height)
    {
        if (!swap_ || width == 0 || height == 0) return true;
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        rtv_.Reset(); // Warning: every back-buffer reference must be released before ResizeBuffers
        if (FAILED(swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, swap_flags_))) return false;
        CreateTarget();
        return rtv_ != nullptr;
    }

    void WaitForNextFrame() const noexcept
    {
        if (latency_wait_ != nullptr) WaitForSingleObjectEx(latency_wait_, 100, TRUE);
    }

    void BindAndClear(const float color[4]) const noexcept
    {
        ID3D11RenderTargetView* views[] = { rtv_.Get() };
        context_->OMSetRenderTargets(1, views, nullptr);
        context_->ClearRenderTargetView(rtv_.Get(), color);
    }

    [[nodiscard]] PresentResult Present(bool vsync) noexcept
    {
        const UINT flags = (!vsync && tearing_) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
        const HRESULT hr = swap_->Present(vsync ? 1u : 0u, flags);
        occluded_ = hr == DXGI_STATUS_OCCLUDED;
        if (occluded_) return PresentResult::Occluded;
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) return PresentResult::DeviceLost;
        return PresentResult::Ok;
    }

    [[nodiscard]] bool StillOccluded() noexcept
    {
        if (!occluded_) return false;
        occluded_ = swap_->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED;
        return occluded_;
    }

    // Copies the current back buffer into a 32-bit top-down BMP (debug captures only).
    bool Capture(const std::wstring& path) const
    {
        ComPtr<ID3D11Texture2D> back;
        if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
        D3D11_TEXTURE2D_DESC desc{};
        back->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &staging))) return false;
        context_->CopyResource(staging.Get(), back.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;

        const UINT w = desc.Width, h = desc.Height;
        std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4);
        for (UINT y = 0; y < h; ++y) {
            const unsigned char* src = static_cast<const unsigned char*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch;
            unsigned char* dst = pixels.data() + static_cast<size_t>(y) * w * 4;
            for (UINT x = 0; x < w; ++x) {
                dst[x * 4 + 0] = src[x * 4 + 2];
                dst[x * 4 + 1] = src[x * 4 + 1];
                dst[x * 4 + 2] = src[x * 4 + 0];
                dst[x * 4 + 3] = 255;
            }
        }
        context_->Unmap(staging.Get(), 0);

        BITMAPFILEHEADER fh{};
        BITMAPINFOHEADER ih{};
        ih.biSize = sizeof(ih);
        ih.biWidth = static_cast<LONG>(w);
        ih.biHeight = -static_cast<LONG>(h);
        ih.biPlanes = 1;
        ih.biBitCount = 32;
        ih.biCompression = BI_RGB;
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof(fh) + sizeof(ih);
        fh.bfSize = fh.bfOffBits + static_cast<DWORD>(pixels.size());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
        out.write(reinterpret_cast<const char*>(&ih), sizeof(ih));
        out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
        return static_cast<bool>(out);
    }

    [[nodiscard]] ID3D11Device* Device() const noexcept { return device_.Get(); }
    [[nodiscard]] ID3D11DeviceContext* Context() const noexcept { return context_.Get(); }

private:
    void CreateTarget()
    {
        ComPtr<ID3D11Texture2D> back_buffer;
        if (SUCCEEDED(swap_->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
            device_->CreateRenderTargetView(back_buffer.Get(), nullptr, rtv_.ReleaseAndGetAddressOf());
    }

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain2> swap_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    HANDLE latency_wait_ = nullptr;
    UINT swap_flags_ = 0;
    bool tearing_ = false;
    bool occluded_ = false;
};

// Owns the ImGui context, both backends and the menu; tears them down in the order ImGui 1.92 requires.
class ImGuiSession {
public:
    ImGuiSession(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context, astral::Menu& menu) : menu_(menu)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        win32_ = ImGui_ImplWin32_Init(hwnd);
        dx11_ = win32_ && ImGui_ImplDX11_Init(device, context);
    }
    ~ImGuiSession()
    {
        if (dx11_) ImGui_ImplDX11_Shutdown(); // releases the GPU copies of every registered texture
        if (win32_) ImGui_ImplWin32_Shutdown();
        menu_.Shutdown();                     // unregisters the icon texture while the context still exists
        ImGui::DestroyContext();
    }
    ImGuiSession(const ImGuiSession&) = delete;
    ImGuiSession& operator=(const ImGuiSession&) = delete;

    [[nodiscard]] bool Ok() const noexcept { return dx11_; }

private:
    astral::Menu& menu_;
    bool win32_ = false;
    bool dx11_ = false;
};

struct Click {
    ImVec2 pos;
    int frame = 0;
    int button = ImGuiMouseButton_Left;
};

struct Options {
    std::wstring shot;
    std::string search;
    int accent = -1;
    unsigned custom_start = 0, custom_end = 0;
    bool custom = false;
    int frames = 90;
    int tab = -1;
    int sub = -1;
    float scale = 0.0f;
    int width = 0, height = 0;
    bool mouse = false;
    ImVec2 mouse_pos = ImVec2(0, 0);
    std::vector<Click> clicks;
};

Options ParseOptions()
{
    Options o;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) return o;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::wstring key = argv[i];
        const wchar_t* value = argv[i + 1];
        if (key == L"--shot") o.shot = value, ++i;
        else if (key == L"--frames") o.frames = _wtoi(value), ++i;
        else if (key == L"--tab") o.tab = _wtoi(value), ++i;
        else if (key == L"--sub") o.sub = _wtoi(value), ++i;
        else if (key == L"--accent") o.accent = _wtoi(value), ++i;
        else if (key == L"--custom") {
            o.custom = swscanf_s(value, L"%x,%x", &o.custom_start, &o.custom_end) == 2;
            ++i;
        } else if (key == L"--search") {
            for (const wchar_t* p = value; *p != L'\0'; ++p) o.search.push_back(static_cast<char>(*p < 128 ? *p : '?'));
            ++i;
        }
        else if (key == L"--scale") o.scale = static_cast<float>(_wtof(value)), ++i;
        else if (key == L"--size") {
            swscanf_s(value, L"%dx%d", &o.width, &o.height);
            ++i;
        } else if (key == L"--mouse") {
            float x = 0, y = 0;
            if (swscanf_s(value, L"%f,%f", &x, &y) == 2) {
                o.mouse = true;
                o.mouse_pos = ImVec2(x, y);
            }
            ++i;
        } else if (key == L"--click" || key == L"--rclick") {
            Click c;
            float x = 0, y = 0;
            if (swscanf_s(value, L"%f,%f@%d", &x, &y, &c.frame) == 3) {
                c.pos = ImVec2(x, y);
                c.button = key == L"--rclick" ? ImGuiMouseButton_Right : ImGuiMouseButton_Left;
                o.clicks.push_back(c);
            }
            ++i;
        }
    }
    LocalFree(argv);
    return o;
}

struct App {
    D3D11Renderer renderer;
    astral::Menu menu;
    UINT pending_w = 0;
    UINT pending_h = 0;
    float dpi = 1.0f;
    bool dpi_dirty = false;
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return TRUE;

    if (msg == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_SIZE:
        if (app != nullptr && wParam != SIZE_MINIMIZED) {
            app->pending_w = LOWORD(lParam);
            app->pending_h = HIWORD(lParam);
        }
        return 0;
    case WM_DPICHANGED:
        if (app != nullptr) {
            app->dpi = static_cast<float>(HIWORD(wParam)) / 96.0f;
            app->dpi_dirty = true;
        }
        if (const RECT* suggested = reinterpret_cast<const RECT*>(lParam))
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void DrawBackdrop()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 a = vp->Pos;
    const ImVec2 b(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
    dl->AddRectFilledMultiColor(a, b, IM_COL32(32, 35, 44, 255), IM_COL32(26, 28, 36, 255), IM_COL32(15, 16, 21, 255),
                                IM_COL32(19, 20, 27, 255));
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    ImGui_ImplWin32_EnableDpiAwareness();
    const Options opt = ParseOptions();

    App app;
    app.dpi = opt.scale > 0.0f ? opt.scale : ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    if (RegisterClassExW(&wc) == 0) return 1;

    RECT rc{ 0, 0, opt.width > 0 ? opt.width : static_cast<LONG>(1280.0f * app.dpi), opt.height > 0 ? opt.height : static_cast<LONG>(800.0f * app.dpi) };
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    HWND hwnd = CreateWindowExW(0, kWindowClass, L"Astral-Remake by kdt255", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                                rc.bottom - rc.top, nullptr, nullptr, instance, &app);
    if (hwnd == nullptr) {
        UnregisterClassW(kWindowClass, instance);
        return 1;
    }

    int exit_code = 0;
    if (!app.renderer.Create(hwnd)) {
        MessageBoxW(hwnd, L"Direct3D 11 initialization failed.", L"Astral", MB_ICONERROR | MB_OK);
        exit_code = 2;
    } else {
        ImGuiSession session(hwnd, app.renderer.Device(), app.renderer.Context(), app.menu);
        if (!session.Ok() || !app.menu.Init(app.dpi)) {
            MessageBoxW(hwnd, L"Dear ImGui initialization failed.", L"Astral", MB_ICONERROR | MB_OK);
            exit_code = 3;
        } else {
            astral::Config& cfg = app.menu.config();
            if (opt.tab >= 0 && opt.tab < static_cast<int>(astral::Tab::Count)) app.menu.SetTab(static_cast<astral::Tab>(opt.tab));
            if (opt.sub >= 0) {
                cfg.visuals.target = opt.sub;
                cfg.misc.page = opt.sub;
                cfg.rage.category = opt.sub;
            }
            if (opt.scale > 0.0f) cfg.misc.dpi_mode = 0;
            if (!opt.shot.empty()) cfg.misc.watermark = false;
            if (!opt.search.empty()) app.menu.SetSearch(opt.search.c_str());
            if (opt.accent >= 0) cfg.misc.accent = opt.accent;
            if (opt.custom) {
                cfg.misc.accent_start = astral::MakeColor(opt.custom_start);
                cfg.misc.accent_end = astral::MakeColor(opt.custom_end);
            }

            ShowWindow(hwnd, opt.shot.empty() ? show : SW_SHOWNOACTIVATE);
            UpdateWindow(hwnd);

            constexpr float kClear[4] = { 0.06f, 0.065f, 0.08f, 1.0f };
            int frame = 0;
            for (bool running = true; running;) {
                for (MSG msg; PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE);) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                    if (msg.message == WM_QUIT) running = false;
                }
                if (!running) break;

                if (opt.shot.empty() && app.renderer.StillOccluded()) {
                    Sleep(10);
                    continue;
                }
                if (app.pending_w != 0 && app.pending_h != 0) {
                    if (!app.renderer.Resize(app.pending_w, app.pending_h)) {
                        exit_code = 4;
                        break;
                    }
                    app.pending_w = app.pending_h = 0;
                }
                if (app.dpi_dirty && opt.scale <= 0.0f) {
                    app.menu.SetSystemDpi(app.dpi);
                    app.dpi_dirty = false;
                }

                app.renderer.WaitForNextFrame();
                app.menu.PreFrame();
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();

                ImGuiIO& io = ImGui::GetIO();
                if (!opt.shot.empty()) io.DeltaTime = 1.0f / 60.0f;
                const ImVec2 origin = app.menu.Placed() ? app.menu.WindowPos() : ImVec2(0, 0);
                bool clicking = false;
                for (const Click& c : opt.clicks) clicking |= frame >= c.frame && frame <= c.frame + 2;
                if (opt.mouse && !clicking) io.AddMousePosEvent(origin.x + opt.mouse_pos.x, origin.y + opt.mouse_pos.y);
                for (const Click& c : opt.clicks) {
                    if (frame == c.frame) {
                        io.AddMousePosEvent(origin.x + c.pos.x, origin.y + c.pos.y);
                        io.AddMouseButtonEvent(c.button, true);
                    } else if (frame == c.frame + 2) {
                        io.AddMouseButtonEvent(c.button, false);
                        if (opt.mouse) io.AddMousePosEvent(origin.x + opt.mouse_pos.x, origin.y + opt.mouse_pos.y);
                    }
                }

                ImGui::NewFrame();
                DrawBackdrop();
                app.menu.Frame();
                ImGui::Render();

                app.renderer.BindAndClear(kClear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                if (!opt.shot.empty() && frame >= opt.frames) {
                    if (!app.renderer.Capture(opt.shot)) exit_code = 6;
                    running = false;
                }
                if (app.renderer.Present(true) == PresentResult::DeviceLost) {
                    // Warning: a robust host recreates the device and all resources; this one exits instead.
                    MessageBoxW(hwnd, L"The GPU device was lost.", L"Astral", MB_ICONERROR | MB_OK);
                    exit_code = 5;
                    break;
                }
                ++frame;
            }
        }
    }

    DestroyWindow(hwnd);
    UnregisterClassW(kWindowClass, instance);
    return exit_code;
}
