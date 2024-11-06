#pragma once
#include "core/Core.h"
#include "core/Ref.h"
#include "scene/System.h"
#include "utils/TimedAction.h"
#include "math/Types.h"


namespace Nv {
   namespace Blast {
      class TkGroup;
   }
}

namespace pbe {
   struct Geom;
   struct Transform;
   struct AABB;

   class Entity;
   class Scene;

   struct RayCastResult;
   struct OverlapResult;

   class DestructEventListener;

   class CORE_API PhysicsScene : public System {
   public:
      PhysicsScene(Scene& scene);
      ~PhysicsScene() override;

      RayCastResult RayCast(const vec3& origin, const vec3& dir, float maxDistance);

      RayCastResult SweepSphere(const vec3& origin, const vec3& dir, float maxDistance); // todo: remove
      RayCastResult Sweep(const Geom& geom, const Transform& trans, const vec3& dir, float maxDistance);

      OverlapResult OverlapAny(const Geom& geom, const Transform& trans);
      Array<OverlapResult> OverlapAll(const Geom& geom, const Transform& trans);

      void SyncPhysicsWithScene();
      void Simulate(float dt);
      void UpdateSceneAfterPhysics();

      void OnUpdate(float dt) override;

   private:
      friend struct RigidBodyComponent;

      physx::PxScene* pxScene = nullptr;
      Nv::Blast::TkGroup* tkGroup = nullptr;
      Own<DestructEventListener> destructEventListener;
      Scene& scene;

      TimedAction stepTimer{60.f}; // todo: config

      void AddTrigger(Entity entity);
      void RemoveTrigger(Entity entity);

      void AddJoint(Entity entity);
      void RemoveJoint(Entity entity);

      // todo:
      std::vector<u8> damageParamsBuffer;
      void* GetDamageParamsPlace(u32 paramSize);
      template<typename T>
      T& GetDamageParamsPlace() {
         return *(T*)GetDamageParamsPlace(sizeof(T));
      }

   };

}
