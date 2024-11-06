#include "pch.h"
#include "Renderer.h"

#include "DbgRend.h"
#include "RendRes.h"
#include "RTRenderer.h"
#include "CommandQueue.h"
#include "Fsr3Upscaler.h"
#include "RenderContext.h"
#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Random.h"
#include "math/Shape.h"
#include "mesh/Mesh.h"
#include "physics/PhysComponents.h"

#include "shared/hlslCppShared.hlsli"
#include "system/Water.h"

/// # TODO
/// - Compile fsr as dll
/// - Outline render
/// - Debug render
/// - Jitter
/// - ffx_backend_dx12_x64d
/// - live objects
/// - ReconstructWorldPosition does not work with jitter depth. Mb use GetWorldPositionFromDepth
/// - NRD jitter
/// - NRD light licking problem
/// - Jitter disable and scale
/// - Fix render shadowmap
/// - viewport picking


namespace pbe {
   CVarValue<bool> cFreezeCullCamera{"render/freeze cull camera", false};
   CVarValue<bool> cUseFrustumCulling{"render/use frustum culling", false};

   CVarValue<bool> cvRenderDecals{"render/decals", true};
   CVarValue<bool> cvRenderOpaqueSort{"render/opaque sort", false};
   CVarValue<bool> cvRenderShadowMap{"render/shadow map", true};
   CVarValue<bool> cvRenderSsao{"render/ssao", false};
   CVarValue<bool> cvRenderTransparency{"render/transparency", true};
   CVarValue<bool> cvRenderTransparencySort{"render/transparency sort", true};

   CVarValue<bool> dbgRenderEnable{"render/debug render", true};
   CVarValue<bool> instancedDraw{"render/instanced draw", true};
   CVarValue<bool> cvOutlineEnable{"render/outline", true};
   CVarValue<bool> indirectDraw{"render/indirect draw", false};
   CVarValue<bool> depthDownsampleEnable{"render/depth downsample enable", false};
   CVarValue<bool> rayTracingSceneRender{"render/ray tracing scene render", true};
   CVarValue<bool> animationTimeUpdate{"render/animation time update", true};

   CVarValue<bool> cvEnableUpscale{"render/upscale/enable", true};
   CVarValue<bool> cvEnableSharpening{"render/upscale/enale sharpness", true};
   CVarSlider<float> cvEnableSharpness{"render/upscale/sharpness", 0.8f, 0, 1};

   CVarValue<bool> cvDestructDbgRend{"phys/destruct dbg rend", false};

   CVarValue<bool> applyFog{"render/fog/enable", false};
   CVarSlider<int> fogNSteps{"render/fog/nSteps", 0, 0, 16};

   CVarSlider<float> tonemapExposition{"render/tonemap/exposion", 1.f, 0.f, 3.f};

   static mat4 NDCToTexSpaceMat4() {
      mat4 scale = glm::scale(mat4{1.f}, {0.5f, -0.5f, 1.f});
      mat4 translate = glm::translate(mat4{1.f}, {0.5f, 0.5f, 0.f});

      return translate * scale;
   }

   vec2 UVToNDC(const vec2& uv) {
      vec2 ndc = uv * 2.f - 1.f;
      ndc.y = -ndc.y;
      return ndc;
   }

   vec2 NDCToUV(const vec2& ndc) {
      vec2 uv = ndc;
      uv.y = -uv.y;
      uv = (uv + 1.f) * 0.5f;
      return uv;
   }

   vec3 GetWorldPosFromUV(const vec2& uv, const mat4& invViewProj) {
      vec2 ndc = UVToNDC(uv);

      vec4 worldPos4 = invViewProj * vec4(ndc, 0, 1);
      vec3 worldPos = worldPos4 / worldPos4.w;

      return worldPos;
   }

   vec2 GetFsr3Jitter(u32 renderWidth, u32 displayWidth, u32 frame) {
      int jitterPhaseCount = ffxFsr3UpscalerGetJitterPhaseCount(renderWidth, displayWidth);

      vec2 jitter;
      ffxFsr3UpscalerGetJitterOffset(&jitter.x, &jitter.y, frame, jitterPhaseCount);
      return jitter;
   }

   void RenderCamera::NextFrame() {
      prevView = view;
      prevProjection = projection;
      jitterPrev = jitter;
   }

   void RenderCamera::FillSCameraCB(SCameraCB& cameraCB) const {
      cameraCB.view = view;
      cameraCB.invView = glm::inverse(cameraCB.view);

      cameraCB.projection = projection;

      cameraCB.viewProjection = GetViewProjection();
      cameraCB.viewProjectionJitter = GetViewProjectionJitter();
      cameraCB.invViewProjection = glm::inverse(cameraCB.viewProjection);
      cameraCB.invViewProjectionJitter = glm::inverse(cameraCB.viewProjectionJitter);

      cameraCB.prevViewProjection = GetPrevViewProjection();
      cameraCB.prevInvViewProjection = glm::inverse(cameraCB.prevViewProjection);

      auto frustumCornerLeftUp = glm::inverse(projectionJitter) * vec4{-1, 1, 0, 1};
      frustumCornerLeftUp /= frustumCornerLeftUp.w;
      frustumCornerLeftUp /= frustumCornerLeftUp.z; // to 1 z plane

      auto frustumSize = glm::inverse(projection) * vec4 { 1, 1, 0, 1 };
      frustumSize /= frustumSize.w;
      frustumSize /= frustumSize.z; // to 1 z plane

      cameraCB.frustumLeftUp_Size = vec4{ frustumCornerLeftUp.x, frustumCornerLeftUp.y, frustumSize.x, frustumSize.y };

      // todo:
      cameraCB.position = position;
      cameraCB.forward = Forward();

      cameraCB.zNear = zNear;
      cameraCB.zFar = zFar;

      Frustum frustum{GetViewProjection()};
      memcpy(cameraCB.frustumPlanes, frustum.planes, sizeof(frustum.planes));
   }

