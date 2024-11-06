#pragma once

#include <d3d12.h>

#include <cstdint>
#include <memory>

namespace pbe {
   class DescriptorAllocatorPage;

   class DescriptorAllocation {
   public:
      // Creates a NULL descriptor.
      DescriptorAllocation();

      DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t numHandles, uint32_t descriptorSize,
                           std::shared_ptr<DescriptorAllocatorPage> page);

      // The destructor will automatically free the allocation.
      ~DescriptorAllocation();

      // Copies are not allowed.
      DescriptorAllocation(const DescriptorAllocation&) = delete;
      DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;

      // Move is allowed.
      DescriptorAllocation(DescriptorAllocation&& allocation) noexcept;
      DescriptorAllocation& operator=(DescriptorAllocation&& other) noexcept;

      // Check if this a valid descriptor.
      bool IsNull() const;

      bool IsValid() const {
         return !IsNull();
      }

      // Get a descriptor at a particular offset in the allocation.
      D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(uint32_t offset = 0) const;

      // Get the number of (consecutive) handles for this allocation.
      uint32_t GetNumHandles() const;

      // Get the heap that this allocation came from.
      // (For internal use only).
      std::shared_ptr<DescriptorAllocatorPage> GetDescriptorAllocatorPage() const;

      // Free the descriptor back to the heap it came from.
      void Free();

   private:

      // The base descriptor.
      D3D12_CPU_DESCRIPTOR_HANDLE m_Descriptor{0};
      // The number of descriptors in this allocation.
      uint32_t m_NumHandles = 0;
      // The offset to the next descriptor.
      uint32_t m_DescriptorSize = 0;

      // A pointer back to the original page where this allocation came from.
      std::shared_ptr<DescriptorAllocatorPage> m_Page;
   };
}
