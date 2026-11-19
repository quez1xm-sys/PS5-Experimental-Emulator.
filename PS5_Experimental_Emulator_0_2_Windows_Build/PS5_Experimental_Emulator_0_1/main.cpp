#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>

using Microsoft::WRL::ComPtr;

static constexpr UINT FRAME_COUNT = 2;
static const wchar_t* APP_TITLE = L"PS5 Experimental Emulator 0.1";

struct Vertex { float x, y, z; };
struct CardInfo { const wchar_t* title; const wchar_t* subtitle; COLORREF color; };

class App {
public:
    HWND hwnd = nullptr;
    HWND cards[5]{};
    HWND status = nullptr;
    HWND openButton = nullptr;
    HWND infoButton = nullptr;
    int selected = 0;

    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12CommandAllocator> allocators[FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValues[FRAME_COUNT]{};
    UINT rtvDescriptorSize = 0;
    UINT frameIndex = 0;
    UINT width = 1280, height = 800;
    bool d3dReady = false;

    static App* self;

    static LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            auto* createdApp = static_cast<App*>(cs->lpCreateParams);
            createdApp->hwnd = h;
        }
        App* app = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (app) return app->HandleMessage(h, m, w, l);
        return DefWindowProcW(h, m, w, l);
    }

    static LRESULT CALLBACK CardProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        App* app = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            app = static_cast<App*>(cs->lpCreateParams);
        }
        if (!app) return DefWindowProcW(h, m, w, l);
        switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps{}; HDC dc = BeginPaint(h, &ps);
            int index = static_cast<int>(GetWindowLongPtrW(h, GWLP_ID)) - 100;
            app->DrawCard(dc, h, index);
            EndPaint(h, &ps); return 0;
        }
        case WM_LBUTTONDOWN: {
            int index = static_cast<int>(GetWindowLongPtrW(h, GWLP_ID)) - 100;
            app->selected = index;
            app->RefreshCards();
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        }
        return DefWindowProcW(h, m, w, l);
    }

    void DrawCard(HDC dc, HWND h, int index) {
        RECT r{}; GetClientRect(h, &r);
        const CardInfo items[] = {
            {L"Библиотека", L"Ваши приложения", RGB(45, 78, 145)},
            {L"Тестовая игра", L"Демонстрационный проект", RGB(94, 57, 143)},
            {L"Графический тест", L"Проверка рендеринга", RGB(20, 105, 119)},
            {L"Настройки", L"Конфигурация приложения", RGB(65, 82, 112)},
            {L"Grand Theft Auto VI", L"Не установлена • карточка-заглушка", RGB(33, 37, 64)}
        };
        HBRUSH bg = CreateSolidBrush(items[index].color);
        FillRect(dc, &r, bg); DeleteObject(bg);

        if (index == 4) {
            // Original GTA-inspired placeholder artwork; not official Rockstar cover art.
            RECT sky{4, 4, r.right - 4, r.bottom - 4};
            HBRUSH sunset = CreateSolidBrush(RGB(183, 91, 92));
            FillRect(dc, &sky, sunset); DeleteObject(sunset);

            RECT horizon{4, 105, r.right - 4, r.bottom - 4};
            HBRUSH dusk = CreateSolidBrush(RGB(54, 44, 78));
            FillRect(dc, &horizon, dusk); DeleteObject(dusk);

            HBRUSH sun = CreateSolidBrush(RGB(255, 190, 137));
            HGDIOBJ old = SelectObject(dc, sun);
            Ellipse(dc, 72, 30, 188, 146);
            SelectObject(dc, old); DeleteObject(sun);

            HBRUSH skyline = CreateSolidBrush(RGB(30, 31, 57));
            old = SelectObject(dc, skyline);
            Rectangle(dc, 8, 100, 48, 182);
            Rectangle(dc, 42, 82, 77, 182);
            Rectangle(dc, 190, 92, 226, 182);
            Rectangle(dc, 220, 115, 252, 182);
            SelectObject(dc, old); DeleteObject(skyline);

            // Simple car silhouette in the foreground.
            HBRUSH car = CreateSolidBrush(RGB(18, 20, 34));
            old = SelectObject(dc, car);
            RoundRect(dc, 42, 166, 222, 207, 18, 18);
            POINT roof[] = {{83, 166}, {108, 143}, {170, 143}, {193, 166}};
            Polygon(dc, roof, 4);
            SelectObject(dc, old); DeleteObject(car);

            HBRUSH wheel = CreateSolidBrush(RGB(8, 9, 17));
            old = SelectObject(dc, wheel);
            Ellipse(dc, 67, 192, 104, 229);
            Ellipse(dc, 169, 192, 206, 229);
            SelectObject(dc, old); DeleteObject(wheel);

            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 244, 232));
            HFONT gtaFont = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");
            old = SelectObject(dc, gtaFont);
            RECT logo{8, 8, r.right - 8, 48};
            DrawTextW(dc, L"VI", -1, &logo, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, old); DeleteObject(gtaFont);
        }

        HPEN pen = CreatePen(PS_SOLID, index == selected ? 4 : 1,
                             index == selected ? RGB(185, 218, 255) : RGB(105, 130, 170));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, 2, 2, r.right - 2, r.bottom - 2, 22, 22);
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(pen);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        HFONT big = CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(dc, big);
        const wchar_t* symbols[] = {L"▦", L"✦", L"◈", L"⚙"};
        RECT iconRect{0, 45, r.right, 135};
        DrawTextW(dc, symbols[index], -1, &iconRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont); DeleteObject(big);

        HFONT titleFont = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        oldFont = (HFONT)SelectObject(dc, titleFont);
        RECT titleRect{18, r.bottom - 78, r.right - 12, r.bottom - 48};
        DrawTextW(dc, items[index].title, -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont); DeleteObject(titleFont);

        HFONT subFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        oldFont = (HFONT)SelectObject(dc, subFont);
        SetTextColor(dc, RGB(220, 231, 249));
        RECT subRect{18, r.bottom - 43, r.right - 12, r.bottom - 16};
        DrawTextW(dc, items[index].subtitle, -1, &subRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont); DeleteObject(subFont);
    }

    void RefreshCards() {
        for (HWND c : cards) if (c) InvalidateRect(c, nullptr, TRUE);
        if (status) SetWindowTextW(status, L"Выбрана карточка. Нажми «Открыть» для демонстрации.");
    }

    void CreateControls() {
        const wchar_t* cls = L"PS5ExpCard";
        WNDCLASSW wc{}; wc.lpfnWndProc = CardProc; wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = cls; wc.hCursor = LoadCursor(nullptr, IDC_HAND);
        RegisterClassW(&wc);

        for (int i = 0; i < 4; ++i)
            cards[i] = CreateWindowExW(0, cls, L"", WS_CHILD | WS_VISIBLE,
                54 + i * 285, 310, 260, 270, hwnd, (HMENU)(INT_PTR)(100 + i),
                GetModuleHandleW(nullptr), this);

        openButton = CreateWindowW(L"BUTTON", L"Открыть", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            56, 620, 150, 42, hwnd, (HMENU)201, GetModuleHandleW(nullptr), nullptr);
        infoButton = CreateWindowW(L"BUTTON", L"Информация", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            220, 620, 160, 42, hwnd, (HMENU)202, GetModuleHandleW(nullptr), nullptr);
        status = CreateWindowW(L"STATIC", L"Прототип интерфейса • эмуляция PS5 пока не реализована",
            WS_CHILD | WS_VISIBLE, 500, 632, 650, 28, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

        HFONT font = CreateFontW(17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");
        SendMessageW(openButton, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(infoButton, WM_SETFONT, (WPARAM)font, TRUE);
    }

    bool InitD3D() {
        UINT dxgiFlags = 0;
#ifdef _DEBUG
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        ComPtr<IDXGIFactory6> factory;
        if (FAILED(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory)))) return false;

        // Prefer a hardware adapter; WARP is a fallback for systems without a supported GPU.
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter.Reset(); continue; }
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                            __uuidof(ID3D12Device), nullptr))) break;
            adapter.Reset();
        }
        if (adapter) {
            if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
                return false;
        } else {
            ComPtr<IDXGIAdapter> warp;
            if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)))) return false;
            if (FAILED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
                return false;
        }

        D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(device->CreateCommandQueue(&q, IID_PPV_ARGS(&commandQueue)))) return false;

        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.BufferCount = FRAME_COUNT; sd.Width = width; sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; sd.SampleDesc.Count = 1;
        ComPtr<IDXGISwapChain1> sc;
        if (FAILED(factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &sd, nullptr, nullptr, &sc)))
            return false;
        if (FAILED(sc.As(&swapChain))) return false;
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.NumDescriptors = FRAME_COUNT; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        if (FAILED(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap)))) return false;
        rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        for (UINT i = 0; i < FRAME_COUNT; ++i)
            if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                       IID_PPV_ARGS(&allocators[i])))) return false;
        if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              allocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList))))
            return false;
        commandList->Close();

        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false;
        fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!fenceEvent) return false;
        CreateRTVs();
        frameIndex = swapChain->GetCurrentBackBufferIndex();
        return true;
    }

    void CreateRTVs() {
        auto handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < FRAME_COUNT; ++i) {
            ComPtr<ID3D12Resource> buffer;
            swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer));
            device->CreateRenderTargetView(buffer.Get(), nullptr, handle);
            handle.ptr += rtvDescriptorSize;
        }
    }

    void WaitForGpu() {
        if (!commandQueue || !fence) return;
        const UINT64 value = ++fenceValues[frameIndex];
        if (SUCCEEDED(commandQueue->Signal(fence.Get(), value)) && fence->GetCompletedValue() < value) {
            fence->SetEventOnCompletion(value, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    }

    void Render() {
        if (!d3dReady) return;
        auto allocator = allocators[frameIndex];
        if (FAILED(allocator->Reset())) return;
        if (FAILED(commandList->Reset(allocator.Get(), nullptr))) return;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = nullptr;
        ComPtr<ID3D12Resource> backBuffer;
        swapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&backBuffer));
        barrier.Transition.pResource = backBuffer.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += frameIndex * rtvDescriptorSize;
        commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        const float clear[] = { 0.025f, 0.045f, 0.095f, 1.0f };
        commandList->ClearRenderTargetView(rtv, clear, 0, nullptr);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        commandList->ResourceBarrier(1, &barrier);
        commandList->Close();
        ID3D12CommandList* lists[] = { commandList.Get() };
        commandQueue->ExecuteCommandLists(1, lists);
        swapChain->Present(1, 0);

        const UINT64 value = ++fenceValues[frameIndex];
        commandQueue->Signal(fence.Get(), value);
        frameIndex = swapChain->GetCurrentBackBufferIndex();
        if (fence->GetCompletedValue() < value) {
            fence->SetEventOnCompletion(value, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    }

    void Resize(UINT w, UINT h) {
        if (!d3dReady || w == 0 || h == 0) return;
        WaitForGpu();
        width = w; height = h;
        for (HWND c : cards) if (c) { DestroyWindow(c); c = nullptr; }
        if (openButton) DestroyWindow(openButton);
        if (infoButton) DestroyWindow(infoButton);
        if (status) DestroyWindow(status);
        for (auto& a : allocators) a.Reset();
        commandList.Reset(); rtvHeap.Reset();
        if (FAILED(swapChain->ResizeBuffers(FRAME_COUNT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0)))
            return;
        D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors = FRAME_COUNT; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        if (FAILED(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap)))) return;
        for (UINT i = 0; i < FRAME_COUNT; ++i)
            device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocators[i]));
        device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators[0].Get(), nullptr,
                                  IID_PPV_ARGS(&commandList));
        commandList->Close();
        CreateRTVs();
        CreateControls();
    }

    LRESULT HandleMessage(HWND h, UINT m, WPARAM w, LPARAM l) {
        switch (m) {
        case WM_CREATE:
            CreateControls();
            d3dReady = InitD3D();
            if (!d3dReady) {
                MessageBoxW(h, L"Не удалось создать устройство DirectX 12. Проверь драйвер видеокарты и Windows SDK.",
                            L"Ошибка DirectX 12", MB_ICONERROR | MB_OK);
                PostQuitMessage(1);
            }
            SetTimer(h, 1, 16, nullptr);
            return 0;
        case WM_TIMER: Render(); return 0;
        case WM_SIZE:
            if (w != SIZE_MINIMIZED && d3dReady) {
                // Keep the prototype's control layout fixed; resize the DirectX back buffers.
                Resize(LOWORD(l), HIWORD(l));
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(w) == 201) {
                const wchar_t* names[] = {L"Библиотека", L"Тестовая игра", L"Графический тест", L"Настройки", L"Grand Theft Auto VI"};
                if (selected == 4) {
                    MessageBoxW(h,
                        L"Grand Theft Auto VI\n\nСтатус: НЕ УСТАНОВЛЕНА.\n\nЭто информационная карточка-заглушка. Кнопка запуска не может открыть игру, потому что игровые файлы не установлены, а эмуляция PS5 в этом проекте не реализована.\n\nУстанови игру из официального источника и используй поддерживаемую платформу.",
                        APP_TITLE, MB_OK | MB_ICONWARNING);
                } else {
                    std::wstring msg = L"Выбрано: "; msg += names[selected];
                    msg += L"\n\nЭто демонстрационная карточка. Запуск игр и эмуляция PS5 ещё не реализованы.";
                    MessageBoxW(h, msg.c_str(), APP_TITLE, MB_OK | MB_ICONINFORMATION);
                }
            } else if (LOWORD(w) == 202) {
                MessageBoxW(h, L"PS5 Experimental Emulator 0.1\n\nРабочая оболочка Windows + инициализация DirectX 12.\nНе является эмулятором PS5 и не содержит GTA 6.",
                            APP_TITLE, MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        case WM_KEYDOWN:
            if (w == VK_LEFT) { selected = (selected + 4) % 5; RefreshCards(); }
            if (w == VK_RIGHT) { selected = (selected + 1) % 5; RefreshCards(); }
            if (w == VK_RETURN) SendMessageW(h, WM_COMMAND, 201, 0);
            return 0;
        case WM_DESTROY:
            KillTimer(h, 1);
            if (d3dReady) WaitForGpu();
            if (fenceEvent) CloseHandle(fenceEvent);
            PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(h, m, w, l);
    }
};

App* App::self = nullptr;

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    App app;
    WNDCLASSW wc{};
    wc.lpfnWndProc = App::WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"PS5ExperimentalWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1320, 790,
        nullptr, nullptr, instance, &app);
    if (!hwnd) return 1;
    app.hwnd = hwnd;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