   Renderer::~Renderer() {
      SAFE_DELETE(fsr3Upscaler);
   }

   void Renderer::Init() {
      rtRenderer.reset(new RTRenderer());

      const u32 underCursorSize = 1;
      underCursorBuffer = Buffer::Create(Buffer::Desc::Structured<u32>(underCursorSize).AllowUAV());
      underCursorBufferReadback = Buffer::Create(Buffer::Desc::Structured<u32>(underCursorSize).InReadback());
   }

   void Renderer::UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs) {
      if (!instanceBuffer || instanceBuffer->NumElements() < renderObjs.size()) {
         auto bufferDesc = Buffer::Desc::Structured("instance buffer", (u32)renderObjs.size(), sizeof(SInstance));
         instanceBuffer = Buffer::Create(bufferDesc);
      }

      std::vector<SInstance> instances;
      instances.reserve(renderObjs.size());
      for (auto& [trans, material] : renderObjs) {
         SMaterial m;
         m.baseColor = material.baseColor;
         m.roughness = material.roughness;
         m.metallic = material.metallic;
         m.emissivePower = material.emissivePower;

         SInstance instance;
         instance.transform = trans.GetWorldMatrix();
         instance.prevTransform = trans.GetPrevMatrix();
         instance.material = m;
         instance.entityID = (u32)trans.entity.GetEntityID();

         instances.emplace_back(instance);
      }

      cmd.UpdateBuffer(*instanceBuffer, 0, DataView{ instances });
   }

   void Renderer::RenderDataPrepare(CommandList& cmd, const Scene& scene, const RenderCamera& cullCamera) {
      opaqueObjs.clear();
      transparentObjs.clear();

      // todo:
      Frustum frustum{cullCamera.GetViewProjection()};

      for (auto [e, sceneTrans, material] :
           scene.View<SceneTransformComponent, MaterialComponent>().each()) {
         // DrawDesc desc;
         // desc.entityID = (u32)e;
         // desc.transform = sceneTrans.GetMatrix();
         // desc.material = { material, true };
         //
         // drawDescs.push_back(desc);

         if (cUseFrustumCulling) {
            vec3 scale = sceneTrans.Scale();
            // todo:
            float sphereRadius = glm::max(scale.x, scale.y);
            sphereRadius = glm::max(sphereRadius, scale.z) * 2;
            if (!frustum.SphereTest({sceneTrans.Position(),})) {
               continue;
            }
         }

         if (material.opaque) {
            opaqueObjs.emplace_back(sceneTrans, material);
         } else {
            transparentObjs.emplace_back(sceneTrans, material);
         }
      }

      if (!instanceBuffer || instanceBuffer->NumElements() < scene.EntitiesCount()) {
         auto bufferDesc = Buffer::Desc::Structured("instance buffer", scene.EntitiesCount(), sizeof(SInstance));
         instanceBuffer = Buffer::Create(bufferDesc);
      }

      {
         auto nLights = (u32)scene.CountEntitiesWithComponents<LightComponent>();

         std::vector<SLight> lights;
         lights.reserve(nLights);

         for (auto [e, trans, light] : scene.View<SceneTransformComponent, LightComponent>().each()) {
            SLight l;
            l.position = trans.Position();
            l.color = light.color * light.intensity;
            l.radius = light.radius;
            l.type = SLIGHT_TYPE_POINT;

            lights.emplace_back(l);
         }

         TryUpdateBuffer(cmd, lightBuffer, Buffer::Desc::Structured<SLight>(nLights).Name("light buffer"),
            std::span{ lights });
      }

      if (!ssaoRandomDirs) {
         int nRandomDirs = 32;
         auto bufferDesc = Buffer::Desc::Structured("ssao random dirs", nRandomDirs, sizeof(vec3));
         ssaoRandomDirs = Buffer::Create(bufferDesc);

         std::vector<vec3> dirs;
         dirs.reserve(nRandomDirs);

         for (int i = 0; i < nRandomDirs; ++i) {
            vec3 test;
            do {
               test = Random::Float3(vec3{-1}, vec3{1});
            } while (glm::length(test) > 1);

            dirs.emplace_back(test);
         }

         cmd.UpdateBuffer(*ssaoRandomDirs, 0, DataView{ dirs });
      }
   }

   void Renderer::RenderScene(CommandList& cmd, const Scene& scene, const RenderCamera& camera,
                              RenderContext& context) {
      COMMAND_LIST_SCOPE(cmd, "Render Scene");
      PROFILE_GPU("Render Scene");
      PIX_EVENT_SYSTEM(Render, "Render Scene");

      static RenderCamera cullCamera = camera;
      if (!cFreezeCullCamera) {
         cullCamera = camera;
      }

      RenderDataPrepare(cmd, scene, cullCamera);

      // todo: may be skipped
      cmd.ClearRenderTarget(*context.colorLDR, vec4{0, 0, 0, 1});
      cmd.ClearRenderTarget(*context.colorHDR, vec4{0, 0, 0, 1});
      cmd.ClearRenderTarget(*context.normalTex, vec4{0, 0, 0, 0});
      cmd.ClearRenderTarget(*context.baseColorTex, vec4{0, 0, 0, 1});
      cmd.ClearRenderTarget(*context.motionTex, vec4{0, 0, 0, 0});
      cmd.ClearRenderTarget(*context.outlineTex, vec4{0, 0, 0, 0});

      // todo: NRD dont work with black textures
      // todo: only in editor
      // cmd.ClearUAVFloat(*context.diffuseHistoryTex, vec4{ 0, 0, 0, 0 });
      // cmd.ClearUAVFloat(*context.specularHistoryTex, vec4{ 0, 0, 0, 0 });

      cmd.ClearRenderTarget(*context.viewz, vec4{1000000.0f, 0, 0, 0}); // todo: why it defaults for NRD?
      cmd.ClearDepthStencil(*context.depth, 1);

      // if (context.IsUpscaleNeeded())
      {
         cmd.ClearDepthStencil(*context.depthDisplay, 1);
         cmd.ClearRenderTarget(*context.motionDisplay, vec4{ 0, 0, 0, 0 });
      }

      cmd.SetRasterizerState(rendres::rasterizerState);

      u32 nDecals = 0;
      if (cvRenderDecals) {
         std::vector<SDecal> decals;

         MaterialComponent decalDefault{}; // todo:
         for (auto [e, trans, decal] : scene.View<SceneTransformComponent, DecalComponent>().each()) {
            decalObjs.emplace_back(trans, decalDefault);

            vec3 size = trans.Scale() * 0.5f;

            mat4 view = glm::lookAt(trans.Position(), trans.Position() + trans.Forward(), trans.Up());
            mat4 projection = glm::ortho(-size.x, size.x, -size.y, size.y, -size.z, size.z);
            mat4 viewProjection = projection * view;
            decals.emplace_back(viewProjection, decal.baseColor, decal.metallic, decal.roughness);
         }

         nDecals = (u32)decals.size();

         TryUpdateBuffer(cmd, decalBuffer, Buffer::Desc::Structured<SDecal>(nDecals).Name("decals buffer"),
            std::span{ decals });
      }

      RenderCamera shadowCamera;

      SSceneCB sceneCB;

      static int iFrame = 0; // todo:
      ++iFrame;
      sceneCB.iFrame = iFrame;
      sceneCB.deltaTime = 1 / 60.0f; // todo:
      sceneCB.time = sceneCB.deltaTime * sceneCB.iFrame;

      sceneCB.animationTime = std::invoke([&] {
         static float animTime = 0;
         if (animationTimeUpdate) {
            animTime += sceneCB.deltaTime;
         }
         return animTime;
      });

      sceneCB.fogNSteps = fogNSteps;

      sceneCB.nLights = scene.CountEntitiesWithComponents<LightComponent>();
      sceneCB.nDecals = (int)nDecals;

      sceneCB.exposition = tonemapExposition;

      sceneCB.directLight.color = {};
      sceneCB.directLight.direction = vec3{1, 0, 0};
      sceneCB.directLight.type = SLIGHT_TYPE_DIRECT;

      bool hasDirectLight = false;
      if (Entity directEntity = Entity::GetAnyWithComponent<DirectLightComponent>(scene)) {
         hasDirectLight = true;

         auto& trans = directEntity.GetTransform();
         auto& directLight = directEntity.Get<DirectLightComponent>();

         sceneCB.directLight.color = directLight.color * directLight.intensity;
         sceneCB.directLight.direction = trans.Forward();

         const float halfSize = 25;
         const float halfDepth = 50;

         auto shadowSpace = glm::lookAt({}, sceneCB.directLight.direction, trans.Up());
         vec3 posShadowSpace = shadowSpace * vec4(camera.position, 1);

         vec2 shadowMapTexels = context.shadowMap->GetDesc().size;
         vec3 shadowTexelSize = vec3{2.f * halfSize / shadowMapTexels, 2.f * halfDepth / (1 << 16)};
         vec3 snappedPosShadowSpace = glm::ceil(posShadowSpace / shadowTexelSize) * shadowTexelSize;

         vec3 snappedPosW = glm::inverse(shadowSpace) * vec4(snappedPosShadowSpace, 1);

         shadowCamera.position = snappedPosW;

         shadowCamera.projection = glm::ortho<float>(-halfSize, halfSize, -halfSize, halfSize, -halfDepth, halfDepth);
         shadowCamera.view = glm::lookAt(shadowCamera.position, shadowCamera.position + sceneCB.directLight.direction,
                                         trans.Up());
         sceneCB.toShadowSpace = NDCToTexSpaceMat4() * shadowCamera.GetViewProjection();
      }

      if (Entity skyEntity = Entity::GetAnyWithComponent<SkyComponent>(scene)) {
         const auto& sky = skyEntity.Get<SkyComponent>();

         sceneCB.skyIntensity = sky.intensity;
      } else {
         sceneCB.skyIntensity = 0;
      }

      cmd.AllocAndSetCB({CB_SLOT_SCENE, REGISTER_SPACE_COMMON}, sceneCB);

      if (cvRenderOpaqueSort) {
         // todo: slow. I assumed
         std::ranges::sort(opaqueObjs, [&](const RenderObject& a, const RenderObject& b) {
            float az = glm::dot(camera.Forward(), a.trans.Position());
            float bz = glm::dot(camera.Forward(), b.trans.Position());
            return az < bz;
         });
      }
      UpdateInstanceBuffer(cmd, opaqueObjs);

      cmd.ClearUAVFloat(context.ssao->GetUAV(), vec4_One);
      cmd.UpdateBuffer(*underCursorBuffer, 0, u32{UINT32_MAX});

      context.underCursorBuffer = underCursorBuffer;

      SCameraCB cameraCB;
      camera.FillSCameraCB(cameraCB);
      cameraCB.rtSize = context.colorHDR->GetDesc().size;
      cmd.AllocAndSetCB({CB_SLOT_CAMERA, REGISTER_SPACE_COMMON}, cameraCB);

      {
         SCameraCB cullCameraCB;
         cullCamera.FillSCameraCB(cullCameraCB);
         cullCameraCB.rtSize = context.colorHDR->GetDesc().size;
         cmd.AllocAndSetCB({CB_SLOT_CULL_CAMERA, REGISTER_SPACE_COMMON}, cullCameraCB);
      }

      SEditorCB editorCB;
      editorCB.cursorPixelIdx = context.cursorPixelIdx;
      cmd.AllocAndSetCB({CB_SLOT_EDITOR, REGISTER_SPACE_COMMON}, editorCB);

      cmd.SetSRV({SRV_SLOT_LIGHTS, REGISTER_SPACE_COMMON }, lightBuffer);

      cmd.SetViewport({}, context.colorHDR->GetDesc().size);
      cmd.SetBlendState(rendres::blendStateDefaultRGBA);

      cmd.SetUAV({ UAV_SLOT_UNDER_CURSOR_BUFFER, REGISTER_SPACE_COMMON }, context.underCursorBuffer);

      {
         COMMAND_LIST_SCOPE(cmd, "ZPass");
         PROFILE_GPU("ZPass");

         // todo: without normal?
         cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
         cmd.SetRenderTarget(context.normalTex, context.depth, true);

         auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ZPassPS");
         programDesc.vs.defines.AddDefine("ZPASS");
         programDesc.ps.defines.AddDefine("ZPASS");
         auto baseZPass = GetGpuProgram(programDesc);

         RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass);
      }

      // todo: mb skip two that same render?
      // todo: can write this in ZPass
      // if (context.IsUpscaleNeeded())
      {
         COMMAND_LIST_SCOPE(cmd, "Depth&Motion display");
         PROFILE_GPU("Depth&Motion display");

         // todo: without normal?
         cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
         cmd.SetRenderTarget(context.motionDisplay, context.depthDisplay, true);
         cmd.SetViewport({}, context.motionDisplay->GetDesc().size);

         auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "MotionPS");
         programDesc.vs.defines.AddDefine("DEPTH_MOTION_DISPLAY");
         programDesc.ps.defines.AddDefine("DEPTH_MOTION_DISPLAY");
         auto baseZPass = GetGpuProgram(programDesc);

         RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass);
      }

      {
         COMMAND_LIST_SCOPE(cmd, "GBuffer");
         PROFILE_GPU("GBuffer");

         cmd.SetViewport({}, context.depth->GetDesc().size);
         cmd.SetDepthStencilState(rendres::depthStencilStateEqual);

         Texture2D* rts[] = {
            context.colorHDR, context.baseColorTex,
            context.normalTex, context.motionTex, context.viewz
         };
         u32 nRts = _countof(rts);
         cmd.SetRenderTargets(nRts, rts, context.depth);

         auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
         programDesc.vs.defines.AddDefine("GBUFFER");
         programDesc.ps.defines.AddDefine("GBUFFER");

         auto baseZPass = GetGpuProgram(programDesc);
         RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass);

         {
            COMMAND_LIST_SCOPE(cmd, "Grass");

            bool grassOutputGeom = true;

            if (grassOutputGeom) {
               if (!context.grassCounters) {
                  context.grassCounters = Buffer::Create(Buffer::Desc::Structured<u32>(2).AllowUAV().Name("grass counters"));
                  context.grassReadback = Buffer::Create(Buffer::Desc::Structured<u32>(2).InReadback().Name("grass readback"));
                  context.grassVertexes = Buffer::Create(Buffer::Desc::Structured<vec3>(100 * 1000).AllowUAV().Name("grass vertexes"));
                  context.grassIndexes = Buffer::Create(Buffer::Desc::Structured<u32>(200 * 1000).AllowUAV().Name("grass indexes"));
               }

               cmd.ClearUAVUint(context.grassCounters->GetClearUAVUint());

               float Nan = std::numeric_limits<float>::quiet_NaN();
               cmd.ClearUAVFloat(context.grassVertexes->GetClearUAVFloat(), {Nan, Nan, Nan, Nan});
               cmd.ClearUAVUint(context.grassIndexes->GetClearUAVUint());
            }
            cmd.SetRenderTargets(nRts, rts, context.depth, true);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);

            auto pass = GetGpuProgram(ProgramDesc::MsPs("meshShaderTest.hlsl", "MS", "PS"));
            cmd.SetProgram(pass);

            auto rasterState = rendres::rasterizerState;
            rasterState.CullMode = D3D12_CULL_MODE_NONE;
            cmd.SetRasterizerState(rasterState);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
            cmd.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

            if (grassOutputGeom) {
               cmd.SetUAV(pass->GetBindPoint("gVertices"), context.grassVertexes);
               cmd.SetUAV(pass->GetBindPoint("gIndexes"), context.grassIndexes);
               cmd.SetUAV(pass->GetBindPoint("gCounter"), context.grassCounters);
            }

            // cmd.DispatchMesh(1);
            cmd.DispatchMesh(100, 100);

            cmd.CopyBufferRegion(*context.grassReadback, 0, *context.grassCounters, 0, sizeof(u32) * 2);

            // todo: use flip flop
            // todo: log to window
            u32* pCounters = (u32*)context.grassReadback->Map();
            // INFO("Grass counters: {}, {}", pCounters[0], pCounters[1]);
            context.grassReadback->Unmap();
         }

         cmd.SetRenderTarget();
      }

      if (rayTracingSceneRender) {
         rtRenderer->RenderScene(cmd, scene, camera, context);
      } else {
         if (cvRenderShadowMap && hasDirectLight) {
            COMMAND_LIST_SCOPE(cmd, "Shadow Map");
            PROFILE_GPU("Shadow Map");

            cmd.ClearDepthStencil(*context.shadowMap, 1);

            cmd.SetRenderTarget(nullptr, context.shadowMap);
            cmd.SetViewport({}, context.shadowMap->GetDesc().size);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);
            cmd.SetBlendState(rendres::blendStateDefaultRGBA);

            SCameraCB shadowCameraCB;
            shadowCamera.FillSCameraCB(shadowCameraCB);
            // shadowCameraCB.rtSize = context.colorHDR->GetDesc().size;

            cmd.AllocAndSetCB({CB_SLOT_CAMERA, REGISTER_SPACE_COMMON}, shadowCameraCB);

            auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main");
            programDesc.vs.defines.AddDefine("ZPASS");
            auto shadowMapPass = GetGpuProgram(programDesc);
            RenderSceneAllObjects(cmd, opaqueObjs, *shadowMapPass);

            cmd.SetRenderTarget();
            cmd.SetSRV({SRV_SLOT_SHADOWMAP, REGISTER_SPACE_COMMON }, context.shadowMap);
         }

         cmd.AllocAndSetCB({CB_SLOT_CAMERA, REGISTER_SPACE_COMMON}, cameraCB); // todo: set twice

         cmd.SetViewport({}, context.colorHDR->GetDesc().size); /// todo:

         if (cvRenderSsao) {
            COMMAND_LIST_SCOPE(cmd, "SSAO");
            PROFILE_GPU("SSAO");

            cmd.SetRenderTarget();

            auto ssaoPass = GetGpuProgram(ProgramDesc::Cs("ssao.hlsl", "main"));

            ssaoPass->Activate(cmd);

            ssaoPass->SetSRV(cmd, "gDepth", *context.depth);
            ssaoPass->SetSRV(cmd, "gRandomDirs", *ssaoRandomDirs);
            ssaoPass->SetSRV(cmd, "gNormal", *context.normalTex);
            ssaoPass->SetUAV(cmd, "gSsao", *context.ssao);

            cmd.Dispatch2D(glm::ceil(vec2{context.colorHDR->GetDesc().size} / vec2{8}));
         }

         cmd.SetSRV({ SRV_SLOT_LIGHTS, REGISTER_SPACE_COMMON }, lightBuffer);

         {
            COMMAND_LIST_SCOPE(cmd, "Deferred");
            PROFILE_GPU("Deferred");

            cmd.SetRenderTarget();

            auto pass = GetGpuProgram(ProgramDesc::Cs("deferred.hlsl", "DeferredCS"));

            pass->Activate(cmd);

            pass->SetSRV(cmd, "gBaseColor", *context.baseColorTex);
            pass->SetSRV(cmd, "gNormal", *context.normalTex);
            pass->SetSRV(cmd, "gViewZ", *context.viewz);
            pass->SetUAV(cmd, "gColorOut", *context.colorHDR);

            cmd.Dispatch2D(context.colorHDR->GetDesc().size, int2{8});
         }

         terrainSystem.Render(cmd, scene, context);
         waterSystem.Render(cmd, scene, context);

         if (cvRenderTransparency && !transparentObjs.empty()) {
            COMMAND_LIST_SCOPE(cmd, "Transparency");
            PROFILE_GPU("Transparency");

            cmd.SetRenderTarget(context.colorHDR, context.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadNoWrite);
            cmd.SetBlendState(rendres::blendStateTransparencyRGB);

            if (cvRenderTransparencySort) {
               // todo: slow. I assumed
               std::ranges::sort(transparentObjs, [&](RenderObject& a, RenderObject& b) {
                  float az = glm::dot(camera.Forward(), a.trans.Position());
                  float bz = glm::dot(camera.Forward(), b.trans.Position());
                  return az > bz;
               });
            }

            auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
            programDesc.ps.defines.AddDefine("TRANSPARENT");
            auto transparentPass = GpuProgram::Create(programDesc);

            UpdateInstanceBuffer(cmd, transparentObjs);
            RenderSceneAllObjects(cmd, transparentObjs, *transparentPass);
         }

         if (0) {
            // todo
            COMMAND_LIST_SCOPE(cmd, "Linearize Depth");
            PROFILE_GPU("Linearize Depth");

            cmd.SetRenderTarget();

            auto linearizeDepthPass = GetGpuProgram(ProgramDesc::Cs("linearizeDepth.hlsl", "main"));
            linearizeDepthPass->Activate(cmd);

            UNIMPLEMENTED();
            linearizeDepthPass->SetSRV(cmd, "gDepth", *context.depth);
            // linearizeDepthPass->SetUAV_Dx11(cmd, "gDepthOut", context.linearDepth->GetMipUav(0));

            cmd.Dispatch2D(context.depth->GetDesc().size, int2{8});
         }

         if (depthDownsampleEnable) {
            COMMAND_LIST_SCOPE(cmd, "Downsample Depth");
            PROFILE_GPU("Downsample  Depth");

            cmd.SetRenderTarget();

            auto downsampleDepthPass = GetGpuProgram(ProgramDesc::Cs("linearizeDepth.hlsl", "downsampleDepth"));
            downsampleDepthPass->Activate(cmd);

            auto& texture = context.linearDepth;

            int nMips = texture->GetDesc().mips;
            int2 size = texture->GetDesc().size;

            for (int iMip = 0; iMip < nMips - 1; ++iMip) {
               size /= 2;

               UNIMPLEMENTED();

               // downsampleDepthPass->SetSRV_Dx11(cmd, "gDepth", texture->GetMipSrv(iMip));
               // downsampleDepthPass->SetUAV_Dx11(cmd, "gDepthOut", texture->GetMipUav(iMip + 1));

               cmd.Dispatch2D(size, int2{8});
            }
         }

         if (applyFog) {
            COMMAND_LIST_SCOPE(cmd, "Fog");
            PROFILE_GPU("Fog");

            cmd.SetRenderTarget();

            auto fogPass = GetGpuProgram(ProgramDesc::Cs("fog.hlsl", "main"));

            fogPass->Activate(cmd);

            fogPass->SetSRV(cmd, "gDepth", *context.depth);
            fogPass->SetUAV(cmd, "gColor", *context.colorHDR);

            cmd.Dispatch2D(context.colorHDR->GetDesc().size, int2{8});
         }
      }

      Ref<Texture2D> displayOutput = context.colorHDR;
      if (cvEnableUpscale) {
         COMMAND_LIST_SCOPE(cmd, "Upscale");

         auto renderSize = context.colorHDR->GetDesc().size;
         auto displaySize = context.colorHDR_Upscaled->GetDesc().size;

         displayOutput = context.colorHDR_Upscaled;

         if (!fsr3Upscaler) {
            // fsr3Upscaler = std::make_unique<Fsr3Upscaler>();
            fsr3Upscaler = new Fsr3Upscaler();
         }
         fsr3Upscaler->CreateContext(displaySize, context);

         cmd.TransitionBarrier(*context.colorHDR, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
         cmd.TransitionBarrier(*context.depth, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
         cmd.TransitionBarrier(*context.motionTex, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
         cmd.TransitionBarrier(*context.colorHDR_Upscaled, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
         cmd.FlushResourceBarriers();

         FfxFsr3UpscalerDispatchDescription dispatchParameters = {};
         dispatchParameters.commandList = ffxGetCommandListDX12(cmd.GetD3D12CommandList().Get());
         dispatchParameters.color = GetFxxResource(context.colorHDR, (wchar_t*)L"FSR3_Input_OutputColor");
         dispatchParameters.depth = GetFxxResource(context.depth, (wchar_t*)L"FSR3_InputDepth");
         dispatchParameters.motionVectors = GetFxxResource(context.motionDisplay, (wchar_t*)L"FSR3_InputMotionVectors");
         dispatchParameters.exposure = GetFxxResource(nullptr, (wchar_t*)L"FSR3_InputExposure");
         dispatchParameters.output = GetFxxResource(context.colorHDR_Upscaled, (wchar_t*)L"FSR3_Output_Output");

         // todo: handle tarnsparency
         dispatchParameters.reactive = GetFxxResource(nullptr, (wchar_t*)L"FSR3_EmptyInputReactiveMap");
         dispatchParameters.transparencyAndComposition = GetFxxResource(nullptr, (wchar_t*)L"FSR3_EmptyTransparencyAndCompositionMap");

         dispatchParameters.dilatedDepth = GetFxxResource(context.dilatedDepth, (wchar_t*)L"FSR3_EmptyDiaDepth");
         dispatchParameters.dilatedMotionVectors = GetFxxResource(context.dilatedMotionVectors, (wchar_t*)L"FSR3_EmptyDialMotionVec");
         dispatchParameters.reconstructedPrevNearestDepth = GetFxxResource(context.reconstructedPrevNearestDepth, (wchar_t*)L"FSR3_EmptyRecPrevDepth");

         dispatchParameters.dilatedDepth.state = FFX_RESOURCE_STATE_UNORDERED_ACCESS;
         dispatchParameters.dilatedMotionVectors.state = FFX_RESOURCE_STATE_UNORDERED_ACCESS;
         dispatchParameters.reconstructedPrevNearestDepth.state = FFX_RESOURCE_STATE_UNORDERED_ACCESS;

         cmd.TrackResource(*context.dilatedDepth);
         cmd.TrackResource(*context.dilatedMotionVectors);
         cmd.TrackResource(*context.reconstructedPrevNearestDepth);

         dispatchParameters.jitterOffset.x = camera.jitter.x;
         dispatchParameters.jitterOffset.y = -camera.jitter.y;
         dispatchParameters.motionVectorScale.x = (float)displaySize.x;
         dispatchParameters.motionVectorScale.y = (float)displaySize.y;
         dispatchParameters.reset = false;
         dispatchParameters.enableSharpening = cvEnableSharpening;
         dispatchParameters.sharpness = cvEnableSharpness;

         // todo:
         float dt = 1.f / 60;
         dispatchParameters.frameTimeDelta = dt * 1000.f; // FSR expects miliseconds

         dispatchParameters.preExposure = 1;// GetScene()->GetSceneExposure();
         dispatchParameters.renderSize.width = renderSize.x;
         dispatchParameters.renderSize.height = renderSize.y;

         static bool s_InvertedDepth = false;
         // Setup camera params as required

         float aspectRatio = (float)renderSize.x / (float)renderSize.y;
         dispatchParameters.cameraFovAngleVertical = camera.fov / aspectRatio;
         ASSERT(camera.fov > 0);
         if (s_InvertedDepth) {
            UNIMPLEMENTED();
            // dispatchParameters.cameraFar = pCamera->GetNearPlane();
            // dispatchParameters.cameraNear = FLT_MAX;
         } else {
            dispatchParameters.cameraFar = camera.zFar;
            dispatchParameters.cameraNear = camera.zNear;
         }

         fsr3Upscaler->Upscale(dispatchParameters);

         cmd.ResetDescriptorHeaps();
         cmd.ResetRootSignature();
      }

      cmd.AllocAndSetCB({ CB_SLOT_SCENE, REGISTER_SPACE_COMMON }, sceneCB);

      // if (cfg.ssao)
      {
         COMMAND_LIST_SCOPE(cmd, "Tonemap");
         PROFILE_GPU("Tonemap");

         cmd.SetRenderTarget();

         auto tonemapPass = GetGpuProgram(ProgramDesc::Cs("tonemap.hlsl", "main"));

         tonemapPass->Activate(cmd);

         tonemapPass->SetSRV(cmd, "gColorHDR", *displayOutput); // todo:
         tonemapPass->SetUAV(cmd, "gColorLDR", *context.colorLDR);

         cmd.Dispatch2D(displayOutput->GetDesc().size, int2{8});
      }

      if (cvOutlineEnable) {
         COMMAND_LIST_SCOPE(cmd, "Outline");
         PROFILE_GPU("Outline");

         {
            COMMAND_LIST_SCOPE(cmd, "Render");
            PROFILE_GPU("Render");

            cmd.SetRenderTarget(&*context.outlineTex);
            cmd.SetViewport({}, context.outlineTex->GetDesc().size);

            RenderOutlines(cmd, scene);
         }

         {
            COMMAND_LIST_SCOPE(cmd, "Blur");
            PROFILE_GPU("Blur");

            cmd.SetRenderTarget();

            auto pass = GetGpuProgram(ProgramDesc::Cs("outline.hlsl", "OutlineBlurCS"));

            pass->Activate(cmd);

            pass->SetSRV(cmd, "gOutline", *context.outlineTex);
            pass->SetUAV(cmd, "gOutlineBlurOut", *context.outlineBlurredTex);

            cmd.Dispatch2D(context.outlineTex->GetDesc().size, int2{8});
         }

         {
            COMMAND_LIST_SCOPE(cmd, "Apply");
            PROFILE_GPU("Apply");

            cmd.SetRenderTarget();

            auto pass = GetGpuProgram(ProgramDesc::Cs("outline.hlsl", "OutlineApplyCS"));

            pass->Activate(cmd);

            pass->SetSRV(cmd, "gOutline", *context.outlineTex);
            pass->SetSRV(cmd, "gOutlineBlur", *context.outlineBlurredTex);
            pass->SetUAV(cmd, "gSceneOut", *context.colorLDR);

            cmd.Dispatch2D(context.outlineTex->GetDesc().size, int2{8});
         }
      }

      if (dbgRenderEnable) {
         COMMAND_LIST_SCOPE(cmd, "Dbg Rend");
         PROFILE_GPU("Dbg Rend");

         DbgRend& dbgRend = *GetDbgRend(scene);

         // int size = 50;
         //
         // for (int i = -size; i <= size; ++i) {
         //    dbgRend.DrawLine(vec3{ i, 0, -size }, vec3{ i, 0, size });
         // }
         //
         // for (int i = -size; i <= size; ++i) {
         //    dbgRend.DrawLine(vec3{ -size, 0, i }, vec3{ size, 0, i });
         // }

         // AABB aabb{ vec3_One, vec3_One * 3.f };
         // dbgRend.DrawAABB(aabb);

         // auto shadowInvViewProjection = glm::inverse(shadowCamera.GetViewProjection());
         // dbgRend.DrawViewProjection(shadowInvViewProjection);

         if (cFreezeCullCamera) {
            auto cullCameraInvViewProjection = glm::inverse(cullCamera.GetViewProjection());
            dbgRend.DrawViewProjection(cullCameraInvViewProjection, Color_White.WithAlpha(0.1f));

            // dbgRend.DrawFrustum(Frustum{ cullCamera.GetViewProjection() },cullCamera.position, cullCamera.Forward());  
         }

         for (auto [entityID, trans, light] : scene.View<SceneTransformComponent, LightComponent>().each()) {
            dbgRend.DrawSphere({trans.Position(), light.radius}, light.color, true, entityID);
         }

         for (auto [entityID, trans, volume] : scene.View<SceneTransformComponent, RTImportanceVolumeComponent>().
                                                     each()) {
            dbgRend.DrawSphere({trans.Position(), volume.radius}, Color_Magenta, true, entityID);
         }

         for (auto [entityID, trans, light] : scene.View<SceneTransformComponent, TriggerComponent>().each()) {
            // todo: OBB
            // todo: box, sphere, capsule
            dbgRend.DrawAABB(
               nullptr, {trans.Position() - trans.Scale() * 0.5f, trans.Position() + trans.Scale() * 0.5f});
         }

         for (auto [entityID, joint] : scene.View<JointComponent>().each()) {
            if (!joint.IsValid()) {
               continue;
            }

            dbgRend.DrawLine(joint.entity0, joint.entity1, Color_White, false, entityID);

            auto trans0 = joint.GetAnchorTransform(JointComponent::Anchor::Anchor0);
            auto trans1 = joint.GetAnchorTransform(JointComponent::Anchor::Anchor1);

            if (!trans0 || !trans1) {
               continue;
            }

            auto pos0 = trans0->position;
            auto pos1 = trans1->position;
            auto dir = glm::normalize(pos1 - pos0);

            auto sphere = Sphere{pos0, 0.2f};

            Color defColor = Color_Blue;
            if (joint.type == JointType::Fixed) {
               dbgRend.DrawSphere(sphere, defColor, false, entityID);
            } else if (joint.type == JointType::Distance) {
               auto posMin = pos0 + dir * joint.distance.minDistance;
               auto posMax = pos0 + dir * joint.distance.maxDistance;
               dbgRend.DrawLine(posMin, posMax, defColor, false, entityID);
            } else if (joint.type == JointType::Revolute) {
               dbgRend.DrawLine(pos0, pos0 + trans0->Right() * 3.f, Color_Red, false, entityID);
               dbgRend.DrawSphere(sphere, defColor, false, entityID);
            } else if (joint.type == JointType::Spherical) {
               dbgRend.DrawSphere(sphere, defColor, false, entityID);
            } else if (joint.type == JointType::Prismatic) {
               dbgRend.DrawLine(
                  pos0 + trans0->Right() * joint.prismatic.lowerLimit,
                  pos0 + trans0->Right() * joint.prismatic.upperLimit,
                  defColor, false, entityID);
            } else {
               UNIMPLEMENTED();
            }
         }

         if (cvDestructDbgRend) {
            DestructDbgRendFlags flags =
               DestructDbgRendFlags::Chunks | DestructDbgRendFlags::ChunksCentroid
               | DestructDbgRendFlags::BondsHealth | DestructDbgRendFlags::BondsCentroid;

            for (auto [_, rb] : scene.View<RigidBodyComponent>().each()) {
               rb.DbgRender(dbgRend, flags);
            }
         }

         cmd.SetRenderTarget(context.colorLDR, context.depthDisplay);
         cmd.SetViewport({}, context.colorHDR->GetDesc().size); /// todo:

         dbgRend.Render(cmd, camera);
         dbgRend.Clear();
      }
   }

   void Renderer::RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs,
                                        GpuProgram& program) {
      program.Activate(cmd);
      program.SetSRV(cmd, "gInstances", *instanceBuffer);

      cmd.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      cmd.SetInputLayout(VertexPosNormal::inputElementDesc);

      auto& mesh = rendres::CubeMesh();

      cmd.SetVertexBuffer(0, *mesh.vertexBuffer, mesh.geom.nVertexByteSize);
      cmd.SetIndexBuffer(*mesh.indexBuffer, DXGI_FORMAT_R16_UINT);

      int instanceID = 0;

      for (const auto& [trans, material] : renderObjs) {
         SDrawCallCB cb;
         cb.instance.transform = glm::translate(mat4(1), trans.Position());
         cb.instance.transform = cb.instance.transform;
         cb.instance.material.roughness = material.roughness;
         cb.instance.material.baseColor = material.baseColor;
         cb.instance.material.metallic = material.metallic;
         cb.instance.entityID = (u32)trans.entity.GetEntityID();
         cb.instanceStart = instanceID++;
         // cb.rdhInstances = instanceBuffer->GetGpuSrv().offset;

         auto dynCB = cmd.AllocDynConstantBuffer(cb);
         program.SetCB<SDrawCallCB>(cmd, "gDrawCallCB", *dynCB.buffer, dynCB.offset);

         if (instancedDraw) {
            if (indirectDraw) {
               // DrawIndexedInstancedArgs args{ (u32)mesh.geom.IndexCount(), 0, 0, 0, 0 };
               // auto dynArgs = cmd.AllocDynDrawIndexedInstancedBuffer(args, 1);
               //
               // if (1) {
               //    auto indirectArgsTest = GetGpuProgram(ProgramDesc::Cs("cull.hlsl", "indirectArgsTest"));
               //
               //    indirectArgsTest->Activate(cmd);
               //
               //    indirectArgsTest->SetUAV(cmd, "gIndirectArgsInstanceCount", *dynArgs.buffer);
               //
               //    uint4 offset{ dynArgs.offset / sizeof(u32), (u32)renderObjs.size(), 0, 0};
               //    auto testCB = cmd.AllocDynConstantBuffer(offset);
               //    // todo: it removes base.hlsl cb
               //    indirectArgsTest->SetCB<uint4>(cmd, "gTestCB", *testCB.buffer, testCB.offset);
               //
               //    cmd.Dispatch1D(1);
               // }
               //
               // program.DrawIndexedInstancedIndirect(cmd, *dynArgs.buffer, dynArgs.offset);
            } else {
               program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount(), (u32)renderObjs.size());
            }
            break;
         } else {
            // program->DrawInstanced(cmd, mesh.geom.VertexCount());
            program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
         }
      }
   }

   void Renderer::RenderOutlines(CommandList& cmd, const Scene& scene) {
      auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
      programDesc.vs.defines.AddDefine("OUTLINES");
      programDesc.ps.defines.AddDefine("OUTLINES");

      auto program = *GetGpuProgram(programDesc);

      program.Activate(cmd);

      cmd.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      cmd.SetInputLayout(VertexPosNormal::inputElementDesc);

      auto& mesh = rendres::CubeMesh();

      cmd.SetVertexBuffer(0, *mesh.vertexBuffer, mesh.geom.nVertexByteSize);
      cmd.SetIndexBuffer(*mesh.indexBuffer, DXGI_FORMAT_R16_UINT);

      for (auto [entityID, trans, outline]
           : scene.View<SceneTransformComponent, OutlineComponent>().each()) {
         Entity entity{entityID, const_cast<Scene*>(&scene)}; // todo:
         if (entity.Has<GeometryComponent>()) {
            SDrawCallCB cb;
            cb.instance.transform = trans.GetWorldMatrix();
            cb.instance.material.baseColor = outline.color;
            cb.instance.entityID = (u32)trans.entity.GetEntityID();

            auto dynCB = cmd.AllocDynConstantBuffer(cb);
            program.SetCB<SDrawCallCB>(cmd, "gDrawCallCB", *dynCB.buffer, dynCB.offset);

            program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
         }
      }
   }

   u32 Renderer::GetEntityIDUnderCursor() {
      CommandQueue& commandQueue = sDevice->GetCommandQueue();

      commandQueue.ExecuteImmediate(
      [&](CommandList& cmd) {
            COMMAND_LIST_SCOPE(cmd, "Copy under cursor data");
            cmd.CopyResource(*underCursorBufferReadback, *underCursorBuffer);
      });

      commandQueue.Flush();

      u32 entityID;
      underCursorBufferReadback->Readback(entityID);
      return entityID;
   }
}
