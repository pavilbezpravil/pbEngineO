#pragma once

#include "Types.h"
#include "core/Assert.h"
#include "core/Core.h"

namespace pbe {

   enum class GeomType {
      Sphere,
      Box,
      Cylinder,
      Cone,
      Capsule,
      Unknown,
   };

   struct GeomSphere {
      float radius = 0.5f;
   };

   struct GeomBox {
      vec3 extends = vec3_Half;
   };

   struct GeomCapsule {
      float radius = 0.5;
      float halfHeight = 0.5f;
   };

   struct Geom {
      Geom() = default;

      Geom(const GeomSphere& sphere)
         : type(GeomType::Sphere), sphere(sphere) {}

      Geom(const GeomBox& box)
         : type(GeomType::Box), box(box) {}

      Geom(const GeomCapsule& capsule)
         : type(GeomType::Capsule), capsule(capsule) {}

      static Geom Sphere(float radius = 0.5f) {
         return Geom(GeomSphere{ radius });
      }

      static Geom Box(const vec3& extends = vec3_Half) {
         return Geom(GeomBox{ extends });
      }

      static Geom Capsule(float radius = 0.5f, float halfHeight = 0.5f) {
         return Geom(GeomCapsule{ radius, halfHeight });
      }

      GeomType GetType() const { return type; }

      const GeomSphere& GetSphere() const {
         ASSERT(type == GeomType::Sphere);
         return sphere;
      }

      const GeomBox& GetBox() const {
         ASSERT(type == GeomType::Box);
         return box;
      }

      const GeomCapsule& GetCapsule() const {
         ASSERT(type == GeomType::Capsule);
         return capsule;
      }

   private:
      GeomType type = GeomType::Unknown;

      union {
         GeomSphere sphere;
         GeomBox box;
         GeomCapsule capsule;
      };
   };

}
