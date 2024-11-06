#pragma once

#include "DefaultVertex.h"
#include "core/Common.h"
#include "core/Core.h"
#include "math/Types.h"
#include "core/Ref.h"
#include "math/Color.h"
#include "scene/Entity.h"

namespace pbe {
   class Buffer;
   struct Transform;
   class Entity;

   struct AABB;
   struct Sphere;
   struct Frustum;
   struct RenderCamera;
   class GpuProgram;

   class CommandList;

   class CORE_API DbgRend {
   public:
      void DrawLine(const vec3& start, const vec3& end, const Color& color = Color_White, bool zTest = true, EntityID entityID = NullEntityID);
      void DrawTriangle(const vec3& v0, const vec3& v1, const vec3& v2, const Color& color = Color_White, bool zTest = true, EntityID entityID = NullEntityID);

      void DrawSphere(const Sphere& sphere, const Color& color = Color_White, bool zTest = true, EntityID entityID = NullEntityID);

      // trans - optional
      void DrawAABB(const Transform* trans, const AABB& aabb, const Color& color = Color_White, bool zTest = true, bool filled = true, EntityID entityID = NullEntityID);
      void DrawAABBOrderPoints(const vec3 points[8], const Color& color = Color_White, bool zTest = true, bool filled = true, EntityID entityID = NullEntityID);

      void DrawViewProjection(const mat4& invViewProjection, const Color& color = Color_White, bool zTest = true);
      void DrawFrustum(const Frustum& frustum, const vec3& pos, const vec3& forward, const Color& color = Color_White, bool zTest = true);

      void DrawLine(const Entity& entity0, const Entity& entity1, const Color& color = Color_White, bool zTest = true, EntityID entityID = NullEntityID);

      void Clear();

      void Render(CommandList& cmd, const RenderCamera& camera);

   private:
      std::vector<VertexPosUintColor> lines;
      std::vector<VertexPosUintColor> linesNoZ;

      std::vector<VertexPosUintColor> triangles;
      std::vector<VertexPosUintColor> trianglesNoZ;
   };

   CORE_API DbgRend* GetDbgRend(const Scene& scene);

   CORE_API void DbgDrawLine(const Scene& scene, const vec3& start, const vec3& end, const Color& color = Color_White, bool zTest = true, EntityID entityID = NullEntityID);
   CORE_API void DbgDrawTriangle(const Scene& scene, const vec3& v0, const vec3& v1, const vec3& v2, const Color& color = Color_White, bool zTest = true, EntityID entityID = NullEntityID);

   CORE_API void DbgDrawSphere(const Scene& scene, const Sphere& sphere, const Color& color = Color_White, bool zTest = true, EntityID entityID = NullEntityID);

}
