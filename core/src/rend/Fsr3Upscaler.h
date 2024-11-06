#pragma once

#include <FidelityFX\host\ffx_fsr3upscaler.h>
#include <FidelityFX\host\backends\dx12\ffx_dx12.h>

#include "Format.h"
#include "core/Assert.h"
#include "core/Core.h"
#include "math/Types.h"

namespace pbe {
   struct RenderContext;

   class Fsr3Upscaler {
   public:
      Fsr3Upscaler();
      ~Fsr3Upscaler();

      void CreateContext(uint2 displaySize, RenderContext& rContext);
      void DestroyContext();

      void Upscale(FfxFsr3UpscalerDispatchDescription& dispatchParameters) {
         fxxCheck(ffxFsr3UpscalerContextDispatch(&context, &dispatchParameters));
      }

      bool contextCreated = false;

      FfxInterface backendInterface;
      FfxDevice device;

      u32 maxContexts = 1;

      std::vector<u8> scratchBuffer;

      FfxFsr3UpscalerContext context;
      uint2 curDisplaySize;
      FfxFsr3UpscalerContextDescription contextDescription;

   private:
      void fxxCheck(FfxErrorCode code) {
         ASSERT(code == FFX_OK);
      }

      static void Fsr3Log(FfxMsgType type, const wchar_t* message) {
         // ERROR("Frs3 msg type: {}, {}", type == FFX_MESSAGE_TYPE_ERROR ? "error" : "warning", message);
         ASSERT(false);
      }
   };

   FfxSurfaceFormat GetFfxFormat(Format format);
   FfxResourceDescription GetFfxResourceDesc(Texture2D* tex);
   FfxResource GetFxxResource(Texture2D* tex, wchar_t* name);
}
