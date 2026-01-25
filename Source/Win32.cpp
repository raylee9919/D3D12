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
    M4x4 SkinningMatrices[MAX_JOINT_COUNT];
};

struct camera 
{
    Vec3 Position;
    quaternion Orientation;
};

// =================================================
// .cpp
// =================================================
#include "Engine/Engine.cpp"
#include "ThirdParty/DirectX/DirectXTex/DDSTextureLoader12.cpp"

static b32 g_Running = true;
static camera* g_Camera;

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


    d3d12 = new d3d12_renderer;
    InitRenderer(d3d12, win32_state->hwnd, 1920, 1080);


    ascii_loader* Loader  = new ascii_loader;
    auto[MannequinMesh, MannequinnSkeleton] = Loader->LoadMeshFromFilePath("Mannequin.mesh");
    animation_clip* Animation  = Loader->LoadAnimationFromFilePath("Run_Forward.anim");
    //animation_clip* Animation  = Loader->LoadAnimationFromFilePath("Standing_Idle.anim");
    d3d12_mesh* Mannequin = new d3d12_mesh;
    M4x4 MannequinWorldTransform = M4x4Identity();

    arena* UpdatePassArena = new arena;

    {
        D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
        ID3D12Resource* VertexBuffer = nullptr;
        D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};
        ID3D12Resource* IndexBuffer = nullptr;

        d3d12->m_ResourceManager->CreateVertexBuffer(sizeof(MannequinMesh->Vertices[0]), MannequinMesh->VerticesCount, MannequinMesh->Vertices, &VertexBufferView, &VertexBuffer);
        d3d12->m_ResourceManager->CreateIndexBuffer(sizeof(MannequinMesh->Indices[0]), MannequinMesh->IndicesCount, MannequinMesh->Indices, &IndexBufferView, &IndexBuffer);

        { // Create root signature and pipeline state.
            ID3D12RootSignature* RootSignature = nullptr;
            ID3D12PipelineState* PipelineState = nullptr;

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
            ASSERT_SUCCEEDED( d3d12->Device->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature)) );

            if (Signature) {
                Signature->Release();
            }
            if (Error) {
                Error->Release();
            }

            // Create pipeline state.
            //
            ID3DBlob* VertexShader = nullptr;
            ID3DBlob* PixelShader  = nullptr;
            ID3DBlob* ErrorBlob    = nullptr;

            UINT CompilerFlags = 0;
