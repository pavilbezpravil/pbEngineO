#include "pch.h"
#include "Transform.h"

namespace pbe {

   Transform Transform_Identity = { vec3_Zero, quat_Identity, vec3_One };

   Transform Transform::operator*(const Transform& other) const {
      Transform result;
      result.position = TransformPoint(other.position);
      result.rotation = TransformRotation(other.rotation);
      result.scale = TransformScale(other.scale);
      return result;
   }

   Transform Transform::TransformInv(const Transform& other) const {
      Transform result;
      result.position = TransformInvPoint(other.position);
      result.rotation = TransformInvRotation(other.rotation);
      result.scale = TransformInvScale(other.scale);
      return result;
   }

   vec3 Transform::TransformPoint(const vec3& point) const {
      return position + rotation * (scale * point);
   }

   quat Transform::TransformRotation(const quat& rot) const {
      return rotation * rot;
   }

   vec3 Transform::TransformScale(const vec3& s) const {
      return scale * s;
   }

   vec3 Transform::TransformInvPoint(const vec3& point) const {
      return glm::inverse(rotation) * (point - position) / scale;
   }

   quat Transform::TransformInvRotation(const quat& rot) const {
      return glm::inverse(rotation) * rot;
   }

   vec3 Transform::TransformInvScale(const vec3& s) const {
      return s / scale;
   }

   vec3 Transform::Rotate(const vec3& dir) const {
      return rotation * dir;
   }

   vec3 Transform::Right() const {
      return Rotate(vec3_Right);
   }

   vec3 Transform::Up() const {
      return Rotate(vec3_Up);
   }

   vec3 Transform::Forward() const {
      return Rotate(vec3_Forward);
   }

   mat4 Transform::GetMatrix() const {
      mat4 transform = glm::translate(mat4(1), position);
      transform *= mat4{ rotation };
      transform *= glm::scale(mat4(1), scale);
      return transform;
   }

   mat3x3 Transform::GetBasis() const {
      return mat3x3{
         Right(),
         Up(),
         Forward(),
      };
   }
}
