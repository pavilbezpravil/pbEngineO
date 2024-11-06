#include "pch.h"
#include "PhysComponents.h"

#include "Phys.h"
#include "PhysUtils.h"
#include "PhysXTypeConvet.h"
#include "DestructEventListener.h"
#include "core/Profiler.h"
#include "gui/Gui.h"
#include "typer/Registration.h"
#include "typer/Serialize.h"
#include "scene/Component.h"

#include <NvBlastTk.h>
#include <NvBlastExtDamageShaders.h>

#include "PhysicsScene.h"
#include "math/Color.h"
#include "math/Common.h"
#include "math/Random.h"
#include "math/Shape.h"
#include "rend/DbgRend.h"


using namespace Nv::Blast;

namespace pbe {

   // todo: auto& c0L = reinterpret_cast<const vec3&>(blastChunk0.centroid);
   static vec3 Float3ToVec3(const float fs[3]) {
      return vec3(fs[0], fs[1], fs[2]);
   }

   static void Vec3ToFloat3(float fs[3], const vec3& v) {
      fs[0] = v.x;
      fs[1] = v.y;
      fs[2] = v.z;
   }

   class FructureGenerator {
   public:
      void AddRoot(const Entity& entity) {
         rootTrans = entity.GetTransform().World();
         rootTrans.scale = vec3{ 1.f };

         AddEntityWithChilds(entity);
      }

      void AddEntityWithChilds(const Entity& entity) {
         if (entity.Has<RigidBodyShapeComponent>()) {
            if (!entity.Has<GeometryComponent>()) {
               WARN("Child {} of destruct actor must have GeometryComponent", entity.GetName());
               return;
            }
            ASSERT(entity.Get<GeometryComponent>().type == GeomType::Box);

            // if (entity.Has<RigidBodyComponent>()) {
            //    WARN("Child {} of destruct actor has RigidBodyComponent", entity.GetName());
            //    return;
            // }

            auto& trans = entity.GetTransform();

            auto relativeTrans = rootTrans.TransformInv(trans.World());
            vec3 relativeScale = relativeTrans.scale;
            relativeScale = relativeTrans.Rotate(relativeScale);
            relativeScale = abs(relativeScale);

            u32 chunkIdx = AddChunk(UINT32_MAX, relativeTrans.position, relativeScale);
            chunkToEntity[chunkIdx] = entity.GetEntityID();
         }

         for (auto& child : entity.GetTransform().children) {
            // todo: mb parent must be entity chunk if it was added
            AddEntityWithChilds(child);
         }
      }

      u32 AddChunk(u32 parentIdx, const vec3& center, const vec3& size, bool replace = false) { // todo:
         u32 chunkIdx = (u32)chunkDescs.size();

         u32 reqSize = chunkIdx;
         if (!replace) {
            ++reqSize;
         }
         chunkDescs.resize(reqSize);
         chunkInfos.resize(reqSize);

         u32 nextChunkIdx = replace ? parentIdx : chunkIdx;
         auto& chunkDesc = chunkDescs[nextChunkIdx];

         chunkDesc.parentChunkDescIndex = parentIdx;

         chunkDesc.centroid[0] = center[0];
         chunkDesc.centroid[1] = center[1];
         chunkDesc.centroid[2] = center[2];

         chunkDesc.volume = size.x * size.y * size.z;
         chunkDesc.flags = NvBlastChunkDesc::NoFlags;
         chunkDesc.userData = nextChunkIdx;

         auto& chunkInfo = chunkInfos[nextChunkIdx];
         chunkInfo.size = size;
         chunkInfo.isLeaf = true;

         return nextChunkIdx;
      }

      // next chunk will be added to the next indexies
      u32 GetChunkCount() const {
         return (u32)chunkDescs.size();
      }

