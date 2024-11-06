#include "pch.h"
#include "PhysicsScene.h"

#include "Phys.h"
#include "PhysComponents.h"
#include "PhysUtils.h"
#include "PhysXTypeConvet.h"
#include "DestructEventListener.h"
#include "core/Profiler.h"
#include "scene/Component.h"
#include "scene/Entity.h"
#include "math/Shape.h"
#include "PhysQuery.h"

#include <NvBlastTk.h>


using namespace Nv::Blast;

namespace pbe {

   PhysicsScene::PhysicsScene(Scene& scene) : scene(scene) {
      scene.RegisterOnComponentEnableDisable<RigidBodyComponent>(
         [&](Entity entity) {
            auto& rb = entity.Get<RigidBodyComponent>();
            rb.pxRigidActor = nullptr;
            rb.destructData = nullptr;
            rb.tkActor = nullptr;

            rb.CreateOrUpdate(*pxScene, entity);
         },
         [&](Entity entity) {
            entity.Get<RigidBodyComponent>().Remove();
         }
      );

      scene.RegisterOnComponentUpdate<RigidBodyComponent>(
         [&](Entity entity) {
            entity.Get<RigidBodyComponent>().CreateOrUpdate(*pxScene, entity);
         }
      );

      scene.RegisterOnComponentEnableDisable<TriggerComponent>(
         [&](Entity entity) {
            entity.Get<TriggerComponent>().pxRigidActor = nullptr;
            AddTrigger(entity);
         },
         [&](Entity entity) {
            RemoveTrigger(entity);
         }
      );

      scene.RegisterOnComponentEnableDisable<JointComponent>(
         [&](Entity entity) {
            entity.Get<JointComponent>().pxJoint = nullptr;
            AddJoint(entity);
         },
         [&](Entity entity) {
            RemoveJoint(entity);
         }
      );

      scene.RegisterOnComponentUpdate<JointComponent>(
         [&](Entity entity) {
            entity.Get<JointComponent>().SetData(entity);
         }
      );

      PxSceneDesc sceneDesc(GetPxPhysics()->getTolerancesScale());
      sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
      sceneDesc.cpuDispatcher = GetPxCpuDispatcher();
      // sceneDesc.filterShader = PxDefaultSimulationFilterShader;
      sceneDesc.filterShader = PxSimulationFilterShader;
      sceneDesc.simulationEventCallback = new SimulationEventCallback(this);
      sceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
      pxScene = GetPxPhysics()->createScene(sceneDesc);
      pxScene->userData = this;

      PxPvdSceneClient* pvdClient = pxScene->getScenePvdClient();
      if (pvdClient) {
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
      }

      TkGroupDesc groupDesc;
      groupDesc.workerCount = 1;
      tkGroup = NvBlastTkFrameworkGet()->createGroup(groupDesc);

      destructEventListener = std::make_unique<DestructEventListener>(*this);

      // todo:
      damageParamsBuffer.reserve(1024 * 64);
   }

   PhysicsScene::~PhysicsScene() {
      ASSERT(tkGroup->getActorCount() == 0);
      PX_RELEASE(tkGroup);

      ASSERT(pxScene->getNbActors(PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC) == 0);
      delete pxScene->getSimulationEventCallback();
      pxScene->userData = nullptr;
      PX_RELEASE(pxScene);
   }

   RayCastResult PhysicsScene::RayCast(const vec3& origin, const vec3& dir, float maxDistance) {
      PxVec3 pxOrigin = ConvertToPx(origin);
      PxVec3 pxDir = ConvertToPx(dir);
      PxRaycastBuffer hit;

      if (pxScene->raycast(pxOrigin, pxDir, maxDistance, hit, PxHitFlag::ePOSITION | PxHitFlag::eNORMAL)) {
         ASSERT(hit.hasBlock);
         ASSERT(hit.nbTouches == 0);
         if (hit.hasBlock) {
            return RayCastResult{
               .physActor = *GetEntity(hit.block.actor),
               .position = ConvertToPBE(hit.block.position),
               .normal = ConvertToPBE(hit.block.normal),
               .distance = hit.block.distance,
            };
         }
      }

      return RayCastResult{};
   }

