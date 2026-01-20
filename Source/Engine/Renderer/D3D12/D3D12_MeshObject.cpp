// Copyright Seong Woo Lee. All Rights Reserved.

void mesh::Draw(ID3D12GraphicsCommandList* CommandList, M4x4 Transform)
{
    descriptor_pool* DescriptorPool = d3d12->_DescriptorPools[d3d12->m_CurrentContextIndex];
    ID3D12DescriptorHeap* DescriptorHeap = DescriptorPool->m_Heap;
    const UINT DescriptorSize = DescriptorPool->m_DescriptorSize;

    CD3DX12_CPU_DESCRIPTOR_HANDLE CPUDescriptorTableHandle = {};
    CD3DX12_GPU_DESCRIPTOR_HANDLE GPUDescriptorTableHandle = {};
    const UINT DescriptorCount = g_DescriptorCountPerMesh + g_DescriptorCountPerSubmesh*m_SubmeshCount;
    DescriptorPool->Alloc(&CPUDescriptorTableHandle, &GPUDescriptorTableHandle, DescriptorCount);


    constant_buffer_pool *ConstantBufferPool = d3d12->_ConstantBufferPools[d3d12->m_CurrentContextIndex];
    cbv_descriptor* CBVDescriptor = ConstantBufferPool->Alloc();

    simple_constant_buffer *MappedConstantBuffer = (simple_constant_buffer *)CBVDescriptor->m_CPUMappedAddress;
    {
        MappedConstantBuffer->World = Transform;
        MappedConstantBuffer->View  = d3d12->m_View;
        MappedConstantBuffer->Proj  = d3d12->m_Proj;
    }

    // Per object.
    CD3DX12_CPU_DESCRIPTOR_HANDLE Dst(CPUDescriptorTableHandle, MESH_DESCRIPTOR_INDEX_PER_OBJ_CBV);
    d3d12->Device->CopyDescriptorsSimple(1, Dst, CBVDescriptor->m_CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Dst.Offset(1, DescriptorSize);

    // Per index group.
    for (UINT i = 0; i < m_SubmeshCount; ++i)
    {
        submesh *Submesh = m_Submesh[i];
        texture *Texture = Submesh->m_Texture;
        ASSERT( Texture );
        d3d12->Device->CopyDescriptorsSimple(1, Dst, Texture->m_CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        Dst.Offset(1, DescriptorSize);
    }

    CommandList->SetGraphicsRootSignature(m_RootSignature);
    CommandList->SetDescriptorHeaps(1, &DescriptorHeap);
    CommandList->SetPipelineState(m_PipelineState);
    CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    CommandList->SetGraphicsRootDescriptorTable(0, GPUDescriptorTableHandle);


    CD3DX12_GPU_DESCRIPTOR_HANDLE GPUDescriptorTableForSubmesh(GPUDescriptorTableHandle, g_DescriptorCountPerMesh, DescriptorSize);
    for (UINT i = 0; i < m_SubmeshCount; ++i)
    {
        submesh *Group = m_Submesh[i];

        CommandList->SetGraphicsRootDescriptorTable(1, GPUDescriptorTableForSubmesh);
        GPUDescriptorTableForSubmesh.Offset(1, DescriptorSize*g_DescriptorCountPerSubmesh);

        CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
        CommandList->IASetIndexBuffer(&Group->m_IndexBufferView);
        CommandList->DrawIndexedInstanced(Group->m_IndexCount, 1, 0, 0, 0);
    }
}