      void SliceAxis(u32 parentChunkIdx, u32 axis, u32 slices, bool replace = false) {
         if (slices == 0) {
            return;
         }

         u32 chunkIdx = (u32)chunkDescs.size();

         u32 reqSize = chunkIdx + slices;
         if (!replace) {
            ++reqSize;
         }
         chunkDescs.resize(reqSize);
         chunkInfos.resize(reqSize);

         vec3 chunkCenter = {
         chunkDescs[parentChunkIdx].centroid[0],
         chunkDescs[parentChunkIdx].centroid[1],
         chunkDescs[parentChunkIdx].centroid[2],
         };

         auto& chunkInfo = chunkInfos[parentChunkIdx];
         if (!replace) {
            chunkInfo.isLeaf = false;
         }
         vec3 chunkSize = chunkInfo.size;

         float sliceAxisStart = -chunkSize[axis] * 0.5f;

         float chunkSliceAxisSize = chunkSize[axis] / (slices + 1);

         float perpendicularArea = chunkSize.x * chunkSize.y * chunkSize.z / chunkSize[axis];

         float slicePos = sliceAxisStart;

         u32 nextParentChunkIdx = replace ? chunkDescs[parentChunkIdx].parentChunkDescIndex : parentChunkIdx;

         for (u32 i = 0; i <= slices; ++i) {
            u32 nextChunkIdx = (replace && i == 0) ? parentChunkIdx : chunkIdx;
            auto& chunkDesc = chunkDescs[nextChunkIdx];

            chunkDesc.parentChunkDescIndex = nextParentChunkIdx;

            chunkDesc.centroid[0] = chunkCenter[0];
            chunkDesc.centroid[1] = chunkCenter[1];
            chunkDesc.centroid[2] = chunkCenter[2];

            float nextSlicePos = sliceAxisStart + chunkSliceAxisSize * (i + 1 + Random::Float(-0.5f, 0.5f) * 1.f);
            if (i == slices) {
               nextSlicePos = -sliceAxisStart;
            }

            chunkDesc.centroid[axis] += (slicePos + nextSlicePos) * 0.5f;

            float sliceSize = nextSlicePos - slicePos;
            slicePos = nextSlicePos;

            chunkDesc.volume = perpendicularArea * sliceSize;
            chunkDesc.flags = NvBlastChunkDesc::NoFlags;
            chunkDesc.userData = nextChunkIdx;

            auto& chunkInfo = chunkInfos[nextChunkIdx];
            chunkInfo.size = chunkSize;
            chunkInfo.size[axis] = sliceSize;
            chunkInfo.isLeaf = true;

            if (!replace || i != 0) {
               ++chunkIdx;
            }
         }
      }

      void Slice(u32 parentChunkIdx, uint3 slices) {
         u32 startIdx = GetChunkCount();

         bool wasSliced = false;
         u32 nSliced = 1;
         for (u32 iAxis = 0; iAxis < 3; ++iAxis) {
            if (slices[iAxis] == 0) {
               continue;
            }

            if (!wasSliced) {
               SliceAxis(parentChunkIdx, iAxis, slices[iAxis], false);
               wasSliced = true;
            } else {
               for (u32 i = 0; i < nSliced; ++i) {
                  SliceAxis(startIdx + i, iAxis, slices[iAxis], true);
               }
            }

            nSliced *= slices[iAxis] + 1;
         }
      }

      // get chunk info func
      const ChunkInfo& GetChunkInfo(u32 chunkIdx) const {
         return chunkInfos[chunkIdx];
      }

      u32 ChunkDepth(u32 chunkIdx) const {
         u32 depth = 0;
         while (chunkIdx != UINT32_MAX) {
            chunkIdx = chunkDescs[chunkIdx].parentChunkDescIndex;
            ++depth;
         }
         return depth;
      }

      void MarkSupportChunkAtDepth(u32 depth) {
         ASSERT(depth > 0);
         depth = std::max(depth, 1u);

         for (u32 chunkIdx = 0; chunkIdx < (u32)chunkDescs.size(); ++chunkIdx) {
            auto chunkDepth = ChunkDepth(chunkIdx);
            if (chunkDepth == depth || (chunkDepth < depth && GetChunkInfo(chunkIdx).isLeaf)) {
               chunkDescs[chunkIdx].flags = NvBlastChunkDesc::SupportFlag;
               chunkInfos[chunkIdx].isSupport = true;
            }
         }
      }