   RayCastResult PhysicsScene::SweepSphere(const vec3& origin, const vec3& dir, float maxDistance) {
      PxVec3 pxOrigin = ConvertToPx(origin);
      PxVec3 pxDir = ConvertToPx(dir);

      PxSweepBuffer hit;
      // todo:
      auto geom = PxSphereGeometry(0.5f);
      if (pxScene->sweep(geom, PxTransform{ pxOrigin }, pxDir, maxDistance, hit, PxHitFlag::ePOSITION | PxHitFlag::eNORMAL)) {
         ASSERT(hit.hasBlock);
         ASSERT(hit.nbTouches == 0);
         if (hit.hasBlock) {
            return RayCastResult{
                  .physActor = *GetEntity(hit.block.actor),
                  .position = ConvertToPBE(hit.block.position),
                  .normal = ConvertToPBE(hit.block.normal),
                  .distance = hit.block.distance,
            };
         }
      }

      return RayCastResult{};
   }

   RayCastResult PhysicsScene::Sweep(const Geom& geom, const Transform& trans, const vec3& dir, float maxDistance) {
      PxGeometryHolder physGeom = ConvertToPx(geom);
      PxTransform pose = ConvertToPx(trans);
      PxVec3 pxDir = ConvertToPx(dir);

      PxSweepBuffer hit;
      if (pxScene->sweep(physGeom.any(), pose, pxDir, maxDistance, hit, PxHitFlag::ePOSITION | PxHitFlag::eNORMAL)) {
         ASSERT(hit.hasBlock);
         ASSERT(hit.nbTouches == 0);
         return RayCastResult{
            .physActor = *GetEntity(hit.block.actor),
            .position = ConvertToPBE(hit.block.position),
            .normal = ConvertToPBE(hit.block.normal),
            .distance = hit.block.distance,
         };
      }

      return RayCastResult{};
   }

   OverlapResult PhysicsScene::OverlapAny(const Geom& geom, const Transform& trans) {
      PxGeometryHolder physGeom = ConvertToPx(geom);
      PxTransform pose = ConvertToPx(trans);

      PxOverlapBuffer hit;
      auto filter = PxQueryFilterData(PxQueryFlag::eDYNAMIC | PxQueryFlag::eSTATIC | PxQueryFlag::eANY_HIT);

      if (pxScene->overlap(physGeom.any(), pose, hit, filter)) {
         ASSERT(hit.hasBlock);
         ASSERT(hit.nbTouches == 0);
         return OverlapResult{
            .physActor = *GetEntity(hit.block.actor),
         };
      }

      return {};
   }

   Array<OverlapResult> PhysicsScene::OverlapAll(const Geom& geom, const Transform& trans) {
      PxGeometryHolder physGeom = ConvertToPx(geom);
      PxTransform pose = ConvertToPx(trans);

      // todo: check if hitBuffer is enough
      PxOverlapBufferN<32> hit;
      auto filter = PxQueryFilterData(PxQueryFlag::eDYNAMIC | PxQueryFlag::eSTATIC);

      if (pxScene->overlap(physGeom.any(), pose, hit, filter)) {
         ASSERT(!hit.hasBlock);
         ASSERT(hit.nbTouches > 0);

         Array<OverlapResult> result(hit.nbTouches);

         for (PxU32 i = 0; i < hit.nbTouches; ++i) {
            result[i] = OverlapResult {
               .physActor = *GetEntity(hit.touches[i].actor),
            };
         }

         return result;
      }

      return {};
   }

   void PhysicsScene::SyncPhysicsWithScene() {
      for (auto [_, trans, trigger] :
         scene.View<SceneTransformComponent, TriggerComponent, TransformChangedMarker>().each()) {
         trigger.pxRigidActor->setGlobalPose(GetTransform(trans));
      }

      for (auto [_, trans, rb] :
         scene.View<SceneTransformComponent, RigidBodyComponent, TransformChangedMarker>().each()) {
         rb.pxRigidActor->setGlobalPose(GetTransform(trans));
         PxWakeUp(rb.pxRigidActor);
      }
   }

