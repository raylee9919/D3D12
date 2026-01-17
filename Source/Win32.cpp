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

struct vertex 
{
    Vec3 Position;
    Vec4 Color;
    Vec2 TexCoord;
};

struct simple_constant_buffer
{
    M4x4 World;
    M4x4 View;
    M4x4 Proj;
};

enum descriptor_index
{
    DESCRIPTOR_INDEX_CONSTANT = 0,
    DESCRIPTOR_INDEX_TEXTURE  = 1,

    DESCRIPTOR_INDEX_COUNT
};

// =================================================
// .cpp
// =================================================
#include "Math.cpp"
#include "D3D12/D3D12.cpp"
#include "ThirdParty/DirectX/DirectXTex/DDSTextureLoader12.cpp"

static b32 g_Running = true;


static u8 *DebugCreateTiledBitmap(u32 Width, u32 Height, u32 TileSize, UINT8 R, UINT8 G, UINT8 B)
{
    u32 Stride = Width*4;
    u8 *Data = new u8[Stride*Height];
    memset(Data, 0, Stride*Height);
    for (u32 Y = 0; Y < Height; ++Y) 
    {
        for (u32 X = 0; X < Width; ++X) 
        {
            if ((X/TileSize + Y/TileSize)%2 == 0)
            {
                u32 *Texel = (u32 *)Data + Width*Y + X;
                *Texel = (UINT32)R | ((UINT32)G<<8) | ((UINT32)B<<16) | 0xff000000; 
            }
        }
    }
    return Data;
}

class mesh_object
{
    private:
        ID3D12RootSignature *m_RootSignature = nullptr;
        ID3D12PipelineState *m_PipelineState = nullptr;
        ID3D12Resource *m_VertexBuffer = nullptr;
        ID3D12Resource *m_IndexBuffer = nullptr;
        D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
        D3D12_INDEX_BUFFER_VIEW  m_IndexBufferView = {};

        u32 m_IndexCount = 0;



