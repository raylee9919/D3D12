/* ========================================================================
   File: %
   Date: %
   Revision: %
   Creator: Seong Woo Lee
   Notice: (C) Copyright % by Seong Woo Lee. All Rights Reserved.
   ======================================================================== */

#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3d12")
#pragma comment(lib, "d3dcompiler")


extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; }
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\x64\\"; }

// [.h]
//
#include "CE_Base.h"
#include "CE_Math.h"
#include "CE_Win32.h"
#include "CE_D3D12.h"
//#include "D3D12/Include.h"

// [.cpp]
//
#include "CE_Math.cpp"
#include "CE_D3D12.cpp"
//#include "D3D12/Include.cpp"

//static b32    g_Running = true;
//static D3D12 *g_D3D12;

Global b32 g_running = true;

struct Vertex {
    Vec3 position;
    Vec4 color;
};

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE , PWSTR , int) 
{
    Win32_State *win32_state = new Win32_State;
    memset(win32_state, 0, sizeof(Win32_State));
    w32Init(win32_state, hinstance);
    win32_state->hwnd = w32CreateWindow(win32_state, L"Engine");


    d3d12 = new D3D12_State;
    memset(d3d12, 0, sizeof(D3D12_State));
    d12Init(win32_state->hwnd);


        Vertex vertices[] = {
            { {-0.25f, -0.25f, 0.0f } },
            { { 0.25f, -0.25f, 0.0f } },
            { {-0.25f,  0.25f, 0.0f } },
            { { 0.25f,  0.25f, 0.0f } },
        };
        u16 indices[] = { 0, 2, 1, 1, 2, 3 };

        ID3D12Resource *vertex_buffer = nullptr;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};

        ID3D12Resource *index_buffer = nullptr;
        D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};

        {
            u32 size = sizeof(vertices);

            // @Temporary: Upload heap bad. Whenever the GPU needs it, it will fetch from the RAM.
            auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
            ASSERT_SUCCEEDED(d3d12->device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertex_buffer)));

            // Copy the triangle data to the vertex buffer.
            u8* mapped;
            CD3DX12_RANGE read_range(0, 0); // no read
            ASSERT_SUCCEEDED(vertex_buffer->Map(0, &read_range, reinterpret_cast<void**>(&mapped)));
            memcpy(mapped, vertices, size);
            vertex_buffer->Unmap(0, nullptr);

            // Init vertex buffer view.
            vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
            vertex_buffer_view.StrideInBytes  = sizeof(vertices[0]);
            vertex_buffer_view.SizeInBytes    = size;
        }

        {
            u32 size = sizeof(indices);

            // @Temporary:
            auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
            ASSERT_SUCCEEDED(d3d12->device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&index_buffer)));

            u8* mapped;
            CD3DX12_RANGE read_range(0, 0); // no read
            ASSERT_SUCCEEDED(index_buffer->Map(0, &read_range, reinterpret_cast<void**>(&mapped)));
            memcpy(mapped, indices, size);
            index_buffer->Unmap(0, nullptr);

            index_buffer_view.BufferLocation = index_buffer->GetGPUVirtualAddress();
            index_buffer_view.SizeInBytes    = size;
            index_buffer_view.Format         = DXGI_FORMAT_R16_UINT; // @Robustness
        }


    while (g_running)
    {
        // Process message.
        for (MSG msg; PeekMessage(&msg, win32_state->hwnd, 0, 0, PM_REMOVE);) {
            if (msg.message == WM_QUIT) { g_running = false; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        
        {
            ASSERT_SUCCEEDED(d3d12->command_allocator->Reset());
            ASSERT_SUCCEEDED(d3d12->command_list->Reset(d3d12->command_allocator, d3d12->pipeline_state));

            auto transition1 = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->render_target_views[d3d12->frame_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            d3d12->command_list->ResourceBarrier(1, &transition1);

            CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(d3d12->rtv_heap->GetCPUDescriptorHandleForHeapStart(), d3d12->frame_index, d3d12->rtv_descriptor_size);
            FLOAT bg_color[4] = { 0.2f, 0.2f, 0.2f, 1.f };
            d3d12->command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
            d3d12->command_list->ClearRenderTargetView(rtv_handle, bg_color, 0, nullptr);

            auto transition2 = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->render_target_views[d3d12->frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            d3d12->command_list->ResourceBarrier(1, &transition2);

            ASSERT_SUCCEEDED(d3d12->command_list->Close());

            ID3D12CommandList *cmd_lists[] = { d3d12->command_list };
            d3d12->command_queue->ExecuteCommandLists(CountOf(cmd_lists), cmd_lists);


            d12Present();



            // @Todo: You don't want to wait until all the commands are executed.
            d12Fence(d3d12->fence);
            d12FenceWait(d3d12->fence);
        }
    }

    return 0;
}

Internal LRESULT w32WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = {};
    switch(msg) 
    {
        case WM_CLOSE:
        case WM_DESTROY:
        g_running = false;
        break;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC hdc = BeginPaint(hwnd, &paint);
            ReleaseDC(hwnd, hdc);
            EndPaint(hwnd, &paint);
        } break;

        default: {
            result = DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
    }

    return result;
}

Internal void w32Init(Win32_State *state, HINSTANCE hinst) {
    state->hinstance = hinst;

    state->window_class.cbSize         = sizeof(state->window_class);
    state->window_class.style          = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    state->window_class.lpfnWndProc    = w32WindowProc;
    state->window_class.cbClsExtra     = 0;
    state->window_class.cbWndExtra     = 0;
    state->window_class.hInstance      = hinst;
    state->window_class.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
    state->window_class.hCursor        = LoadCursor(NULL, IDC_ARROW);
    state->window_class.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    state->window_class.lpszMenuName   = NULL;
    state->window_class.lpszClassName  = L"Win32WindowClass";

    if (!RegisterClassExW(&state->window_class)) {
        Assert(!"Couldn't register window class."); 
    }
}


Internal HWND w32CreateWindow(Win32_State *state, WCHAR *title) {
    HWND result = CreateWindowExW(WS_EX_APPWINDOW, state->window_class.lpszClassName, title,
                                  WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  0, 0, state->hinstance, 0);
    
    if (!result) {
        Assert(!"Couldn't create a window."); 
    }

    return result;
}



#if 0
    { // Create sample texture.
        u32 *Data = nullptr;

        int Dim = 64;
        u32 w = 1024;
        u32 h = 1024;
        Data = new u32[w*h];
        memset(Data, 0, sizeof(Data[0])*w*h);
        for (u32 r = 0; r < h; ++r) {
            for (u32 c = 0; c < w; ++c) {
                if ((r/Dim + c/Dim)%2 == 0) { Data[w*r + c] = 0xffffffff; }
            }
        }

        if (Data) {
            g_D3D12->CreateTexture(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Data);
        }

        delete(Data);
    }
#endif

