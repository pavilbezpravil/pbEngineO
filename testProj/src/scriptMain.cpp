#include "pch.h"

#include "app/Input.h"
#include "math/Random.h"
#include "physics/PhysComponents.h"
#include "physics/PhysicsScene.h"
#include "scene/Component.h"
#include "scene/Utils.h"
#include "script/Script.h"
#include "typer/Registration.h"
#include "utils/TimedAction.h"
#include "physics/PhysQuery.h"
#include "rend/DbgRend.h"
#include "math/Shape.h"
#include "math/Common.h"


namespace pbe {

   struct TestValues {
      float floatValue = 2;
      int intValue = 2;
   };

   STRUCT_BEGIN(TestValues)
      STRUCT_FIELD(floatValue)
      STRUCT_FIELD(intValue)
   STRUCT_END()

   class TestScript : public Script {
   public:
      void OnEnable() override {
         INFO("OnEnable {}", GetName());
      }

      void OnDisable() override {
         INFO("OnDisable {}", GetName());
      }

      void OnUpdate(float dt) override {
         if (doMove) {
            auto& tran= owner.Get<SceneTransformComponent>();
            tran.Local().position.y += speed * dt;
         }
      }

      TestValues values;
      float speed = 1;
      bool doMove = false;
   };

   STRUCT_BEGIN(TestScript)
      STRUCT_FIELD(values)
      STRUCT_FIELD(speed)
      STRUCT_FIELD(doMove)
   STRUCT_END()
   TYPER_REGISTER_SCRIPT(TestScript);


   class TransformPrinterScript : public Script {
   public:
      void OnUpdate(float dt) override {
         if (print) {
            const auto& trans = owner.GetTransform();
            INFO("{}: position {}, rotation {}, scale {}", owner.GetName(), trans.Position(), trans.Rotation(), trans.Scale());
         }
      }

      bool print = true;
   };

   STRUCT_BEGIN(TransformPrinterScript)
      STRUCT_FIELD(print)
   STRUCT_END()
   TYPER_REGISTER_SCRIPT(TransformPrinterScript);


   class CubeSpawnerScript : public Script {
   public:
      void OnEnable() override {
         timer = {freq};
         INFO(__FUNCTION__);
      }

      void OnDisable() override {
         INFO(__FUNCTION__);
      }

      void OnUpdate(float dt) override {
         if (spawn) {
            auto& tran = owner.GetTransform();

            int steps = timer.Update(dt);
            while (steps-- > 0) {
               Entity e = CreateCube(GetScene(), CubeDesc {
                     .parent = owner,
                     .color = Random::Color(),
                  });

               e.Get<RigidBodyComponent>().SetLinearVelocity(tran.Forward() * initialSpeed);
            }
         } else {
            timer.Reset();
         }
      }

      bool spawn = false;
      float freq = 1;
      float initialSpeed = 0;

   private:
      TimedAction timer;
   };

   STRUCT_BEGIN(CubeSpawnerScript)
      STRUCT_FIELD(spawn)
      STRUCT_FIELD(freq)
      STRUCT_FIELD(initialSpeed)
   STRUCT_END()
   TYPER_REGISTER_SCRIPT(CubeSpawnerScript);

   RayCastResult RayCastWithDebug(Scene& scene, const vec3& origin, const vec3& direction, float maxDistance) {
      // todo: remove dbg draw on dist build

      auto result = scene.GetPhysics()->RayCast(origin, direction, maxDistance);
      if (result) {
         DbgDrawLine(scene, origin, result.position, Color_Red);
         DbgDrawSphere(scene, Sphere{ result.position, 0.2f }, Color_Red);
      } else {
         DbgDrawLine(scene, origin, origin + direction * maxDistance, Color_Green);
      }

      return result;
   }

