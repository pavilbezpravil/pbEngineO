#include "pch.h"
#include "NRDDenoiser.h"

#include "Device.h"
#include "core/Assert.h"
#include "math/Types.h"
#include "Texture2D.h"

#include "NRD.h"
#include "NRDIntegration.h"
#include "core/CVar.h"

/*

NRD\Shaders\Include\REBLUR\REBLUR_DiffuseSpecular_TemporalAccumulation.hlsli

gIn_Prev_InternalData

uint4 TexGather(Texture2D<uint> tex, float2 uv)
{
    uint2 size;
    tex.GetDimensions(size.x, size.y);

    // xy = uv / gInvScreenSize;
    int2 xy = int2(uv * float2(size));

    int2 tlPos = min(max(xy + int2(-1, -1), 0), size - 1);
    int2 trPos = min(max(xy + int2( 0, -1), 0), size - 1);
    int2 blPos = min(max(xy + int2(-1,  0), 0), size - 1);
    int2 brPos = min(max(xy, 0), size - 1);

    uint tl = tex.Load(int3(tlPos, 0)); // Top-Left
    uint tr = tex.Load(int3(trPos, 0)); // Top-Right
    uint bl = tex.Load(int3(blPos, 0)); // Bottom-Left
    uint br = tex.Load(int3(brPos, 0)); // Bottom-Right

    return uint4(tl, tr, bl, br);
}
 
 */

namespace pbe {

   NrdIntegration NRD = NrdIntegration();

   uint2 nriRenderResolution = { UINT_MAX, UINT_MAX };

   constexpr nrd::Identifier NRD_DENOISE_DIFFUSE_SPECULAR = 1;
   constexpr nrd::Identifier NRD_DENOISE_SHADOW = 2;

   static void NRDDenoisersInit(uint2 renderResolution) {
      const nrd::DenoiserDesc denoiserDescs[] =
      {
         { NRD_DENOISE_DIFFUSE_SPECULAR, nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR, (uint16_t)renderResolution.x, (uint16_t)renderResolution.y },
         { NRD_DENOISE_SHADOW, nrd::Denoiser::SIGMA_SHADOW_TRANSLUCENCY, (uint16_t)renderResolution.x, (uint16_t)renderResolution.y },
      };

      nrd::InstanceCreationDesc instanceCreationDesc = {};
      instanceCreationDesc.denoisers = denoiserDescs;
      instanceCreationDesc.denoisersNum = _countof(denoiserDescs);

      bool result = NRD.Initialize(instanceCreationDesc);
      ASSERT(result);
   }

   void NRDDenoise(CommandList& cmd, const DenoiseCallDesc& desc) {
      nrd::CommonSettings commonSettings = {};

      // todo:
      static u32 frameIdx = 0;
      frameIdx++;
      commonSettings.frameIndex = frameIdx;

      memcpy(commonSettings.viewToClipMatrix, &desc.mViewToClip, sizeof(desc.mViewToClip));
      memcpy(commonSettings.viewToClipMatrixPrev, &desc.mViewToClipPrev, sizeof(desc.mViewToClipPrev));
      memcpy(commonSettings.worldToViewMatrix, &desc.mWorldToView, sizeof(desc.mWorldToView));
      memcpy(commonSettings.worldToViewMatrixPrev, &desc.mWorldToViewPrev, sizeof(desc.mWorldToViewPrev));

      // commonSettings.motionVectorScale[0] = 1.0f / float(desc.textureSize.x);
      // commonSettings.motionVectorScale[1] = 1.0f / float(desc.textureSize.y);
      // commonSettings.motionVectorScale[2] = 0.0f;

      commonSettings.motionVectorScale[0] = 1;
      commonSettings.motionVectorScale[1] = 1;
      commonSettings.motionVectorScale[2] = 1;
      commonSettings.isMotionVectorInWorldSpace = true;

      // todo: better nrd::AccumulationMode::RESTART
      commonSettings.accumulationMode = desc.clearHistory ? nrd::AccumulationMode::CLEAR_AND_RESTART : nrd::AccumulationMode::CONTINUE;
      commonSettings.enableValidation = desc.validation;
      commonSettings.splitScreen = desc.splitScreen;
      commonSettings.cameraJitter[0] = desc.jitter.x;
      commonSettings.cameraJitter[1] = desc.jitter.y;
      commonSettings.cameraJitterPrev[0] = desc.jitterPrev.x;
      commonSettings.cameraJitterPrev[1] = desc.jitterPrev.y;

      NRD.SetCommonSettings(commonSettings);

      nrd::ReblurSettings settings = {};
      settings.enablePerformanceMode = desc.perfMode;
      NRD.SetDenoiserSettings(NRD_DENOISE_DIFFUSE_SPECULAR, &settings);

      nrd::SigmaSettings sigmaSettings = {};
      NRD.SetDenoiserSettings(NRD_DENOISE_SHADOW, &sigmaSettings);

      NrdUserPool userPool = {};
      {
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_MV, desc.IN_MV);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_NORMAL_ROUGHNESS, desc.IN_NORMAL_ROUGHNESS);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_VIEWZ, desc.IN_VIEWZ);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, desc.IN_DIFF_RADIANCE_HITDIST);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST, desc.IN_SPEC_RADIANCE_HITDIST);

         NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, desc.OUT_DIFF_RADIANCE_HITDIST);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST, desc.OUT_SPEC_RADIANCE_HITDIST);

         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_SHADOWDATA, desc.IN_SHADOWDATA);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_SHADOW_TRANSLUCENCY, desc.IN_SHADOW_TRANSLUCENCY);
         NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY, desc.OUT_SHADOW_TRANSLUCENCY);

         if (desc.validation) {
            NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_VALIDATION, desc.OUT_VALIDATION);
         }
      }

      nrd::Identifier denoisers[2];
      u32 nDenoisers = 0;

      if (desc.diffuseSpecular) {
         denoisers[nDenoisers++] = NRD_DENOISE_DIFFUSE_SPECULAR;
      }
      if (desc.shadow) {
         denoisers[nDenoisers++] = NRD_DENOISE_SHADOW;
      }

      if (nDenoisers > 0) {
         NRD.Denoise(denoisers, nDenoisers, cmd, userPool);
      }
   }

   void NRDTerm() {
      if (!NRD.Inited()) {
         return;
      }

      nriRenderResolution = { UINT_MAX, UINT_MAX };

      // Also NRD needs to be recreated on "resize"
      NRD.Destroy();
   }

   void NRDResize(uint2 renderResolution) {
      if (nriRenderResolution == renderResolution) {
         return;
      }

      if (NRD.Inited()) {
         NRD.Destroy();
      }
      NRDDenoisersInit(renderResolution);

      nriRenderResolution = renderResolution;
   }

}