      void BondGeneration() {
         std::vector<u32> supportChunkIdxs;
         supportChunkIdxs.reserve(chunkDescs.size());

         for (u32 i = 0; i < chunkDescs.size(); ++i) {
            if (chunkDescs[i].flags & NvBlastChunkDesc::SupportFlag) {
               supportChunkIdxs.push_back(i);
            }
         }

         u32 nSupportChunks = (u32)supportChunkIdxs.size();

         std::vector<AABB> aabbs(nSupportChunks);
         for (u32 i = 0; i < nSupportChunks; ++i) {
            u32 chunkIdx = supportChunkIdxs[i];
            aabbs[i] = AABB::FromExtends(
               Float3ToVec3(chunkDescs[chunkIdx].centroid),
               GetChunkInfo(chunkIdx).size / 2.f);
            aabbs[i].Expand(0.01f); // for future intersection test
         }

         bondDescs.reserve(nSupportChunks * 3); // assume min 3 bonds per chunk in general case

         for (u32 i = 0; i < nSupportChunks; ++i) {
            u32 chunkIdx = supportChunkIdxs[i];
            const auto& chunkAABB = aabbs[i];

            vec3 chunkCenter = chunkAABB.Center();
            vec3 chunkSize = chunkAABB.Size();

            float chunkVolume = chunkSize.x * chunkSize.y * chunkSize.z;

            // todo: slow. accelerate with spatial partitioning
            for (u32 iTested = i + 1; iTested < nSupportChunks; ++iTested) {
               u32 testedChunkIdx = supportChunkIdxs[iTested];
               const auto& testedChunkAABB = aabbs[iTested];
               if (chunkAABB.Intersects(testedChunkAABB)) {
                  vec3 testedChunkCenter = testedChunkAABB.Center();
                  vec3 testedChunkSize = testedChunkAABB.Size();

                  NvBlastBondDesc boundDesc;

                  boundDesc.chunkIndices[0] = chunkIdx;
                  boundDesc.chunkIndices[1] = testedChunkIdx;

                  vec3 normal = testedChunkCenter - chunkCenter;
                  normal = glm::normalize(normal);

                  Vec3ToFloat3(boundDesc.bond.normal, normal);

                  float testedChunkVolume = testedChunkSize.x * testedChunkSize.y * testedChunkSize.z;

                  // todo:
                  boundDesc.bond.area = (pow(chunkVolume, 2.f / 3.f) + pow(testedChunkVolume, 2.f / 3.f)) / 2.f;

                  auto chunk0Center = chunkDescs[boundDesc.chunkIndices[0]].centroid;
                  auto chunk1Center = chunkDescs[boundDesc.chunkIndices[1]].centroid;

                  boundDesc.bond.centroid[0] = (chunk0Center[0] + chunk1Center[0]) / 2.f;
                  boundDesc.bond.centroid[1] = (chunk0Center[1] + chunk1Center[1]) / 2.f;
                  boundDesc.bond.centroid[2] = (chunk0Center[2] + chunk1Center[2]) / 2.f;

                  bondDescs.push_back(boundDesc);
               }
            }
         }

         // bondDescs[0].userData = 0;  // this can be used to tell the user more information about this
         // bond for example to create a joint when this bond breaks

         // bondDescs[1].chunkIndices[0] = 1;
         // bondDescs[1].chunkIndices[1] = ~0;  // ~0 (UINT32_MAX) is the "invalid index."  This creates a world bond
         // ... etc. for bondDescs[1], all other fields are filled in as usual
      }  

      TkAssetDesc GetTkAssetDesc() {
         auto tkFramework = NvBlastTkFrameworkGet();

         bool wasCoverage = tkFramework->ensureAssetExactSupportCoverage(chunkDescs.data(), (u32)chunkDescs.size());
         ASSERT(wasCoverage);
         if (!wasCoverage) {
            WARN("ChunkDescs was not coverage");
         }

         // todo: 'chunkReorderMap' may be skipped
         std::vector<uint32_t> chunkReorderMap(chunkDescs.size());  // Will be filled with a map from the original chunk descriptor order to the new one
         bool requireReordering = !tkFramework->reorderAssetDescChunks(chunkDescs.data(), (u32)chunkDescs.size(),
            bondDescs.data(), (u32)bondDescs.size(), chunkReorderMap.data());

         if (requireReordering) {
            std::vector<ChunkInfo> newChunkInfos;
            newChunkInfos.resize(chunkInfos.size());

            for (u32 i = 0; i < (u32)chunkReorderMap.size(); ++i) {
               newChunkInfos[i] = chunkInfos[chunkReorderMap[i]];
            }

            std::swap(chunkInfos, newChunkInfos);
         }

         TkAssetDesc assetDesc;
         assetDesc.chunkCount = (u32)chunkDescs.size();
         assetDesc.chunkDescs = chunkDescs.data();
         assetDesc.bondCount = (u32)bondDescs.size();
         assetDesc.bondDescs = bondDescs.data();
         assetDesc.bondFlags = nullptr;

         return assetDesc;
      }

      // todo:
      std::vector<ChunkInfo> chunkInfos;
      std::unordered_map<u32, EntityID> chunkToEntity;
   private:
      std::vector<NvBlastChunkDesc> chunkDescs;
      std::vector<NvBlastBondDesc> bondDescs;

      Transform rootTrans;
   };

