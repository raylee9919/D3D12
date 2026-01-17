// Copyright Seong Woo Lee. All Rights Reserved.

#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3d12")
#pragma comment(lib, "d3dcompiler")


extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; }
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\x64\\"; }


// @Temporary
//
#define BUILD_USE_SSE 1

// =================================================
// .h
// =================================================
#include "Base.h"
#include "Math.h"
#include "Win32.h"
#include "D3D12/D3D12.h"
#include "ThirdParty/DirectX/DirectXTex/DDSTextureLoader12.h"

struct simple_constant_buffer
{
    M4x4 World;
    M4x4 View;
    M4x4 Proj;
};

// =================================================
// .cpp
// =================================================
#include "Math.cpp"
#include "D3D12/D3D12.cpp"
#include "ThirdParty/DirectX/DirectXTex/DDSTextureLoader12.cpp"

static b32 g_Running = true;


static mesh_object *CreateCubeMeshObject()
{
    mesh_object *Result = new mesh_object;

    const vertex Vertices[] = {
        vertex{ Vec3( 0.5f, 0.5f, 0.5f), Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3( 0.5f,-0.5f, 0.5f), Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3( 0.5f,-0.5f,-0.5f), Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3( 0.5f, 0.5f,-0.5f), Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3(-0.5f,-0.5f,-0.5f), Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3(-0.5f,-0.5f, 0.5f), Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3(-0.5f, 0.5f,-0.5f), Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3(-0.5f, 0.5f, 0.5f), Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3(-0.5f, 0.5f, 0.5f), Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3( 0.5f, 0.5f, 0.5f), Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3(-0.5f, 0.5f,-0.5f), Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3( 0.5f, 0.5f,-0.5f), Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3( 0.5f,-0.5f, 0.5f), Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3(-0.5f,-0.5f, 0.5f), Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3( 0.5f,-0.5f,-0.5f), Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3(-0.5f,-0.5f,-0.5f), Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3( 0.5f,-0.5f,-0.5f), Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3(-0.5f,-0.5f,-0.5f), Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3( 0.5f, 0.5f,-0.5f), Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3(-0.5f, 0.5f,-0.5f), Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3(-0.5f,-0.5f, 0.5f), Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3( 0.5f,-0.5f, 0.5f), Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3(-0.5f, 0.5f, 0.5f), Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3( 0.5f, 0.5f, 0.5f), Vec4(1,1,1,1), Vec2(1,0) },
    };

    const u16 Indices[] = {
        0,  3,  1,  1,  3,  2,
        4,  6,  7,  4,  7,  5,
        8, 10, 11,  8, 11,  9,
        12, 14, 15, 12, 15, 13,
        16, 18, 19, 16, 19, 17,
        20, 22, 23, 20, 23, 21,
    };


    { // Create vertex buffer.
        const UINT VertexSize  = sizeof(Vertices[0]);
        const UINT VertexCount = _countof(Vertices);
        const UINT Size = VertexSize*VertexCount;

        // @Temporary: Upload heap (Copied by GPU's DMA engine) isnt' feasible for static data like vertex and index data.
        // It first writes to the system memory (runtime? UMD?) and GPU fetches through bus whenver it needs it.
        // Bandwidth is worse than default heap.
        //
        ID3D12Resource *VertexBuffer = nullptr;
        auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&VertexBuffer)) );

        void *Mapped = nullptr;
        CD3DX12_RANGE ReadRange(0, 0);
        ASSERT_SUCCEEDED( VertexBuffer->Map(0, &ReadRange, &Mapped) );
        memcpy(Mapped, Vertices, Size);
        VertexBuffer->Unmap(0, nullptr);

        D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {
            .BufferLocation = VertexBuffer->GetGPUVirtualAddress(),
            .SizeInBytes    = Size,
            .StrideInBytes  = sizeof(Vertices[0]),
        };

        Result->m_VertexBuffer     = VertexBuffer;
        Result->m_VertexBufferView = VertexBufferView;
    }


    { // Create root signature and pipeline state.
        ID3D12RootSignature *RootSignature = nullptr;
        ID3D12PipelineState *PipelineState = nullptr;

        ID3D12Device5 *Device = d3d12->Device;
        ID3DBlob *Signature = nullptr;
        ID3DBlob *Error = nullptr;

        CD3DX12_DESCRIPTOR_RANGE RangesPerObj[1] = {};
        RangesPerObj[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0

        CD3DX12_DESCRIPTOR_RANGE RangesPerGroup[1] = {};
        RangesPerGroup[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

        CD3DX12_ROOT_PARAMETER RootParamters[2] = {};
        RootParamters[0].InitAsDescriptorTable(_countof(RangesPerObj), RangesPerObj, D3D12_SHADER_VISIBILITY_ALL);
        RootParamters[1].InitAsDescriptorTable(_countof(RangesPerGroup), RangesPerGroup, D3D12_SHADER_VISIBILITY_ALL);

        UINT SamplerRegisterIndex = 0;
        D3D12_STATIC_SAMPLER_DESC SamplerDesc = {
            .Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .MipLODBias       = 0.f,
            .MaxAnisotropy    = 16,
            .ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER,
            .BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
            .MinLOD           = -FLT_MAX,
            .MaxLOD           = D3D12_FLOAT32_MAX,
            .ShaderRegister   = SamplerRegisterIndex,
            .RegisterSpace    = 0,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
        };
        SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

        CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
        RootSignatureDesc.Init(_countof(RootParamters), RootParamters, 1, &SamplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


        ASSERT_SUCCEEDED( D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error) );
        ASSERT_SUCCEEDED( Device->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature)) );

        if (Signature)
        {
            Signature->Release();
        }
        if (Error)
        {
            Error->Release();
        }

        // Create pipeline state.
        //
        ID3DBlob *VertexShader = nullptr;
        ID3DBlob *PixelShader  = nullptr;
        ID3DBlob *ErrorBlob    = nullptr;

        UINT CompilerFlags = 0;
#if BUILD_DEBUG
        CompilerFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        // @Temporary
        if (FAILED( D3DCompileFromFile(L"./Shaders/Shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", CompilerFlags, 0, &VertexShader, &ErrorBlob) ))
        {
            LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
            OutputDebugStringA(Error);
            ASSERT( !"Vertex Shader Compilation Failed." );
        }
        if (FAILED( D3DCompileFromFile(L"./Shaders/Shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", CompilerFlags, 0, &PixelShader, &ErrorBlob) ))
        {
            LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
            OutputDebugStringA(Error);
            ASSERT( !"Pixel Shader Compilation Failed." );
        }

        // @Temporary
        D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(vertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertex, TexCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc = {
            .pRootSignature         = RootSignature,
            .VS                     = CD3DX12_SHADER_BYTECODE(VertexShader->GetBufferPointer(), VertexShader->GetBufferSize()),
            .PS                     = CD3DX12_SHADER_BYTECODE(PixelShader->GetBufferPointer(), PixelShader->GetBufferSize()),
            .BlendState             = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask             = UINT_MAX,
            .RasterizerState        = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState      = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .InputLayout            = { InputElementDescs, _countof(InputElementDescs) }, 
            .PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets       = 1,
            .DSVFormat              = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {
                .Count = 1,
            }
        };
        PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        PipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        PipelineStateDesc.DepthStencilState.StencilEnable = FALSE;

        ASSERT_SUCCEEDED( Device->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&PipelineState)) );

        if (VertexShader)
        {
            VertexShader->Release();
        }

        if (PixelShader)
        {
            PixelShader->Release();
        }

        Result->m_RootSignature = RootSignature;
        Result->m_PipelineState = PipelineState;
    }


    // Create index groups.
    for (int i = 0; i < 6; ++i)
    {
        // @Temporary
        const WCHAR *FileNames[6] = {
            L"AnimeGirl0.dds",
            L"AnimeGirl1.dds",
            L"AnimeGirl2.dds",
            L"AnimeGirl3.dds",
            L"AnimeGirl4.dds",
            L"AnimeGirl5.dds",
        };
        const UINT IndexCount = 6;
        const UINT IndexSize  = sizeof(Indices[0]);
        const UINT Size = IndexSize*IndexCount;

        const u16 *Src = Indices + IndexCount*i;
        ID3D12Resource *IndexBuffer = nullptr;

        // @Temporary:
        auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&IndexBuffer)) );

        void *Mapped = nullptr;
        CD3DX12_RANGE ReadRange(0, 0);
        ASSERT_SUCCEEDED(IndexBuffer->Map(0, &ReadRange, &Mapped));
        memcpy(Mapped, Src, Size);
        IndexBuffer->Unmap(0, nullptr);

        D3D12_INDEX_BUFFER_VIEW IndexBufferView = {
            .BufferLocation = IndexBuffer->GetGPUVirtualAddress(),
            .SizeInBytes    = Size,
            .Format         = DXGI_FORMAT_R16_UINT, // @Robustness
        };

        index_group *Group = new index_group;
        {
            Group->m_IndexCount      = IndexCount;
            Group->m_IndexBuffer     = IndexBuffer;
            Group->m_IndexBufferView = IndexBufferView;
            Group->m_Texture         = CreateTextureFromFile(FileNames[i]);
        }

        // @Temporary
        Result->m_IndexGroup.push_back(Group);
        Result->m_IndexGroupCount++;
    }

    return Result;
}

