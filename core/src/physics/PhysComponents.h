#pragma once
#include "core/Core.h"
#include "math/Transform.h"
#include "scene/Entity.h"


class NvBlastExtDamageAccelerator;

namespace Nv {
   namespace Blast {
      class TkActor;
      class TkAsset;
   }
}

namespace pbe {
   class DbgRend;

   // todo: while have two way for handle change in component data
   // in future choose one


   // todo:
   struct ChunkInfo {
      vec3 size;
      // u32 depth; // todo:
      bool isSupport = false;
      bool isLeaf;
   };

   // todo: name
   struct DestructData {
      Nv::Blast::TkAsset* tkAsset = nullptr;
      NvBlastExtDamageAccelerator* damageAccelerator = nullptr;
      std::vector<ChunkInfo> chunkInfos;
   };

   enum class DestructDbgRendFlags {
      None = 0,
      Chunks = BIT(1),
      ChunksCentroid = BIT(2),
      BondsHealth = BIT(3),
      BondsCentroid = BIT(4),
   };

   DEFINE_ENUM_FLAG_OPERATORS(DestructDbgRendFlags)

   enum class ForceMode {
      Force,
      Impulse,
      VelocityChange,
      Acceleration,
   };

   struct CORE_API RigidBodyComponent {

      void ApplyDamage(const vec3& posW, float damage);

      vec3 GetLinearVelocity() const;
      void SetLinearVelocity(const vec3& v, bool autowake = true);

      vec3 GetAngularVelocity() const;
      void SetAngularVelocity(const vec3& v, bool autowake = true);

      void AddForce(const vec3& force, ForceMode mode = ForceMode::Force, bool autowake = true);
      void AddTorque(const vec3& torque, ForceMode mode = ForceMode::Force, bool autowake = true);

      void MakeDestructable();

      // todo: internal
      void SetDestructible(Nv::Blast::TkActor& tkActor, DestructData& destructData);

      bool IsDestructible() const { return destructible; }
      physx::PxRigidActor* GetPxRigidActor() { return pxRigidActor; }

      void DbgRender(DbgRend& dbgRend, DestructDbgRendFlags flags) const;

      bool dynamic = false;
      bool destructible = false;

      // todo:
      float hardness = 50.f;

      float linearDamping = 0.03f;
      float angularDamping = 0.03f;


      // todo: private:
      physx::PxRigidActor* pxRigidActor = nullptr;
      Nv::Blast::TkActor* tkActor = nullptr;

      DestructData* destructData = nullptr;  // todo:
      // todo:
      std::unordered_map<u32, EntityID> chunkToEntity;

   private:
      void CreateOrUpdate(physx::PxScene& pxScene, Entity& entity);
      void Remove();
      void RemoveDestruct();

      void AddShapesHier(const Entity& entity);

      PhysicsScene& GetPhysScene() {
         return *(PhysicsScene*)pxRigidActor->getScene()->userData;
      }

      Entity& GetEntity() const {
         return *(Entity*)pxRigidActor->userData;
      }

      friend class PhysicsScene;
   };

   struct CORE_API RigidBodyShapeComponent {
      // todo:
      float friction = 0.7f;
   };

   // struct CORE_API DestructionChunkComponent {
   //
   // };

   struct TriggerComponent {
      physx::PxRigidActor* pxRigidActor = nullptr;
   };

   enum class JointType {
      Fixed,
      Distance,
      Revolute,
      Spherical,
      Prismatic,
      // D6,
      // Gear,
      // RackAndPinion,
      // Contact
   };

   struct JointAnchor {
      vec3 position{};
      quat rotation = quat_Identity;
   };

   struct JointComponent {
      JointType type = JointType::Distance;

      Entity entity0;
      Entity entity1;

      JointAnchor anchor0;
      JointAnchor anchor1;

      struct FixedJoint {
      } fixed;

      struct DistanceJoint {
         float minDistance = 0;
         float maxDistance = 0;

         float stiffness = 1000.f;
         float damping = 0.5;
      } distance;

      struct RevoluteJoint {
         // limit
         bool limitEnable = false; // todo: make flags
         float lowerLimit = 0;
         float upperLimit = 0;
         float stiffness = 0;
         float damping = 0;

         bool driveEnable = false; // todo: make flags
         bool driveFreespin = false; // todo: make flags
         float driveVelocity = 0;
         float driveForceLimit = INFINITY;
         float driveGearRatio = 1;
      } revolute;

      struct SphericalJoint {
      } spherical;

      struct PrismaticJoint {
         float lowerLimit = -1;
         float upperLimit = 1;
      } prismatic;

      float breakForce = INFINITY;
      float breakTorque = INFINITY;
      bool collisionEnable = false;

      physx::PxJoint* pxJoint = nullptr;

      JointComponent() = default;
      JointComponent(JointType type);

      bool IsValid() const { return pxJoint != nullptr && entity0 && entity1; }

      void SetData(const Entity& entity);

      void WakeUp();

      enum class Anchor {
         Anchor0,
         Anchor1,
      };

      std::optional<Transform> GetAnchorTransform(Anchor anchor) const;
   };

}
