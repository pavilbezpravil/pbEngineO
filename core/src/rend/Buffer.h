#pragma once

#include "Common.h"
#include "Format.h"
#include "GlobalDescriptorHeap.h"
#include "GpuResource.h"
#include "core/Assert.h"
#include "math/Types.h"

namespace pbe {
   class Buffer : public GpuResource {
   public:
      struct Desc : ResourceDesc, ResourceDescBuilder<Desc> {
         u64 size = 0;

         u32 structureByteStride = 0;
         u64 numElements = 0;

         Format format = DXGI_FORMAT_UNKNOWN;

         static Desc Default(u64 size) {
            Desc desc{};
            desc.size = size;
            return desc;
         }

         static Desc Upload(u64 size) {
            Desc desc{};
            desc.heapType = D3D12_HEAP_TYPE_UPLOAD;
            desc.size = size;

            return desc;
         }

         static Desc Structured(u64 numElements, u32 structureByteStride) {
            Desc desc{};
            desc.size = structureByteStride * numElements;
            desc.structureByteStride = structureByteStride;
            desc.numElements = numElements;

            return desc;
         }

         static Desc StructuredWithSize(u64 size, u64 numElements, u32 structureByteStride) {
            ASSERT(size >= structureByteStride * numElements);
            Desc desc{};
            desc.size = size;
            desc.structureByteStride = structureByteStride;
            desc.numElements = numElements;

            return desc;
         }

         template <typename ElementType>
         static Desc Structured(u64 numElements) {
            return Structured(numElements, sizeof(ElementType));
         }

         template <typename ElementType>
         static Desc StructuredWithSize(u64 size, u64 numElements) {
            return StructuredWithSize(size, numElements, sizeof(ElementType));
         }

         static Desc Structured(std::string_view name, u64 count, u32 structureSizeInByte) {
            Desc desc{};
            desc.size = structureSizeInByte * count;
            desc.structureByteStride = structureSizeInByte;
            desc.numElements = count;
            desc.name = name;

            return desc;
         }

         static Desc IndirectArgs(std::string_view name, u64 size) {
            Desc desc{};
            desc.size = size;
            desc.allowUAV = true;
            desc.name = name;

            return desc;
         }

         static Desc AccelerationStructure(u64 size) {
            Desc desc{};
            desc.size = size;
            desc.initialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            desc.allowUAV = true;
            return desc;
         }

         Desc Format(Format format) {
            this->format = format;
            return *this;
         }
      };

      static Ref<Buffer> Create(const Desc& desc, void* data = nullptr);

      Desc GetDesc() const { return desc; }
      const ResourceDesc& GetResourceDesc() const override { return desc; }

      u64 NumElements() const;
      u32 StructureByteStride() const;

      UnorderedAccessView GetClearUAVUint();
      UnorderedAccessView GetClearUAVFloat();

      // todo: range
      void* Map() {
         // todo: check that it is possible to map
         void* pData;
         pResource->Map(0, nullptr, &pData);
         return pData;
      }

      void Unmap() {
         pResource->Unmap(0, nullptr);
      }

      template <typename T>
      void Readback(T& data, u64 offsetInBytes = 0) {
         ASSERT(GetDesc().heapType == D3D12_HEAP_TYPE_READBACK);
         ASSERT(offsetInBytes + sizeof(T) <= GetDesc().size);

         void* bufferData = (u8*)Map() + offsetInBytes;
         memcpy(&data, bufferData, sizeof(T));
         Unmap();
      }

      // todo:
      D3D12_VERTEX_BUFFER_VIEW GetVertexView(u32 vertexSizeInBytes) const {
         D3D12_VERTEX_BUFFER_VIEW view;
         view.BufferLocation = pResource->GetGPUVirtualAddress();
         view.SizeInBytes = (u32)GetDesc().size;
         view.StrideInBytes = vertexSizeInBytes;
         return view;
      }

      D3D12_INDEX_BUFFER_VIEW GetIndexView(Format format) const {
         D3D12_INDEX_BUFFER_VIEW view;
         view.BufferLocation = pResource->GetGPUVirtualAddress();
         view.SizeInBytes = (u32)GetDesc().size;
         view.Format = format;
         return view;
      }

      bool Valid() const { return pResource; }

      const GpuDescriptorAllocation& GetGpuSrv() const { return gpuSrv; }

   private:
      friend Ref<Buffer>;

      Buffer(const Desc& desc, void* data);

      Desc desc;

      GpuDescriptorAllocation gpuSrv;

      DescriptorAllocation clearUintUAV;
      DescriptorAllocation clearFloatUAV;

      ResourceDesc& GetResourceDesc() override { return desc; }
   };
}
