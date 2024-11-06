#include "pch.h"
#include "Profiler.h"

namespace pbe {

   static Profiler* sProfiler = nullptr;

   void Profiler::Init() {
      sProfiler = new Profiler();
   }

   void Profiler::Term() {
      delete sProfiler;
   }

   Profiler& Profiler::Get() {
      return *sProfiler;
   }

   Profiler::GpuEvent::GpuEvent() {
      for (auto& timer : timers) {
         timer = Ref<GpuTimer>::Create();
      }
   }
}
