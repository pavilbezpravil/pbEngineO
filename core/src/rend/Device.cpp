#include "pch.h"
#include "Device.h"

#include "dxgidebug.h"
#include "app/Window.h"
#include "Common.h"
#include "DescriptorAllocator.h"
#include "Texture2D.h"
#include "Buffer.h"
#include "GlobalDescriptorHeap.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include "PipelineStateObject.h"
#include "core/Assert.h"
#include "core/Common.h"
#include "core/CVar.h"
#include "core/Log.h"

namespace pbe {
   /// # TODO
   /// - remove #define PROFILE_GPU as duplicate of COMMAND_LIST_SCOPE
   /// - unify setup D3D12SDKVersion, D3D12SDKPath
   /// - Set graphics and compute descriptors separately
   /// - optimized clear value
   /// - Fix shadow map samplers
   /// - GpuTimers only for development
   /// - Check GlobalDescriptorHeap dynamic range overflow
   /// - Free list on GlobalDescriptorHeap allocation
   /// - Show in UI stats about command lists, descriptors etc
   /// - change vertex buffer to structured
   /// - CommandList shares upload pages
   /// - Use shader cache
   /// - Check perf with RT triangles

   CVarValue<bool> cfgVSyncEnable{"render/vsync", true};
   CVarValue<bool> cfgEnableDebugLayer{"render/enable debug layer", true};

   Device* sDevice = nullptr;

   std::string ToString(std::wstring_view wstr) {
      std::string result;

      int sz = WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), 0, 0, 0, 0);
      result = std::string(sz, 0);
      WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), &result[0], sz, 0, 0);

      return result;
   }

   std::string AdapterToString(IDXGIAdapter1& adapter) {
      DXGI_ADAPTER_DESC1 desc;
      adapter.GetDesc1(&desc);

      auto deviceName = desc.Description;
      auto name = ToString(std::wstring_view{deviceName, wcslen(deviceName)});
      return std::format("{} {} Mb", name, desc.DedicatedVideoMemory / 1024 / 1024);
   }

   Microsoft::WRL::ComPtr<IDXGIAdapter1> CreateDeviceWithMostPowerfulAdapter() {
      Microsoft::WRL::ComPtr<IDXGIFactory1> dxgiFactory;
      HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)dxgiFactory.GetAddressOf());

      if (FAILED(hr)) {
         return {};
      }

      Microsoft::WRL::ComPtr<IDXGIAdapter1> mostPowerfulAdapter{};
      size_t maxDedicatedVideoMemory = 0;

      INFO("Adapter list:");
      for (UINT i = 0; ; ++i) {
         Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
         if (dxgiFactory->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
            break; // No more adapters to enumerate
         }

         DXGI_ADAPTER_DESC1 desc;
         adapter->GetDesc1(&desc);

         if (desc.DedicatedVideoMemory > maxDedicatedVideoMemory) {
            maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
            mostPowerfulAdapter = adapter;
         }

         INFO("\t{}. {}", i, AdapterToString(*adapter.Get()));
      }

      INFO("Use adapter: {}", AdapterToString(*mostPowerfulAdapter.Get()));

      return mostPowerfulAdapter;
   }

   Device::Device() {
      sDevice = this;

      // todo: it is not setting of specific device
      if (cfgEnableDebugLayer) {
         Device::EnableDebugLayer();
      }

      auto adapter = CreateDeviceWithMostPowerfulAdapter();

      // todo: debug info
      ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&g_Device)));
      SetupDebugMsg();
      SetupFeatures();

      INFO("Device support:");
      INFO("\tBindless          {}", features.allowBindless);
      INFO("\tRayTracing        {}", features.raytracing);
      INFO("\tInline RayTracing {}", features.inlineRaytracing);
      INFO("\tMesh shader       {}", features.meshShader);

      if (!features.allowBindless) {
         FATAL("Device does not support resources bindless");
         std::exit(-1); // todo:
      }

      pCommandQueue = std::make_unique<CommandQueue>(D3D12_COMMAND_LIST_TYPE_DIRECT);

      for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
         m_DescriptorAllocators[i] =
            std::make_unique<DescriptorAllocator>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
      }

      pGlobalDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] =
         std::make_unique<GlobalDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024 * 10, 1024 * 50);
      pGlobalDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] =
         std::make_unique<GlobalDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32, 1024);

      ComPtr<IDXGIFactory4> dxgiFactory4;
      UINT createFactoryFlags = 0;
