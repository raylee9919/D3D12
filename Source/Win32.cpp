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
#include "Engine/Engine.h"
#include "Win32.h"
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
#include "Engine/Engine.cpp"
#include "ThirdParty/DirectX/DirectXTex/DDSTextureLoader12.cpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "ThirdParty/tinyobjloader/tiny_obj_loader.h"


static b32 g_Running = true;
static Vec3 g_CameraPosition = Vec3(0.f, 3.f, 4.2f);
static M4x4 g_Rotation = M4x4Identity();


static mesh* CreateMesh(vertex* Vertices, u32 VerticesCount, u16* Indices, u32 IndicesCount)
{
    mesh *Result = new mesh;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
    ID3D12Resource* VertexBuffer = nullptr;

    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};
    ID3D12Resource* IndexBuffer = nullptr;

    d3d12->m_ResourceManager->CreateVertexBuffer(sizeof(Vertices[0]), VerticesCount, Vertices, &VertexBufferView, &VertexBuffer);
    d3d12->m_ResourceManager->CreateIndexBuffer(IndicesCount, Indices, &IndexBufferView, &IndexBuffer);

    { // Create root signature and pipeline state.
        ID3D12RootSignature* RootSignature = nullptr;
        ID3D12PipelineState* PipelineState = nullptr;

        ID3D12Device5* Device = d3d12->Device;
        ID3DBlob* Signature = nullptr;
        ID3DBlob* Error = nullptr;

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
        if (FAILED( D3DCompileFromFile(L"./Assets/Shaders/Simple.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", CompilerFlags, 0, &VertexShader, &ErrorBlob) ))
        {
            LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
            OutputDebugStringA(Error);
            ASSERT( !"Vertex Shader Compilation Failed." );
        }
        if (FAILED( D3DCompileFromFile(L"./Assets/Shaders/Simple.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", CompilerFlags, 0, &PixelShader, &ErrorBlob) ))
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
            .pRootSignature        = RootSignature,
            .VS                    = CD3DX12_SHADER_BYTECODE(VertexShader->GetBufferPointer(), VertexShader->GetBufferSize()),
            .PS                    = CD3DX12_SHADER_BYTECODE(PixelShader->GetBufferPointer(), PixelShader->GetBufferSize()),
            .BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask            = UINT_MAX,
            .RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .InputLayout           = { InputElementDescs, _countof(InputElementDescs) }, 
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets      = 1,
            .DSVFormat             = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {
                .Count = 1,
            }
        };
        PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        PipelineStateDesc.RasterizerState.FrontCounterClockwise = true;
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

    submesh *Submesh = new submesh;
    {
        Submesh->m_IndexCount      = IndicesCount;
        Submesh->m_IndexBuffer     = IndexBuffer;
        Submesh->m_IndexBufferView = IndexBufferView;
        Submesh->m_Texture         = d3d12->m_ResourceManager->CreateTextureFromFileName(L"Assets/AnimeGirl0.dds", (UINT)wcslen(L"Assets/AnimeGirl0.dds"));
    }

    // @Temporary
    Result->m_Submesh.push_back(Submesh);
    Result->m_SubmeshCount++;

    Result->m_VertexBuffer = VertexBuffer;
    Result->m_VertexBufferView = VertexBufferView;
    Result->m_IndexBuffer = IndexBuffer;
    Result->m_IndexBufferView = IndexBufferView;

    return Result;
}

// Helper for hash map deduplication
bool operator==(const vertex& a, const vertex& b) 
{
    return (a.Position.X == b.Position.X && a.Position.Y == b.Position.Y && a.Position.Z == b.Position.Z &&
            a.TexCoord.X == b.TexCoord.X && a.TexCoord.Y == b.TexCoord.Y &&
            a.Color.E[0] == b.Color.E[0] && a.Color.E[1] == b.Color.E[1] && a.Color.E[2] == b.Color.E[2] && a.Color.E[3] == b.Color.E[3]);
}

struct vertex_hash {
    size_t operator()(const vertex& v) const 
    {
        return std::hash<float>()(v.Position.X) ^
            (std::hash<float>()(v.Position.Y) << 1) ^
            (std::hash<float>()(v.Position.Z) << 2) ^
            (std::hash<float>()(v.TexCoord.X) << 3) ^
            (std::hash<float>()(v.TexCoord.Y) << 4) ^
            (std::hash<float>()(v.Color.E[0]) << 5) ^
            (std::hash<float>()(v.Color.E[1]) << 6) ^
            (std::hash<float>()(v.Color.E[2]) << 7) ^
            (std::hash<float>()(v.Color.E[3]) << 8);
    }
};