   static DestructData* GetDestructData(Entity& entity) {
      FructureGenerator fructureGenerator;

      {
         fructureGenerator.AddRoot(entity);

         // todo: not optimal
         for (size_t depth = 1; depth < 6; depth++) {
            bool wasSliced = false;

            u32 chunkCount = fructureGenerator.GetChunkCount();
            for (u32 iChunk = 0; iChunk < chunkCount; ++iChunk) {
               if (fructureGenerator.ChunkDepth(iChunk) == depth) {
                  auto curChunkSize = fructureGenerator.GetChunkInfo(iChunk).size;

                  auto axisIdx = VectorUtils::LargestAxisIdx(curChunkSize);

                  if (curChunkSize[axisIdx] < 1.f) {
                     continue;
                  }

                  vec3 normalizedChunkSize = curChunkSize / curChunkSize[axisIdx];

                  uint3 slices = uint3(
                     normalizedChunkSize.x > 0.75f ? 1 : 0,
                     normalizedChunkSize.y > 0.75f ? 1 : 0,
                     normalizedChunkSize.z > 0.75f ? 1 : 0
                  );
                  fructureGenerator.Slice(iChunk, slices);

                  wasSliced = true;
               }
            }

            if (!wasSliced) {
               break;
            }
         }

         fructureGenerator.MarkSupportChunkAtDepth(6);

         // subsupport chunks
         if (0) {
            u32 chunkCount = fructureGenerator.GetChunkCount();
            for (u32 iChunk = 0; iChunk < chunkCount; ++iChunk) {
               auto& chunkInfo = fructureGenerator.GetChunkInfo(iChunk);
               if (!chunkInfo.isLeaf) {
                  continue;
               }

               auto curChunkSize = chunkInfo.size;
               auto axisIdx = VectorUtils::LargestAxisIdx(curChunkSize);

               if (curChunkSize[axisIdx] < 0.3f) {
                  continue;
               }

               vec3 normalizedChunkSize = curChunkSize / curChunkSize[axisIdx];

               uint3 slices = uint3(
                  normalizedChunkSize.x > 0.75f ? 1 : 0,
                  normalizedChunkSize.y > 0.75f ? 1 : 0,
                  normalizedChunkSize.z > 0.75f ? 1 : 0
               );
               fructureGenerator.Slice(iChunk, slices);
            }
         }
      }

      fructureGenerator.BondGeneration();

      TkAssetDesc assetDesc = fructureGenerator.GetTkAssetDesc();

      auto tkFramework = NvBlastTkFrameworkGet();
      TkAsset* tkAsset = tkFramework->createAsset(assetDesc);

      // todo:
      DestructData* destructData = new DestructData{};
      destructData->tkAsset = tkAsset;
      destructData->chunkInfos = std::move(fructureGenerator.chunkInfos);
      destructData->damageAccelerator = NvBlastExtDamageAcceleratorCreate(destructData->tkAsset->getAssetLL(), 3);

      // todo: find direct and readably way
      entity.Get<RigidBodyComponent>().chunkToEntity = std::move(fructureGenerator.chunkToEntity);

      return destructData;
   }

   void RigidBodyComponent::ApplyDamage(const vec3& posW, float damage) {
      if (!tkActor) {
         return;
      }

      auto posL = ConvertToPBE(pxRigidActor->getGlobalPose().transformInv(ConvertToPx(posW)));

      NvBlastExtMaterial material;
      material.health = hardness;
      material.minDamageThreshold = 0.15f;
      material.maxDamageThreshold = 1.0f;

      float normalizedDamage = material.getNormalizedDamage(damage);
      if (normalizedDamage == 0.f) {
         return;
      }

      // INFO("Normalized damage: {}", normalizedDamage);

      static NvBlastDamageProgram damageProgram = { NvBlastExtFalloffGraphShader, NvBlastExtFalloffSubgraphShader };

      auto& damageDesc = GetPhysScene().GetDamageParamsPlace<NvBlastExtRadialDamageDesc>();
      auto& params = GetPhysScene().GetDamageParamsPlace<NvBlastExtProgramParams>();

      damageDesc = { normalizedDamage, {posL.x, posL.y, posL.z}, 1.0f, 2.f };
      params = { &damageDesc, nullptr, destructData->damageAccelerator };

      tkActor->damage(damageProgram, &params);
   }

   vec3 RigidBodyComponent::GetLinearVelocity() const {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      return ConvertToPBE(dynamic->getLinearVelocity());
   }

   void RigidBodyComponent::SetLinearVelocity(const vec3& v, bool autowake) {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->setLinearVelocity(ConvertToPx(v), autowake);
   }

   vec3 RigidBodyComponent::GetAngularVelocity() const {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      return ConvertToPBE(dynamic->getAngularVelocity());
   }

   void RigidBodyComponent::SetAngularVelocity(const vec3& v, bool autowake) {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->setAngularVelocity(ConvertToPx(v), autowake);
   }

   void RigidBodyComponent::AddForce(const vec3& force, ForceMode mode, bool autowake) {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->addForce(ConvertToPx(force), (PxForceMode::Enum)mode, autowake);
   }

   void RigidBodyComponent::AddTorque(const vec3& torque, ForceMode mode, bool autowake) {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->addTorque(ConvertToPx(torque), (PxForceMode::Enum)mode, autowake);
   }

   void RigidBodyComponent::MakeDestructable() {
      if (!destructible) {
         destructible = true;
         CreateOrUpdate(*GetPhysScene().pxScene, GetEntity());
      }
   }

