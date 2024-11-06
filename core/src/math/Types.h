#pragma once

// todo:
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// for HLSL
using uint = uint32_t;

using int2 = glm::ivec2;
using uint2 = glm::uvec2;
using int3 = glm::ivec3;
using uint3 = glm::uvec3;
using int4 = glm::ivec4;
using uint4 = glm::uvec4;

using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;
using float4x4 = glm::mat4;

namespace pbe {

   constexpr float EPSILON = FLT_EPSILON;
   constexpr float PI = glm::pi<float>();
   constexpr float PI2 = PI * 2.f;
   constexpr float PIHalf = PI * 0.5f;

   constexpr float ToRadians(float degrees) {
      return degrees / 180.f * PI;
   }

   constexpr float ToDegrees(float radians) {
      return radians * 180.f / PI;
   }

   using bool2 = glm::bvec2;
   using bool3 = glm::bvec3;
   using bool4 = glm::bvec4;

   using glm::vec2;
   using glm::vec3;
   using glm::vec4;

   using glm::mat2;
   using glm::mat3;
   using glm::mat4;
   using glm::mat3x3;

   using glm::quat;

   constexpr vec2 vec2_One = vec2(1, 1);
   constexpr vec2 vec2_Half = vec2_One * 0.5f;
   constexpr vec2 vec2_Zero = vec2(0, 0);
   constexpr vec2 vec2_X = vec2(1, 0);
   constexpr vec2 vec2_XNeg = vec2(-1, 0);
   constexpr vec2 vec2_Y = vec2(0, 1);
   constexpr vec2 vec2_YNeg = vec2(0, -1);

   constexpr vec3 vec3_One = vec3(1, 1, 1);
   constexpr vec3 vec3_Half = vec3_One * 0.5f;
   constexpr vec3 vec3_Zero = vec3(0, 0, 0);
   constexpr vec3 vec3_X = vec3(1, 0, 0);
   constexpr vec3 vec3_XNeg = vec3(-1, 0, 0);
   constexpr vec3 vec3_Y = vec3(0, 1, 0);
   constexpr vec3 vec3_YNeg = vec3(0, -1, 0);
   constexpr vec3 vec3_Z = vec3(0, 0, 1);
   constexpr vec3 vec3_ZNeg = vec3(0, 0, -1);

   constexpr vec3 vec3_Right = vec3_X;
   constexpr vec3 vec3_Up = vec3_Y;
   constexpr vec3 vec3_Forward = vec3_Z;

   constexpr vec4 vec4_One = vec4(1, 1, 1, 1);
   constexpr vec4 vec4_Half = vec4_One * 0.5f;
   constexpr vec4 vec4_Zero = vec4(0, 0, 0, 0);
   constexpr vec3 vec4_X = vec4(1, 0, 0, 0);
   constexpr vec3 vec4_XNeg = vec4(-1, 0, 0, 0);
   constexpr vec3 vec4_Y = vec4(0, 1, 0, 0);
   constexpr vec3 vec4_YNeg = vec4(0, -1, 0, 0);
   constexpr vec3 vec4_Z = vec4(0, 0, 1, 0);
   constexpr vec3 vec4_ZNeg = vec4(0, 0, -1, 0);
   constexpr vec3 vec4_W = vec4(0, 0, 0, 1);
   constexpr vec3 vec4_WNeg = vec4(0, 0, 0, -1);

   constexpr quat quat_Identity = quat(1, 0, 0, 0);

   GLM_FUNC_QUALIFIER GLM_CONSTEXPR bool2 operator<(const glm::ivec2& lhs, const glm::ivec2& rhs) {
      return glm::lessThan(lhs, rhs);
   }

   GLM_FUNC_QUALIFIER GLM_CONSTEXPR bool2 operator>(const glm::ivec2& lhs, const glm::ivec2& rhs) {
      return glm::greaterThan(lhs, rhs);
   }

   GLM_FUNC_QUALIFIER GLM_CONSTEXPR bool2 operator<(const glm::ivec2& lhs, int rhs) {
      return glm::lessThan(lhs, glm::ivec2{ rhs });
   }

   GLM_FUNC_QUALIFIER GLM_CONSTEXPR bool2 operator>(const glm::ivec2& lhs, int rhs) {
      return glm::greaterThan(lhs, glm::ivec2{ rhs });
   }

   GLM_FUNC_QUALIFIER GLM_CONSTEXPR bool2 operator<(const glm::vec2& lhs, const glm::vec2& rhs) {
      return glm::lessThan(lhs, rhs);
   }

   GLM_FUNC_QUALIFIER GLM_CONSTEXPR bool2 operator>(const glm::vec2& lhs, const glm::vec2& rhs) {
      return glm::greaterThan(lhs, rhs);
   }

   GLM_FUNC_QUALIFIER GLM_CONSTEXPR bool3 operator<(const vec3& lhs, const vec3& rhs) {
      return glm::lessThan(lhs, rhs);
   }

   GLM_FUNC_QUALIFIER GLM_CONSTEXPR bool3 operator>(const vec3& lhs, const vec3& rhs) {
      return glm::greaterThan(lhs, rhs);
   }

}

template<typename OStream>
OStream& operator<<(OStream& os, const int2& v) {
   return os << "(" << v.x << ", " << v.y << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const uint2& v) {
   return os << "(" << v.x << ", " << v.y << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const int3& v) {
   return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const uint3& v) {
   return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const glm::vec2& v) {
   return os << "(" << v.x << ", " << v.y << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const glm::vec3& v) {
   return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const glm::vec4& v) {
   return os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const glm::quat& v) {
   return os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
}
