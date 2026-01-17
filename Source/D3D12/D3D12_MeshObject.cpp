// Copyright Seong Woo Lee. All Rights Reserved.

void mesh_object::Draw(M4x4 Transform)
{
    descriptor_pool *DescriptorPool = d3d12->m_DescriptorPool;
    ID3D12DescriptorHeap *DescriptorHeap = DescriptorPool->m_Heap;
    const UINT DescriptorSize = DescriptorPool->m_DescriptorSize;

    CD3DX12_CPU_DESCRIPTOR_HANDLE CPUDescriptorTableHandle = {};
    CD3DX12_GPU_DESCRIPTOR_HANDLE GPUDescriptorTableHandle = {};
    const UINT DescriptorCount = g_DescriptorCountPerMeshObject + g_DescriptorCountPerIndexGroup*m_IndexGroupCount;
    DescriptorPool->Alloc(&CPUDescriptorTableHandle, &GPUDescriptorTableHandle, DescriptorCount);


    constant_buffer_pool *ConstantBufferPool = d3d12->m_ConstantBufferPool;
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
    for (UINT i = 0; i < m_IndexGroupCount; ++i)
    {
        index_group *IndexGroup = m_IndexGroup[i];
        texture *Texture = IndexGroup->m_Texture;
        ASSERT( Texture );
        d3d12->Device->CopyDescriptorsSimple(1, Dst, Texture->m_CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        Dst.Offset(1, DescriptorSize);
    }

    auto CmdList = d3d12->CommandList;

    CmdList->SetGraphicsRootSignature(m_RootSignature);
    CmdList->SetDescriptorHeaps(1, &DescriptorHeap);
    CmdList->SetPipelineState(m_PipelineState);
    CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    CmdList->SetGraphicsRootDescriptorTable(0, GPUDescriptorTableHandle);


    CD3DX12_GPU_DESCRIPTOR_HANDLE GPUDescriptorTableForIndexGroup(GPUDescriptorTableHandle, g_DescriptorCountPerMeshObject, DescriptorSize);
    for (UINT i = 0; i < m_IndexGroupCount; ++i)
    {
        index_group *Group = m_IndexGroup[i];

        CmdList->SetGraphicsRootDescriptorTable(1, GPUDescriptorTableForIndexGroup);
        GPUDescriptorTableForIndexGroup.Offset(1, DescriptorSize*g_DescriptorCountPerIndexGroup);

        CmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
        CmdList->IASetIndexBuffer(&Group->m_IndexBufferView);
        CmdList->DrawIndexedInstanced(Group->m_IndexCount, 1, 0, 0, 0);
    }
}