#if BUILD_DEBUG
            CompilerFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

            // @Temporary
            if (FAILED( D3DCompileFromFile(L"./Assets/Shaders/Normal.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", CompilerFlags, 0, &VertexShader, &ErrorBlob) ))
            {
                LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
                OutputDebugStringA(Error);
                ASSERT( !"Vertex Shader Compilation Failed." );
            }
            if (FAILED( D3DCompileFromFile(L"./Assets/Shaders/Normal.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", CompilerFlags, 0, &PixelShader, &ErrorBlob) ))
            {
                LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
                OutputDebugStringA(Error);
                ASSERT( !"Pixel Shader Compilation Failed." );
            }

            // @Temporary
            D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vert, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vert, Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(vert, Weights), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "JOINTS", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, offsetof(vert, Joints), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

            D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {
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
            PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
            PsoDesc.RasterizerState.FrontCounterClockwise = true;
            PsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            PsoDesc.DepthStencilState.StencilEnable = FALSE;

            ASSERT_SUCCEEDED( d3d12->Device->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&PipelineState)) );

            if (VertexShader) {
                VertexShader->Release();
            }

            if (PixelShader) {
                PixelShader->Release();
            }

            Mannequin->m_RootSignature = RootSignature;
            Mannequin->m_PipelineState = PipelineState;
        }

        submesh *Submesh = new submesh;
        {
            Submesh->m_IndexCount      = MannequinMesh->IndicesCount;
            Submesh->m_IndexBuffer     = IndexBuffer;
            Submesh->m_IndexBufferView = IndexBufferView;
            Submesh->m_Texture         = d3d12->m_ResourceManager->CreateTextureFromFileName(L"Assets/AnimeGirl0.dds", (UINT)wcslen(L"Assets/AnimeGirl0.dds"));
        }

        // @Temporary
        Mannequin->m_Submesh.push_back(Submesh);
        Mannequin->m_SubmeshCount++;

        Mannequin->m_VertexBuffer = VertexBuffer;
        Mannequin->m_VertexBufferView = VertexBufferView;
        Mannequin->m_IndexBuffer = IndexBuffer;
        Mannequin->m_IndexBufferView = IndexBufferView;
    }

    g_Camera = new camera;
    g_Camera->Position = Vec3(2.0f, 2.0f, 2.0f);

    // @Temporary
    skeleton_pose* SkeletonPose = nullptr;


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
        //printf("fps:%.2f\n", 1.0f/DeltaTime);
        OldTimer = NewTimer;
        AccumulatedTime += DeltaTime;
        Time += DeltaTime;
        for (;AccumulatedTime >= TickRate; AccumulatedTime -= TickRate)
        {
            // Update Here.
            //
            UpdatePassArena->Clear();
            const f32 DeltaTime = TickRate;

            SkeletonPose = UpdatePassArena->Push<skeleton_pose>(1);
            {
                SkeletonPose->Skeleton           = MannequinnSkeleton;
                SkeletonPose->LocalPoses         = UpdatePassArena->Push<joint_pose>(SkeletonPose->Skeleton->JointsCount);
                SkeletonPose->ModelSpaceMatrices = UpdatePassArena->Push<M4x4>(SkeletonPose->Skeleton->JointsCount);
                SkeletonPose->SkinningMatrices   = UpdatePassArena->Push<M4x4>(SkeletonPose->Skeleton->JointsCount);
            }

            const f32 TimeBetweenSamples = 0.025f;
            const u32 SamplesCount = Animation->SamplesCount;
            const f32 AnimDuration = SamplesCount * TimeBetweenSamples;
            static f32 AnimTime = 0.0f;
            AnimTime += DeltaTime;
            AnimTime = fmod_cycling(AnimTime, AnimDuration);

            const u32 SampleIndex1 = (u32)(AnimTime / TimeBetweenSamples);
            const u32 SampleIndex2 = (SampleIndex1 + 1) % SamplesCount;
            const f32 BlendFactor = AnimTime/TimeBetweenSamples - (f32)SampleIndex1;

            for (u32 JointIndex = 0; JointIndex < Animation->JointsCount; ++JointIndex)
            {
                sample Sample1 = *(Animation->Samples + JointIndex*Animation->SamplesCount + SampleIndex1);
                sample Sample2 = *(Animation->Samples + JointIndex*Animation->SamplesCount + SampleIndex2);

                joint_pose* LocalPose = &SkeletonPose->LocalPoses[JointIndex];
                {
                    // When thinking about the axis-angle representation of the quaternions,
                    // if the dot product is negative, that means that the axes or rotation are
                    // at least 90 degrees apart. Negating the axis and angle (the whole quaternion)
                    // makes the quaternions' axes less than 90 degrees apart in that case, without
                    // changing the rotation the quaternion represents, and makes the interpolation
                    // take the shortest path.
                    quaternion First  = Sample1.Rotation;
                    quaternion Second = Sample2.Rotation;
                    if (Dot(First, Second) < 0.0f) {
                        Second = -Second;
                    }
                    LocalPose->Rotation    = Lerp(First, Second, BlendFactor);
                    LocalPose->Translation = Lerp(Sample1.Translation, Sample2.Translation, BlendFactor);
                    LocalPose->Scaling     = Lerp(Sample1.Scaling, Sample2.Scaling, BlendFactor);
                }
            }

            for (u32 JointIndex = 0; JointIndex < Animation->JointsCount; ++JointIndex)
            {
                joint* Joint = SkeletonPose->Skeleton->Joints + JointIndex;
                joint_pose* JointPose = SkeletonPose->LocalPoses + JointIndex;

                // @Todo: Correct?
                M4x4 LocalTransform = M4x4Translation(JointPose->Translation) * M4x4Rotation(JointPose->Rotation) * M4x4Scale(JointPose->Scaling);

                if (Joint->Parent >= 0) {
                    SkeletonPose->ModelSpaceMatrices[JointIndex] = SkeletonPose->ModelSpaceMatrices[Joint->Parent] * LocalTransform; 
                }
                else {
                    SkeletonPose->ModelSpaceMatrices[JointIndex] = Joint->LocalTransform;
                }
            }

            for (u32 JointIndex = 0; JointIndex < Animation->JointsCount; ++JointIndex)
            {
                joint* Joint = SkeletonPose->Skeleton->Joints + JointIndex;
                SkeletonPose->SkinningMatrices[JointIndex] = SkeletonPose->ModelSpaceMatrices[JointIndex] * Joint->InverseRestPose; 
            }


            // @Temporary: Update camera.
            //
            const f32 QuaterOfPI = 0.785398163f;
            const f32 AspectRatio = 16.f/9.f;
            const f32 NearZ = 0.1f;
            const f32 FarZ  = 1000.f;
            d3d12->m_View = M4x4LookAtRH(g_Camera->Position, Vec3(0.f, 0.5f, 0.f), Vec3(0.f, 1.f, 0.f));
            d3d12->m_Proj = M4x4PerspectiveLH(QuaterOfPI, AspectRatio, NearZ, FarZ);
        }


        {
            { // Begin
                const UINT ArbitraryThreadIndex = 0;
                command_list_pool* CommandListPool = d3d12->m_CommandListPools[d3d12->m_CurrentContextIndex][ArbitraryThreadIndex];
                ID3D12GraphicsCommandList* CommandList = CommandListPool->GetCurrent()->CommandList;
                {
                    auto Transition = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->m_RenderTargets[d3d12->m_RenderTargetIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
                    CommandList->ResourceBarrier(1, &Transition);

                    CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle = d3d12->GetRtvHandle();
                    CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHandle = d3d12->GetDsvHandle();

                    FLOAT ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.f };
                    CommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);
                    CommandList->ClearDepthStencilView(DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
                }
                CommandListPool->CloseAndSubmit();
            }

            // Push mannequin
            if (SkeletonPose) 
            {
                render_item RenderItem = {
                    .m_Type = RENDER_ITEM_SKINNED_MESH,
                    .SkinnedMesh = {
                        .Mesh = Mannequin,
                        .WorldMatrix = MannequinWorldTransform,
                        .SkinningMatrices = SkeletonPose->SkinningMatrices,
                        .MatricesCount = SkeletonPose->Skeleton->JointsCount
                    }
                };
                d3d12->m_RenderQueues[d3d12->m_CurrentThreadIndex]->Push(RenderItem);
            }

            { // End
                d3d12->m_RemainThreadCount = RENDER_THREAD_COUNT;
                for (UINT i = 0; i < RENDER_THREAD_COUNT; ++i)
                {
                    SetEvent(d3d12->m_RenderThreadContexts[i]->Event);
                }
                WaitForSingleObject(d3d12->m_CompletionEvent, INFINITE);


                {
                    const UINT ArbitrayThreadIndex = 0;
                    command_list_pool* CommandListPool = d3d12->m_CommandListPools[d3d12->m_CurrentContextIndex][ArbitrayThreadIndex];
                    ID3D12GraphicsCommandList* CommandList = CommandListPool->GetCurrent()->CommandList;
                    {
                        auto TransitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->m_RenderTargets[d3d12->m_RenderTargetIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
                        CommandList->ResourceBarrier(1, &TransitionToPresent);
                    }
                    CommandListPool->CloseAndSubmit();
                }
            }

            { // Present
                UINT SyncInterval = 0; // VSync (1: on, 0: off)
                UINT Flags = SyncInterval ? 0 : DXGI_PRESENT_ALLOW_TEARING;

                ASSERT_SUCCEEDED( d3d12->m_SwapChain->Present(SyncInterval, Flags) );

                d3d12->m_RenderTargetIndex = d3d12->m_SwapChain->GetCurrentBackBufferIndex();

                UINT NextContextIndex = (d3d12->m_CurrentContextIndex + 1) % MAX_PENDING_FRAME_COUNT;

                d3d12->Fence();
                d3d12->FenceWait(d3d12->m_LastFenceValues[NextContextIndex]);


                for (UINT i = 0; i < RENDER_THREAD_COUNT; ++i)
                {
                    d3d12->m_DescriptorPools[NextContextIndex][i]->Clear();
                    d3d12->m_ConstantBufferPools[NextContextIndex][i]->Clear();
                    d3d12->m_CommandListPools[NextContextIndex][i]->Reset();
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
                d3d12->UpdateFramebuffer(Width, Height);
            }
        } break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            //b32 WasDown = !!(lparam & (1<<30));
            //b32 IsDown  =  !(lparam & (1<<31));

            // @Temporary
            //if (wparam == 'W')
            //{
            //    g_CameraPosition.Z -= DeltaPos;
            //}
            //else if (wparam == 'A')
            //{
            //    g_CameraPosition.X -= DeltaPos;
            //}
            //else if (wparam == 'S')
            //{
            //    g_CameraPosition.Z += DeltaPos;
            //}
            //else if (wparam == 'D')
            //{
            //    g_CameraPosition.X += DeltaPos;
            //}
            //else if (wparam == 'Q')
            //{
            //    g_CameraPosition.Y += DeltaPos;
            //}
            //else if (wparam == 'E')
            //{
            //    g_CameraPosition.Y -= DeltaPos;
            //}
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
