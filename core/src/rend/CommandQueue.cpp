#include "pch.h"
#include "CommandQueue.h"
#include "CommandList.h"

// todo: under define
#include "WinPixEventRuntime/pix3.h"

#include "Common.h"

namespace pbe {

   CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type) : type(type) {
      auto device = GetD3D12Device();

      D3D12_COMMAND_QUEUE_DESC desc = {};
      desc.Type = type;
      desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
      desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
      desc.NodeMask = 0;

      ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));
      ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

      fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
      assert(fenceEvent && "Failed to create fence event.");

      D3D12_QUERY_HEAP_DESC queryDesc = {};
      queryDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
      queryDesc.Count = MaxGpuTimers * 2;
      ThrowIfFailed(device->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(GpuTimeQueryHeap.ReleaseAndGetAddressOf())));
      GpuTimeQueryHeap->SetName(L"GPUTimerHeap");
   }

   CommandQueue::~CommandQueue() {
      ASSERT(inflyCommandListQueue.empty());
   }

   u64 CommandQueue::Signal() {
      fenceNextValue++;
      ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceNextValue));

      return fenceNextValue;
   }

   bool CommandQueue::IsFenceComplete(u64 fenceValue) {
      return fence->GetCompletedValue() >= fenceValue;
   }

   void CommandQueue::WaitForFenceValue(u64 fenceValue) {
      if (fence->GetCompletedValue() < fenceValue) {
         std::chrono::milliseconds duration = std::chrono::milliseconds::max();

         ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
         ::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
      }
   }

   void CommandQueue::Flush() {
      WaitForFenceValue(Signal());

      CheckInflyCommandLists();
      ASSERT(inflyCommandListQueue.empty());
   }

   CommandList& CommandQueue::GetCommandList() {
      Ref<CommandList> commandList;

      CheckInflyCommandLists();

      if (!availableCommandListQueue.empty()) {
         commandList = availableCommandListQueue.back();
         availableCommandListQueue.pop_back();
      } else {
         commandList = Ref<CommandList>::Create(this);
      }

      commandList->Reset();

      commandList->IncRefCount(); // todo:
      ASSERT(commandList->GetRefCount() == 2);

      return *commandList;
   }

   u64 CommandQueue::Execute(CommandList& commandList) {
      ASSERT(commandList.GetRefCount() == 1 && "You lose owning of command list when pass it in 'CommandQueue::Execute'");

      commandList.Close();

      ID3D12CommandList* const ppCommandLists[] = {
         commandList.GetD3D12CommandList().Get()
      };

      commandQueue->ExecuteCommandLists(1, ppCommandLists);
      u64 fenceValue = Signal();

      // todo:
      auto refCommandList = &commandList;
      refCommandList->DecRefCount();

      inflyCommandListQueue.push(CommandListEntry{ fenceValue, refCommandList });

      return fenceValue;
   }

   u64 CommandQueue::ExecuteImmediate(std::function<void(CommandList&)>&& func) {
      CommandQueue& commandQueue = sDevice->GetCommandQueue();
      CommandList& cmd = commandQueue.GetCommandList();

      func(cmd);

      return commandQueue.Execute(cmd);
   }

   void CommandQueue::BeginEvent(std::string_view name) {
      // todo: cant be formatted
      // todo: color
      PIXBeginEvent(commandQueue.Get(), PIX_COLOR_INDEX((BYTE)12), name.data());
   }

   void CommandQueue::EndEvent() {
      PIXEndEvent(commandQueue.Get());
   }

   u64 CommandQueue::GetTimestampFrequency() const {
      u64 GpuFreq = 0;
      ThrowIfFailed(commandQueue->GetTimestampFrequency(&GpuFreq));
      return GpuFreq;
   }

   ID3D12QueryHeap* CommandQueue::GetTimeQueryHeap() const {
      return GpuTimeQueryHeap.Get();
   }

   D3D12_COMMAND_LIST_TYPE CommandQueue::GetType() const {
      return type;
   }

   CommandQueue::CommandListEntry::~CommandListEntry() {
   }

   void CommandQueue::CheckInflyCommandLists() {
      while (!inflyCommandListQueue.empty()) {
         auto [fenceValue, cmd] = inflyCommandListQueue.front();
         if (!IsFenceComplete(fenceValue)) {
            break;
         }

         inflyCommandListQueue.pop();
         cmd->ReleaseResources();

         availableCommandListQueue.push_back(cmd);
      }
   }
}
