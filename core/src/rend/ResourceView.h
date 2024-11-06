#pragma once
#include "core/Ref.h"

namespace pbe {
   class GpuResource;

   struct UnorderedAccessView {
      Ref<GpuResource> resource;
      D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {0};
   };
}