   void RigidBodyComponent::SetDestructible(Nv::Blast::TkActor& tkActor, DestructData& destructData) {
      ASSERT(!destructible);

      destructible = true;
      this->tkActor = &tkActor;
      this->destructData = &destructData;
      this->tkActor->userData = new Entity{ *(Entity*)pxRigidActor->userData }; // todo: use fixed allocator
   }

   void RigidBodyComponent::DbgRender(DbgRend& dbgRend, DestructDbgRendFlags flags) const {
      if (!tkActor) {
         return;
      }

      auto transWorld = GetEntity().GetTransform().World();

      // todo: first destruct actor has size != 1, next has size == 1
      transWorld.scale = vec3{ 1.f };

      auto tkAsset = tkActor->getAsset();
      auto tkChunks = tkAsset->getChunks();

      if (bool(flags & (DestructDbgRendFlags::Chunks | DestructDbgRendFlags::ChunksCentroid))) {
         uint32_t visibleChunkCount = tkActor->getVisibleChunkCount();
         std::vector<u32> visibleChunkIndices(visibleChunkCount);
         tkActor->getVisibleChunkIndices(visibleChunkIndices.data(), visibleChunkCount);

         for (u32 chunkIndex : visibleChunkIndices) {
            NvBlastChunk chunk = tkChunks[chunkIndex];

            vec3 chunkCentroidL = vec3{ chunk.centroid[0], chunk.centroid[1], chunk.centroid[2] };
            vec3 chunkCentroidW = transWorld.TransformPoint(chunkCentroidL);

            if (bool(flags & DestructDbgRendFlags::ChunksCentroid)) {
               dbgRend.DrawSphere(Sphere{ chunkCentroidW, 0.03f }, Color_White, false);
            }

            if (bool(flags & DestructDbgRendFlags::Chunks)) {
               AABB chunkAABB = AABB::FromExtends(chunkCentroidL, destructData->chunkInfos[chunkIndex].size / 2.f);
               dbgRend.DrawAABB(&transWorld, chunkAABB, Color_White, false);
            }
         }
      }

      if (bool(flags & (DestructDbgRendFlags::BondsCentroid | DestructDbgRendFlags::BondsHealth))) {
         uint32_t nodeCount = tkActor->getGraphNodeCount();
         if (nodeCount == 0) { // subsupport chunks don't have graph nodes
            return;
         }

         std::vector<uint32_t> nodes(nodeCount);
         tkActor->getGraphNodeIndices(nodes.data(), static_cast<uint32_t>(nodes.size()));

         const float* bondHealths = tkActor->getBondHealths();

         auto& tkFamily = tkActor->getFamily();

         const NvBlastChunk* chunks = tkFamily.getAsset()->getChunks();
         const NvBlastBond* bonds = tkFamily.getAsset()->getBonds();
         const NvBlastSupportGraph graph = tkFamily.getAsset()->getGraph();

         for (uint32_t node0 : nodes) {
            const uint32_t chunkIndex0 = graph.chunkIndices[node0];
            const NvBlastChunk& chunk0 = chunks[chunkIndex0];

            for (uint32_t adjacencyIndex = graph.adjacencyPartition[node0]; adjacencyIndex < graph.adjacencyPartition[node0 + 1]; adjacencyIndex++) {
               uint32_t node1 = graph.adjacentNodeIndices[adjacencyIndex];
               const uint32_t chunkIndex1 = graph.chunkIndices[node1];
               const NvBlastChunk& chunk1 = chunks[chunkIndex1];

               if (node0 > node1) {
                  continue;
               }

               uint32_t bondIndex = graph.adjacentBondIndices[adjacencyIndex];
               float healthVal = bondHealths[bondIndex];
               if (healthVal == 0.f) {
                  continue;
               }

               Color color{ glm::mix((vec4)Color_Red, (vec4)Color_Green, healthVal) };

               const NvBlastBond& bond = bonds[bondIndex];
               const auto centroidL = reinterpret_cast<const vec3&>(bond.centroid);

               if (bool(flags & DestructDbgRendFlags::BondsHealth)) {
                  auto& c0L = reinterpret_cast<const vec3&>(chunk0.centroid);
                  auto& c1L = reinterpret_cast<const vec3&>(chunk1.centroid);

                  vec3 c0 = transWorld.TransformPoint(c0L);
                  vec3 c1 = transWorld.TransformPoint(c1L);

                  dbgRend.DrawLine(c0, c1, color, false);
               }

               if (bool(flags & DestructDbgRendFlags::BondsCentroid)) {
                  // todo: area
                  const vec3& normalL = reinterpret_cast<const vec3&>(bond.normal);

                  vec3 centroidW = transWorld.TransformPoint(centroidL);
                  vec3 normalW = transWorld.Rotate(normalL);

                  dbgRend.DrawSphere(Sphere{ centroidW, 0.03f }, color, false);

                  dbgRend.DrawLine(centroidW, centroidW + normalW * 0.1f, Color_Blue, false);
               }
            }
         }
      }
   }

