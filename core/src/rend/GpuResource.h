#pragma once

#include "core/Core.h"
#include "core/Common.h"
#include "Common.h"
#include "DescriptorAllocation.h"
#include "ResourceView.h"
#include "core/Ref.h"

namespace pbe {
   // class CORE_API GpuHeap : public RefCounted {
   // public:
   //
   //    struct Desc {
   //       D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
   //       u64 size = 0;
   //       u64 alignment = 0;
   //
   //       std::string name;
   //    };
   //
   //    GpuHeap() {
   //       auto heapProps = CD3DX12_HEAP_PROPERTIES(desc.heapType);
   //
   //       D3D12_HEAP_DESC heapDesc = {};
   //       heapDesc.SizeInBytes = desc.size;
   //       heapDesc.Alignment = desc.alignment;
   //       heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
   //       heapDesc.Properties = heapProps;
   //
   //       ThrowIfFailed(sDevice->g_Device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)));
   //    }
   //
   //    Desc desc;
   //    ComPtr<ID3D12Heap> heap;
   // };

   class CORE_API GpuResource : public RefCounted {
      NON_COPYABLE(GpuResource);
   public:

      struct ResourceDesc {
         D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
         bool allowUAV = false;
         u64 alignment = 0;
         D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

         std::string name;

         bool IsAccelerationStructure() const {
            return initialState == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
         }
      };

      template <typename ActualDesc>
      struct ResourceDescBuilder {
         ActualDesc InReadback() {
            CastToDesc()->heapType = D3D12_HEAP_TYPE_READBACK;
            CastToDesc()->initialState = D3D12_RESOURCE_STATE_COPY_DEST;
            return *CastToDesc();
         }

         ActualDesc AllowUAV(bool allowUAV = true) {
            CastToDesc()->allowUAV = allowUAV;
            return *CastToDesc();
         }

         ActualDesc Alignment(u32 alignment) {
            CastToDesc()->alignment = alignment;
            return *CastToDesc();
         }

         ActualDesc InitialState(D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON) {
            CastToDesc()->initialState = initialState;
            return *CastToDesc();
         }

         ActualDesc Name(std::string_view name) {
            CastToDesc()->name = name;
            return *CastToDesc();
         }
      private:
         ActualDesc* CastToDesc() { return (ActualDesc*)this; }
      };

      GpuResource(ComPtr<ID3D12Resource> pRes);
      virtual ~GpuResource();

      ID3D12Resource* GetD3D12Resource() const {
         return pResource.Get();
      }

      virtual const ResourceDesc& GetResourceDesc() const = 0;
      void SetName(std::string_view name);

      D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return pResource->GetGPUVirtualAddress(); }

      ComPtr<ID3D12Resource> pResource;
      mutable D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

      D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const {
         return m_ShaderResourceView.GetDescriptorHandle();
      }

      UnorderedAccessView GetUAV(uint32_t mip = 0) {
         return UnorderedAccessView{
            .resource = this,
            .cpuHandle = m_UnorderedAccessView.GetDescriptorHandle(mip),
         };
      }

   protected:
      DescriptorAllocation m_ShaderResourceView;
      DescriptorAllocation m_UnorderedAccessView;

      virtual ResourceDesc& GetResourceDesc() = 0;
   };
}