#if BUILD_DEBUG
int wmain(void) 
{
    HINSTANCE hinstance = GetModuleHandle(0);
#else
int APIENTRY wWinMain(HINSTANCE hinstance, HINSTANCE , PWSTR , int) 
{
#endif
    Win32_State *win32_state = new Win32_State;
    memset(win32_state, 0, sizeof(Win32_State));
    w32Init(win32_state, hinstance);
    win32_state->hwnd = w32CreateWindow(win32_state, L"Window");


    d3d12 = new D3D12_State;
    memset(d3d12, 0, sizeof(D3D12_State));
    d12Init(win32_state->hwnd);




    mesh_object *Mesh = CreateCubeMeshObject();

    // @Temporary
    M4x4 Translation1 = M4x4Identity();
    M4x4 Rotation1 = M4x4Identity();
    M4x4 Translation2 = M4x4Translation(1.0f, 0.f, 0.2f);
    M4x4 Rotation2 = M4x4Identity();


    // Main Loop
    //
    const f32 TickRate = 1.f / 60.f; 
    f32 AccumulatedTime = 0.f;
    u64 OldTimer = OS::ReadTimer();
    f32 Time = 0.f;
    while (g_Running)
    {
        for (MSG Msg; PeekMessage(&Msg, win32_state->hwnd, 0, 0, PM_REMOVE);) 
        {
            if (Msg.message == WM_QUIT) 
            {
                g_Running = false; 
            }
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }

        f64 InverseTimerFrequency = 1.0f / OS::GetTimerFrequency();
        u64 NewTimer = OS::ReadTimer();
        f32 DeltaTime = (f32)((f64)(NewTimer - OldTimer)*InverseTimerFrequency);
        OldTimer = NewTimer;
        AccumulatedTime += DeltaTime;
        Time += DeltaTime;
        for (;AccumulatedTime >= TickRate; AccumulatedTime -= TickRate)
        {
            // Update Here.
            //
            Rotation1 = RotationX(TickRate)*Rotation1;
            Rotation2 = RotationY(TickRate)*Rotation2;

            // @Temporary: Update camera.
            const f32 QuaterOfPI = 0.785398163f;
            const f32 AspectRatio = 16.f/9.f;
            const f32 NearZ = 0.1f;
            const f32 FarZ  = 1000.f;
            d3d12->m_View = M4x4LookAtRH(Vec3(0.f, 0.5f, 3.f), Vec3(0.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f));
            d3d12->m_Proj = M4x4PerspectiveLH(QuaterOfPI, AspectRatio, NearZ, FarZ);
        }


        {
            ASSERT_SUCCEEDED( d3d12->CommandAllocator->Reset() );
            ASSERT_SUCCEEDED( d3d12->CommandList->Reset(d3d12->CommandAllocator, nullptr/*dummy pipeline state that the app doesn't have to worry about.*/) );

            auto TransitionToTarget = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->m_RenderTargets[d3d12->m_RenderTargetIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            d3d12->CommandList->ResourceBarrier(1, &TransitionToTarget);

            CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(d3d12->m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), d3d12->m_RenderTargetIndex, d3d12->m_RTVDescriptorSize);
            CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHandle(d3d12->m_DSVHeap->GetCPUDescriptorHandleForHeapStart());

            FLOAT ClearColor[4] = { 0.2f, 0.2f, 0.2f, 1.f };
            d3d12->CommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);
            d3d12->CommandList->ClearDepthStencilView(DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

            d3d12->CommandList->RSSetViewports(1, &d3d12->m_Viewport);
            d3d12->CommandList->RSSetScissorRects(1, &d3d12->m_ScissorRect);
            d3d12->CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, &DSVHandle);



            { // Draw mesh.
                Mesh->Draw(Translation1*Rotation1);
                Mesh->Draw(Translation2*Rotation2);
            }


            

            auto TransitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->m_RenderTargets[d3d12->m_RenderTargetIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            d3d12->CommandList->ResourceBarrier(1, &TransitionToPresent);

            ASSERT_SUCCEEDED( d3d12->CommandList->Close() );

            ID3D12CommandList *CommandLists[] = { d3d12->CommandList };
            d3d12->CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);


            d12Present();



            // @Todo: You don't want to wait until all the commands are executed.
            d12Fence(d3d12->m_Fence);
            d12FenceWait(d3d12->m_Fence);

            d3d12->End();
        }
    }

    return 0;
}

