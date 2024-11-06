#include "pch.h"
#include "Buffer.h"
#include "Device.h"
#include "core/Assert.h"
#include "core/Log.h"

namespace pbe {
   Ref<Buffer> Buffer::Create(const Desc& desc, void* data) {
      return Ref<Buffer>::Create(desc, data);
   }

   u64 Buffer::NumElements() const {
      ASSERT(desc.structureByteStride > 0);
      return desc.numElements;
   }

   u32 Buffer::StructureByteStride() const {
      ASSERT(desc.structureByteStride > 0);
      return desc.structureByteStride;
   }

   UnorderedAccessView Buffer::GetClearUAVUint() {
      if (clearUintUAV.IsNull()) {
         D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
         uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
         uavDesc.Buffer.FirstElement = 0;
         uavDesc.Buffer.NumElements = (u32)(desc.size / sizeof(u32));
         uavDesc.Buffer.StructureByteStride = 0;
         uavDesc.Format = DXGI_FORMAT_R32_UINT;

         clearUintUAV = sDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
         sDevice->g_Device->CreateUnorderedAccessView(pResource.Get(),
            nullptr, &uavDesc, clearUintUAV.GetDescriptorHandle());
      }

      return UnorderedAccessView{
         .resource = this,
         .cpuHandle = clearUintUAV.GetDescriptorHandle(),
      };
   }

   UnorderedAccessView Buffer::GetClearUAVFloat() {
      if (clearFloatUAV.IsNull()) {
         D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
         uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
         uavDesc.Buffer.FirstElement = 0;
         uavDesc.Buffer.NumElements = (u32)(desc.size / sizeof(float));
         uavDesc.Buffer.StructureByteStride = 0;
         uavDesc.Format = DXGI_FORMAT_R32_FLOAT;

         clearFloatUAV = sDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
         sDevice->g_Device->CreateUnorderedAccessView(pResource.Get(),
            nullptr, &uavDesc, clearFloatUAV.GetDescriptorHandle());
      }

      return UnorderedAccessView{
         .resource = this,
         .cpuHandle = clearFloatUAV.GetDescriptorHandle(),
      };
   }

   Buffer::Buffer(const Desc& desc, void* data) : GpuResource(nullptr), desc(desc) {
      ASSERT(!data && "Use CommandList for initalize buffer");

      if (desc.size == 0) {
         ASSERT(false); // todo: should I allow it?
         return;
      }

      auto& device = *sDevice;

      D3D12_RESOURCE_FLAGS resourceFlags = desc.allowUAV
                                              ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                              : D3D12_RESOURCE_FLAG_NONE;

      // todo: same check in texture
      if (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS || resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
         ASSERT(desc.heapType != D3D12_HEAP_TYPE_UPLOAD && desc.heapType != D3D12_HEAP_TYPE_READBACK);
      }

      auto heapProps = CD3DX12_HEAP_PROPERTIES(desc.heapType);
      auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(desc.size, resourceFlags, desc.alignment);

      ThrowIfFailed(device.g_Device->CreateCommittedResource(
         &heapProps,
         D3D12_HEAP_FLAG_NONE,
         &resDesc,
         desc.initialState,
         nullptr,
         IID_PPV_ARGS(&pResource)));

      if (!pResource) {
         WARN("Cant create buffer!");
         return;
      }

      state = desc.initialState;

      SetName(desc.name);

      bool accelerationStructure = desc.IsAccelerationStructure();

      // todo: remove SRV, UAV creation from init. Do it lazily

      if (desc.structureByteStride != 0 || accelerationStructure) {
         // for structured buffer
         D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

         if (accelerationStructure) {
            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
            SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            SRVDesc.RaytracingAccelerationStructure.Location = GetGPUVirtualAddress();
         } else {
            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            SRVDesc.Format = desc.format;
            SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            SRVDesc.Buffer.NumElements = (u32)NumElements();
            SRVDesc.Buffer.StructureByteStride = desc.structureByteStride;
            SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

            // todo:
            // gpuSrv = device.AllocateGpuDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            // device.g_Device->CreateShaderResourceView(pResource.Get(),
            //    &SRVDesc, gpuSrv.GetCpuHandle());
         }

         m_ShaderResourceView = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
         device.g_Device->CreateShaderResourceView(accelerationStructure ? nullptr : pResource.Get(),
            &SRVDesc, m_ShaderResourceView.GetDescriptorHandle());
      }

      if (desc.allowUAV && desc.structureByteStride > 0) {
         D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
         UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
         UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
         UAVDesc.Buffer.CounterOffsetInBytes = 0;
         UAVDesc.Buffer.NumElements = (u32)NumElements();
         UAVDesc.Buffer.StructureByteStride = desc.structureByteStride;
         UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

         m_UnorderedAccessView = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
         device.g_Device->CreateUnorderedAccessView(pResource.Get(),
            nullptr, &UAVDesc, m_UnorderedAccessView.GetDescriptorHandle());
      }
   }
}