    public:
        void Init(vertex *Vertices, u32 VertexCount, u16 *Indices, u32 IndexCount)
        {
            m_IndexCount = IndexCount;

            {
                UINT Size = sizeof(Vertices[0])*VertexCount;

                // @Temporary: Upload heap (Copied by GPU's DMA engine) isnt' feasible for static data like vertex and index data.
                // It first writes to the system memory (runtime? UMD?) and GPU fetches through bus whenver it needs it.
                // Bandwidth is worse than default heap.
                //
                auto HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
                auto Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);
                ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_VertexBuffer)) );

                void *Mapped = nullptr;
                CD3DX12_RANGE read_range(0, 0); // no read
                ASSERT_SUCCEEDED( m_VertexBuffer->Map(0, &read_range, &Mapped) );
                memcpy(Mapped, Vertices, Size);
                m_VertexBuffer->Unmap(0, nullptr);

                m_VertexBufferView = {
                    .BufferLocation = m_VertexBuffer->GetGPUVirtualAddress(),
                    .SizeInBytes    = Size,
                    .StrideInBytes  = sizeof(Vertices[0]),
                };
            }

            {
                UINT Size = sizeof(Indices[0])*IndexCount;

                // @Temporary:
                auto HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
                auto Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);
                ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_IndexBuffer)) );

                void *Mapped = nullptr;
                CD3DX12_RANGE read_range(0, 0); // no read
                ASSERT_SUCCEEDED(m_IndexBuffer->Map(0, &read_range, &Mapped));
                memcpy(Mapped, Indices, Size);
                m_IndexBuffer->Unmap(0, nullptr);

                m_IndexBufferView = {
                    .BufferLocation = m_IndexBuffer->GetGPUVirtualAddress(),
                    .SizeInBytes    = Size,
                    .Format         = DXGI_FORMAT_R16_UINT, // @Robustness
                };
            }

            { // Create root signature which binds resources to the pipeline.
                ID3D12Device5 *Device = d3d12->Device;
                ID3DBlob *Signature = nullptr;
                ID3DBlob *Error = nullptr;

                CD3DX12_DESCRIPTOR_RANGE Ranges[2] = {};
                Ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0
                Ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

                CD3DX12_ROOT_PARAMETER RootParamters[1] = {};
                RootParamters[0].InitAsDescriptorTable(_countof(Ranges), Ranges, D3D12_SHADER_VISIBILITY_ALL);

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
                ASSERT_SUCCEEDED( Device->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)) );

                if (Signature)
                {
                    Signature->Release();
                }
                if (Error)
                {
                    Error->Release();
                }
            }

            { // Create pipeline state.
                ID3D12Device5 *Device = d3d12->Device;

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
                    Assert( !"Vertex Shader Compilation Failed." );
                }
                if (FAILED( D3DCompileFromFile(L"./Shaders/Shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", CompilerFlags, 0, &PixelShader, &ErrorBlob) ))
                {
                    LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
                    OutputDebugStringA(Error);
                    Assert( !"Pixel Shader Compilation Failed." );
                }

                // @Temporary
                D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(vertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertex, TexCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                };

                D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc = {
                    .pRootSignature         = m_RootSignature,
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

                ASSERT_SUCCEEDED( Device->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&m_PipelineState)) );

                if (VertexShader)
                {
                    VertexShader->Release();
                }

                if (PixelShader)
                {
                    PixelShader->Release();
                }
            }
        }

        void Draw(M4x4 Translation, M4x4 Rotation, texture *Texture)
        {
            descriptor_pool *DescriptorPool = d3d12->m_DescriptorPool;
            ID3D12DescriptorHeap *DescriptorHeap = DescriptorPool->m_Heap;
            UINT DescriptorSize = DescriptorPool->m_DescriptorSize;

            CD3DX12_CPU_DESCRIPTOR_HANDLE CPUDescriptorTableHandle = {};
            CD3DX12_GPU_DESCRIPTOR_HANDLE GPUDescriptorTableHandle = {};
            DescriptorPool->Alloc(&CPUDescriptorTableHandle, &GPUDescriptorTableHandle, DESCRIPTOR_INDEX_COUNT);


            constant_buffer_pool *ConstantBufferPool = d3d12->m_ConstantBufferPool;
            cbv_descriptor* CBVDescriptor = ConstantBufferPool->Alloc();

            simple_constant_buffer *MappedConstantBuffer = (simple_constant_buffer *)CBVDescriptor->m_CPUMappedAddress;
            MappedConstantBuffer->World = Translation*Rotation;
            MappedConstantBuffer->View = d3d12->m_View;
            MappedConstantBuffer->Proj = d3d12->m_Proj;


            auto CmdList = d3d12->CommandList;

            CmdList->SetGraphicsRootSignature(m_RootSignature);

            CmdList->SetDescriptorHeaps(1, &DescriptorHeap);

            // Copy b0
            CD3DX12_CPU_DESCRIPTOR_HANDLE CBVDst(CPUDescriptorTableHandle, DESCRIPTOR_INDEX_CONSTANT, DescriptorSize);
            d3d12->Device->CopyDescriptorsSimple(1, CBVDst, CBVDescriptor->m_CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            // Copy t0
            if (Texture->m_CPUHandle.ptr)
            {
                CD3DX12_CPU_DESCRIPTOR_HANDLE SRVDst(CPUDescriptorTableHandle, DESCRIPTOR_INDEX_TEXTURE, DescriptorSize);
                d3d12->Device->CopyDescriptorsSimple(1, SRVDst, Texture->m_CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }

            CmdList->SetGraphicsRootDescriptorTable(0, GPUDescriptorTableHandle);

            CmdList->SetPipelineState(m_PipelineState);
            CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            CmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
            CmdList->IASetIndexBuffer(&m_IndexBufferView);
            CmdList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
        }
};


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




    // @Temporary
    vertex Vertices[] = {
        { Vec3(-0.25f, -0.25f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec2(0.f, 1.f) },
        { Vec3( 0.25f, -0.25f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec2(1.f, 1.f) },
        { Vec3(-0.25f,  0.25f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec2(0.f, 0.f) },
        { Vec3( 0.25f,  0.25f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec2(1.f, 0.f) },
    };
    u32 VertexCount = _countof(Vertices);
    u16 Indices[] = { 0, 2, 1, 1, 2, 3 };
    UINT IndexCount = _countof(Indices);


    mesh_object *Mesh = new mesh_object;
    Mesh->Init(Vertices, VertexCount, Indices, IndexCount);


    UINT Width = 1024;
    UINT Height = 1024;
    u8 *Bitmap1 = DebugCreateTiledBitmap(Width, Height, 64, 0xff, 0xff, 0xff);
    //texture *Texture1 = CreateTexture(Bitmap1, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM);
    texture *Texture1 = CreateTextureFromFile(L"AnimeGirl1.dds");
    texture *Texture2 = CreateTextureFromFile(L"AnimeGirl0.dds");


    // @Temporary
    M4x4 Translation1 = M4x4Identity();
    M4x4 Rotation1 = M4x4Identity();
    M4x4 Translation2 = M4x4Translation(0.3f, 0.f, 0.2f);
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
            d3d12->m_View = M4x4LookAtRH(Vec3(-2.f*sinf(Time), 0.5f, 2.f*cosf(Time)), Vec3(0.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f));
            d3d12->m_Proj = M4x4PerspectiveLH(QuaterOfPI, AspectRatio, NearZ, FarZ);
        }


        {
            ASSERT_SUCCEEDED( d3d12->CommandAllocator->Reset() );
            ASSERT_SUCCEEDED( d3d12->CommandList->Reset(d3d12->CommandAllocator, nullptr/*dummy pipeline state that the app doesn't have to worry about.*/) );

            auto TransitionToTarget = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->m_RenderTargets[d3d12->frame_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            d3d12->CommandList->ResourceBarrier(1, &TransitionToTarget);

            CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(d3d12->m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), d3d12->frame_index, d3d12->m_RTVDescriptorSize);
            CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHandle(d3d12->m_DSVHeap->GetCPUDescriptorHandleForHeapStart());

            FLOAT ClearColor[4] = { 0.2f, 0.2f, 0.2f, 1.f };
            d3d12->CommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);
            d3d12->CommandList->ClearDepthStencilView(DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

            d3d12->CommandList->RSSetViewports(1, &d3d12->m_Viewport);
            d3d12->CommandList->RSSetScissorRects(1, &d3d12->m_ScissorRect);
            d3d12->CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, &DSVHandle);



            { // Draw mesh.
                Mesh->Draw(Translation1, Rotation1, Texture1);
                Mesh->Draw(Translation2, Rotation2, Texture2);
            }


            

            auto TransitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->m_RenderTargets[d3d12->frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
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