static LRESULT w32WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) 
{
    LRESULT Result = {};
    switch(msg) 
    {
        case WM_CLOSE:
        case WM_DESTROY:
        {
            g_Running = false;
        } break;

        case WM_PAINT: 
        {
            PAINTSTRUCT paint;
            HDC hdc = BeginPaint(hwnd, &paint);
            ReleaseDC(hwnd, hdc);
            EndPaint(hwnd, &paint);
        } break;

        case WM_SIZE:
        {
            if (d3d12)
            {
                RECT Rect;
                GetClientRect(hwnd, &Rect);
                DWORD Width  = Rect.right  - Rect.left;
                DWORD Height = Rect.bottom - Rect.top;
                d12UpdateFramebuffer(Width, Height);
            }
        } break;

        default: 
        {
            Result = DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
    }

    return Result;
}

static void w32Init(Win32_State *State, HINSTANCE hinst) 
{
    State->hinstance = hinst;

    State->window_class.cbSize         = sizeof(State->window_class);
    State->window_class.style          = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    State->window_class.lpfnWndProc    = w32WindowProc;
    State->window_class.cbClsExtra     = 0;
    State->window_class.cbWndExtra     = 0;
    State->window_class.hInstance      = hinst;
    State->window_class.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
    State->window_class.hCursor        = LoadCursor(NULL, IDC_ARROW);
    State->window_class.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    State->window_class.lpszMenuName   = NULL;
    State->window_class.lpszClassName  = L"Win32WindowClass";

    if (!RegisterClassExW(&State->window_class)) 
    {
        ASSERT( !"Couldn't register window class." ); 
    }
}

static HWND w32CreateWindow(Win32_State *State, const WCHAR *Title) 
{
    HWND Result = CreateWindowExW(WS_EX_APPWINDOW, State->window_class.lpszClassName, Title,
                                  WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  0, 0, State->hinstance, 0);

    if (!Result) 
    {
        ASSERT( !"Couldn't create a window." ); 
    }

    return Result;
}

namespace OS
{
    static u64 ReadTimer()
    {
        LARGE_INTEGER Result;
        QueryPerformanceCounter(&Result);
        return Result.QuadPart;
    }

    static u64 GetTimerFrequency()
    {
        LARGE_INTEGER Result;
        QueryPerformanceFrequency(&Result);
        return Result.QuadPart;
    }
};
