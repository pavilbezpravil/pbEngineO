#pragma once

#include "Buffer.h"
#include "Device.h"
#include "RTRenderer.h"
#include "Texture2D.h"
#include "Shader.h"
#include "math/Types.h"
#include "scene/Component.h"

#include "system/Terrain.h"
#include "system/Water.h"

struct SCameraCB;

namespace pbe {
   class Fsr3Upscaler;

   template <typename T>
   class CVarValue;

   extern CORE_API CVarValue<bool> rayTracingSceneRender;

   // todo: move to utils
   vec2 UVToNDC(const vec2& uv);
   vec2 NDCToUV(const vec2& ndc);
   vec3 GetWorldPosFromUV(const vec2& uv, const mat4& invViewProj);

   vec2 CORE_API GetFsr3Jitter(u32 renderWidth, u32 displayWidth, u32 frame);

   struct CORE_API RenderCamera {
      vec3 position{};

      mat4 view{};
      mat4 projection{};
      mat4 projectionJitter{};

      mat4 prevView{};
      mat4 prevProjection{};

      float zNear = 0.1f;
      float zFar = 1000.f;

      float fov = -1;

      vec2 jitter = {0, 0};
      vec2 jitterPrev = {0, 0};

      void UpdateProjectionJitter(uint2 renderSize) {
         const float jitterX = 2.0f * jitter.x / (float)renderSize.x;
         const float jitterY = 2.0f * jitter.y / (float)renderSize.y;
         mat4 jitterTranslationMatrix = glm::translate(mat4(1), vec3(jitterX, jitterY, 0));
         projectionJitter =  jitterTranslationMatrix * projection;
      }

      vec3 Right() const {
         return vec3{ view[0][0], view[1][0] , view[2][0] };
      }

      vec3 Up() const {
         return vec3{ view[0][1], view[1][1] , view[2][1] };
      }

      vec3 Forward() const {
         return vec3{view[0][2], view[1][2] , view[2][2] };
      }

      mat4 GetViewProjection() const {
         return projection * view;
      }

      mat4 GetViewProjectionJitter() const {
         return projectionJitter * view;
      }

      mat4 GetPrevViewProjection() const {
         return prevProjection * prevView;
      }

      mat4 GetInvViewProjection() const {
         return glm::inverse(GetViewProjection());
      }

      vec3 GetWorldSpaceRayDirFromUV(const vec2& uv) const {
         vec3 worldPos = GetWorldPosFromUV(uv, GetInvViewProjection());
         return glm::normalize(worldPos - position);
      }

      void NextFrame();

      void UpdateProj(int2 size, float fov = 90.f / 180 * PI) {
         this->fov = fov;
         projection = glm::perspectiveFov(fov, (float)size.x, (float)size.y, zNear, zFar);
      }

      void UpdateViewByDirection(const vec3& direction, const vec3& up = vec3_Y) {
         view = glm::lookAt(position, position + direction, up);
      }

      void FillSCameraCB(SCameraCB& cameraCB) const;
   };

   class CORE_API Renderer {
   public:
      ~Renderer();

      Own<RTRenderer> rtRenderer;
      // std::unique_ptr<Fsr3Upscaler> fsr3Upscaler;
      Fsr3Upscaler* fsr3Upscaler = nullptr;

      Ref<Buffer> instanceBuffer;
      Ref<Buffer> decalBuffer;
      Ref<Buffer> lightBuffer;
      Ref<Buffer> ssaoRandomDirs;

      Ref<Buffer> underCursorBuffer;
      Ref<Buffer> underCursorBufferReadback;

      Water waterSystem;
      Terrain terrainSystem;

      struct RenderObject {
         // todo: component is overkill )
         // const SceneTransformComponent& trans;
         // const MaterialComponent& material;
         SceneTransformComponent trans;
         MaterialComponent material;
      };

      std::vector<RenderObject> opaqueObjs;
      std::vector<RenderObject> transparentObjs;
      std::vector<RenderObject> decalObjs;

      void Init();

      void UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs);
      void RenderDataPrepare(CommandList& cmd, const Scene& scene, const RenderCamera& cullCamera);

      void RenderScene(CommandList& cmd, const Scene& scene, const RenderCamera& camera, RenderContext& context);
      void RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs, GpuProgram& program);
      void RenderOutlines(CommandList& cmd, const Scene& scene);

      u32 GetEntityIDUnderCursor();
   };

}
