#pragma once

#include "Types.h"
#include "core/Core.h"

namespace pbe {

   struct CORE_API Ray {
      vec3 origin;
      vec3 direction;
   };

   struct CORE_API Sphere {
      vec3 center;
      float radius = 0.5f;
   };

   struct CORE_API AABB {
      vec3 min = vec3{ FLT_MAX };
      vec3 max = vec3{ -FLT_MAX };

      static AABB Empty();
      static AABB FromMinMax(const vec3& min, const vec3& max);
      static AABB FromExtends(const vec3& center, const vec3& extends);
      static AABB FromSize(const vec3& center, const vec3& size);
      static AABB FromAABBs(const AABB* aabbs, u32 size);

      static AABB FromUnion(const AABB& a, const AABB& b);

      void AddPoint(const vec3& p);
      void AddAABB(const AABB& aabb);

      void Translate(const vec3& v);
      void Expand(float expand); // increase size by expand in all directions

      vec3 Center() const;
      vec3 Size() const;
      vec3 Extents() const;

      vec3 Offset(const vec3& p) const { return (p - min) / Size(); }

      bool Contains(const vec3& p) const;
      bool Intersects(const AABB& aabb) const;

      float Volume() const;
      float Area() const;

      bool IsEmpty() const;
   };

   struct CORE_API Plane {
      vec3 normal;
      float d;

      static Plane FromPointNormal(const vec3& p, const vec3& n);
      static Plane FromPoints(const vec3& p0, const vec3& p1, const vec3& p2);

      float RayIntersectionT(const Ray& ray) const {
         return -Distance(ray.origin) / glm::dot(normal, ray.direction);
      }

      vec3 RayIntersectionAt(const Ray& ray) const {
         float t = RayIntersectionT(ray);
         return ray.origin + ray.direction * t;
      }

      vec3 Project(const vec3& p) const {
         return p - normal * Distance(p);
      }

      float Distance(vec3 p) const {
         return glm::dot(normal, p) + d;
      }
   };

   struct CORE_API Frustum {
      enum Side {
         RIGHT,
         LEFT,
         BOTTOM,
         TOP,
         BACK,
         FRONT
      };

      Plane planes[6];

      Frustum(const mat4& m);

      bool PointTest(vec3 p);
      bool SphereTest(const Sphere& s);
   };

}