   class MovementControllerScript : public Script {
   public:
      void OnUpdate(float dt) override {
         if (!owner.Has<RigidBodyComponent>()) {
            return;
         }

         auto& trans = owner.GetTransform();
         auto rb = *owner.TryGet<RigidBodyComponent>();

         if (!cameraAngleInited) {
            cameraAngleInited = true;

            vec3 direction = trans.Forward();
            cameraAngle.x = -atan2(direction.x, direction.z);
            cameraAngle.y = asin(direction.y);
         }

         float cameraMouseSpeed = 0.1f / 50;
         cameraAngle += vec2(Input::GetMouseDelta()) * cameraMouseSpeed * vec2(1, 1);

         // todo:
         trans.SetRotation(vec3{cameraAngle.y, cameraAngle.x, 0});

         vec3 cameraInput{};
         if (Input::IsKeyPressing(KeyCode::A)) {
            cameraInput.x = -1;
         }
         if (Input::IsKeyPressing(KeyCode::D)) {
            cameraInput.x = 1;
         }
         if (Input::IsKeyPressing(KeyCode::W)) {
            cameraInput.z = 1;
         }
         if (Input::IsKeyPressing(KeyCode::S)) {
            cameraInput.z = -1;
         }

         if (auto rb = owner.TryGet<RigidBodyComponent>()) {
            if (Input::IsKeyDown(KeyCode::Space)) {
               rb->AddForce(vec3_Up * 5.f, ForceMode::VelocityChange);
            }
         }

         if (cameraInput != vec3{}) {
            cameraInput = Normalize(cameraInput);

            float moveSpeed = speed;
            if (Input::IsKeyPressing(KeyCode::Shift)) {
               moveSpeed *= 5;
            }

            vec3 alignForward = Normalize(Cross(trans.Right(), vec3_Up));
            vec3 alignRight = -Normalize(Cross(alignForward, vec3_Up));
            vec3 moveDirection = alignRight * cameraInput.x + alignForward * cameraInput.z;

            // todo: time independent lerp
            moveSpeed = Lerp(Length(rb.GetLinearVelocity()), speed, 0.1f);

            rb.SetLinearVelocity(moveDirection * moveSpeed);
         }
      }

      float speed = 5;

      bool cameraAngleInited = false;
      vec2 cameraAngle{};
   };

   STRUCT_BEGIN(MovementControllerScript)
      STRUCT_FIELD(speed)
   STRUCT_END()
   TYPER_REGISTER_SCRIPT(MovementControllerScript);

   vec3 Truncate(const vec3& v, float max) {
      float length = Length(v);
      return length > max ? v / length * max : v;
   }

   class SteeringAgent {
   public:
      vec3 position;
      vec3 velocity;
      float maxSpeed = 1;
      float maxForce = 1;

      void Seek(const vec3& target) {
         vec3 desired = Normalize(target - position) * maxSpeed;
         vec3 steer = desired - velocity;
         accumulatedSteering += steer;
      }

      vec3 GetSteering() {
         auto steering= Truncate(accumulatedSteering, maxForce);
         accumulatedSteering = vec3_Zero;
         return steering;
      }

      void ApplySteering() {
         velocity += GetSteering();
      }

   private:
      vec3 accumulatedSteering = vec3_Zero;
   };

   template<typename T>
   vec2 GetXZ(const T& v) {
      return {v.x, v.z};
   }

