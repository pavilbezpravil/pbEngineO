#include "pch.h"
#include "GpuResource.h"
#include "Common.h"

namespace pbe {
   GpuResource::GpuResource(ComPtr<ID3D12Resource> pRes) : pResource(pRes) {
   }

   GpuResource::~GpuResource() {
   }

   void GpuResource::SetName(std::string_view name) {
      GetResourceDesc().name = name;

      if (pResource) {
         pbe::SetDbgName(pResource.Get(), name);
      }
   }
}
