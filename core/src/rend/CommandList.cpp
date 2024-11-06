#include "pch.h"
#include "CommandList.h"
#include "Shader.h"
#include "UploadBuffer.h"
#include "DynamicDescriptorHeap.h"
#include "GpuTimer.h"
#include "ResourceView.h"

// todo: under define
#include "CommandQueue.h"
#include "WinPixEventRuntime/pix3.h"

#include "RendRes.h"
#include "shared/hlslCppShared.hlsli"
#include "utils/Algorithm.h"

namespace pbe {
   CommandList::CommandList(CommandQueue* ownerCommandQueue) : ownerCommandQueue(ownerCommandQueue) {
      D3D12_COMMAND_LIST_TYPE type = ownerCommandQueue->GetType();
      auto device = sDevice->g_Device;
      ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));
      ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
      ThrowIfFailed(commandList->Close());

      m_UploadBuffer = std::make_unique<UploadBuffer>();

      for (auto [i, heap] : std::views::enumerate(m_DynamicDescriptorHeap)) {
         m_DynamicDescriptorHeap[i] =
            std::make_unique<DynamicDescriptorHeap>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
      }

      for (int i = 0; i < _countof(m_DynamicDescriptorHeap); ++i) {
         m_DynamicDescriptorHeap[i] =
            std::make_unique<DynamicDescriptorHeap>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
      }

      GpuTimeQueryBuffer = Buffer::Create(
         Buffer::Desc::Default(CommandQueue::MaxGpuTimers * 2 * sizeof(u64))
         .InReadback().Name("CommandList GPUTimerBuffer"));
   }

   CommandList::~CommandList() {
   }

   void CommandList::CopyResource(GpuResource& dst, const GpuResource& src) {
      TrackResource(dst);
      TrackResource(src);

      TransitionBarrier(dst, D3D12_RESOURCE_STATE_COPY_DEST);
      TransitionBarrier(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
      FlushResourceBarriers();

      commandList->CopyResource(dst.pResource.Get(), src.pResource.Get());
   }

   void CommandList::CopyBufferRegion(Buffer& dst, u64 dstOffset, const Buffer& src, u64 srcOffset,
      u64 sizeInBytes) {
      ASSERT(dstOffset + sizeInBytes <= dst.GetDesc().size);
      ASSERT(srcOffset + sizeInBytes <= src.GetDesc().size);

      TrackResource(dst);
      TrackResource(src);

      TransitionBarrier(dst, D3D12_RESOURCE_STATE_COPY_DEST);
      TransitionBarrier(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
      FlushResourceBarriers();

      commandList->CopyBufferRegion(dst.pResource.Get(), dstOffset, src.pResource.Get(), srcOffset, sizeInBytes);
   }


   void CommandList::UpdateBuffer(Buffer& buffer, u32 bufferOffsetInBytes, const void* data, size_t dataSizeInBytes) {
      if (buffer.Valid() && dataSizeInBytes > 0 && data) {
         ASSERT(bufferOffsetInBytes + dataSizeInBytes <= buffer.GetDesc().size);

         auto allocation = m_UploadBuffer->Allocate(dataSizeInBytes, 4);
         memcpy(allocation.CPU, data, dataSizeInBytes);

         CopyBufferRegion(buffer, bufferOffsetInBytes,
            *allocation.uploadBuffer, allocation.uploadBufferOffset, dataSizeInBytes);
      }
   }

   Ref<Buffer> CommandList::CreateBuffer(const Buffer::Desc& desc, const void* bufferData, u64 bufferSize) {
      ASSERT(bufferSize <= desc.size);
      if (desc.size == 0) {
         return {};
      }

      auto buffer = Buffer::Create(desc);

#if 0
         if (bufferData != nullptr) {
            auto uploadResource = Buffer::Create(Buffer::Desc::Upload(bufferSize)
               .InitialState(D3D12_RESOURCE_STATE_GENERIC_READ));

            D3D12_SUBRESOURCE_DATA subresourceData = {};
            subresourceData.pData = bufferData;
            subresourceData.RowPitch = bufferSize;
            subresourceData.SlicePitch = subresourceData.RowPitch;

            TransitionBarrier(*buffer, D3D12_RESOURCE_STATE_COPY_DEST);
            FlushResourceBarriers();

            UpdateSubresources(commandList.Get(),
               buffer->pResource.Get(), uploadResource->pResource.Get(),
               0, 0, 1, &subresourceData);

            TrackResource(*uploadResource);
            TrackResource(*buffer);
         }
#else
      UpdateBuffer(*buffer, 0, bufferData, bufferSize);
#endif

      return buffer;
   }

   Ref<Buffer> CommandList::CreateBuffer(const void* bufferData, u64 bufferSize) {
      return CreateBuffer(Buffer::Desc::Default(bufferSize), bufferData, bufferSize);
   }

   void CommandList::UpdateTextureSubresources(Texture2D& texture, u32 firstSubresource, u32 numSubresources,
      D3D12_SUBRESOURCE_DATA* subresourceData) {
      TransitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST);
      FlushResourceBarriers();

      auto destinationResource = texture.GetD3D12Resource();

      UINT64 requiredSize = GetRequiredIntermediateSize(destinationResource, firstSubresource, numSubresources);

      auto intermediateResource = Buffer::Create(
         Buffer::Desc::Upload(requiredSize).InitialState(D3D12_RESOURCE_STATE_GENERIC_READ));

      UpdateSubresources(commandList.Get(), destinationResource,
                         intermediateResource->GetD3D12Resource(), 0,
                         firstSubresource, numSubresources, subresourceData);

      TrackResource(*intermediateResource);
      TrackResource(destinationResource);
   }

   void CommandList::ClearRenderTarget(Texture2D& texture, const vec4& clearColor) {
      TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, true);
      commandList->ClearRenderTargetView(texture.GetRTV(), &clearColor.x, 0, nullptr);

      TrackResource(texture);
   }

   void CommandList::ClearDepthStencil(Texture2D& texture, float depth, D3D12_CLEAR_FLAGS clearFlags, uint8_t stencil) {
      TransitionBarrier(texture, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, true);
      commandList->ClearDepthStencilView(texture.GetDSVReadWrite(), clearFlags, depth, stencil, 0, nullptr);

      TrackResource(texture);
   }

   void CommandList::ClearUAVFloat(const UnorderedAccessView& uav, const vec4& v) {
      TransitionBarrier(*uav.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      FlushResourceBarriers();
      TrackResource(*uav.resource);

      D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = uav.cpuHandle;
      D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor =
         m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->CopyDescriptor(*this, cpuDescriptor);
      commandList->ClearUnorderedAccessViewFloat(gpuDescriptor, cpuDescriptor, uav.resource->GetD3D12Resource(), &v.x, 0,
         nullptr);
   }

   void CommandList::ClearUAVUint(const UnorderedAccessView& uav, const uint4& v) {
      TransitionBarrier(*uav.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      FlushResourceBarriers();
      TrackResource(*uav.resource);

      D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = uav.cpuHandle;
      D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor =
         m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->CopyDescriptor(*this, cpuDescriptor);
      commandList->ClearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, uav.resource->GetD3D12Resource(), &v.x, 0,
                                                nullptr);
   }

   void CommandList::SetGraphics32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants,
      const void* constants) {
      commandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
   }

   void CommandList::SetCompute32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants,
      const void* constants) {
      commandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
   }

   void CommandList::SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, size_t sizeInBytes,
      const void* bufferData) {
      auto heapAllococation = m_UploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
      memcpy(heapAllococation.CPU, bufferData, sizeInBytes);
      commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, heapAllococation.GPU);
   }

   void CommandList::SetVertexBuffer(u32 slot, const Buffer& buffer, u32 vertexSizeInBytes) {
      TransitionBarrier(buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
      TrackResource(buffer);

      auto view = buffer.GetVertexView(vertexSizeInBytes);
      commandList->IASetVertexBuffers(slot, 1, &view);
   }

   void CommandList::SetDynamicVertexBuffer(uint32_t slot, size_t numVertices, size_t vertexSize,
                                            const void* vertexBufferData) {
      size_t bufferSize = numVertices * vertexSize;

      auto heapAllocation = m_UploadBuffer->Allocate(bufferSize, vertexSize);
      memcpy(heapAllocation.CPU, vertexBufferData, bufferSize);

      D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
      vertexBufferView.BufferLocation = heapAllocation.GPU;
      vertexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
      vertexBufferView.StrideInBytes = static_cast<UINT>(vertexSize);

      commandList->IASetVertexBuffers(slot, 1, &vertexBufferView);
   }

   void CommandList::SetIndexBuffer(const Buffer& buffer, Format format) {
      TransitionBarrier(buffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
      TrackResource(buffer);

      auto view = buffer.GetIndexView(format);
      commandList->IASetIndexBuffer(&view);
   }

   void CommandList::SetDynamicIndexBuffer(size_t numIndices, DXGI_FORMAT indexFormat, const void* indexBufferData) {
      size_t indexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
      size_t bufferSize = numIndices * indexSizeInBytes;

      auto heapAllocation = m_UploadBuffer->Allocate(bufferSize, indexSizeInBytes);
      memcpy(heapAllocation.CPU, indexBufferData, bufferSize);

      D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
      indexBufferView.BufferLocation = heapAllocation.GPU;
      indexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
      indexBufferView.Format = indexFormat;

      commandList->IASetIndexBuffer(&indexBufferView);
   }

   void CommandList::SetConstantBufferView(uint32_t rootParameterIndex, const Buffer* buffer, size_t bufferOffset) {
      SetCBV_SRV_UAV(rootParameterIndex, buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, bufferOffset);
   }

   void CommandList::SetShaderResourceView(uint32_t rootParameterIndex, const Buffer* buffer, size_t bufferOffset) {
      SetCBV_SRV_UAV(rootParameterIndex, buffer, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, bufferOffset);
   }

   void CommandList::SetUnorderedAccessView(uint32_t rootParameterIndex, const Buffer* buffer, size_t bufferOffset) {
      SetCBV_SRV_UAV(rootParameterIndex, buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, bufferOffset);
   }

   void CommandList::SetShaderResourceView(uint32_t rootParameterIndex, uint32_t descriptorOffset,
      const GpuResource* resource) {
      if (resource) {
         // todo:
         if (!resource->GetResourceDesc().IsAccelerationStructure()) {
            TransitionBarrier(*resource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
         }
         TrackResource(*resource);

         m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
            rootParameterIndex, descriptorOffset, 1, resource->GetSRV());
      }
   }

   void CommandList::SetUnorderedAccessView(uint32_t rootParameterIndex, uint32_t descriptorOffset,
      GpuResource* resource) {
      if (resource) {
         TransitionBarrier(*resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
         TrackResource(*resource);

         m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
            rootParameterIndex, descriptorOffset, 1, resource->GetUAV().cpuHandle);
      }
   }

   void CommandList::SetSampler(u32 rootParameterIndex, u32 descriptorOffset,
      const DescriptorAllocation& samplerDescriptor) {
      m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->StageDescriptors(
         rootParameterIndex, descriptorOffset, 1, samplerDescriptor.GetDescriptorHandle());
   }

   void CommandList::SetViewport(vec2 top, vec2 size, vec2 minMaxDepth) {
      D3D12_VIEWPORT viewport = {
         top.x, top.y, size.x, size.y, minMaxDepth.x, minMaxDepth.y
      };

      commandList->RSSetViewports(1, &viewport);
   }

   void CommandList::SetScissorRect(int2 leftTop, int2 rightBottom) {
      D3D12_RECT scissorRect = {
         leftTop.x, leftTop.y, rightBottom.x, rightBottom.y
      };

      commandList->RSSetScissorRects(1, &scissorRect);
   }

   void CommandList::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType) {
      if (psoDescStream.PrimitiveTopologyType != primitiveTopologyType) {
         MarkPSODirty(false);
         psoDescStream.PrimitiveTopologyType = primitiveTopologyType;
      }
   }

   void CommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology) {
      commandList->IASetPrimitiveTopology(primitiveTopology);

      D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;

      switch (primitiveTopology) {
      case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
         break;
      case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
         primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
         break;
      case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
      case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
      case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
      case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
         primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
         break;
      case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
      case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
      case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
      case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
         primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
         break;
      case D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST:
      case D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST:
         primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
         break;
      default: UNIMPLEMENTED();
      }

      SetPrimitiveTopologyType(primitiveTopologyType);
   }

   void CommandList::SetRootSignature(RootSignature* signature) {
      if (rootSignature == signature) {
         return;
      }

      MarkPSODirty(false);
      MarkPSODirty(true);
      rootSignature = signature;

      if (rootSignature) {
         for (auto& heap : m_DynamicDescriptorHeap) {
            heap->ParseRootSignature(rootSignature);
         }

         auto d3d12RootSignature = rootSignature->GetD3D12RootSignature().Get();
         commandList->SetGraphicsRootSignature(d3d12RootSignature);
         commandList->SetComputeRootSignature(d3d12RootSignature);

         psoDescStream.pRootSignature = d3d12RootSignature;

         TrackResource(d3d12RootSignature);
      }
   }

   bool CommandList::SetProgram(GpuProgram* pProgram) {
      // todo: compare with current program

      MarkPSODirty(false);
      MarkPSODirty(true);

      D3D12_SHADER_BYTECODE NullByteCode{nullptr, 0};

      psoDescStream.AS = NullByteCode;
      psoDescStream.MS = NullByteCode;
      psoDescStream.VS = NullByteCode;
      psoDescStream.HS = NullByteCode;
      psoDescStream.DS = NullByteCode;
      psoDescStream.GS = NullByteCode;
      psoDescStream.PS = NullByteCode;

      psoDescStream.CS = NullByteCode;

      if (!pProgram || !pProgram->Valid()) {
         return false;
      }

      if (pProgram->IsCompute()) {
         SetInputLayout();
         psoDescStream.CS = pProgram->cs ? pProgram->cs->GetShaderByteCode() : NullByteCode;
      } else {
         if (pProgram->IsMeshShader()) {
            SetInputLayout();
            psoDescStream.AS = pProgram->as ? pProgram->as->GetShaderByteCode() : NullByteCode;
            psoDescStream.MS = pProgram->ms ? pProgram->ms->GetShaderByteCode() : NullByteCode;
         } else {
            psoDescStream.VS = pProgram->vs ? pProgram->vs->GetShaderByteCode() : NullByteCode;
            psoDescStream.HS = pProgram->hs ? pProgram->hs->GetShaderByteCode() : NullByteCode;
            psoDescStream.DS = pProgram->ds ? pProgram->ds->GetShaderByteCode() : NullByteCode;
            psoDescStream.GS = pProgram->gs ? pProgram->gs->GetShaderByteCode() : NullByteCode;
         }

         psoDescStream.PS = pProgram->ps ? pProgram->ps->GetShaderByteCode() : NullByteCode;
      }

      return true;
   }

   bool CommandList::SetComputeProgram(GpuProgram* pCompute) {
      return SetProgram(pCompute);
   }

   bool CommandList::SetGraphicsProgram(GpuProgram* pGraphics) {
      return SetProgram(pGraphics);
   }

   void CommandList::BeginEvent(std::string_view name) {
      // todo: cant be formatted
      // todo: color
      PIXBeginEvent(commandList.Get(), PIX_COLOR_INDEX((BYTE)15), name.data());
   }

   void CommandList::EndEvent() {
      PIXEndEvent(commandList.Get());
   }

   void CommandList::BeginTimeQuery(Ref<GpuTimer>& timer) {
      ASSERT(timer->IsReady());

      u32 nextIdx = (u32)GpuTimers.size() * 2;
      timer->isReady = false;
      timer->beginIdx = nextIdx;
      commandList->EndQuery(ownerCommandQueue->GetTimeQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, timer->beginIdx + 0);

      GpuTimers.push_back(timer);
   }

   void CommandList::EndTimeQuery(Ref<GpuTimer>& timer) {
      ASSERT(!timer->IsReady());
      commandList->EndQuery(ownerCommandQueue->GetTimeQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, timer->beginIdx + 1);
   }

   void CommandList::ResolveTimeQueryData() {
      u32 timeSlots = (u32)GpuTimers.size() * 2;
      if (timeSlots > 0) {
         commandList->ResolveQueryData(ownerCommandQueue->GetTimeQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 0,
            timeSlots, GpuTimeQueryBuffer->GetD3D12Resource(), 0);
      }
   }

   void CommandList::ReleaseResources() {
      m_UploadBuffer->Reset();

      ReleaseTrackedObjects();

      for (auto& heap : m_DynamicDescriptorHeap) {
         heap->Reset();
      }

      for (auto& heap : m_DescriptorHeaps) {
         heap = nullptr;
      }

      {
         u64* pData = (u64*)GpuTimeQueryBuffer->Map();

         u64 gpuFreq = ownerCommandQueue->GetTimestampFrequency();

         for (int i = 0; i < GpuTimers.size(); ++i) {
            auto timer = GpuTimers[i];
            timer->start = pData[i * 2 + 0];
            timer->stop = pData[i * 2 + 1];
            timer->timeMs = float(double(timer->stop - timer->start) / gpuFreq) * 1000.f;
            timer->isReady = true;
         }

         GpuTimeQueryBuffer->Unmap();

         GpuTimers.clear();
      }

      rootSignature = nullptr;
      pipelineState = nullptr;
   }

   void CommandList::Reset() {
      ReleaseResources();

      ThrowIfFailed(commandAllocator->Reset());
      ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

      MemsetZero(psoDescStream.Flags.Get());
      MemsetZero(psoDescStream.NodeMask.Get());
      MemsetZero(psoDescStream.pRootSignature.Get());
      MemsetZero(psoDescStream.InputLayout.Get());
      MemsetZero(psoDescStream.IBStripCutValue.Get());
      MemsetZero(psoDescStream.PrimitiveTopologyType.Get());

      MemsetZero(psoDescStream.VS.Get());
      MemsetZero(psoDescStream.GS.Get());
      MemsetZero(psoDescStream.StreamOutput.Get());
      MemsetZero(psoDescStream.HS.Get());
      MemsetZero(psoDescStream.DS.Get());
      MemsetZero(psoDescStream.PS.Get());
      MemsetZero(psoDescStream.AS.Get());
      MemsetZero(psoDescStream.MS.Get());
      MemsetZero(psoDescStream.CS.Get());

      MemsetZero(psoDescStream.BlendState.Get());
      MemsetZero(psoDescStream.DepthStencilState.Get());
      MemsetZero(psoDescStream.DSVFormat.Get());
      MemsetZero(psoDescStream.RasterizerState.Get());
      MemsetZero(psoDescStream.RTVFormats.Get());
      MemsetZero(psoDescStream.SampleDesc.Get());
      MemsetZero(psoDescStream.SampleMask.Get());
      MemsetZero(psoDescStream.CachedPSO.Get());
      MemsetZero(psoDescStream.ViewInstancingDesc.Get());

      auto& device = *sDevice;

      // must be called before `SetRootSignature`
      for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1; ++i) {
         SetDescriptorHeap((D3D12_DESCRIPTOR_HEAP_TYPE)i
            , device.GetGlobalDescriptorHeap((D3D12_DESCRIPTOR_HEAP_TYPE)i)->GetD3DHeap());
      }

      SetRootSignature(rendres::pDefaultRootSignature);
      SetScissorRect();
   }

   void CommandList::SetCBV_SRV_UAV(uint32_t rootParameterIndex, const Buffer* buffer, D3D12_RESOURCE_STATES stateAfter,
      size_t bufferOffset) {
      if (buffer) {
         TransitionBarrier(*buffer, stateAfter);
         TrackResource(*buffer);

         m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageInlineCBV(
            rootParameterIndex, buffer->GetD3D12Resource()->GetGPUVirtualAddress() + bufferOffset);
      }
   }

   void CommandList::CommitBeforeDraw() {
      FlushResourceBarriers();
      UpdatePipelineState(false);

      for (auto& heap : m_DynamicDescriptorHeap) {
         heap->CommitStagedDescriptorsForDraw(*this);
      }
   }

   void CommandList::CommitBeforeDispatch() {
      FlushResourceBarriers();
      UpdatePipelineState(true);

      for (auto& heap : m_DynamicDescriptorHeap) {
         // CommitStagedDescriptorsForDraw commits for dispatch too
         heap->CommitStagedDescriptorsForDraw(*this);
         // m_DynamicDescriptorHeap[i]->CommitStagedDescriptorsForDispatch(*this);
      }
   }

   OffsetedBuffer CommandList::AllocDynConstantBuffer(const void* bufferData, u32 sizeInBytes) {
      auto heapAllococation = m_UploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
      memcpy(heapAllococation.CPU, bufferData, sizeInBytes);

      return OffsetedBuffer{ heapAllococation.uploadBuffer, (u32)heapAllococation.uploadBufferOffset };
   }
}
