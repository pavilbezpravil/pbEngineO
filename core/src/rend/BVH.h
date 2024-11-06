#pragma once
#include "core/Ref.h"
#include "math/Shape.h"
#include "shared/rt.hlsli"


namespace pbe {
   class DbgRend;

   struct BVH {
      enum class SplitMethod {
         Middle,
         EqualCounts,
         SAH,
      };

      struct BuildNode {
         AABB aabb;
         u32 children[2] = {UINT_MAX, UINT_MAX};
         u32 objIdx = UINT_MAX; // todo: aabb idx
      };

      struct AABBInfo {
         AABB aabb;
         u32 idx;
      };

      std::vector<BuildNode> nodes;
      std::vector<BVHNode> linearNodes;

      void Build(const std::span<AABB>& aabbs, SplitMethod splitMethod = SplitMethod::SAH);

      void Flatten();
      u32 RecursiveFlatten(u32 nodeIdx, u32& offset);

      void Render(DbgRend& dbgRend, u32 showLevel = -1, u32 nodeIdx = 0, u32 level = 0);

   private:
      u32 BuildRecursive(std::span<AABBInfo> aabbInfos, SplitMethod splitMethod);
   };
}
