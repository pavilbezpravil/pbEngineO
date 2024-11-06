#include "pch.h"
#include "PhysXTypeConvet.h"
#include "math/Geom.h"

namespace pbe {
   vec2 ConvertToPBE(const PxVec2& v) {
      return {v.x, v.y};
   }

   vec3 ConvertToPBE(const PxVec3& v) {
      return {v.x, v.y, v.z};
   }

   vec4 ConvertToPBE(const PxVec4& v) {
      return {v.x, v.y, v.z, v.w};
   }

   quat ConvertToPBE(const PxQuat& q) {
      return {q.w, q.x, q.y, q.z};
   }

   Transform ConvertToPBE(const PxTransform& trans, const vec3& scale) {
      return Transform{ConvertToPBE(trans.p), ConvertToPBE(trans.q), scale};
   }

   PxVec2 ConvertToPx(const vec2& v) {
      return {v.x, v.y};
   }

   PxVec3 ConvertToPx(const vec3& v) {
      return {v.x, v.y, v.z};
   }

   PxVec4 ConvertToPx(const vec4& v) {
      return {v.x, v.y, v.z, v.w};
   }

   PxQuat ConvertToPx(const quat& q) {
      return {q.x, q.y, q.z, q.w};
   }

   PxTransform ConvertToPx(const Transform& trans) {
      return PxTransform{ConvertToPx(trans.position), ConvertToPx(trans.rotation)};
   }

   PxGeometryHolder ConvertToPx(const Geom& geom) {
      switch (geom.GetType()) {
      case GeomType::Sphere:
         return PxSphereGeometry(geom.GetSphere().radius);
      case GeomType::Box:
         return PxBoxGeometry(ConvertToPx(geom.GetBox().extends));
      case GeomType::Capsule:
         return PxCapsuleGeometry(geom.GetCapsule().radius, geom.GetCapsule().halfHeight);
      default:
         UNIMPLEMENTED();
      }

      return {};
   }
}
