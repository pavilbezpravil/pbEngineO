#pragma once

#include "Buffer.h"
#include "Device.h"
#include "Texture2D.h"
#include "core/Core.h"
#include "BindPoint.h"
#include "RootSignature.h"
#include "core/Assert.h"
#include "PipelineStateObject.h"

// todo;
#include <shared/IndirectArgs.hlsli>

#include "utils/Memory.h"

namespace pbe {
   struct UnorderedAccessView;
   class GpuTimer;
   class PipelineStateObject;
   class DynamicDescriptorHeap;
   class UploadBuffer;
   class GpuProgram;

   struct OffsetedBuffer {
      Buffer* buffer{}; // todo: ref
      u32 offset = 0;
   };

   class CORE_API CommandList : public RefCounted {
      NON_COPYABLE(CommandList);
   public:
      CommandList(CommandQueue* ownerCommandQueue);
      ~CommandList();

      void CopyResource(GpuResource& dst, const GpuResource& src);
      void CopyBufferRegion(Buffer& dst, u64 dstOffset, const Buffer& src, u64 srcOffset, u64 sizeInBytes);

      void UpdateBuffer(Buffer& buffer, u32 bufferOffsetInBytes, const void* data, size_t dataSizeInBytes);
      template <typename T>
      void UpdateBuffer(Buffer& buffer, u32 bufferOffsetInBytes, const T& data) {
         UpdateBuffer(buffer, bufferOffsetInBytes, &data, sizeof(T));
      }
      template <typename T>
      void UpdateBuffer(Buffer& buffer, u32 bufferOffsetInBytes, const DataView<T>& data) {
         UpdateBuffer(buffer, bufferOffsetInBytes, data.data(), data.size() * sizeof(T));
      }

      Ref<Buffer> CreateBuffer(const Buffer::Desc& desc, const void* bufferData, u64 bufferSize);
      Ref<Buffer> CreateBuffer(const void* bufferData, u64 bufferSize); // todo: remove

      template <typename T>
      Ref<Buffer> CreateBuffer(const Buffer::Desc& desc, const DataView<T>& data) {
         return CreateBuffer(desc, data.data(), data.size() * sizeof(T));
      }

      void UpdateTextureSubresources(Texture2D& texture, u32 firstSubresource,
         u32 numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData);

      void ClearRenderTarget(Texture2D& texture, const vec4& clearColor);
      void ClearDepthStencil(Texture2D& texture, float depth = 1, D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH,
                             uint8_t stencil = 0);

      void ClearUAVFloat(const UnorderedAccessView& uav, const vec4& v = vec4_Zero);
      void ClearUAVUint(const UnorderedAccessView& uav, const uint4& v = uint4{0});

