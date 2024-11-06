#include "pch.h"
#include "RenderContext.h"

#include "Texture2D.h"
#include "Buffer.h"

namespace pbe {
   RenderContext::~RenderContext() {
   }

   uint2 RenderContext::GetRenderSize() const {
      return colorHDR->GetDesc().size;
   }

   uint2 RenderContext::GetDisplaySize() const {
      return colorHDR_Upscaled ? colorHDR_Upscaled->GetDesc().size : GetRenderSize();
   }

   bool RenderContext::IsUpscaleNeeded() const {
      return GetRenderSize() != GetDisplaySize();
   }

   RenderContext CreateRenderContext(uint2 renderSize, uint2 displaySize) {
      RenderContext context;

      bool useUpscale = renderSize != displaySize;

      context.colorHDR = Texture2D::Create(
         Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize).AllowUAV().Name("scene colorHDR"));

      // if (useUpscale) {
      context.colorHDR_Upscaled = Texture2D::Create(
         Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, displaySize).AllowUAV().Name("scene colorHDR"));
      // }

      context.colorLDR = Texture2D::Create(
         Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, displaySize).AllowUAV().Name("scene colorLDR"));

      context.waterRefraction = Texture2D::Create(
         Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize).Name("water refraction"));

      // D3D12_CLEAR_VALUE optimizedClearValue = {};
      // optimizedClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
      // optimizedClearValue.DepthStencil = { 1.0F, 0 };
      // if (useUpscale)
      {
         context.depthDisplay = Texture2D::Create(
            Texture2D::Desc::DepthStencil(DXGI_FORMAT_R32_FLOAT, displaySize).Name("depth display"));

         context.motionDisplay = Texture2D::Create(Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16_FLOAT, displaySize)
            .AllowUAV().Name("motion vector NDC display"));
      }

      context.depth = Texture2D::Create(
         Texture2D::Desc::DepthStencil(DXGI_FORMAT_R32_FLOAT, renderSize).Name("scene depth"));
      context.depthWithoutWater = Texture2D::Create(
         Texture2D::Desc::DepthStencil(DXGI_FORMAT_R32_FLOAT, renderSize).Name("scene depth without water"));
      context.linearDepth = Texture2D::Create(
         Texture2D::Desc::DepthStencil(DXGI_FORMAT_R16_TYPELESS, renderSize).Name("scene linear depth"));

      context.ssao = Texture2D::Create(
         Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize).Name("scene ssao").AllowUAV());

      if (!context.shadowMap) {
         context.shadowMap = Texture2D::Create(
            Texture2D::Desc::DepthStencil(DXGI_FORMAT_R32_FLOAT, { 1024, 1024 })
            .Name("shadow map").Format(DXGI_FORMAT_R16_TYPELESS));
      }

      // rt

      context.baseColorTex = Texture2D::Create(Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("scene base color"));
      context.motionTex = Texture2D::Create(Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("scene motion"));
      context.normalTex = Texture2D::Create(Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("scene normal"));
      context.viewz = Texture2D::Create(Texture2D::Desc::RenderTarget(DXGI_FORMAT_R32_FLOAT, renderSize)
         .AllowUAV().Name("scene view z"));

      context.diffuseTex = Texture2D::Create(Texture2D::Desc::Default(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("rt diffuse"));
      context.diffuseHistoryTex = Texture2D::Create(Texture2D::Desc::Default(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("rt diffuse"));

      context.specularTex = Texture2D::Create(Texture2D::Desc::Default(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("rt specular"));
      context.specularHistoryTex = Texture2D::Create(Texture2D::Desc::Default(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("rt specular"));

      context.directLightingUnfilteredTex = Texture2D::Create(
         Texture2D::Desc::Default(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("directLightingUnfilteredTex"));

      context.shadowDataTex = Texture2D::Create(Texture2D::Desc::Default(DXGI_FORMAT_R16G16_FLOAT, renderSize)
         .AllowUAV().Name("shadow data"));

      context.shadowDataTranslucencyTex = Texture2D::Create(
         Texture2D::Desc::Default(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("shadow data translucency"));

      context.shadowDataTranslucencyHistoryTex = Texture2D::Create(
         Texture2D::Desc::Default(DXGI_FORMAT_R16G16B16A16_FLOAT, renderSize)
         .AllowUAV().Name("shadow data translucency history"));

      context.outlineTex = Texture2D::Create(Texture2D::Desc::RenderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT, displaySize)
         .AllowUAV().Name("outline"));
      context.outlineBlurredTex = Texture2D::Create(Texture2D::Desc::Default(DXGI_FORMAT_R16G16B16A16_FLOAT, displaySize)
         .AllowUAV().Name("outlines blurred"));

      return context;
   }

}
