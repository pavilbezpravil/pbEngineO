#include "pch.h"
#include "GlobalDescriptorHeap.h"

#include "d3dx12.h"
#include "Device.h"

namespace pbe {
   GpuDescriptorAllocation::GpuDescriptorAllocation(u32 offset, u32 count, GlobalDescriptorHeap* owner):
      offset(offset),
      count(count),
      owner(owner) {
   }

   GpuDescriptorAllocation::~GpuDescriptorAllocation() {
      if (IsValid()) {
         owner->FreeGpuDescriptors(*this);
      }
   }

   D3D12_CPU_DESCRIPTOR_HANDLE GpuDescriptorAllocation::GetCpuHandle() const {
      ASSERT(IsValid());
      return owner->GetCpuHandle(offset);
   }

   bool GpuDescriptorAllocation::IsValid() const {
      return count != 0;
   }

   GlobalDescriptorHeap::GlobalDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numGpuDescriptors,
                                              u32 numDynamicDescriptors) :
      numGpuDescriptors(numGpuDescriptors), numDynamicDescriptors(numDynamicDescriptors) {
      D3D12_DESCRIPTOR_HEAP_DESC desc = {};
      desc.NumDescriptors = GetTotalDescriptors();
      desc.Type = type;
      desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

      ThrowIfFailed(sDevice->g_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));
      descriptorIncrementSize = sDevice->g_Device->GetDescriptorHandleIncrementSize(type);
   }

   GpuDescriptorAllocation GlobalDescriptorHeap::AllocateGpuDescriptors(u32 numDescriptors) {
      GpuDescriptorAllocation allocation{gpuNextIndex, numDescriptors, this};
      gpuNextIndex += numDescriptors;
      ASSERT(gpuNextIndex <= numGpuDescriptors);
      return allocation;
   }

   void GlobalDescriptorHeap::FreeGpuDescriptors(GpuDescriptorAllocation& handle) {
      // todo: 
   }

   u32 GlobalDescriptorHeap::GetNumGpuDescriptors() const {
      return numGpuDescriptors;
   }

   u32 GlobalDescriptorHeap::GetNumDynamicDescriptors() const {
      return numDynamicDescriptors;
   }

   std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> GlobalDescriptorHeap::GetDynamicDescriptors(
      u32 numDescriptors) {
      if (numDynamicDescriptors - curDynamicIndex < numDescriptors) {
         curDynamicIndex = 0;
      }

      std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> res{
         CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart()
                                       , numGpuDescriptors + curDynamicIndex, descriptorIncrementSize),
         CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart()
                                       , numGpuDescriptors + curDynamicIndex, descriptorIncrementSize)
      };

      curDynamicIndex += numDescriptors;
      ASSERT(curDynamicIndex <= numDynamicDescriptors);

      return res;
   }

   D3D12_CPU_DESCRIPTOR_HANDLE GlobalDescriptorHeap::GetCpuHandle(u32 offset) const {
      return CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart()
                                           , offset, descriptorIncrementSize);
   }

   u32 GlobalDescriptorHeap::GetTotalDescriptors() const {
      return numGpuDescriptors + numDynamicDescriptors;
   }
}
