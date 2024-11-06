#pragma once
#include "Common.h"
#include "core/Common.h"
#include "core/Core.h"

namespace pbe {
   class GlobalDescriptorHeap;

   struct GpuDescriptorAllocation {
      ONLY_MOVE(GpuDescriptorAllocation);

      GpuDescriptorAllocation() = default;
      GpuDescriptorAllocation(u32 offset, u32 count, GlobalDescriptorHeap* owner);
      ~GpuDescriptorAllocation();

      u32 offset = UINT_MAX;
      u32 count = 0;

      D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const;
      bool IsValid() const;

   private:
      GlobalDescriptorHeap* owner = nullptr;
   };

   class GlobalDescriptorHeap {
   public:
      GlobalDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numGpuDescriptors, u32 numDynamicDescriptors);

      GpuDescriptorAllocation AllocateGpuDescriptors(u32 numDescriptors);
      void FreeGpuDescriptors(GpuDescriptorAllocation& handle);

      u32 GetNumGpuDescriptors() const;
      u32 GetNumDynamicDescriptors() const;

      std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> GetDynamicDescriptors(u32 numDescriptors);

      D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(u32 offset) const;

      ID3D12DescriptorHeap* GetD3DHeap() const { return descriptorHeap.Get(); }

      u32 GetTotalDescriptors() const;

   private:
      ComPtr<ID3D12DescriptorHeap> descriptorHeap;
      u32 descriptorIncrementSize;

      u32 numGpuDescriptors;
      u32 numDynamicDescriptors;

      // todo:
      u32 gpuNextIndex = 0;

      u32 curDynamicIndex = 0;
   };
}