   static void RemoveSceneRigidActor(PxScene& pxScene, PxRigidActor& pxRigidActor) {
      pxScene.removeActor(pxRigidActor);
      delete (Entity*)pxRigidActor.userData;
      pxRigidActor.userData = nullptr;
      pxRigidActor.release();
   }

   void RigidBodyComponent::CreateOrUpdate(PxScene& pxScene, Entity& entity) {
      if (!pxRigidActor || (bool)pxRigidActor->is<PxRigidDynamic>() != dynamic) {
         auto& trans = entity.Get<SceneTransformComponent>();
         PxTransform physTrans = GetTransform(trans);

         auto oldPxRigidActor = pxRigidActor;
         if (dynamic) {
            pxRigidActor = GetPxPhysics()->createRigidDynamic(physTrans);
         } else {
            pxRigidActor = GetPxPhysics()->createRigidStatic(physTrans);
         }
         pxRigidActor->userData = new Entity{ entity }; // todo: use fixed allocator

         AddShapesHier(entity);

         if (dynamic) {
            float density = 1.f; // todo:
            PxRigidBodyExt::updateMassAndInertia(*GetPxRigidDynamic(pxRigidActor), density);
         }

         pxScene.addActor(*pxRigidActor);

         if (oldPxRigidActor) {
            PxU32 nbConstrains = oldPxRigidActor->getNbConstraints();

            if (nbConstrains > 0) {
               std::array<PxConstraint*, 8> constrains;
               ASSERT(nbConstrains <= constrains.size());
               nbConstrains = std::min(nbConstrains, (u32)constrains.size());
               oldPxRigidActor->getConstraints(constrains.data(), nbConstrains);

               for (PxU32 i = 0; i < nbConstrains; i++) {
                  auto pxConstrain = constrains[i];
                  auto pEntity = (Entity*)pxConstrain->userData;
                  // todo: mb PxConstraint::userData should be PxJoint?
                  auto pxJoint = pEntity->Get<JointComponent>().pxJoint;

                  PxRigidActor *actor0, *actor1;
                  pxJoint->getActors(actor0, actor1);

                  if (actor0 == oldPxRigidActor) {
                     pxJoint->setActors(pxRigidActor, actor1);
                  } else {
                     ASSERT(actor1 == oldPxRigidActor);
                     pxJoint->setActors(actor0, pxRigidActor);
                  }
               }
            }

            RemoveSceneRigidActor(pxScene, *oldPxRigidActor);
         }
      }

      if (dynamic) {
         auto dynamic = GetPxRigidDynamic(pxRigidActor);
         dynamic->setLinearDamping(linearDamping);
         dynamic->setAngularDamping(angularDamping);
      }

      // todo: handle change
      if (destructible && tkActor == nullptr) {
         // auto [trans, geom]  = entity.Get<SceneTransformComponent, GeometryComponent>();
         // ASSERT(geom.type == GeomType::Box);

         ASSERT(!destructData);
         destructData = GetDestructData(entity);

         auto tkFramework = NvBlastTkFrameworkGet();

         TkActorDesc actorDesc;
         actorDesc.asset = destructData->tkAsset;
         actorDesc.uniformInitialBondHealth = 1;
         actorDesc.uniformInitialLowerSupportChunkHealth = 1;

         auto& physScene = GetPhysScene();
         tkActor = tkFramework->createActor(actorDesc);
         tkActor->getFamily().addListener(*physScene.destructEventListener);
         physScene.tkGroup->addActor(*tkActor);

         ASSERT(tkActor->userData == nullptr);
         tkActor->userData = new Entity{ entity }; // todo: use fixed allocator
      } else if (!destructible && tkActor != nullptr) {
         RemoveDestruct();
      }
   }

   void RigidBodyComponent::Remove() {
      // todo: for already Disabled components dont call Remove funcs
      if (!pxRigidActor) {
         return;
      }

      RemoveDestruct();

      RemoveSceneRigidActor(*GetPhysScene().pxScene, *pxRigidActor);
      pxRigidActor = nullptr;
   }

   void RigidBodyComponent::RemoveDestruct() {
      if (!tkActor) {
         return;
      }

      auto& tkFamily = tkActor->getFamily();

      delete (Entity*)tkActor->userData;
      tkActor->userData = nullptr;
      PX_RELEASE(tkActor);

      if (tkFamily.getActorCount() == 0) {
         // INFO("Destrcut actors count ZERO. Delete tkAsset");
         PX_RELEASE(destructData->tkAsset);
         delete destructData;
         destructData = nullptr;
      }
   }