      void SetGraphics32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants);
      template <typename T>
      void SetGraphics32BitConstants(uint32_t rootParameterIndex, const T& constants) {
         static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
         SetGraphics32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32_t), &constants);
      }

      void SetCompute32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants);
      template <typename T>
      void SetCompute32BitConstants(uint32_t rootParameterIndex, const T& constants) {
         static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
         SetCompute32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32_t), &constants);
      }

      // todo: only graphics
      void SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, size_t sizeInBytes, const void* bufferData);
      template <typename T>
      void SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, const T& data) {
         SetGraphicsDynamicConstantBuffer(rootParameterIndex, sizeof(T), &data);
      }

      void SetVertexBuffer(u32 slot, const Buffer& buffer, u32 vertexSizeInBytes);
      void SetDynamicVertexBuffer(uint32_t slot, size_t numVertices, size_t vertexSize,
                                  const void* vertexBufferData);

      void SetIndexBuffer(const Buffer& buffer, Format format);
      void SetDynamicIndexBuffer(size_t numIndices, DXGI_FORMAT indexFormat, const void* indexBufferData);

      void SetConstantBufferView(uint32_t rootParameterIndex, const Buffer* buffer, size_t bufferOffset = 0);

      // todo: remove?
      void SetShaderResourceView(uint32_t rootParameterIndex, const Buffer* buffer, size_t bufferOffset = 0);
      void SetUnorderedAccessView(uint32_t rootParameterIndex, const Buffer* buffer, size_t bufferOffset = 0);

      void SetShaderResourceView(uint32_t rootParameterIndex, uint32_t descriptorOffset, const GpuResource* resource);
      void SetUnorderedAccessView(uint32_t rootParameterIndex, uint32_t descriptorOffset, GpuResource* resource);

      void SetSampler(u32 rootParameterIndex, u32 descriptorOffset, const DescriptorAllocation& samplerDescriptor);

      void SetViewport(vec2 top, vec2 size, vec2 minMaxDepth = {0, 1});
      void SetScissorRect(int2 leftTop = {0, 0}, int2 rightBottom = { INT_MAX, INT_MAX });

      void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType);
      void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      void SetRootSignature(RootSignature* signature);

      bool SetProgram(GpuProgram* pProgram);
      bool SetComputeProgram(GpuProgram* pCompute);
      bool SetGraphicsProgram(GpuProgram* pGraphics);

      CmdBindPoint GetCmdBindPoint(BindType bindType, const BindPoint& bindPoint) {
         ASSERT(rootSignature);
         return rootSignature->GetCmdBindPoint(bindType, bindPoint);
      }

      void SetCB(const BindPoint& bind, Buffer* buffer, u32 offsetInBytes = 0) {
         if (!bind) {
            return;
         }

         auto cmdBindPoint = GetCmdBindPoint(BindType::CBV, bind);
         ASSERT(cmdBindPoint.isRoot);
         SetConstantBufferView(cmdBindPoint.rootParameterIndex, buffer, offsetInBytes);
      }

      void AllocAndSetCB(const BindPoint& bind, const void* data, u32 dataSize) {
         auto dynCB = AllocDynConstantBuffer(data, dataSize);
         SetCB(bind, dynCB.buffer, dynCB.offset);
      }

      // todo: rename
      template <typename T>
      void AllocAndSetCB(const BindPoint& bind, const T& data) {
         AllocAndSetCB(bind, (const void*)&data, sizeof(T));
      }

      void SetSRV(const BindPoint& bind, const GpuResource* resource = nullptr) {
         if (!bind || !resource) {
            return;
         }

         // todo: this resource may be already set

         auto cmdBindPoint = GetCmdBindPoint(BindType::SRV, bind);
         ASSERT(!cmdBindPoint.isRoot);
         SetShaderResourceView(cmdBindPoint.rootParameterIndex, cmdBindPoint.descriptorOffset, resource);
      }

      void SetUAV(const BindPoint& bind, GpuResource* resource = nullptr) {
         if (!bind || !resource) {
            return;
         }

         auto cmdBindPoint = GetCmdBindPoint(BindType::UAV, bind);
         ASSERT(!cmdBindPoint.isRoot);
         SetUnorderedAccessView(cmdBindPoint.rootParameterIndex, cmdBindPoint.descriptorOffset, resource);
      }

      void SetSampler(const BindPoint& bind, const DescriptorAllocation& samplerDescriptor) {
         if (!bind) {
            return;
         }

         auto cmdBindPoint = GetCmdBindPoint(BindType::Sampler, bind);
         ASSERT(!cmdBindPoint.isRoot);
         SetSampler(cmdBindPoint.rootParameterIndex, cmdBindPoint.descriptorOffset, samplerDescriptor);
      }

      // tdoo:
      void Dispatch3D(int3 groups) {
         Dispatch_(groups.x, groups.y, groups.z);
      }

      void Dispatch3D(int3 size, int3 groupSize) {
         auto groups = glm::ceil(vec3{size} / vec3{groupSize});
         Dispatch3D(int3{groups});
      }

      void Dispatch2D(int2 groups) {
         Dispatch3D(int3{groups, 1});
      }

      void Dispatch2D(int2 size, int2 groupSize) {
         auto groups = glm::ceil(vec2{size} / vec2{groupSize});
         Dispatch2D(groups);
      }

      void Dispatch1D(u32 size) {
         Dispatch3D(int3{size, 1, 1});
      }

      void DrawInstanced(u32 vertexCount, u32 instanceCount, u32 startVertex = 0, u32 startInstance = 0) {
         CommitBeforeDraw();
         commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
      }

      void DrawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 startIndex = 0, u32 baseVertex = 0,
                                u32 startInstance = 0) {
         CommitBeforeDraw();
         commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
      }

      void DrawIndexedInstancedIndirect(Buffer& args, u32 offset) {
         UNIMPLEMENTED();
         // CommitBeforeDraw();
         //
         // // todo:
         // ID3D12CommandSignature* pCommandSignature;
         // UINT MaxCommandCount;
         // ID3D12Resource* pArgumentBuffer;
         // UINT64 ArgumentBufferOffset;
         // ID3D12Resource* pCountBuffer;
         // UINT64 CountBufferOffset;
         //
         // commandList->ExecuteIndirect(pCommandSignature, MaxCommandCount,
         //                              pArgumentBuffer, ArgumentBufferOffset,
         //                              pCountBuffer, CountBufferOffset);
      }

      void Dispatch_(u32 numGroupsX, u32 numGroupsY = 1, u32 numGroupsZ = 1) {
         CommitBeforeDispatch();
         commandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
      }

      void DispatchMesh(u32 numGroupsX, u32 numGroupsY = 1, u32 numGroupsZ = 1) {
         CommitBeforeDraw();
         commandList->DispatchMesh(numGroupsX, numGroupsY, numGroupsZ);
      }

      template <typename T>
      OffsetedBuffer AllocDynConstantBuffer(const T& data) {
         return AllocDynConstantBuffer((const void*)&data, sizeof(T));
      }

      void SetBlendState(D3D12_BLEND_DESC blendDesc) {
         MarkPSODirty(false);
         psoDescStream.BlendState = CD3DX12_BLEND_DESC{ blendDesc };
      }

      void SetRasterizerState(D3D12_RASTERIZER_DESC rasterizerState) {
         MarkPSODirty(false);
         psoDescStream.RasterizerState = CD3DX12_RASTERIZER_DESC{ rasterizerState };
      }

      void SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC1 depthStencilState) {
         MarkPSODirty(false);
         psoDescStream.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1{ depthStencilState };
      }

      void SetInputLayout(D3D12_INPUT_LAYOUT_DESC inputLayout = D3D12_INPUT_LAYOUT_DESC{ .NumElements = 0 }) {
         MarkPSODirty(false);
         psoDescStream.InputLayout = inputLayout;
      }

      // todo:
      void SetInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& desc) {
         SetInputLayout(D3D12_INPUT_LAYOUT_DESC{desc.data(), (u32)desc.size()});
      }

      void SetRenderTargets(u32 nRTs, Texture2D** rts = nullptr, Texture2D* depth = nullptr, bool depthWrite = false) {
         MarkPSODirty(false);
         psoDescStream.RTVFormats.Get().NumRenderTargets = nRTs;
         MemsetZero(psoDescStream.RTVFormats.Get().RTFormats);
         psoDescStream.DSVFormat = DXGI_FORMAT_UNKNOWN;
         psoDescStream.SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
         psoDescStream.SampleMask = UINT_MAX;

         ASSERT(nRTs <= 5);
         D3D12_CPU_DESCRIPTOR_HANDLE renderTargetDescriptors[5];

         for (u32 i = 0; i < nRTs; ++i) {
            Texture2D& texture = *rts[i];
            psoDescStream.RTVFormats.Get().RTFormats[i] = texture.GetDesc().format;
            renderTargetDescriptors[i] = texture.GetRTV();

            TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
            TrackResource(texture);
         }

         CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilDescriptor(D3D12_DEFAULT);
         if (depth) {
            depthStencilDescriptor = depthWrite ? depth->GetDSVReadWrite() : depth->GetDSVReadOnly();
            psoDescStream.DSVFormat = GetDSVFormat(depth->GetDesc().format);

            TransitionBarrier(*depth, depthWrite ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_DEPTH_READ);
            TrackResource(*depth);
         }

         D3D12_CPU_DESCRIPTOR_HANDLE* pDSV = depthStencilDescriptor.ptr != 0 ? &depthStencilDescriptor : nullptr;

         commandList->OMSetRenderTargets(nRTs, renderTargetDescriptors, false, pDSV);
      }

      void SetRenderTarget(Texture2D* rt = nullptr, Texture2D* depth = nullptr, bool depthWrite = true) {
         int nRTs = rt ? 1 : 0;
         Texture2D* rts[] = { rt };
         SetRenderTargets(nRTs, rts, depth, depthWrite);
      }

      void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap) {
         if (m_DescriptorHeaps[heapType] != heap) {
            m_DescriptorHeaps[heapType] = heap;
            BindDescriptorHeaps();
         }
      }

      void ResetDescriptorHeaps() {
         BindDescriptorHeaps();
      }

      void ResetRootSignature() {
         auto backup = rootSignature;
         SetRootSignature(nullptr);
         SetRootSignature(backup);
      }

      void TransitionBarrier(const GpuResource& resource, D3D12_RESOURCE_STATES stateAfter,
                             UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flushBarriers = false) {
         if ((resource.state & stateAfter) != 0) {
            return;
         }

         CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource.pResource.Get(),
            resource.state, stateAfter, subresource);

         barriers.push_back(barrier);
         resource.state = stateAfter; // todo: handle subresource state

         if (flushBarriers) {
            FlushResourceBarriers();
         }
      }

      void UAVBarrier(const GpuResource& resource, bool flushBarriers = false) {
         auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.pResource.Get());
         barriers.push_back(barrier);

         if (flushBarriers) {
            FlushResourceBarriers();
         }
      }

      void FlushResourceBarriers() {
         if (!barriers.empty()) {
            commandList->ResourceBarrier((u32)barriers.size(), barriers.data());
            barriers.clear();
         }
      }

      void BeginEvent(std::string_view name);
      void EndEvent();

      void BeginTimeQuery(Ref<GpuTimer>& timer);
      void EndTimeQuery(Ref<GpuTimer>& timer);

      ComPtr<ID3D12GraphicsCommandList6> GetD3D12CommandList() {
         return commandList;
      }

      // todo: move to private. BLAS and TLAS uses it
      void TrackResource(const GpuResource& res) {
         m_TrackedObjects.push_back(res.pResource);
      }

   private:
      friend class CommandQueue;

      CommandQueue* ownerCommandQueue = nullptr;

      ComPtr<ID3D12CommandAllocator> commandAllocator;
      ComPtr<ID3D12GraphicsCommandList6> commandList;

      Ref<RootSignature> rootSignature;
      Ref<PipelineStateObject> pipelineState;

      CD3DX12_PIPELINE_STATE_STREAM2 psoDescStream = {};
      D3DX12_MESH_SHADER_PIPELINE_STATE_DESC msGraphicsPSODesc = {};
      D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPSODesc{};
      D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc{};

      bool psoDirty = false;

      void MarkPSODirty(bool compute) {
         psoDirty = true;
      }

      bool IsPSODirty() const {
         return psoDirty;
      }

      std::unique_ptr<UploadBuffer> m_UploadBuffer;
      std::unique_ptr<DynamicDescriptorHeap> m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1];
      ID3D12DescriptorHeap* m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {nullptr};
      std::vector<D3D12_RESOURCE_BARRIER> barriers;

      // todo: one resource referenced many times
      std::vector<ComPtr<ID3D12Object>> m_TrackedObjects;

      std::vector<Ref<GpuTimer>> GpuTimers;
      Ref<Buffer> GpuTimeQueryBuffer;

      void TrackResource(ID3D12Object* obj) {
         m_TrackedObjects.push_back(obj);
      }

      void ReleaseTrackedObjects() {
         m_TrackedObjects.clear();
      }

      void ResolveTimeQueryData();

      // todo: private
      void ReleaseResources();
      void Reset();

      void Close() {
         FlushResourceBarriers();
         ResolveTimeQueryData();
         ThrowIfFailed(commandList->Close());
      }

      // todo: private
      void BindDescriptorHeaps() {
         UINT numDescriptorHeaps = 0;
         ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

         for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
            ID3D12DescriptorHeap* descriptorHeap = m_DescriptorHeaps[i];
            if (descriptorHeap) {
               descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
            }
         }

         commandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
      }

   private:

      void SetCBV_SRV_UAV(uint32_t rootParameterIndex, const Buffer* buffer,
                          D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                          size_t bufferOffset = 0);

      // todo: private
      void CommitBeforeDraw();

      void CommitBeforeDispatch();

      void UpdatePipelineState(bool compute) {
         if (!IsPSODirty()) {
            return;
         }

         u64 psoHash = PipelineStateObject::PSODescHash(psoDescStream);

         auto nextPipelineState = sDevice->GetPSOFromCache(psoHash);
         if (nextPipelineState != nullptr && pipelineState.Raw() == nextPipelineState) {
            return;
         }

         pipelineState = nextPipelineState;

         if (!pipelineState) {
            pipelineState = Ref<PipelineStateObject>::Create(psoDescStream);
            sDevice->AddPSOToCache(psoHash, pipelineState);
         }

         if (pipelineState) {
            auto d3d12PipelineStateObject = pipelineState->GetD3D12PipelineState().Get();
            commandList->SetPipelineState(d3d12PipelineStateObject);
            TrackResource(d3d12PipelineStateObject);
         }
      }

      // todo: slow
      OffsetedBuffer AllocDynConstantBuffer(const void* bufferData, u32 sizeInBytes);
   };

   // todo: move to separate file
   inline void TryUpdateBuffer(CommandList& cmd, Ref<Buffer>& buffer, const Buffer::Desc& desc, const void* data, size_t dataSizeInBytes) {
      if (data == nullptr || dataSizeInBytes == 0) {
         return;
      }

      if (!buffer || buffer->GetDesc().size < dataSizeInBytes) {
         buffer = Buffer::Create(desc);
      }
      cmd.UpdateBuffer(*buffer, 0, data, dataSizeInBytes);
   }

   template<typename T>
   void TryUpdateBuffer(CommandList& cmd, Ref<Buffer>& buffer, const Buffer::Desc& desc, const DataView<T>& data) {
      TryUpdateBuffer(cmd, buffer, desc, data.data(), data.size() * sizeof(T));
   }

   struct CommandListScope {
      CommandListScope(CommandList& cmd, std::string_view name) : cmd(cmd) {
         cmd.BeginEvent(name);
      }

      ~CommandListScope() {
         cmd.EndEvent();
      }

      CommandList& cmd;
   };

#define COMMAND_LIST_SCOPE(Cmd, Name) CommandListScope scope{ Cmd, Name };
}
