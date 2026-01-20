// Copyright Seong Woo Lee. All Rights Reserved.

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


static texture *CreateTexture(void *Data, UINT Width, UINT Height, DXGI_FORMAT Format)
{
    texture* Result = new texture;
    ID3D12Resource *Resource = nullptr;

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
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&DefaultHeapProp, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&Resource) ) );

        if (Data)
        {
            D3D12_RESOURCE_DESC Desc = Resource->GetDesc();
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
            {
                d3d12->Device->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, nullptr, nullptr, nullptr);
            }

            BYTE *Mapped = nullptr;
            CD3DX12_RANGE ReadRange(0, 0);

            UINT64 UploadBufferSize = GetRequiredIntermediateSize(Resource, 0, 1);

            ID3D12Resource *UploadBuffer = nullptr;

            auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);
            ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)) );

            ASSERT_SUCCEEDED( UploadBuffer->Map(0, &ReadRange, (void **)&Mapped) );

            BYTE *Src = (BYTE *)Data;
            BYTE *Dst = Mapped;
            UINT Stride = Width*4; // @Hack
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

                D3D12_RESOURCE_DESC Desc = Resource->GetDesc();
                Assert( Desc.MipLevels <= (UINT)_countof(Footprint) );

                d3d12->Device->GetCopyableFootprints(&Desc, 0, Desc.MipLevels, 0, Footprint, Rows, RowSize, &TotalBytes);

                ASSERT_SUCCEEDED( d3d12->CommandAllocator->Reset() );

                ASSERT_SUCCEEDED( d3d12->CommandList->Reset(d3d12->CommandAllocator, nullptr) );

                auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(Resource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
                d3d12->CommandList->ResourceBarrier(1, &Transition1);
                for (DWORD i = 0; i < Desc.MipLevels; i++)
                {
                    D3D12_TEXTURE_COPY_LOCATION	destLocation = {
                        .pResource = Resource,
                        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                        .SubresourceIndex = i,
                    };

                    D3D12_TEXTURE_COPY_LOCATION	srcLocation = {
                        .pResource = UploadBuffer,
                        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                        .PlacedFootprint = Footprint[i],
                    };

                    d3d12->CommandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
                }
                auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(Resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
                d3d12->CommandList->ResourceBarrier(1, &Transition2);
                d3d12->CommandList->Close();

                ID3D12CommandList *CommandLists[] = { d3d12->CommandList };
                d3d12->CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);


                d12Fence(d3d12->m_Fence);
                d12FenceWait(d3d12->m_Fence);
            }

            UploadBuffer->Release();
        }
    }


    CD3DX12_CPU_DESCRIPTOR_HANDLE SRVHandle = {};
    d3d12->m_SRVDescriptorPool->Alloc(&SRVHandle, nullptr, 1);
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
        .Format = Format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = {
            .MipLevels = 1,
        },
    };
    d3d12->Device->CreateShaderResourceView(Resource, &SRVDesc, SRVHandle);

    Result->m_Resource  = Resource;
    Result->m_CPUHandle = SRVHandle;
    return Result;
}



    
    // @Temporary: Load GLTF
    //
    cgltf_options Options = {};
    cgltf_data* Data = nullptr;
    cgltf_result Result = cgltf_parse_file(&Options, "./Assets/Fox.gltf", &Data);
    ASSERT( Result == cgltf_result_success );

    mesh* FoxMesh = nullptr;
    for (int MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
    {
        cgltf_mesh* Mesh = &Data->meshes[MeshIndex];
        for (int PrimitiveIndex = 0; PrimitiveIndex < Mesh->primitives_count; ++PrimitiveIndex)
        {
            cgltf_primitive* Primitive = &Mesh->primitives[PrimitiveIndex];
            ASSERT( Primitive->type == cgltf_primitive_type_triangles );
            for (int AttributeIndex = 0; AttributeIndex < Primitive->attributes_count; ++AttributeIndex)
            {
                cgltf_attribute* Attribute = &Primitive->attributes[AttributeIndex];

                switch (Attribute->type)
                {
                    case cgltf_attribute_type_position:
                    {
                        cgltf_accessor* Accessor = Attribute->data;
                        ASSERT(Accessor->type == cgltf_type_vec3);
                        ASSERT(Accessor->component_type == cgltf_component_type_r_32f);
                        u64 Count = Accessor->count;

                        cgltf_buffer_view* BufferView = Accessor->buffer_view;
                        cgltf_buffer* Buffer = BufferView->buffer;
                        ASSERT(Buffer);

                        if (Buffer->data)
                        {
                            ASSERT( !"Not implemented yet." );
                        }
                        else if (Buffer->uri)
                        {
                            const char *uri = Buffer->uri;
                        }
                        else
                        {
                            ASSERT( !"Invalid Code Path" );
                        }
                    } break;

                    case cgltf_attribute_type_texcoord:
                    {
                        cgltf_accessor* Accessor = Attribute->data;
                        ASSERT(Accessor->type == cgltf_type_vec2);
                        ASSERT(Accessor->component_type == cgltf_component_type_r_32f);
                        u64 Count = Accessor->count;

                        cgltf_buffer_view* BufferView = Accessor->buffer_view;
                        cgltf_buffer* Buffer = BufferView->buffer;
                        ASSERT(Buffer);

                        if (Buffer->data)
                        {
                            ASSERT( !"Not implemented yet." );
                        }
                        else if (Buffer->uri)
                        {
                            const char *uri = Buffer->uri;
                        }
                        else
                        {
                            ASSERT( !"Invalid Code Path" );
                        }
                    } break;

                    default:
                    {
                    } break;
                }
            }
        }
    }


    cgltf_free(Data);



static mesh* CreateCubeMeshObject(f32 Scale)
{
    mesh *Result = new mesh;

    const vertex Vertices[] = {
        vertex{ Vec3( 0.5f, 0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3( 0.5f,-0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3( 0.5f,-0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3( 0.5f, 0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3(-0.5f,-0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3(-0.5f,-0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3(-0.5f, 0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3(-0.5f, 0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3(-0.5f, 0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3( 0.5f, 0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3(-0.5f, 0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3( 0.5f, 0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3( 0.5f,-0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3(-0.5f,-0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3( 0.5f,-0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3(-0.5f,-0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3( 0.5f,-0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3(-0.5f,-0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3( 0.5f, 0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3(-0.5f, 0.5f,-0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,0) },

        vertex{ Vec3(-0.5f,-0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,1) },
        vertex{ Vec3( 0.5f,-0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,1) },
        vertex{ Vec3(-0.5f, 0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(0,0) },
        vertex{ Vec3( 0.5f, 0.5f, 0.5f)*Scale, Vec4(1,1,1,1), Vec2(1,0) },
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

        // @Temporary
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


    // Create index groups.
    for (int i = 0; i < 6; ++i)
    {
        // @Temporary
        const WCHAR *FileNames[6] = {
            L"Assets/AnimeGirl0.dds",
            L"Assets/AnimeGirl1.dds",
            L"Assets/AnimeGirl2.dds",
            L"Assets/AnimeGirl3.dds",
            L"Assets/AnimeGirl4.dds",
            L"Assets/AnimeGirl5.dds",
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

        submesh *Submesh = new submesh;
        {
            Submesh->m_IndexCount      = IndexCount;
            Submesh->m_IndexBuffer     = IndexBuffer;
            Submesh->m_IndexBufferView = IndexBufferView;
            Submesh->m_Texture         = d3d12->m_TextureManager->CreateTextureFromFileName(FileNames[i], (UINT)wcslen(FileNames[i]));
        }

        // @Temporary
        Result->m_Submesh.push_back(Submesh);
        Result->m_SubmeshCount++;
    }

    return Result;
}
