#include "pch.h"
#include "BVH.h"

#include "DbgRend.h"
#include "math/Common.h"
#include "math/Random.h"
#include "math/Shape.h"


namespace pbe {
   void BVH::Build(const std::span<AABB>& aabbs, SplitMethod splitMethod) {
      nodes.clear();

      u32 size = (u32)aabbs.size();
      if (size == 0) {
         return;
      }

      std::vector<AABBInfo> aabbInfos(size);
      for (u32 i = 0; i < size; ++i) {
         aabbInfos[i] = AABBInfo{aabbs[i], i};
      }

      nodes.reserve(size * 3); // todo: not best size
      BuildRecursive(std::span{aabbInfos.begin(), aabbInfos.size()}, splitMethod);
   }

   void BVH::Flatten() {
      linearNodes.resize(nodes.size());
      if (nodes.empty()) {
         return;
      }

      u32 offset = 0;
      RecursiveFlatten(0, offset);
   }

   u32 BVH::RecursiveFlatten(u32 nodeIdx, u32& offset) {
      auto& node = nodes[nodeIdx];

      auto& linearNode = linearNodes[offset];
      u32 currentOffset = offset++;
      linearNode.aabbMin = node.aabb.min;
      linearNode.aabbMax = node.aabb.max;
      linearNode.objIdx = node.objIdx;

      if (node.objIdx == UINT_MAX) {
         RecursiveFlatten(node.children[0], offset);
         linearNode.secondChildOffset = offset;
         RecursiveFlatten(node.children[1], offset);
      }

      return currentOffset;
   }

   void BVH::Render(DbgRend& dbgRend, u32 showLevel, u32 nodeIdx, u32 level) {
      if (nodeIdx == UINT_MAX || nodes.empty()) {
         return;
      }

      auto& node = nodes[nodeIdx];

      if (showLevel == -1 || showLevel == level) {
         dbgRend.DrawAABB(nullptr, node.aabb, Random::Color(level));
      }

      if (node.objIdx == UINT_MAX) {
         Render(dbgRend, showLevel, node.children[0], level + 1);
         Render(dbgRend, showLevel, node.children[1], level + 1);
      }
   }

   u32 BVH::BuildRecursive(std::span<AABBInfo> aabbInfos, SplitMethod splitMethod) {
      u32 count = (u32)aabbInfos.size();
      if (count == 0) {
         return UINT_MAX;
      }

      u32 buildNodeIdx = (u32)nodes.size();
      nodes.push_back({});
      auto& buildNode = nodes.back();

      AABB combinedAABB = aabbInfos[0].aabb;
      for (u32 i = 1; i < count; ++i) {
         combinedAABB.AddAABB(aabbInfos[i].aabb);
      }

      buildNode.aabb = combinedAABB;
      if (count == 1) {
         buildNode.objIdx = aabbInfos[0].idx;
         return buildNodeIdx;
      }

      u32 mid = count / 2;
      auto axisIdx = VectorUtils::LargestAxisIdx(combinedAABB.Size());

      auto SplitEqualCount = [&] {
         mid = count / 2;
         std::ranges::nth_element(aabbInfos, aabbInfos.begin() + mid,
                                  [&](const AABBInfo& a, const AABBInfo& b) {
                                     return a.aabb.Center()[axisIdx] < b.aabb.Center()[axisIdx];
                                  });
      };

      switch (splitMethod) {
      case SplitMethod::Middle: {
         float pivot = combinedAABB.Center()[axisIdx];
         auto it = std::ranges::partition(aabbInfos,
                                          [&](const AABBInfo& a) { return a.aabb.Center()[axisIdx] < pivot; });
         mid = (u32)it.size();
         if (mid != 0 && mid != aabbInfos.size())
            break;
      }
      case SplitMethod::EqualCounts:
         SplitEqualCount();
         break;
      case SplitMethod::SAH:
         if (count <= 2) {
            mid = count / 2;
            std::ranges::nth_element(aabbInfos, aabbInfos.begin() + mid,
                                     [&](const AABBInfo& a, const AABBInfo& b) {
                                        return a.aabb.Center()[axisIdx] < b.aabb.Center()[axisIdx];
                                     });
            break;
         }

         struct BucketInfo {
            int count = 0;
            AABB bounds = AABB::Empty();
         };

         constexpr u32 nAxis = 3;
         constexpr u32 nBuckets = 4;
         BucketInfo buckets[nBuckets * nAxis];

         float cost[(nBuckets - 1) * nAxis];

         for (u32 axisIdx = 0; axisIdx < nAxis; ++axisIdx) {
            for (auto& aabbInfo : aabbInfos) {
               auto& aabb = aabbInfo.aabb;
               u32 b = (u32)(nBuckets * combinedAABB.Offset(aabb.Center())[axisIdx]);
               ASSERT(b < nBuckets);

               auto& bucket = buckets[b + axisIdx * nBuckets];
               bucket.count++;
               bucket.bounds.AddAABB(aabb);
            }

            for (u32 i = 0; i < nBuckets - 1; ++i) {
               AABB b0, b1;

               u32 count0 = 0, count1 = 0;
               for (u32 j = 0; j <= i; ++j) {
                  auto& bucket = buckets[j + axisIdx * nBuckets];
                  b0.AddAABB(bucket.bounds);
                  count0 += bucket.count;
               }

               for (int j = i + 1; j < nBuckets; ++j) {
                  auto& bucket = buckets[j + axisIdx * nBuckets];
                  b1.AddAABB(bucket.bounds);
                  count1 += bucket.count;
               }

               cost[i + axisIdx * (nBuckets - 1)] = 1
                  + (count0 * b0.Area() + count1 * b1.Area())
                  / combinedAABB.Area();
            }
         }

         float minCost = cost[0];
         int minCostSplitBucket = 0;
         for (u32 i = 1; i < (nBuckets - 1) * nAxis; ++i) {
            if (cost[i] < minCost) {
               minCost = cost[i];
               minCostSplitBucket = i;
            }
         }

         u32 iSplitAxis = minCostSplitBucket / (nBuckets - 1);
         u32 iSplitBucket = minCostSplitBucket % (nBuckets - 1);

         auto it = std::ranges::partition(
            aabbInfos,
            [&](const AABBInfo& a) {
               auto& aabb = a.aabb;
               u32 b = (u32)(nBuckets * combinedAABB.Offset(aabb.Center())[iSplitAxis]);
               ASSERT(b < nBuckets);

               return b <= iSplitBucket;
            }
         );
         mid = count - (u32)it.size();

         if (mid == 0 || mid == count) {
            INFO("BVH build: SAH fallback to split equal count");
            SplitEqualCount();
         }

         break;
      };

      ASSERT(mid >= 1);
      buildNode.children[0] = BuildRecursive(std::span<AABBInfo>{aabbInfos.begin(), mid}, splitMethod);
      buildNode.children[1] = BuildRecursive(std::span<AABBInfo>{aabbInfos.begin() + mid, aabbInfos.end()},
                                             splitMethod);

      return buildNodeIdx;
   }
}
