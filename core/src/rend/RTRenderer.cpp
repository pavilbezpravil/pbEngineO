#include "pch.h"
#include "RTRenderer.h"

// todo: remove include
#include "NRD.h"
#include <NRDSettings.h>
#include <numeric>

#include "AccelerationStructure.h"
#include "Buffer.h"
#include "BVH.h"
#include "CommandList.h"
#include "DbgRend.h"
#include "NRDDenoiser.h"
#include "RenderContext.h"
#include "Renderer.h" // todo:
#include "Shader.h"
#include "Texture2D.h"
#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Common.h"
#include "math/Random.h"
#include "math/Shape.h"

#include "scene/Component.h"
#include "scene/Scene.h"

#include "shared/hlslCppShared.hlsli"
#include "shared/rt.hlsli"


namespace pbe {

   // todo: n rays -> n samples
   CVarSlider<int> cvNRays{ "render/rt/nRays", 1, 1, 32 };
   // todo: depth -> bounce
   CVarSlider<int> cvRayDepth{ "render/rt/rayDepth", 2, 1, 8 };

   CVarValue<bool> cvCustomTrace{ "render/rt/custom trace", false };
   CVarValue<bool> cvAccumulate{ "render/rt/accumulate", false };
   CVarValue<bool> cvRTDiffuse{ "render/rt/diffuse", true };
   CVarValue<bool> cvRTSpecular{ "render/rt/specular", true }; // todo
   CVarValue<bool> cvBvhAABBRender{ "render/rt/bvh aabb render", false };
   CVarValue<u32> cvBvhAABBRenderLevel{ "render/rt/bvh aabb show level", UINT_MAX };
   CVarValue<u32> cvBvhSplitMethod{ "render/rt/bvh split method", 2 };
   CVarValue<bool> cvUsePSR{ "render/rt/use psr", false }; // todo: on my laptop it takes 50% more time

   CVarValue<bool> cvDenoise{ "render/denoise/enable", true };
   CVarValue<bool> cvNRDValidation{ "render/denoise/nrd validation", false };
   CVarTrigger cvClearHistory{ "render/denoise/clear history" };

   CVarSlider<float>  cvNRDSplitScreen{ "render/denoise/split screen", 0, 0, 1 };
   CVarValue<bool> cvNRDPerfMode{ "render/denoise/perf mode", true };
   CVarValue<bool> cvDenoiseDiffSpec{ "render/denoise/diffuse&specular", true };
   CVarValue<bool> cvDenoiseShadow{ "render/denoise/shadow", true };

   CVarValue<bool> cvFog{ "render/rt/fog enable", false };

   RTRenderer::~RTRenderer() {
      // todo: not here
      NRDTerm();
   }