   void PhysicsScene::Simulate(float dt) {
      // todo: this called at the beginning of update, but it's better to call it at the end of update
      PROFILE_CPU("Phys simulate");

      int steps = stepTimer.Update(dt);
      if (steps > 2) {
         stepTimer.Reset();
         steps = 2;
      }

      for (int i = 0; i < steps; ++i) {
         pxScene->simulate(stepTimer.GetActTime());
         pxScene->fetchResults(true);
         UpdateSceneAfterPhysics();

         // todo: or before simulate?
         tkGroup->process();
         damageParamsBuffer.clear();

         // for example destroy delayed entities
         scene.OnSync();
      }
   }

   void PhysicsScene::UpdateSceneAfterPhysics() {
      PxU32 nbActiveActors;
      PxActor** activeActors = pxScene->getActiveActors(nbActiveActors);

      for (PxU32 i = 0; i < nbActiveActors; ++i) {
         Entity entity = *static_cast<Entity*>(activeActors[i]->userData);
         auto& trans = entity.Get<SceneTransformComponent>();

         PxRigidActor* rbActor = activeActors[i]->is<PxRigidActor>();
         ASSERT_MESSAGE(rbActor, "It must be rigid actor");
         if (rbActor) {
            PxTransform pxTrans = rbActor->getGlobalPose();
            trans.SetPosition(ConvertToPBE(pxTrans.p));
            trans.SetRotation(ConvertToPBE(pxTrans.q));
         }
      }
   }

   void PhysicsScene::OnUpdate(float dt) {
      Simulate(dt);
   }

   void PhysicsScene::AddTrigger(Entity entity) {
      auto [trans, geom, trigger] = entity.Get<SceneTransformComponent, GeometryComponent, TriggerComponent>();

      PxGeometryHolder physGeom = GetPhysGeom(trans, geom);

      const PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eTRIGGER_SHAPE;
      PxShape* shape = GetPxPhysics()->createShape(physGeom.any(), *GetPxMaterial(), true, shapeFlags);

      PxRigidStatic* actor = GetPxPhysics()->createRigidStatic(GetTransform(trans));
      actor->attachShape(*shape);
      actor->userData = new Entity{ entity }; // todo: use fixed allocator
      pxScene->addActor(*actor);
      shape->release();

      trigger.pxRigidActor = actor;
   }

   void PhysicsScene::RemoveTrigger(Entity entity) {
      auto& trigger = entity.Get<TriggerComponent>();
      if (!trigger.pxRigidActor) {
         return;
      }

      pxScene->removeActor(*trigger.pxRigidActor);
      delete (Entity*)trigger.pxRigidActor->userData;
      trigger.pxRigidActor->userData = nullptr;

      trigger.pxRigidActor = nullptr;
   }

   void PhysicsScene::AddJoint(Entity entity) {
      auto& joint = entity.Get<JointComponent>();
      joint.pxJoint = nullptr; // todo: ctor copy this by value
      joint.SetData(entity);
   }

   void PhysicsScene::RemoveJoint(Entity entity) {
      auto& joint = entity.Get<JointComponent>();
      if (!joint.pxJoint) {
         return;
      }

      joint.WakeUp();

      delete (Entity*)joint.pxJoint->userData;
      joint.pxJoint->getConstraint()->userData = nullptr;
      joint.pxJoint->userData = nullptr;

      joint.pxJoint->release();
      joint.pxJoint = nullptr;
      
   }

   void* PhysicsScene::GetDamageParamsPlace(u32 paramSize) {
      u32 curSize = (u32)damageParamsBuffer.size();
      u32 reqSize = curSize + paramSize;
      ASSERT(reqSize <= damageParamsBuffer.capacity());
      damageParamsBuffer.resize(reqSize);
      return damageParamsBuffer.data() + curSize;
   }

}
