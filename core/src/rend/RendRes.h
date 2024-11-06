#pragma once

#include "RootSignature.h"
#include "DescriptorAllocation.h"
#include "core/Core.h"
#include "math/Types.h"

namespace pbe {
   class Mesh;

   struct CORE_API rendres {
      static void Init();
      static void Term();

      static inline D3D12_RASTERIZER_DESC rasterizerState;
      static inline D3D12_RASTERIZER_DESC rasterizerStateWireframe;

      // static inline D3D12_SAMPLER_DESC samplerStateWrapPoint;
      // static inline D3D12_SAMPLER_DESC samplerStateWrapLinear;
      // static inline D3D12_SAMPLER_DESC samplerStateClampPoint;
      // static inline D3D12_SAMPLER_DESC samplerStateShadow;

      static inline D3D12_DEPTH_STENCIL_DESC1 depthStencilStateDepthReadWrite;
      static inline D3D12_DEPTH_STENCIL_DESC1 depthStencilStateDepthReadNoWrite;
      static inline D3D12_DEPTH_STENCIL_DESC1 depthStencilStateEqual;
      static inline D3D12_DEPTH_STENCIL_DESC1 depthStencilStateNo;

      static inline D3D12_BLEND_DESC blendStateDefaultRGB;
      static inline D3D12_BLEND_DESC blendStateDefaultRGBA;
      static inline D3D12_BLEND_DESC blendStateTransparencyRGB;

      static inline DescriptorAllocation samplerPointClamp;
      static inline DescriptorAllocation samplerLinearClamp;
      static inline DescriptorAllocation samplerPointWrap;
      static inline DescriptorAllocation samplerLinearWrap;
      static inline DescriptorAllocation samplerPointMirror;
      static inline DescriptorAllocation samplerLinearMirror;

      static inline DescriptorAllocation samplerPointClampBoarderZero;

      static inline Ref<RootSignature> pDefaultRootSignature;

      static Mesh& CubeMesh();
   private:
      static Ref<Mesh> pCubeMesh;
   };
}
