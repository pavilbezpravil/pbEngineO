#pragma once
#include "core/Ref.h"
#include "math/Types.h"

namespace pbe {
   class Buffer;

   struct CORE_API RenderContext {
      Ref<Texture2D> colorHDR;
      Ref<Texture2D> colorHDR_Upscaled;
      Ref<Texture2D> colorLDR;

      Ref<Texture2D> dilatedDepth;
      Ref<Texture2D> dilatedMotionVectors;
      Ref<Texture2D> reconstructedPrevNearestDepth;

      Ref<Texture2D> depth;
      Ref<Texture2D> depthDisplay;

      Ref<Texture2D> viewz; // linear view z

      Ref<Texture2D> depthWithoutWater;
      Ref<Texture2D> linearDepth;

      Ref<Texture2D> waterRefraction;
      Ref<Texture2D> ssao;

      Ref<Texture2D> shadowMap;

      Ref<Texture2D> baseColorTex;
      Ref<Texture2D> normalTex;

      Ref<Texture2D> motionTex;
      Ref<Texture2D> motionDisplay;

      Ref<Texture2D> diffuseTex;
      Ref<Texture2D> diffuseHistoryTex;

      Ref<Texture2D> specularTex;
      Ref<Texture2D> specularHistoryTex;

      Ref<Texture2D> shadowDataTex;
      Ref<Texture2D> shadowDataTranslucencyTex;
      Ref<Texture2D> shadowDataTranslucencyHistoryTex;

      Ref<Texture2D> directLightingUnfilteredTex;

      Ref<Texture2D> outlineTex;
      Ref<Texture2D> outlineBlurredTex;

      // todo:
      Ref<Buffer> underCursorBuffer;
      int2 cursorPixelIdx{ -1 };

      // todo:
      Ref<Buffer> grassVertexes;
      Ref<Buffer> grassIndexes;
      Ref<Buffer> grassCounters;
      Ref<Buffer> grassReadback;

      ~RenderContext();

      uint2 GetRenderSize() const;
      uint2 GetDisplaySize() const;
      bool IsUpscaleNeeded() const;
   };

   CORE_API RenderContext CreateRenderContext(uint2 renderSize, uint2 displaySize);

}
