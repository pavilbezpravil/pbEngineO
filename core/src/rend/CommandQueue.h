#pragma once
#include <queue>

#include "CommandList.h"
#include "Common.h"
#include "core/Common.h"
#include "core/Core.h"
#include "core/Ref.h"

namespace pbe {
   class CommandList;

   class CORE_API CommandQueue {
      NON_COPYABLE(CommandQueue);
   public:
      CommandQueue(D3D12_COMMAND_LIST_TYPE type);
      ~CommandQueue();

      u64 Signal();
      bool IsFenceComplete(u64 fenceValue);
      void WaitForFenceValue(u64 fenceValue);
      void Flush();

      CommandList& GetCommandList();
      u64 Execute(CommandList& commandList);

      u64 ExecuteImmediate(std::function<void(CommandList&)>&& func);

      void BeginEvent(std::string_view name);
      void EndEvent();

      u64 GetTimestampFrequency() const;
      ID3D12QueryHeap* GetTimeQueryHeap() const;

      ID3D12CommandQueue* GetD3DCommandQueue() { return commandQueue.Get(); }
      D3D12_COMMAND_LIST_TYPE GetType() const;

      static const size_t MaxGpuTimers = 256;

   private:
      D3D12_COMMAND_LIST_TYPE type;
      ComPtr<ID3D12CommandQueue> commandQueue;
      ComPtr<ID3D12Fence> fence;
      HANDLE fenceEvent;
      u64 fenceNextValue = 0;

      ComPtr<ID3D12QueryHeap> GpuTimeQueryHeap;

      struct CommandListEntry {
         uint64_t fenceValue;
         Ref<CommandList> commandList;

         // todo: dtor unknown type Ref<CommandList>
         ~CommandListEntry();
      };

      std::queue<CommandListEntry> inflyCommandListQueue;
      std::vector<Ref<CommandList>> availableCommandListQueue;

      void CheckInflyCommandLists();
   };


   struct CommandQueueScope {
      CommandQueueScope(CommandQueue& queue, std::string_view name) : queue(queue) {
         queue.BeginEvent(name);
      }

      ~CommandQueueScope() {
         queue.EndEvent();
      }

      CommandQueue& queue;
   };

#define COMMAND_QUEUE_SCOPE(Queue, Name) CommandQueueScope scope{ Queue, Name };
}
