#include "pch.h"
#include "Fsr3Upscaler.h"

#include "RenderContext.h"
#include "Renderer.h"

namespace pbe {
   Fsr3Upscaler::Fsr3Upscaler() {
      size_t scratchBufferSize = ffxGetScratchMemorySizeDX12(maxContexts);
      scratchBuffer.resize(scratchBufferSize);

      fxxCheck(ffxGetInterfaceDX12(&backendInterface, sDevice->g_Device.Get(),
                                   scratchBuffer.data(), scratchBufferSize, maxContexts));
   }

   Fsr3Upscaler::~Fsr3Upscaler() {
      DestroyContext();
   }

   void Fsr3Upscaler::CreateContext(uint2 displaySize, RenderContext& rContext) {
      if (!rContext.dilatedDepth) {
         rContext.dilatedDepth = Texture2D::Create(Texture2D::Desc::RenderTarget(DXGI_FORMAT_R32_FLOAT, displaySize)
            .AllowUAV().InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Aliasable().Name("dilatedDepth"));

         rContext.dilatedMotionVectors = Texture2D::Create(Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16_FLOAT, displaySize)
            .AllowUAV().InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Aliasable().Name("dilatedMotionVectors"));

         rContext.reconstructedPrevNearestDepth = Texture2D::Create(Texture2D::Desc::Default(DXGI_FORMAT_R32_UINT, displaySize)
            .AllowUAV().InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Aliasable().Name("reconstructedPrevNearestDepth"));
      }

      if (curDisplaySize == displaySize) {
         return;
      }

      DestroyContext();
      curDisplaySize = displaySize;

      contextDescription.flags = FFX_FSR3UPSCALER_ENABLE_AUTO_EXPOSURE;
      contextDescription.flags |= FFX_FSR3UPSCALER_ENABLE_HIGH_DYNAMIC_RANGE;
      contextDescription.flags |= FFX_FSR3UPSCALER_ENABLE_DYNAMIC_RESOLUTION;
      // contextDescription.flags |= FFX_FSR3UPSCALER_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;
      contextDescription.flags |= FFX_FSR3UPSCALER_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS;
#if _DEBUG
      contextDescription.flags |= FFX_FSR3UPSCALER_ENABLE_DEBUG_CHECKING;
      contextDescription.fpMessage = Fsr3Log;
#endif

      contextDescription.maxRenderSize = { displaySize.x, displaySize.y };
      contextDescription.displaySize = { displaySize.x, displaySize.y };
      contextDescription.backendInterface = backendInterface;

      fxxCheck(ffxFsr3UpscalerContextCreate(&context, &contextDescription));

      FfxFsr3UpscalerSharedResourceDescriptions SharedResources;
      ffxFsr3UpscalerGetSharedResourceDescriptions(&context, &SharedResources);

      contextCreated = true;
   }

   void Fsr3Upscaler::DestroyContext() {
      if (contextCreated) {
         ffxFsr3UpscalerContextDestroy(&context);
      }
   }

   FfxSurfaceFormat GetFfxFormat(Format format) {
      switch (format) {
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
         return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
      case DXGI_FORMAT_R32_FLOAT:
         return FFX_SURFACE_FORMAT_R32_FLOAT;
      case DXGI_FORMAT_R16G16_FLOAT:
         return FFX_SURFACE_FORMAT_R16G16_FLOAT;
      case DXGI_FORMAT_R32_UINT:
         return FFX_SURFACE_FORMAT_R32_UINT;
      default:
         UNIMPLEMENTED();
      }

      return FFX_SURFACE_FORMAT_UNKNOWN;
   }

   FfxResourceDescription GetFfxResourceDesc(Texture2D* tex) {
      if (!tex) {
         return { };
      }

      auto& desc = tex->GetDesc();

      FfxResourceDescription ffxDesc;
      ffxDesc.type = FFX_RESOURCE_TYPE_TEXTURE2D;
      ffxDesc.format = GetFfxFormat(desc.format);
      ffxDesc.width = desc.size.x;
      ffxDesc.height = desc.size.y;
      ffxDesc.depth = 1; // todo:
      ffxDesc.mipCount = desc.mips;
      ffxDesc.flags = FFX_RESOURCE_FLAGS_NONE;

      ffxDesc.usage = FFX_RESOURCE_USAGE_UAV;
      if (desc.allowDepthStencil) {
         ffxDesc.usage = FFX_RESOURCE_USAGE_DEPTHTARGET;
      }
      if (desc.allowRenderTarget) {
         ffxDesc.usage = FFX_RESOURCE_USAGE_RENDERTARGET;
      }
      if (desc.allowUAV) {
         ffxDesc.usage = (FfxResourceUsage)(ffxDesc.usage | FFX_RESOURCE_USAGE_UAV);
      }

      return ffxDesc;
   }

   FfxResource GetFxxResource(Texture2D* tex, wchar_t* name) {
      auto& desc = tex->GetDesc();

      return ffxGetResourceDX12(tex ? tex->GetD3D12Resource() : nullptr, GetFfxResourceDesc(tex),
         name, FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
   }
}
