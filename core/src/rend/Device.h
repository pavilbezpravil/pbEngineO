#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <queue>

#include "d3dx12.h"


#include "Common.h"
#include "GlobalDescriptorHeap.h"
#include "core/Common.h"

#include "core/Core.h"
#include "core/Ref.h"
#include "math/Types.h"

namespace pbe {
   class CommandList;
   class Buffer;
   class PipelineStateObject;
   class CommandQueue;
   class DescriptorAllocator;

   class Texture2D;

   // class SwapChain
   // {
   // public:
   //    static constexpr u32 BufferCount = 2;
   //
   //    SwapChain();
   //
   // };

   class DescriptorAllocation;
   class GlobalDescriptorHeap;
   struct GpuDescriptorAllocation;

   class CORE_API Device {
      NON_COPYABLE(Device);
   public:
      Device();
      ~Device();

      GlobalDescriptorHeap* GetGlobalDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
      DescriptorAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors = 1);
      void ReleaseStaleDescriptors();

      GpuDescriptorAllocation AllocateGpuDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors = 1);

      CommandQueue& GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

      void Flush();

      void Resize(int2 size);
      Texture2D& WaitBackBuffer();

      void Present();

      Ref<PipelineStateObject> GetPSOFromCache(u64 psoHash) const;
      void AddPSOToCache(u64 psoHash, Ref<PipelineStateObject> pso);

      // todo: move to private
      ComPtr<ID3D12Device5> g_Device;
      ComPtr<IDXGISwapChain4> g_SwapChain;

      std::unique_ptr<CommandQueue> pCommandQueue;

      static constexpr int BufferCount = 2;
      u64 g_FrameFenceValues[BufferCount] = {0, 0};

      Ref<Texture2D> backBuffer[BufferCount]{};
      bool created = false;

      struct Features {
         bool allowBindless = false;
         bool raytracing = false;
         bool inlineRaytracing = false;
         bool meshShader = false;

         // todo: name it. what does it allow to do?
         D3D_ROOT_SIGNATURE_VERSION highestRootSignatureVersion = D3D_ROOT_SIGNATURE_VERSION_1_1; // todo:
      };

      const Features& GetFeatures() const;

      static void EnableDebugLayer();
      static void ReportLiveObjects();

   private:
      std::unique_ptr<DescriptorAllocator> m_DescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
      std::unique_ptr<GlobalDescriptorHeap> pGlobalDescriptorHeap[2]; // 0 - CBV_SRV_UAV, 1 - SAMPLER
      std::unordered_map<u64, Ref<PipelineStateObject>> psoCache;

      Features features;

      void SetupFeatures();
      void SetupBackbuffer();

      void SetupDebugMsg();
   };

   extern CORE_API Device* sDevice;
   extern CORE_API ID3D12Device2* GetD3D12Device();


}
