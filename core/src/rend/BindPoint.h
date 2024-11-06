#pragma once
#include "math/Types.h"
#include "core/Core.h"

namespace pbe {

   struct BindPoint {
      u32 slot = UINT32_MAX;
      u32 space = 0;

      operator bool() const { return slot != UINT32_MAX; }
   };

   enum class BindType {
      CBV,
      SRV,
      UAV,
      Sampler,
   };

   struct CmdBindPoint {
      bool isRoot = false;
      u32 rootParameterIndex = -1;
      u32 descriptorOffset = -1;
   };

}