   void RTRenderer::RenderScene(CommandList& cmd, const Scene& scene, const RenderCamera& camera, RenderContext& context) {
      COMMAND_LIST_SCOPE(cmd, "RT Scene");
      PROFILE_GPU("RT Scene");

      PIX_EVENT_SYSTEM(Render, "RT Render Scene");

      std::vector<SRTObject> objs;

      std::vector<AABB> aabbs;

      for (auto [e, trans, material, geom]
         : scene.View<SceneTransformComponent, MaterialComponent, GeometryComponent>().each()) {
         auto rotation = trans.Rotation();
         auto position = trans.Position();
         auto scale = trans.Scale();

         SRTObject obj;
         obj.position = position;
         obj.id = (u32)e;

         obj.rotation = glm::make_vec4(glm::value_ptr(rotation));

         obj.geomType = (int)geom.type;
         obj.halfSize = geom.sizeData / 2.f * scale;

         obj.baseColor = material.baseColor;
         obj.metallic = material.metallic;
         obj.roughness = material.roughness;
         obj.emissivePower = material.emissivePower;

         // todo: sphere may be optimized
         // todo: not fastest way

         auto extends = scale * 0.5f;
         auto aabb = AABB::Empty();

         if (geom.type == GeomType::Box) {
            aabb.AddPoint(rotation * extends);
            aabb.AddPoint(rotation * vec3{ -extends.x, extends.y, extends.z });
            aabb.AddPoint(rotation * vec3{ extends.x, -extends.y, extends.z });
            aabb.AddPoint(rotation * vec3{ extends.x, extends.y, -extends.z });

            vec3 absMax = glm::max(abs(aabb.min), abs(aabb.max));
            aabb.min = -absMax;
            aabb.max = absMax;

            aabb.Translate(position);
         } else {
            auto sphereExtends = vec3{ extends.x };
            aabb = AABB::FromExtends(trans.Position(), sphereExtends);
         }

         aabbs.emplace_back(aabb);

         objs.emplace_back(obj);
      }

      u32 nBvhNodes = 0;
      if (cvCustomTrace) {
         // todo: dont create every frame
         BVH bvh;
         bvh.Build(aabbs, (BVH::SplitMethod)(u32)cvBvhSplitMethod);

         if (cvBvhAABBRender) {
            auto& dbgRender = *GetDbgRend(scene);
            // for (auto& aabb : aabbs) {
            //    dbgRender.DrawAABB(aabb);
            // }
            bvh.Render(dbgRender, cvBvhAABBRenderLevel);
         }

         bvh.Flatten();


         // todo: make it dynamic
         auto& bvhNodes = bvh.linearNodes;
         nBvhNodes = (u32)bvhNodes.size();
         if (!bvhNodesBuffer || bvhNodesBuffer->NumElements() < nBvhNodes) {
            auto bufferDesc = Buffer::Desc::Structured("BVHNodes", nBvhNodes, sizeof(BVHNode));
            bvhNodesBuffer = Buffer::Create(bufferDesc);
         }
         cmd.UpdateBuffer(*bvhNodesBuffer, 0, DataView{ bvhNodes });
      } else {
         COMMAND_LIST_SCOPE(cmd, "Build AS");
         PROFILE_GPU("Build AS");

         std::vector<AccelerationStructure::AABB> blasAabbs;
         for (auto&& aabb : aabbs) {
            AccelerationStructure::AABB blasAabb {
               .minX = aabb.min.x,
               .minY = aabb.min.y,
               .minZ = aabb.min.z,
               .maxX = aabb.max.x,
               .maxY = aabb.max.y,
               .maxZ = aabb.max.z,
            };
            blasAabbs.push_back(blasAabb);
         }

         auto blas = Ref<AccelerationStructure>::Create();
         blas->AddAABBs(AccelerationStructure::CreateAABBs(cmd, DataView{ blasAabbs }));
         blas->Build(cmd);

         Array<AccelerationStructure::Instance> instances;
         instances.emplace_back(Transform_Identity, blas);

         if (false) {
            vec3 vertexes[3] = {
               {0, 0, 0},
               {0, 10, 0},
               {10, 10, 0},
            };

            u32 indices[3] = { 0, 1, 2 };

            auto vertexesBuffer = cmd.CreateBuffer(Buffer::Desc::Structured<vec3>(3)
               // .Format(DXGI_FORMAT_R32G32B32_FLOAT)
               , DataView{ vertexes });
            auto indexesBuffer = cmd.CreateBuffer(Buffer::Desc::Structured<u32>(3)
               // .Format(DXGI_FORMAT_R32_UINT)
               , DataView{ indices });

            auto blasTri = Ref<AccelerationStructure>::Create();
            blasTri->AddTriangles(vertexesBuffer, indexesBuffer);
            blasTri->Build(cmd);

            instances.emplace_back(Transform_Identity, blasTri);
         }

         if (context.grassCounters) {
            auto grassBlas = Ref<AccelerationStructure>::Create();
            grassBlas->AddTriangles(context.grassVertexes, context.grassIndexes);
            grassBlas->Build(cmd);

            instances.emplace_back(Transform_Identity, grassBlas);
         }

         // AccelerationStructure::Instance instance{ Transform_Identity, blas };

         auto tlas = Ref<AccelerationStructure>::Create();
         tlas->SetInstances(AccelerationStructure::CreateInstances(cmd, DataView{ instances }));
         tlas->Build(cmd);

         cmd.SetSRV({ SCENE_AS_SLOT }, tlas->GetASBuffer());
      }

      u32 nObj = (u32)objs.size();

      // todo: make it dynamic
      if (!rtObjectsBuffer || rtObjectsBuffer->NumElements() < nObj) {
         auto bufferDesc = Buffer::Desc::Structured("RtObjects", nObj, sizeof(SRTObject));
         rtObjectsBuffer = Buffer::Create(bufferDesc);
      }

      cmd.UpdateBuffer(*rtObjectsBuffer, 0, DataView{ objs });

      u32 nImportanceVolumes = 0;
      {
         std::vector<SRTImportanceVolume> importanceVolumes;

         for (auto [e, trans, volume]
            : scene.View<SceneTransformComponent, RTImportanceVolumeComponent>().each()) {
            SRTImportanceVolume v;
            v.position = trans.Position();
            v.radius = volume.radius; // todo: mb radius is scale?

            importanceVolumes.emplace_back(v);
         }

         nImportanceVolumes = (u32)importanceVolumes.size();

         TryUpdateBuffer(cmd, importanceVolumesBuffer
            , Buffer::Desc::Structured<SRTImportanceVolume>(nImportanceVolumes).Name("ImportanceVolumes")
            , std::span{importanceVolumes});
      }

      auto& outTexture = *context.colorHDR;
      auto outTexSize = outTexture.GetDesc().size;

      // cmd.ClearRenderTarget(*context.colorHDR, vec4{ 0, 0, 0, 1 });

      static int accumulatedFrames = 0;

      bool resetHistory = cvClearHistory;

      float historyWeight = 0;

      if (cvAccumulate) {
         historyWeight = (float)accumulatedFrames / (float)(accumulatedFrames + cvNRays);
         accumulatedFrames += cvNRays;

         resetHistory |= camera.GetPrevViewProjection() != camera.GetViewProjection();
      }

      if (resetHistory || !cvAccumulate) {
         // cmd.ClearUAVUint(*context.reprojectCountTexPrev);
         // cmd.ClearUAVFloat(*context.historyTex);

         accumulatedFrames = 0;
      }

      SRTConstants rtCB;
      rtCB.rtSize = outTexSize;
      rtCB.rayDepth = cvRayDepth;
      rtCB.nObjects = nObj;
      rtCB.nRays = cvNRays;
      rtCB.random01 = Random::Float(0.f, 1.f);
      rtCB.historyWeight = historyWeight;
      rtCB.nImportanceVolumes = nImportanceVolumes;

      rtCB.bvhNodes = nBvhNodes;

      nrd::HitDistanceParameters hitDistanceParametrs{};
      rtCB.nrdHitDistParams = { hitDistanceParametrs.A, hitDistanceParametrs.B,
         hitDistanceParametrs.C, hitDistanceParametrs.D };

      auto rtConstantsCB = cmd.AllocDynConstantBuffer(rtCB);

      auto setSharedResource = [&](GpuProgram& pass) {
         cmd.SetCB(pass.GetBindPoint("gRTConstantsCB"), rtConstantsCB.buffer, rtConstantsCB.offset);
         cmd.SetSRV(pass.GetBindPoint("gRtObjects"), rtObjectsBuffer);
         cmd.SetSRV(pass.GetBindPoint("gBVHNodes"), bvhNodesBuffer);
      };

      // todo: generate by rasterizator
      if (cvDenoise && false) {
         COMMAND_LIST_SCOPE(cmd, "GBuffer");
         PROFILE_GPU("GBuffer");

         auto pass = GetGpuProgram(ProgramDesc::Cs("rt.hlsl", "GBufferCS"));
         cmd.SetComputeProgram(pass);
         setSharedResource(*pass);

         // cmd.SetUAV(pass->GetBindPoint("gDepthOut"), context.depthTex);
         cmd.SetUAV(pass->GetBindPoint("gNormalOut"), context.normalTex);

         cmd.SetUAV(pass->GetBindPoint("gUnderCursorBuffer"), context.underCursorBuffer); // todo: only for editor

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }

      if (0) {
         COMMAND_LIST_SCOPE(cmd, "Ray Trace");
         PROFILE_GPU("Ray Trace");

         auto desc = ProgramDesc::Cs("rt.hlsl", "rtCS");
         // if (cvImportanceSampling) {
         //    desc.cs.defines.AddDefine("IMPORTANCE_SAMPLING");
         // }

         auto pass = GetGpuProgram(desc);
         cmd.SetComputeProgram(pass);
         setSharedResource(*pass);

         // cmd.SetUAV(pass->GetBindPoint("gColorOut"), context.rtColorNoisyTex);

         cmd.Dispatch2D(outTexSize, int2{8, 8});
      }

      if (cvRTDiffuse) {
         COMMAND_LIST_SCOPE(cmd, "RT Diffuse&Specular");
         PROFILE_GPU("RT Diffuse&Specular");

         auto desc = ProgramDesc::Cs("rt.hlsl", "RTDiffuseSpecularCS");
         if (cvUsePSR) {
            desc.cs.defines.AddDefine("USE_PSR");
         }

         auto pass = GetGpuProgram(desc);
         cmd.SetComputeProgram(pass);
         setSharedResource(*pass);

         pass->SetSRV(cmd, "gImportanceVolumes", importanceVolumesBuffer);

         if (cvUsePSR) {
            pass->SetUAV(cmd, "gViewZOut", context.viewz);
            pass->SetUAV(cmd, "gNormalOut", context.normalTex);
            pass->SetUAV(cmd, "gColorOut", context.colorHDR);
            pass->SetUAV(cmd, "gBaseColorOut", context.baseColorTex);
         } else {
            pass->SetSRV(cmd, "gViewZ", context.viewz);
            pass->SetSRV(cmd, "gNormal", context.normalTex);
            pass->SetSRV(cmd, "gBaseColor", context.baseColorTex);
         }

         pass->SetUAV(cmd, "gDiffuseOut", context.diffuseTex);
         pass->SetUAV(cmd, "gSpecularOut", context.specularTex);
         pass->SetUAV(cmd, "gDirectLightingOut", context.directLightingUnfilteredTex);

         pass->SetUAV(cmd, "gShadowDataOut", context.shadowDataTex);
         pass->SetUAV(cmd, "gShadowDataTranslucencyOut", context.shadowDataTranslucencyTex);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }

      Texture2D* diffuse = context.diffuseTex;
      Texture2D* specular = context.specularTex;
      Texture2D* shadow = context.shadowDataTranslucencyTex;

      if (cvDenoise) {
         COMMAND_LIST_SCOPE(cmd, "Denoise");
         PROFILE_GPU("Denoise");

         DenoiseCallDesc desc;

         desc.textureSize = outTexSize;
         desc.clearHistory = cvClearHistory;
         desc.diffuseSpecular = cvDenoiseDiffSpec;
         desc.shadow = cvDenoiseShadow;
         desc.perfMode = cvNRDPerfMode;
         desc.splitScreen = cvNRDSplitScreen;
         desc.jitter = camera.jitter;
         desc.jitterPrev = camera.jitterPrev;

         desc.mViewToClip = camera.projection;
         desc.mViewToClipPrev = camera.prevProjection;
         desc.mWorldToView = camera.view;
         desc.mWorldToViewPrev = camera.prevView;

         desc.IN_MV = context.motionTex;
         desc.IN_NORMAL_ROUGHNESS = context.normalTex;
         desc.IN_VIEWZ = context.viewz;
         desc.IN_DIFF_RADIANCE_HITDIST = context.diffuseTex;
         desc.IN_SPEC_RADIANCE_HITDIST = context.specularTex;
         desc.OUT_DIFF_RADIANCE_HITDIST = context.diffuseHistoryTex;
         desc.OUT_SPEC_RADIANCE_HITDIST = context.specularHistoryTex; // todo: names

         desc.IN_SHADOWDATA = context.shadowDataTex;
         desc.IN_SHADOW_TRANSLUCENCY = context.shadowDataTranslucencyTex;
         desc.OUT_SHADOW_TRANSLUCENCY = context.shadowDataTranslucencyHistoryTex;

         if (cvNRDValidation) {
            desc.validation = true;
            desc.OUT_VALIDATION = context.colorHDR;
         }

         NRDResize(outTexSize);
         NRDDenoise(cmd, desc);

         if (desc.diffuseSpecular) {
            diffuse = context.diffuseHistoryTex;
            specular = context.specularHistoryTex;
         }
         if (desc.shadow) {
            shadow = context.shadowDataTranslucencyHistoryTex;
         }
      }

      if (!cvNRDValidation) {
         COMMAND_LIST_SCOPE(cmd, "RT Combine");
         PROFILE_GPU("RT Combine");

         auto desc = ProgramDesc::Cs("rt.hlsl", "RTCombineCS");

         auto pass = GetGpuProgram(desc);
         cmd.SetComputeProgram(pass);
         setSharedResource(*pass);

         pass->SetSRV(cmd, "gViewZ", context.viewz);
         pass->SetSRV(cmd, "gBaseColor", context.baseColorTex);
         pass->SetSRV(cmd, "gNormal", context.normalTex);
         pass->SetSRV(cmd, "gDiffuse", diffuse);
         pass->SetSRV(cmd, "gSpecular", specular);
         pass->SetSRV(cmd, "gDirectLighting", context.directLightingUnfilteredTex);
         pass->SetSRV(cmd, "gShadowDataTranslucency", shadow);

         pass->SetUAV(cmd, "gColorOut", context.colorHDR);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }

      // todo:
      // if (cvFog)
      {
         COMMAND_LIST_SCOPE(cmd, "RT Fog");
         PROFILE_GPU("RT Fog");

         auto desc = ProgramDesc::Cs("rt.hlsl", "RTFogCS");

         auto pass = GetGpuProgram(desc);
         cmd.SetComputeProgram(pass);
         setSharedResource(*pass);

         pass->SetSRV(cmd, "gViewZ", context.viewz);
         pass->SetUAV(cmd, "gColorOut", context.colorHDR);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }
   }

}