   class EnemyScript : public Script {
   public:
      void OnUpdate(float dt) override {
         if (!owner.Has<RigidBodyComponent>()) {
            return;
         }

         auto& trans = owner.GetTransform();
         auto rb = *owner.TryGet<RigidBodyComponent>();

         // todo: dont use FindByName
         Entity targetEntity = GetScene().FindByName("Player");
         if (!targetEntity) {
            return;
         }

         auto position = trans.Position();
         auto forward = trans.Forward();

         vec3 extends = vec3_Half;

         auto OverlapAABBWithDebug = [] (Scene& scene, const vec3& center, const vec3& extends) {
            bool overlapping = scene.GetPhysics()->OverlapAny(Geom::Box(extends), Transform{ center });
            Color color = overlapping ? Color_Red : Color_Green;
            GetDbgRend(scene)->DrawAABB(nullptr, AABB::FromExtends(center, extends), color.WithAlpha(0.1f));
         };

         i32 xHalfSize = 20;
         i32 zHalfSize = 20;

         for (int y = 0; y < 1; ++y) {
            for (int x = -xHalfSize; x < xHalfSize; ++x) {
               for (int z = -zHalfSize; z < zHalfSize; ++z) {
                  vec3 point = extends + vec3{ x, y, z } * extends * 2.f;
                  OverlapAABBWithDebug(GetScene(), point, extends * 0.95f);
               }
            }
         }

         auto overlapped = GetScene().GetPhysics()->OverlapAll(Geom::Box(vec3_One * 10.f), Transform{ position });

         agent.position = position;
         agent.velocity = rb.GetLinearVelocity();
         agent.maxSpeed = movementSpeed;
         agent.maxForce = 1;

         agent.Seek(targetEntity.GetTransform().Position());
         agent.ApplySteering();

         // todo: dont use y component
         auto linearVelocity = agent.velocity;
         linearVelocity.y = rb.GetLinearVelocity().y;
         rb.SetLinearVelocity(linearVelocity);

         bool desiredForwardOnRight = Dot(trans.Right(), agent.velocity) > 0.f;
         float angleWithVelocity = acos(Dot(GetXZ(forward), GetXZ(NormalizeSafe(agent.velocity))));
         vec3 angularSpeed = vec3_Up * Saturate(angleWithVelocity / 0.1f) * 5.f;
         rb.SetAngularVelocity(desiredForwardOnRight ? angularSpeed : -angularSpeed);

         auto targetPos = targetEntity.GetTransform().Position();
         auto directionToTarget = Normalize(targetPos - position);
         float angle = acos(Dot(forward, directionToTarget));

         auto velocity= rb.GetLinearVelocity();

         // if (angle > viewAngle / 2) {
         //    // rb.SetAngularVelocity(vec3_Up);
         //    return;
         // }

         // auto result = RayCastWithDebug(GetScene(), origin + directionToPlayer * 1.f, directionToPlayer, viewDistance);
         // if (!result || result.physActor != player) {
         //    return;
         // }

         vec3 targetDirection = directionToTarget;

         bool useObstacleAvoidance = false;
         if (useObstacleAvoidance) {
            float obstacleViewDistance = 3.f;

            i32 numRays = 1;

            float halfViewAngle = viewAngle / 2;

            targetDirection = vec3_Zero;

            bool hasTargetDirection = false;
            for (int i = 0; i < numRays; ++i) {
               float angle = Lerp(-halfViewAngle, halfViewAngle, (float)i / (numRays - 1));
               vec3 direction = forward * quat{ vec3{0, angle, 0} };
               auto hitResult = RayCastWithDebug(GetScene(), position + direction * 1.f, direction, obstacleViewDistance);
               if (!hitResult || hitResult.physActor == targetEntity) {
                  // targetDirection += direction;
                  // hasTargetDirection = true;
               }

               targetDirection += direction;
               hasTargetDirection = true;
            }

            if (!hasTargetDirection) {
               return;
            }

            targetDirection = Normalize(targetDirection);

            INFO("targetDirection   {}", targetDirection);
            INFO("directionToPlayer {}", directionToTarget);
         }

         // bool targetOnRight = Dot(trans.Right(), targetDirection) > 0.f;
         // float angleWithTarget = acos(Dot(forward, targetDirection));
         // vec3 angularSpeed = vec3_Up * Saturate(angleWithTarget / 0.1f);
         // rb.SetAngularVelocity(targetOnRight ? angularSpeed : -angularSpeed);

         // if (angle < viewAngle / 4) {
         //    auto linearVelocity = forward * movementSpeed;
         //    linearVelocity.y = rb.GetLinearVelocity().y;
         //    rb.SetLinearVelocity(linearVelocity);
         // }

         // Steering behavior


      }

      SteeringAgent agent;

      float movementSpeed = 2;
      float viewAngle = PI / 2;
      float viewDistance = 10.f;
   };

   STRUCT_BEGIN(EnemyScript)
      STRUCT_FIELD(movementSpeed)
      STRUCT_FIELD(viewAngle)
      STRUCT_FIELD(viewDistance)
   STRUCT_END()
   TYPER_REGISTER_SCRIPT(EnemyScript);

}
