#pragma once

#include <chrono>
#include <deque>

#include "Assert.h"
#include "Common.h"
#include "optick.h"
#include "rend/GpuTimer.h"

// todo:
// #if !defined(RELEASE)
   #define USE_PROFILE
   #define USE_PIX_RETAIL
#include "rend/CommandList.h"
   #include "WinPixEventRuntime/pix3.h"
// #endif


namespace pbe {

   struct CpuTimer {
      void Start() {
         tStart = std::chrono::high_resolution_clock::now();
      }

      float ElapsedMs(bool restart = false) {
         return Elapsed(restart) * 1000.f;
      }

      float Elapsed(bool restart = false) {
         std::chrono::time_point tEnd = std::chrono::high_resolution_clock::now();
         float elapsed = std::chrono::duration<float>(tEnd - tStart).count();

         if (restart) {
            tStart = tEnd;
         }

         return elapsed;
      }

   private:
      std::chrono::time_point<std::chrono::high_resolution_clock> tStart = std::chrono::high_resolution_clock::now();
   };

   class CORE_API Profiler {
      NON_COPYABLE(Profiler);
   public:
      Profiler() = default;

      static void Init();
      static void Term();
      static Profiler& Get();

      struct AverageTime {
         void Add(float time, int maxValues) {
            average += time;
            times.push_back(time);
            while (times.size() > maxValues) {
               average -= times.front();
               times.pop_front();
            }
         }

         float GetCur() const {
            return times.empty() ? 0 : times.back();
         }

         float GetAverage() const {
            return times.empty() ? 0 : average / times.size();
         }

         void Clear() {
            times.clear();
            average = 0;
         }

      private:
         std::deque<float> times;
         float average = 0;
      };

      struct CpuEvent {
         std::string name;
         CpuTimer timer;
         float elapsedMsCur = 0;

         AverageTime averageTime;

         bool usedInFrame = false;

         void Start() {
            ASSERT(usedInFrame == false);
            usedInFrame = true;
            timer.Start();
         }

         void Stop() {
            elapsedMsCur = timer.ElapsedMs();
         }
      };

      struct GpuEvent {
         std::string name;
         Ref<GpuTimer> timers[3];
         int timerIdx = 0;

         AverageTime averageTime;

         bool usedInFrame = false;

         GpuEvent();

         void Start(CommandList& cmd) {
            ASSERT(usedInFrame == false);
            usedInFrame = true;
            cmd.BeginTimeQuery(timers[timerIdx]);
         }

         void Stop(CommandList& cmd) {
            cmd.EndTimeQuery(timers[timerIdx]);
         }
      };

      int historyLength = 30;

      void NextFrame() {
         for (auto& [name, event] : cpuEvents) {
            if (event.usedInFrame) {
               event.usedInFrame = false;

               event.averageTime.Add(event.elapsedMsCur, historyLength);
               event.elapsedMsCur = -1;
            } else {
               event.averageTime.Clear();
            }
         }

         for (auto& [name, event] : gpuEvents) {
            if (event.usedInFrame) {
               event.usedInFrame = false;

               event.timerIdx = (event.timerIdx + 1) % 3;

               ASSERT(event.timers[event.timerIdx]->IsReady());

               if (event.timers[event.timerIdx]->IsReady()) {
                  event.averageTime.Add(event.timers[event.timerIdx]->GetTimeMs(), historyLength);
               }
            } else {
               event.averageTime.Clear();
            }
         }
      }

      CpuEvent& CreateCpuEvent(std::string_view name) {
         if (cpuEvents.find(name) == cpuEvents.end()) {
            cpuEvents[name] = CpuEvent{name.data()};
         }

         CpuEvent& cpuEvent = cpuEvents[name];
         return cpuEvent;
      }

      GpuEvent& CreateGpuEvent(std::string_view name) {
         if (gpuEvents.find(name) == gpuEvents.end()) {
            gpuEvents[name] = GpuEvent{};
            gpuEvents[name].name = name.data();
         }

         GpuEvent& gpuEvent = gpuEvents[name];
         return gpuEvent;
      }

      std::unordered_map<std::string_view, CpuEvent> cpuEvents;
      std::unordered_map<std::string_view, GpuEvent> gpuEvents;
   };

   struct CpuEventGuard {
      CpuEventGuard(Profiler::CpuEvent& cpu_event)
         : cpuEvent(cpu_event) {
         cpuEvent.Start();
      }

      ~CpuEventGuard() {
         cpuEvent.Stop();
      }

      Profiler::CpuEvent& cpuEvent;
   };

   struct GpuEventGuard {
      GpuEventGuard(CommandList& cmd, Profiler::GpuEvent& gpuEvent)
         : cmd(cmd), gpuEvent(gpuEvent) {
         gpuEvent.Start(cmd);
      }

      ~GpuEventGuard() {
         gpuEvent.Stop(cmd);
      }

      CommandList& cmd;
      Profiler::GpuEvent& gpuEvent;
   };

   enum class ProfileEventType : u8 {
      Frame,
      Physics,
      Render,
      UI,
      Window,
   };

#ifdef USE_PROFILE
   // PIXSetMarker(PIX_COLOR_INDEX(17), "Some data");

   #define PIX_EVENT_COLOR(Color, Name) PIXScopedEvent(Color, Name)
   #define PIX_EVENT(Name) PIX_EVENT_COLOR(PIX_COLOR(255, 255, 255), Name)
   #define PIX_EVENT_SYSTEM(System, Name) PIX_EVENT_COLOR(PIX_COLOR_INDEX((BYTE)ProfileEventType::System), Name)

   // todo: move to mb Core.h?
   #define __SCOPED_NAME(Name, line) CONCAT(Name, line)
   #define SCOPED_NAME(Name) __SCOPED_NAME(Name, __LINE__)

   #define PROFILE_CPU(Name) CpuEventGuard SCOPED_NAME(cpuEvent){ Profiler::Get().CreateCpuEvent(Name) }; PIX_EVENT(Name)
   #define __PROFILE_GPU(Cmd, Name) GpuEventGuard SCOPED_NAME(gpuEvent){ Cmd, Profiler::Get().CreateGpuEvent(Name) }
   #define PROFILE_GPU(Name) __PROFILE_GPU(cmd, Name)
#else
   #define PIX_EVENT_COLOR(Color, Name)
   #define PIX_EVENT(Name)
   #define PIX_EVENT_SYSTEM(System, Name)

   #define PROFILE_CPU(Name)
   #define PROFILE_GPU(Name)
#endif

}
