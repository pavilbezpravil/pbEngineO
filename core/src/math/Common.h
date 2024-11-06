#pragma once

#include "Types.h"
#include "core/Core.h"


namespace pbe {

   struct VectorUtils {
      static int LargestAxisIdx(const vec3& v);
   };

   template<typename T>
   constexpr T Clamp(const T& value, const T& minValue, const T& maxValue) {
      return glm::clamp(value, minValue, maxValue);
   }

   template<typename T>
   constexpr T Saturate(const T& value) {
      return glm::clamp(value, T{0}, T{1});
   }

   template<typename T, typename U>
   constexpr T Lerp(const T& x, const T& y, U t) {
      return glm::mix(x, y, t);
   }

   template<typename T>
   constexpr auto Length(const T& v) {
      return glm::length(v);
   }

   template<typename T>
   constexpr auto Normalize(const T& v) {
      ASSERT(Length(v) > EPSILON);
      return glm::normalize(v);
   }

   template<typename T>
   constexpr auto NormalizeSafe(const T& v) {
      auto l = Length(v);
      return l < EPSILON ? T{ 0 } : v / l;
   }

   template<typename T>
   constexpr auto Dot(const T& a, const T& b) {
      return glm::dot(a, b);
   }

   template<typename T>
   constexpr auto Cross(const T& a, const T& b) {
      return glm::cross(a, b);
   }

   template <typename T>
   constexpr T AlignUpWithMask(T value, size_t mask)
   {
      return (T)(((size_t)value + mask) & ~mask);
   }

   template <typename T>
   constexpr T AlignDownWithMask(T value, size_t mask)
   {
      return (T)((size_t)value & ~mask);
   }

   template <typename T>
   constexpr T AlignUp(T value, size_t alignment)
   {
      return AlignUpWithMask(value, alignment - 1);
   }

   template <typename T>
   constexpr T AlignDown(T value, size_t alignment)
   {
      return AlignDownWithMask(value, alignment - 1);
   }

   template <typename T>
   constexpr bool IsAligned(T value, size_t alignment)
   {
      return 0 == ((size_t)value & (alignment - 1));
   }

   template <typename T>
   constexpr T DivideByMultiple(T value, size_t alignment)
   {
      return (T)((value + alignment - 1) / alignment);
   }

   constexpr uint32_t NextHighestPow2(uint32_t v)
   {
      v--;
      v |= v >> 1;
      v |= v >> 2;
      v |= v >> 4;
      v |= v >> 8;
      v |= v >> 16;
      v++;

      return v;
   }

   constexpr uint64_t NextHighestPow2(uint64_t v)
   {
      v--;
      v |= v >> 1;
      v |= v >> 2;
      v |= v >> 4;
      v |= v >> 8;
      v |= v >> 16;
      v |= v >> 32;
      v++;

      return v;
   }

   CORE_API std::tuple<glm::vec3, glm::quat, glm::vec3> GetTransformDecomposition(const glm::mat4& transform);


}
