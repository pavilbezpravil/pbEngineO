#pragma once

#include <d3d12.h>

#include <deque>
#include <memory>

#include "Common.h"
#include "core/Ref.h"
#include "utils/Memory.h"

namespace pbe {
   class Buffer;
   class Device;

   class UploadBuffer {
   public:
      // Use to upload data to the GPU
      struct Allocation {
         void* CPU;
         D3D12_GPU_VIRTUAL_ADDRESS GPU;
         Buffer* uploadBuffer;
         size_t uploadBufferOffset;
      };

      /**
       * @param pageSize The size to use to allocate new pages in GPU memory.
       */
      explicit UploadBuffer(size_t pageSize = 2 * MB);
      virtual ~UploadBuffer();

      /**
       * The maximum size of an allocation is the size of a single page.
       */
      size_t GetPageSize() const {
         return m_PageSize;
      }

      /**
       * Allocate memory in an Upload heap.
       * An allocation must not exceed the size of a page.
       * Use a memcpy or similar method to copy the
       * buffer data to CPU pointer in the Allocation structure returned from
       * this function.
       */
      Allocation Allocate(size_t sizeInBytes, size_t alignment);

      /**
       * Release all allocated pages. This should only be done when the command list
       * is finished executing on the CommandQueue.
       */
      void Reset();

   private:
      // A single page for the allocator.
      struct Page {
         Page(size_t sizeInBytes);
         ~Page();

         // Check to see if the page has room to satisfy the requested
         // allocation.
         bool HasSpace(size_t sizeInBytes, size_t alignment) const;

         // Allocate memory from the page.
         // Throws std::bad_alloc if the the allocation size is larger
         // that the page size or the size of the allocation exceeds the
         // remaining space in the page.
         Allocation Allocate(size_t sizeInBytes, size_t alignment);

         // Reset the page for reuse.
         void Reset();

      private:
         Ref<Buffer> m_d3d12Resource;

         // Base pointer.
         void* m_CPUPtr = nullptr;
         D3D12_GPU_VIRTUAL_ADDRESS m_GPUPtr = {0};

         // Allocated page size.
         size_t m_PageSize ;
         // Current allocation offset in bytes.
         size_t m_Offset = 0;
      };

      // A pool of memory pages.
      using PagePool = std::deque<std::shared_ptr<Page>>;

      // Request a page from the pool of available pages
      // or create a new page if there are no available pages.
      std::shared_ptr<Page> RequestPage();

      PagePool m_PagePool;
      PagePool m_AvailablePages;

      std::shared_ptr<Page> m_CurrentPage;

      // The size of each page of memory.
      size_t m_PageSize;
   };
}