static void LoadObj(const char* FileName, std::vector<vertex>& OutVertices, std::vector<u16>& OutIndices,
                    bool VertexColorFromFile = true, Vec4 DefaultColor = {1,1,1,1})
{
    tinyobj::attrib_t Attrib;
    std::vector<tinyobj::shape_t> Shapes;
    std::vector<tinyobj::material_t> Materials;
    std::string Error;
    std::string Warning;

    const char* mtl_basedir = nullptr;
    bool bTriangulate = true;
    ASSERT( tinyobj::LoadObj(&Attrib, &Shapes, &Materials, &Warning, &Error, FileName, mtl_basedir, bTriangulate) );
    ASSERT( Warning.empty() && Error.empty() );

    std::unordered_map<vertex, u16, vertex_hash> VertexSet;

    for (const auto& Shape : Shapes)
    {
        int Offset = 0;

        const int FaceCount = (int)Shape.mesh.num_face_vertices.size();
        for (int FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
        {
            const int VertCount = Shape.mesh.num_face_vertices[FaceIndex];
            ASSERT( VertCount == 3 );

            for (int VertexIndex = 0; VertexIndex < VertCount; ++VertexIndex)
            {
                tinyobj::index_t Index = Shape.mesh.indices[Offset + VertexIndex];

                vertex Vertex{};

                Vertex.Position.X = Attrib.vertices[3*Index.vertex_index + 0];
                Vertex.Position.Y = Attrib.vertices[3*Index.vertex_index + 1];
                Vertex.Position.Z = Attrib.vertices[3*Index.vertex_index + 2];

                if (Index.texcoord_index >= 0 && !Attrib.texcoords.empty())
                {
                    Vertex.TexCoord.X = Attrib.texcoords[2*Index.texcoord_index + 0];
                    Vertex.TexCoord.Y = Attrib.texcoords[2*Index.texcoord_index + 1];
                }

                if (VertexColorFromFile && Index.vertex_index >= 0 && !Attrib.colors.empty())
                {
                    Vertex.Color.E[0] = Attrib.colors[3*Index.vertex_index + 0];
                    Vertex.Color.E[1] = Attrib.colors[3*Index.vertex_index + 1];
                    Vertex.Color.E[2] = Attrib.colors[3*Index.vertex_index + 2];
                    Vertex.Color.E[3] = 1.0f;
                }
                else
                {
                    Vertex.Color = DefaultColor;
                }

                if (VertexSet.find(Vertex) == VertexSet.end())
                {
                    u16 NewIndex = static_cast<u16>(OutVertices.size());
                    VertexSet[Vertex] = NewIndex;
                    OutVertices.push_back(Vertex);
                }
                OutIndices.push_back(VertexSet[Vertex]);
            }

            Offset += VertCount;
        }
    }
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




    std::vector<vertex> Vertices;
    std::vector<u16> Indices;
    LoadObj("./Assets/StanfordBunny.obj", Vertices, Indices);
    mesh* StanfordBunnyMesh = CreateMesh(&Vertices[0], (int)Vertices.size(), &Indices[0], (int)Indices.size());


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
        printf("elapsed: %.2fms, fps: %.2f\n", DeltaTime*1000.f, 1.f/DeltaTime);
        OldTimer = NewTimer;
        AccumulatedTime += DeltaTime;
        Time += DeltaTime;
        for (;AccumulatedTime >= TickRate; AccumulatedTime -= TickRate)
        {
            // Update Here.
            //
            const f32 dt = TickRate;
            g_Rotation = RotationY(dt)*g_Rotation;

            // @Temporary: Update camera.
            const f32 QuaterOfPI = 0.785398163f;
            const f32 AspectRatio = 16.f/9.f;
            const f32 NearZ = 0.1f;
            const f32 FarZ  = 1000.f;
            d3d12->m_View = M4x4LookAtRH(g_CameraPosition, Vec3(0.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f));
            d3d12->m_Proj = M4x4PerspectiveLH(QuaterOfPI, AspectRatio, NearZ, FarZ);
        }


        {
            { // Begin
                const UINT ArbitraryThreadIndex = 0;
                command_list_pool* CommandListPool = d3d12->_CommandListPools[d3d12->m_CurrentContextIndex][ArbitraryThreadIndex];
                ID3D12GraphicsCommandList* CommandList = CommandListPool->GetCurrent()->CommandList;
                {
                    auto Transition = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->m_RenderTargets[d3d12->m_RenderTargetIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
                    CommandList->ResourceBarrier(1, &Transition);

                    CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(d3d12->m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), d3d12->m_RenderTargetIndex, d3d12->m_RTVDescriptorSize);
                    CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHandle(d3d12->m_DSVHeap->GetCPUDescriptorHandleForHeapStart());

                    FLOAT ClearColor[4] = { 0.2f, 0.2f, 0.2f, 1.f };
                    CommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);
                    CommandList->ClearDepthStencilView(DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
                }
                CommandListPool->CloseAndSubmit();

                d3d12->Fence();
            }

            // Push meshes to render queue.
            // Rendering 1024 bunnies.
            int HalfDim = 4;
            f32 Scale = 0.125f;
            for (int X = -HalfDim; X < HalfDim; ++X)
            {
                for (int Z = -HalfDim; Z < HalfDim; ++Z)
                {
                    M4x4 WorldMatrix = M4x4Translation((f32)X*Scale, 0.f, (f32)Z*Scale)*g_Rotation;

                    render_item RenderItem = {
                        .m_Type = RENDER_ITEM_MESH,
                        .m_MeshData = {
                            .m_Mesh = StanfordBunnyMesh,
                            .m_WorldMatrix = WorldMatrix
                        }
                    };
                    d3d12->m_RenderQueues[d3d12->m_CurrentThreadIndex]->Push(RenderItem);

                    // Update thread index to use.
                    UINT NextThreadIndex = (d3d12->m_CurrentThreadIndex + 1) % RENDER_THREAD_COUNT;
                    d3d12->m_CurrentThreadIndex = NextThreadIndex;
                }
            }

            { // End
#if 1
                d3d12->m_RemainThreadCount = RENDER_THREAD_COUNT;
                for (UINT i = 0; i < RENDER_THREAD_COUNT; ++i)
                {
                    SetEvent(d3d12->m_RenderThreadContexts[i].Event);
                }
                WaitForSingleObject(d3d12->m_CompletionEvent, INFINITE);
#else
                for (UINT i = 0; i < RENDER_THREAD_COUNT; ++i)
                {
                    command_list_pool* CommandListPool = d3d12->_CommandListPools[d3d12->m_CurrentContextIndex][i];
                    d3d12->m_RenderQueues[i]->Execute(CommandListPool, d3d12->m_CommandQueue, 256);
                }
#endif


                {
                    const UINT ArbitrayThreadIndex = 0;
                    command_list_pool* CommandListPool = d3d12->_CommandListPools[d3d12->m_CurrentContextIndex][ArbitrayThreadIndex];
                    ID3D12GraphicsCommandList* CommandList = CommandListPool->GetCurrent()->CommandList;
                    {
                        auto TransitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->m_RenderTargets[d3d12->m_RenderTargetIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
                        CommandList->ResourceBarrier(1, &TransitionToPresent);
                    }
                    CommandListPool->CloseAndSubmit();
                }
            }

            { // Present
                d3d12->Fence();

                UINT SyncInterval = 0; // VSync (1: on, 0: off)
                UINT Flags = SyncInterval ? 0 : DXGI_PRESENT_ALLOW_TEARING;

                ASSERT_SUCCEEDED( d3d12->m_SwapChain->Present(SyncInterval, Flags) );

                d3d12->m_RenderTargetIndex = d3d12->m_SwapChain->GetCurrentBackBufferIndex();

                UINT NextContextIndex = (d3d12->m_CurrentContextIndex + 1) % MAX_PENDING_FRAME_COUNT;

                d3d12->FenceWait(d3d12->m_LastFenceValues[NextContextIndex]);

                d3d12->_DescriptorPools[NextContextIndex]->Clear();
                d3d12->_ConstantBufferPools[NextContextIndex]->Clear();

                for (UINT i = 0; i < RENDER_THREAD_COUNT; ++i)
                {
                    d3d12->_CommandListPools[NextContextIndex][i]->Reset();
                }

                d3d12->m_CurrentContextIndex = NextContextIndex;
            }
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

        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            //b32 WasDown = !!(lparam & (1<<30));
            //b32 IsDown  =  !(lparam & (1<<31));

            const f32 DeltaPos = 0.10f;

            // @Temporary
            if (wparam == 'W')
            {
                g_CameraPosition.Z -= DeltaPos;
            }
            else if (wparam == 'A')
            {
                g_CameraPosition.X -= DeltaPos;
            }
            else if (wparam == 'S')
            {
                g_CameraPosition.Z += DeltaPos;
            }
            else if (wparam == 'D')
            {
                g_CameraPosition.X += DeltaPos;
            }
            else if (wparam == 'Q')
            {
                g_CameraPosition.Y += DeltaPos;
            }
            else if (wparam == 'E')
            {
                g_CameraPosition.Y -= DeltaPos;
            }
        } break;

        default: 
        {
            Result = DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
    }

    return Result;
}

static void w32Init(Win32_State* State, HINSTANCE hinst) 
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
    ASSERT( Result ); 
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
