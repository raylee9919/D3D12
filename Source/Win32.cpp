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

// =================================================
// .h
// =================================================
#include "Base.h"
#include "Math.h"
#include "Win32.h"
#include "D3D12.h"

struct vertex 
{
    Vec3 Position;
    Vec4 Color;
    Vec2 TexCoord;
};

struct constant_buffer
{
    Vec2 Offset;
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
#include "D3D12.cpp"

static b32 g_Running = true;



struct mesh
{
    virtual void Init(vertex *Vertices, u32 VertexCount, u16 *Indices, u32 IndexCount) = 0;
    virtual void Draw() = 0;
};

struct d3d12_mesh : public mesh
{
    ID3D12RootSignature *m_RootSignature = nullptr;
    ID3D12PipelineState *m_PipelineState = nullptr;
    ID3D12Resource *m_VertexBuffer = nullptr;
    ID3D12Resource *m_IndexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW  m_IndexBufferView = {};

    u32 m_IndexCount;

    virtual void Init(vertex *Vertices, u32 VertexCount, u16 *Indices, u32 IndexCount) override
    {
        m_IndexCount = IndexCount;

        {
            UINT Size = sizeof(Vertices[0])*VertexCount;

            // @Temporary: Upload heap bad. Whenever the GPU needs it, it will fetch from the RAM.
            auto HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);
            ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_VertexBuffer)) );

            void *Mapped;
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

            void *Mapped;
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
                Assert(!"Vertex Shader Compilation Failed.");
            }
            if (FAILED( D3DCompileFromFile(L"./Shaders/Shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", CompilerFlags, 0, &PixelShader, &ErrorBlob) ))
            {
                LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
                OutputDebugStringA(Error);
                Assert(!"Pixel Shader Compilation Failed.");
            }

            D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

            D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
                .pRootSignature         = m_RootSignature,
                .VS                     = CD3DX12_SHADER_BYTECODE(VertexShader->GetBufferPointer(), VertexShader->GetBufferSize()),
                .PS                     = CD3DX12_SHADER_BYTECODE(PixelShader->GetBufferPointer(), PixelShader->GetBufferSize()),
                .BlendState             = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
                .SampleMask             = UINT_MAX,
                .RasterizerState        = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
                .DepthStencilState      = {
                    .DepthEnable   = FALSE,
                    .StencilEnable = FALSE,
                },
                .InputLayout            = { InputElementDescs, _countof(InputElementDescs) }, 
                .PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
                .NumRenderTargets       = 1,
                .SampleDesc = {
                    .Count = 1,
                }
            };
            PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

            ASSERT_SUCCEEDED( Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&m_PipelineState)) );

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

    virtual void Draw() override
    {
        auto CmdList = d3d12->CommandList;

        CmdList->SetGraphicsRootSignature(m_RootSignature);

        CmdList->SetDescriptorHeaps(1, &DescriptorHeap);

        CD3DX12_GPU_DESCRIPTOR_HANDLE GPUDescriptorTable(DescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        CmdList->SetGraphicsRootDescriptorTable(0, GPUDescriptorTable);

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
    win32_state->hwnd = w32CreateWindow(win32_state, L"Engine");


    d3d12 = new D3D12_State;
    memset(d3d12, 0, sizeof(D3D12_State));
    d12Init(win32_state->hwnd);






    vertex Vertices[] = {
        { {-0.25f, -0.25f, 0.0f }, {0.0f, 0.0f, 0.0f, 1.0f}, {0.f, 1.f} },
        { { 0.25f, -0.25f, 0.0f }, {1.0f, 0.0f, 0.0f, 1.0f}, {1.f, 1.f} },
        { {-0.25f,  0.25f, 0.0f }, {0.0f, 1.0f, 0.0f, 1.0f}, {0.f, 0.f} },
        { { 0.25f,  0.25f, 0.0f }, {0.0f, 0.0f, 1.0f, 1.0f}, {1.f, 0.f} },
    };
    u32 VertexCount = _countof(Vertices);
    u16 Indices[] = { 0, 2, 1, 1, 2, 3 };
    UINT IndexCount = _countof(Indices);


    d3d12_mesh *Mesh = new d3d12_mesh;
    Mesh->Init(Vertices, VertexCount, Indices, IndexCount);


    // @Temporary
    ID3D12Resource *ConstantBuffer = nullptr;
    constant_buffer *SystemConstantBuffer = nullptr;

    ID3D12DescriptorHeap *DescriptorHeap = nullptr;
    UINT DescriptorSize = 0;


    u32 Dim    = 64;
    u32 Width  = 1024;
    u32 Height = 1024;
    u32 Stride = Width*4;
    DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    u8 *Bitmap = new u8[Stride*Height];
    memset(Bitmap, 0, Stride*Height);
    for (u32 Y = 0; Y < Height; ++Y) 
    {
        for (u32 X = 0; X < Width; ++X) 
        {
            if ((X/Dim + Y/Dim)%2 == 0)
            {
                u32 *Texel = (u32 *)Bitmap + Width*Y + X;
                *Texel = 0xffffffff; 
            }
        }
    }
    
    ID3D12Resource *TextureResource = nullptr;

    {
        D3D12_RESOURCE_DESC TextureDesc = {
            .Dimension =  D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = Width,
            .Height = Height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = Format,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        auto DefaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&DefaultHeapProp, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&TextureResource) ) );

        if (Bitmap)
        {
            D3D12_RESOURCE_DESC Desc = TextureResource->GetDesc();
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
            {
                d3d12->Device->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, nullptr, nullptr, nullptr);
            }

            BYTE *Mapped = nullptr;
            CD3DX12_RANGE ReadRange(0, 0);

            UINT64 UploadBufferSize = GetRequiredIntermediateSize(TextureResource, 0, 1);

            ID3D12Resource *UploadBuffer = nullptr;

            auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);
            ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)) );

            ASSERT_SUCCEEDED( UploadBuffer->Map(0, &ReadRange, (void **)&Mapped) );

            BYTE *Src = (BYTE *)Bitmap;
            BYTE *Dst = Mapped;
            for (UINT Y = 0; Y < Height; Y++)
            {
                memcpy(Dst, Src, Stride);
                Src += Stride;
                Dst += Footprint.Footprint.RowPitch;
            }
            UploadBuffer->Unmap(0, nullptr);

            {
                const DWORD MAX_SUB_RESOURCE_NUM = 32;
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint[MAX_SUB_RESOURCE_NUM] = {};
                UINT	Rows[MAX_SUB_RESOURCE_NUM] = {};
                UINT64	RowSize[MAX_SUB_RESOURCE_NUM] = {};
                UINT64	TotalBytes = 0;

                D3D12_RESOURCE_DESC Desc = TextureResource->GetDesc();
                Assert( Desc.MipLevels <= (UINT)_countof(Footprint) );

                d3d12->Device->GetCopyableFootprints(&Desc, 0, Desc.MipLevels, 0, Footprint, Rows, RowSize, &TotalBytes);

                ASSERT_SUCCEEDED( d3d12->CommandAllocator->Reset() );

                ASSERT_SUCCEEDED( d3d12->CommandList->Reset(d3d12->CommandAllocator, nullptr) );

                auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(TextureResource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
                d3d12->CommandList->ResourceBarrier(1, &Transition1);
                for (DWORD i = 0; i < Desc.MipLevels; i++)
                {
                    D3D12_TEXTURE_COPY_LOCATION	destLocation = {};
                    destLocation.PlacedFootprint = Footprint[i];
                    destLocation.pResource = TextureResource;
                    destLocation.SubresourceIndex = i;
                    destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

                    D3D12_TEXTURE_COPY_LOCATION	srcLocation = {};
                    srcLocation.PlacedFootprint = Footprint[i];
                    srcLocation.pResource = UploadBuffer;
                    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

                    d3d12->CommandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
                }
                auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(TextureResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
                d3d12->CommandList->ResourceBarrier(1, &Transition2);
                d3d12->CommandList->Close();

                ID3D12CommandList *CommandLists[] = { d3d12->CommandList };
                d3d12->CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);


                d12Fence(d3d12->fence);
                d12FenceWait(d3d12->fence);
            }

            UploadBuffer->Release();
        }

        delete Bitmap;
    }

    
    { // Create descriptor table.
        DescriptorSize = d3d12->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_DESCRIPTOR_HEAP_DESC CommonHeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = DESCRIPTOR_INDEX_COUNT,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        };

        ASSERT_SUCCEEDED( d3d12->Device->CreateDescriptorHeap(&CommonHeapDesc, IID_PPV_ARGS(&DescriptorHeap)) );
    }

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
            .Format = Format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {
                .MipLevels = 1,
            },
        };

        CD3DX12_CPU_DESCRIPTOR_HANDLE SRVHandle(DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), DESCRIPTOR_INDEX_TEXTURE, DescriptorSize);
        d3d12->Device->CreateShaderResourceView(TextureResource, &SRVDesc, SRVHandle);
    }

    {
        const UINT ConstantBufferSize = ( sizeof(constant_buffer) + 255 & (~255) );

        auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ConstantBufferSize);
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&ConstantBuffer) ) );

        D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc = {
            .BufferLocation = ConstantBuffer->GetGPUVirtualAddress(),
            .SizeInBytes    = ConstantBufferSize
        };

        CD3DX12_CPU_DESCRIPTOR_HANDLE CBVHandle(DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), DESCRIPTOR_INDEX_CONSTANT, DescriptorSize);
        d3d12->Device->CreateConstantBufferView(&CBVDesc, CBVHandle);

        CD3DX12_RANGE ReadRange(0, 0);
        ASSERT_SUCCEEDED( ConstantBuffer->Map(0, &ReadRange, (void **)&SystemConstantBuffer) );

        SystemConstantBuffer->Offset.X = 0.2f;
        SystemConstantBuffer->Offset.Y = 0.f;

        // @Temporary: No Unmap().
    }



    // Main Loop
    //
    const f32 TickRate = 1.f / 60.f; 
    f32 AccumulatedTime = 0.f;
    u64 OldTimer = OS::ReadTimer();
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
        f32 DeltaTime = (f32)((f64)(NewTimer - OldTimer)*InverseTimerFrequency + 0.5f);
        AccumulatedTime += DeltaTime;
        for (;AccumulatedTime >= TickRate; AccumulatedTime -= TickRate)
        {
            // @Todo Update(TickRate);
        }


        {
            ASSERT_SUCCEEDED( d3d12->CommandAllocator->Reset() );
            ASSERT_SUCCEEDED( d3d12->CommandList->Reset(d3d12->CommandAllocator, nullptr/*dummy pipeline state that the app doesn't have to worry about.*/) );

            auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->render_target_views[d3d12->frame_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            d3d12->CommandList->ResourceBarrier(1, &Transition1);

            CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(d3d12->rtv_heap->GetCPUDescriptorHandleForHeapStart(), d3d12->frame_index, d3d12->rtv_descriptor_size);
            d3d12->CommandList->RSSetViewports(1, &d3d12->Viewport);
            d3d12->CommandList->RSSetScissorRects(1, &d3d12->ScissorRect);
            d3d12->CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, nullptr);

            FLOAT ClearColor[4] = { 0.2f, 0.2f, 0.2f, 1.f };
            d3d12->CommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);



            { // Draw mesh.
            }


            

            auto TransitionFromRenderTargetToPresent = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->render_target_views[d3d12->frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            d3d12->CommandList->ResourceBarrier(1, &TransitionFromRenderTargetToPresent);

            ASSERT_SUCCEEDED( d3d12->CommandList->Close() );

            ID3D12CommandList *CmdLists[] = { d3d12->CommandList };
            d3d12->CommandQueue->ExecuteCommandLists(_countof(CmdLists), CmdLists);


            d12Present();



            // @Todo: You don't want to wait until all the commands are executed.
            d12Fence(d3d12->fence);
            d12FenceWait(d3d12->fence);
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
        Assert(!"Couldn't register window class."); 
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
        Assert(!"Couldn't create a window."); 
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