   void RigidBodyComponent::AddShapesHier(const Entity& entity) {
      auto& trans = entity.Get<SceneTransformComponent>();

      if (entity.Has<RigidBodyShapeComponent>()) {
         const auto& geom = entity.Get<GeometryComponent>();

         PxGeometryHolder physGeom = GetPhysGeom(trans, geom);

         PxShape* shape = GetPxPhysics()->createShape(physGeom.any(), *GetPxMaterial(), true);

         // todo: do it by pbe::Transform
         PxTransform shapeTrans = GetTransform(trans);
         PxTransform actorTrans = pxRigidActor->getGlobalPose();
         PxTransform shapeOffset = actorTrans.transformInv(shapeTrans);

         shape->setLocalPose(shapeOffset);

         pxRigidActor->attachShape(*shape);
         shape->release();
      }

      for (auto& child : trans.children) {
         ASSERT(!child.Has<RigidBodyComponent>());
         if (child.Has<RigidBodyComponent>()) {
            WARN("RigidBodyComponent::AddShapesHier: child entity has no RigidBodyComponent");
         } else {
            AddShapesHier(child);
         }
      }
   }

   JointComponent::JointComponent(JointType type) : type(type) { }

   void JointComponent::SetData(const Entity& entity) {
      // todo: ctor
      // todo: if type state the same it is not need to recreate joint
      if (pxJoint) {
         // todo: several place for this
         delete (Entity*)pxJoint->userData;
         pxJoint->userData = nullptr;
         pxJoint->release();
         pxJoint = nullptr;
      }

      auto actor0 = GetPxActor(entity0);
      auto actor1 = GetPxActor(entity1);

      if (!actor0 || !actor1) {
         return;
      }

      auto localFrame0 = PxTransform{ ConvertToPx(anchor0.position), ConvertToPx(anchor0.rotation) };
      auto localFrame1 = PxTransform{ ConvertToPx(anchor1.position), ConvertToPx(anchor1.rotation) };

      if (!PxIsRigidDynamic(actor0) && !PxIsRigidDynamic(actor1)) {
         WARN("Joint: at least one actor must be non-static");
         return;
      }

      if (type == JointType::Fixed) {
         auto pxFixedJoint = PxFixedJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxFixedJoint;
         if (!pxJoint) {
            WARN("Cant create fixed joint");
            return;
         }
      } else if (type == JointType::Distance) {
         auto pxDistanceJoint = PxDistanceJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxDistanceJoint;
         if (!pxJoint) {
            WARN("Cant create distance joint");
            return;
         }

         pxDistanceJoint->setMinDistance(distance.minDistance);
         pxDistanceJoint->setMaxDistance(distance.maxDistance);

         pxDistanceJoint->setDamping(distance.damping);
         pxDistanceJoint->setStiffness(distance.stiffness);

         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, distance.minDistance > 0);
         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, distance.maxDistance > 0);
         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eSPRING_ENABLED, distance.stiffness > 0);
      } else if (type == JointType::Revolute) {
         auto pxRevoluteJoint = PxRevoluteJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxRevoluteJoint;
         if (!pxJoint) {
            WARN("Cant create revolute joint");
            return;
         }

         if (revolute.limitEnable) {
            pxRevoluteJoint->setLimit(PxJointAngularLimitPair{ revolute.lowerLimit, revolute.upperLimit,
               PxSpring{ revolute.stiffness, revolute.damping } });
            pxRevoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eLIMIT_ENABLED, revolute.limitEnable);
         }

         if (revolute.driveEnable) {
            pxRevoluteJoint->setDriveVelocity(revolute.driveVelocity);
            pxRevoluteJoint->setDriveForceLimit(FloatInfToMax(revolute.driveForceLimit));
            pxRevoluteJoint->setDriveGearRatio(revolute.driveGearRatio);

            pxRevoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, revolute.driveEnable);
            pxRevoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_FREESPIN, revolute.driveFreespin);
         }
      } else if (type == JointType::Spherical) {
         auto pxSphericalJoint = PxSphericalJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxSphericalJoint;
         if (!pxJoint) {
            WARN("Cant create spherical joint");
            return;
         }
         // todo:
      } else if (type == JointType::Prismatic) {
         auto pxPrismaticJoint = PxPrismaticJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxPrismaticJoint;
         if (!pxJoint) {
            WARN("Cant create prismatic joint");
            return;
         }
         // todo:

         const auto tolerances = GetPxPhysics()->getTolerancesScale(); // todo: or set from component?

         // todo: name
         pxPrismaticJoint->setLimit(PxJointLinearLimitPair{ tolerances, prismatic.lowerLimit, prismatic.upperLimit });
         pxPrismaticJoint->setPrismaticJointFlag(PxPrismaticJointFlag::eLIMIT_ENABLED, true);
      } else {
         UNIMPLEMENTED();
      }

      pxJoint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, collisionEnable);
      pxJoint->setBreakForce(FloatInfToMax(breakForce), FloatInfToMax(breakTorque));

      auto pEntity = new Entity{ entity }; // todo: allocation
      pxJoint->userData = pEntity;
      pxJoint->getConstraint()->userData = pEntity;

      WakeUp();
   }

   void JointComponent::WakeUp() {
      auto actor0 = GetPxActor(entity0);
      auto actor1 = GetPxActor(entity1);

      PxWakeUp(actor0);
      PxWakeUp(actor1);
   }

   std::optional<Transform> JointComponent::GetAnchorTransform(Anchor anchor) const {
      auto actor = GetPxActor(anchor == Anchor::Anchor0 ? entity0 : entity1);
      if (!actor) {
         return {};
      }
      auto localFrame = anchor == Anchor::Anchor0 ? anchor0 : anchor1;
      // todo: use pb math instead of px
      auto globalFrame = actor->getGlobalPose() * PxTransform{ ConvertToPx(localFrame.position), ConvertToPx(localFrame.rotation) };
      return std::optional{ Transform{ConvertToPBE(globalFrame.p), ConvertToPBE(globalFrame.q)} };
   }


   STRUCT_BEGIN(RigidBodyComponent)
      STRUCT_FIELD(dynamic)
      STRUCT_FIELD(destructible)
      STRUCT_FIELD(hardness)
      STRUCT_FIELD(linearDamping)
      STRUCT_FIELD(angularDamping)
   STRUCT_END()

   STRUCT_BEGIN(RigidBodyShapeComponent)
   STRUCT_END()

   STRUCT_BEGIN(TriggerComponent)
   STRUCT_END()

   ENUM_BEGIN(JointType)
      ENUM_VALUE(Fixed)
      ENUM_VALUE(Distance)
      ENUM_VALUE(Revolute)
      ENUM_VALUE(Spherical)
      ENUM_VALUE(Prismatic)
   ENUM_END()

   STRUCT_BEGIN(JointAnchor)
      STRUCT_FIELD(position)
      STRUCT_FIELD(rotation)
   STRUCT_END()

   STRUCT_BEGIN(JointComponent::DistanceJoint)
      STRUCT_FIELD(minDistance)
      STRUCT_FIELD(maxDistance)

      STRUCT_FIELD(stiffness)
      STRUCT_FIELD(damping)
   STRUCT_END()

   STRUCT_BEGIN(JointComponent::RevoluteJoint)
      STRUCT_FIELD(limitEnable)
      STRUCT_FIELD(lowerLimit)
      STRUCT_FIELD(upperLimit)
      STRUCT_FIELD(stiffness)
      STRUCT_FIELD(damping)

      STRUCT_FIELD(driveEnable)
      STRUCT_FIELD(driveFreespin)
      STRUCT_FIELD(driveVelocity)
      STRUCT_FIELD(driveForceLimit)
      STRUCT_FIELD(driveGearRatio)
   STRUCT_END()

   STRUCT_BEGIN(JointComponent::PrismaticJoint)
      STRUCT_FIELD(lowerLimit)
      STRUCT_FIELD(upperLimit)
   STRUCT_END()

   auto CheckJointType(JointType type) {
      return [=](auto* byte) { return ((JointComponent*)byte)->type == type; };
   }

   STRUCT_BEGIN(JointComponent)
      STRUCT_FIELD(type)
   
      STRUCT_FIELD(entity0)
      STRUCT_FIELD(entity1)

      STRUCT_FIELD(anchor0)
      STRUCT_FIELD(anchor1)

      STRUCT_FIELD_USE(CheckJointType(JointType::Distance))
      // STRUCT_FIELD_FLAG(SkipName)
      STRUCT_FIELD(distance)

      STRUCT_FIELD_USE(CheckJointType(JointType::Revolute))
      // STRUCT_FIELD_FLAG(SkipName)
      STRUCT_FIELD(revolute)

      STRUCT_FIELD_USE(CheckJointType(JointType::Prismatic))
      // STRUCT_FIELD_FLAG(SkipName)
      STRUCT_FIELD(prismatic)

      STRUCT_FIELD(breakForce)
      STRUCT_FIELD(breakTorque)
      STRUCT_FIELD(collisionEnable)
   STRUCT_END()

   TYPER_REGISTER_COMPONENT(RigidBodyComponent);
   TYPER_REGISTER_COMPONENT(RigidBodyShapeComponent);
   TYPER_REGISTER_COMPONENT(TriggerComponent);
   TYPER_REGISTER_COMPONENT(JointComponent);

}
