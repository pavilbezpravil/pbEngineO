#pragma once
#include "Common.h"
#include "core/Core.h"
#include "core/Ref.h"


namespace pbe {
   class CommandList;

   class CORE_API GpuTimer : public RefCounted {
   public:
      bool IsReady() const;
      float GetTimeMs() const;
   private:
      friend class CommandList;

      u32 beginIdx = 0;

      u64 start = 0;
      u64 stop = 0;

      bool isReady = true;
      float timeMs = 0;
   };

}
