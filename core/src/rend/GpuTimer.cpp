#include "pch.h"
#include "GpuTimer.h"

#include "Device.h"
#include "core/Assert.h"


namespace pbe {
   bool GpuTimer::IsReady() const {
      return isReady;
   }

   float GpuTimer::GetTimeMs() const {
      ASSERT(IsReady());
      return timeMs;
   }
}