#if defined(_DEBUG)
      createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

      ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

      HWND hWnd = sWindow->hwnd;

      // Client area is less that window size
      uint2 size;// = sWindow->GetDesc().size;

      // Query the windows client width and height.
      RECT windowRect;
      ::GetClientRect(hWnd, &windowRect);

      size.x = windowRect.right - windowRect.left;
      size.y = windowRect.bottom - windowRect.top;

      DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
      swapChainDesc.Width = size.x;
      swapChainDesc.Height = size.y;
      swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      swapChainDesc.Stereo = FALSE;
      swapChainDesc.SampleDesc = {1, 0};
      swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      swapChainDesc.BufferCount = BufferCount;
      swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
      swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
      swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
      // It is recommended to always allow tearing if tearing support is available.
      // swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
      swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

      ComPtr<IDXGISwapChain1> swapChain1;
      ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
         pCommandQueue->GetD3DCommandQueue(),
         hWnd,
         &swapChainDesc,
         nullptr,
         nullptr,
         &swapChain1));

      // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
      // will be handled manually.
      ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
      ThrowIfFailed(swapChain1.As(&g_SwapChain));

      INFO("Device init success");

      SetupBackbuffer();

      created = true;
   }

   Device::~Device() {
      for (auto& backBuffer : backBuffer) {
         backBuffer = nullptr;
      }

      for (auto& heap : pGlobalDescriptorHeap) {
         heap = nullptr;
      }

      g_SwapChain = nullptr;
      pCommandQueue = nullptr;

      Device::ReportLiveObjects();

      g_Device = nullptr;
   }

   GlobalDescriptorHeap* Device::GetGlobalDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) {
      return pGlobalDescriptorHeap[type].get();
   }

   DescriptorAllocation Device::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors) {
      return m_DescriptorAllocators[type]->Allocate(numDescriptors);
   }

   void Device::ReleaseStaleDescriptors() {
      for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
         m_DescriptorAllocators[i]->ReleaseStaleDescriptors();
      }
   }

   GpuDescriptorAllocation Device::AllocateGpuDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors) {
      return pGlobalDescriptorHeap[type]->AllocateGpuDescriptors(numDescriptors);
   }

   CommandQueue& Device::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) {
      ASSERT(type == D3D12_COMMAND_LIST_TYPE_DIRECT);
      return *pCommandQueue;
   }

   void Device::Flush() {
      pCommandQueue->Flush();
   }

   void Device::Resize(int2 size) {
      INFO("SwapChain resize {}", size);

      for (int i = 0; i < BufferCount; ++i) {
         backBuffer[i] = nullptr;
         // g_FrameFenceValues[i] = g_FrameFenceValues[g_CurrentBackBufferIndex];
      }

      Flush();

      DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
      ThrowIfFailed(g_SwapChain->GetDesc(&swapChainDesc));
      ThrowIfFailed(g_SwapChain->ResizeBuffers(BufferCount, size.x, size.y,
         swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

      SetupBackbuffer();
   }

   Texture2D& Device::WaitBackBuffer() {
      int backBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();
      pCommandQueue->WaitForFenceValue(g_FrameFenceValues[backBufferIndex]);
      return *backBuffer[g_SwapChain->GetCurrentBackBufferIndex()];
   }

   void Device::Present() {
      int syncInterval = cfgVSyncEnable ? 1 : 0;
      // todo:
      // UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
      UINT presentFlags = 0;
      ThrowIfFailed(g_SwapChain->Present(syncInterval, presentFlags));

      g_FrameFenceValues[g_SwapChain->GetCurrentBackBufferIndex()] = pCommandQueue->Signal();

      ReleaseStaleDescriptors();
   }

   Ref<PipelineStateObject> Device::GetPSOFromCache(u64 psoHash) const {
      auto iter = psoCache.find(psoHash);
      return iter == psoCache.end() ? Ref<PipelineStateObject>{} : iter->second;
   }

   void Device::AddPSOToCache(u64 psoHash, Ref<PipelineStateObject> pso) {
      if (pso) {
         ASSERT(!psoCache.contains(psoHash));
         psoCache[psoHash] = pso;
      }
   }

   const Device::Features& Device::GetFeatures() const {
      return features;
   }

   void Device::EnableDebugLayer() {
      ComPtr<ID3D12Debug> debugInterface;
      ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
      debugInterface->EnableDebugLayer();
   }

   void Device::ReportLiveObjects() {
      IDXGIDebug1* dxgiDebug;
      DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

      dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
      dxgiDebug->Release();
   }

   void Device::SetupFeatures() {
      D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
      featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
      if (FAILED(g_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
         ASSERT(false);
         features.highestRootSignatureVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
      }

      D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{
         .HighestShaderModel = D3D_SHADER_MODEL_6_6
      };
      ThrowIfFailed(g_Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,
         &shaderModel, sizeof(shaderModel)));
      features.allowBindless = shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6;

      D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
      ThrowIfFailed(g_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
         &options5, sizeof(options5)));
      features.raytracing = options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
      features.inlineRaytracing = options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1;

      D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
      ThrowIfFailed(g_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7,
         &options7, sizeof(options7)));
      features.meshShader = options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
   }

   void Device::SetupBackbuffer() {
      for (UINT i = 0; i < BufferCount; ++i) {
         ComPtr<ID3D12Resource> pBackBuffer;
         ThrowIfFailed(g_SwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));

         backBuffer[i] = Texture2D::Create(pBackBuffer);
         backBuffer[i]->SetName(std::string("back buffer ") + std::to_string(i));
      }
   }

   void Device::SetupDebugMsg() {
      // Enable debug messages (only works if the debug layer has already been enabled).
      ComPtr<ID3D12InfoQueue> pInfoQueue;
      if (SUCCEEDED(g_Device.As(&pInfoQueue))) {
         // pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE );
         pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, TRUE );
         // pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, TRUE );

         // pInfoQueue->SetBreakOnCategory(D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED, true);
         // pInfoQueue->SetBreakOnID(D3D12_MESSAGE_ID_LIVE_DEVICE, false);

         // Suppress whole categories of messages
         // D3D12_MESSAGE_CATEGORY Categories[] = {};

         // Suppress messages based on their severity level
         D3D12_MESSAGE_SEVERITY Severities[] = {D3D12_MESSAGE_SEVERITY_INFO};

         // Suppress individual messages by their ID
         D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
            // D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
            // D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            // D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
         };

         D3D12_INFO_QUEUE_FILTER NewFilter = {};
         // NewFilter.DenyList.NumCategories = _countof(Categories);
         // NewFilter.DenyList.pCategoryList = Categories;
         NewFilter.DenyList.NumSeverities = _countof(Severities);
         NewFilter.DenyList.pSeverityList = Severities;
         NewFilter.DenyList.NumIDs = _countof(DenyIds);
         NewFilter.DenyList.pIDList = DenyIds;

         ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
      }
   }

   ID3D12Device2* GetD3D12Device() {
      return sDevice->g_Device.Get();
   }
}
