#include "pch.h"
#include "Scene.h"

#include "Component.h"
#include "Entity.h"
#include "typer/Typer.h"
#include "fs/FileSystem.h"
#include "rend/DbgRend.h"
#include "script/Script.h"
#include "typer/Serialize.h"
#include "physics/PhysicsScene.h"
#include "SceneHier.h"

namespace pbe {

   struct DelayedDestroyMarker {
   };

   Scene::Scene(bool withRoot) {
      if (withRoot) {
         SetRootEntity(CreateWithUUID(UUID{}, Entity{}, "Scene"));
      }

      // todo: for all scene is it needed?
      AddSystem(std::make_unique<PhysicsScene>(*this));
   }

   Scene::~Scene() {
      registry.clear(); // registry doesn't have call destructor for components without direct call clear
   }

   Entity Scene::Create(std::string_view name) {
      return CreateWithUUID(UUID{}, GetRootEntity(), name);
   }

   void Scene::DestroyDelayed(EntityID entityID) {
      GetOrAddComponent<DelayedDestroyMarker>(entityID);
   }

   void Scene::DestroyImmediate(EntityID entityID) {
      auto entity = Entity{ entityID, this };
      auto& trans = entity.GetTransform();

      // todo: mb use iterator on children?
      for (int iChild = (int)trans.children.size(); iChild > 0; --iChild) {
         auto& child = trans.children[iChild - 1];
         DestroyImmediate(child.GetEntityID());

         // note: scene component may be shrink during child destroy
         trans = entity.GetTransform();
      }
      trans.SetParentInternal();

      uuidToEntities.erase(entity.GetUUID());
      registry.destroy(entity.GetEntityID());
   }

   Entity Scene::GetEntity(UUID uuid) {
      auto it = uuidToEntities.find(uuid);
      return it == uuidToEntities.end() ? Entity{} : Entity{ uuidToEntities[(u64)uuid], this };
   }

   Entity Scene::GetRootEntity() {
      return Entity{ rootEntityId, this };
   }

   void Scene::SetRootEntity(const Entity& entity) {
      ASSERT(entity.GetTransform().HasParent() == false);
      ASSERT(rootEntityId == entt::null);
      rootEntityId = entity.GetEntityID();
   }

   Entity Scene::Duplicate(const Entity& entity) {
      // todo: dirty code
      auto name = entity.GetName();
      int nameLen = (int)strlen(name);

      int iter = nameLen - 1;
      while (iter >= 0 && (std::isdigit(name[iter]) || name[iter] == ' ')) {
         iter--;
      }

      int idx = iter + 1 != nameLen ? atoi(name + iter + 1) : 0;
      string_view namePrefix = string_view(name, iter + 1);
      //

      Entity duplicatedEntity = Create(std::format("{} {}", namePrefix, ++idx));

      DuplicateHier(duplicatedEntity, entity, false);

      auto& trans = entity.GetTransform();
      duplicatedEntity.GetTransform().SetParent(trans.parent, trans.GetChildIdx() + 1);
      duplicatedEntity.GetTransform().Local() = entity.GetTransform().Local();

      return duplicatedEntity;
   }

   bool Scene::EntityEnabled(EntityID entityID) const {
      return !HasAnyComponent<DisableMarker, DelayedDisableMarker, DelayedEnableMarker>(entityID);
   }

   void Scene::EntityEnable(EntityID entityID, bool enable, bool withChilds) {
      auto entity = Entity{ entityID, this };

      if (enable) {
         if (entity.Has<DelayedEnableMarker>()) {
            return;
         }
         if (entity.Has<DelayedDisableMarker>()) {
            entity.Remove<DelayedDisableMarker>();
            return;
         }
         if (!entity.Has<DisableMarker>()) {
            return;
         }

         if (withChilds) {
            for (auto& child : entity.GetTransform()) {
               EntityEnable(child.GetEntityID(), true);
            }
         }

         entity.Add<DelayedEnableMarker>();
         ASSERT(entity.Has<DisableMarker>() && !entity.Has<DelayedDisableMarker>());
      } else {
         // todo: when add child to disabled entity, it must be disabled too
         if (entity.HasAny<DisableMarker, DelayedDisableMarker>()) {
            return;
         }
         if (entity.Has<DelayedEnableMarker>()) {
            entity.Remove<DelayedEnableMarker>();
            return;
         }

         if (withChilds) {
            for (auto& child : entity.GetTransform()) {
               EntityEnable(child.GetEntityID(), false);
            }
         }

         entity.Add<DelayedDisableMarker>();
         ASSERT(!(entity.HasAny<DisableMarker, DelayedEnableMarker>()));
      }
   }

   void Scene::OnSync() {
      // destroy delayed entities
      for (auto e : registry.view<DelayedDestroyMarker>()) {
         DestroyImmediate(e);
      }
      registry.clear<DelayedDestroyMarker>();

      ProcessDelayedEnable();
   }

   void Scene::OnTick() {
      OnSync();

      // sync phys scene with changed transforms outside physics
      GetPhysics()->SyncPhysicsWithScene();
      ClearComponent<TransformChangedMarker>();

      for (auto [entityID, trans] : View<SceneTransformComponent>().each()) {
         trans.UpdatePrevTransform();
      }
   }

   void Scene::OnStart() {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.scripts) {
         si.sceneApplyFunc(*this, [](Entity entity, Script& script) { script.owner = entity; });
      }

      for (const auto& si : typer.scripts) {
         si.sceneApplyFunc(*this, [](Entity entity, Script& script) { script.OnEnable(); });
      }
   }

   void Scene::OnUpdate(float dt) {
      for (auto [entityID, td] : View<TimedDieComponent>().each()) {
         td.time -= dt;
         if (td.time < 0.f) {
            DestroyDelayed(entityID);
         }
      }
      OnSync();

      for (auto& system : systems) {
         system->OnUpdate(dt);
      }

      const auto& typer = Typer::Get();

      for (const auto& si : typer.scripts) {
         si.sceneApplyFunc(*this, [dt](Entity entity, Script& script) { script.OnUpdate(dt); });
      }
   }

   void Scene::OnStop() {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.scripts) {
         si.sceneApplyFunc(*this, [](Entity entity, Script& script) { script.OnDisable(); });
      }
   }

   void Scene::AddSystem(Own<System>&& system) {
      system->SetScene(this);
      systems.emplace_back(std::move(system));
   }

   Entity Scene::FindByName(std::string_view name) {
      for (auto [e, tag] : View<TagComponent>().each()) {
         if (tag.tag == name) {
            return Entity{e, this};
         }
      }
      return {};
   }

   u32 Scene::EntitiesCount() const {
      return (u32)uuidToEntities.size();
   }

   Own<Scene> Scene::Copy() const {
      auto pScene = std::make_unique<Scene>(false);

      Entity srcRoot{ rootEntityId, const_cast<Scene*>(this) };
      Entity dstRoot = pScene->CreateWithUUID(srcRoot.GetUUID(), Entity{}, srcRoot.GetName());
      pScene->SetRootEntity(dstRoot);

      pScene->DuplicateHier(dstRoot, srcRoot, true);

      return pScene;
   }

   static Scene* gCurrentDeserializedScene = nullptr;

   Scene* Scene::GetCurrentDeserializedScene() {
      return gCurrentDeserializedScene;
   }

   Entity Scene::CreateWithUUID(UUID uuid, const Entity& parent, std::string_view name) {
      auto entityID = registry.create();

      auto entity = Entity{ entityID, this };
      entity.Add<UUIDComponent>(uuid);
      if (!name.empty()) {
         entity.Add<TagComponent>(name.data());
      }
      else {
         // todo: support entity without name
         entity.Add<TagComponent>(std::format("{} {}", "Entity", EntitiesCount()));
      }

      entity.Add<SceneTransformComponent>(entity, parent);

      ASSERT(uuidToEntities.find(uuid) == uuidToEntities.end());
      uuidToEntities[uuid] = entityID;

      return entity;
   }

   void Scene::ProcessDelayedEnable() {
      // disable
      if (ViewAll<DelayedDisableMarker>()) {
         for (auto [_, handlers] : componentEventMap) {
            handlers.onDisableMultiple(*this);
         }
      }

      // todo: iterate over all systems or only scripts
      // const auto& typer = Typer::Get();
      // for (const auto& si : typer.scripts) {
      //    si.sceneApplyFunc(*this, [](Script& script) { script.OnDisable(); });
      // }

      for (auto e : ViewAll<DelayedDisableMarker>()) {
         registry.emplace<DisableMarker>(e);
      }
      registry.clear<DelayedDisableMarker>();

      // enable
      if (ViewAll<DelayedEnableMarker>()) {
         for (auto [_, handlers] : componentEventMap) {
            handlers.onEnableMultiple(*this);
         }
      }

      // todo: iterate over all systems or only scripts
      // for (const auto& si : typer.scripts) {
      //    si.sceneApplyFunc(*this, [](Script& script) { script.OnEnable(); });
      // }

      // todo: move to RenderScene
      for (auto [entityID, trans] : ViewAll<DelayedEnableMarker, SceneTransformComponent>().each()) {
         trans.UpdatePrevTransform();
         registry.erase<DisableMarker>(entityID);
      }
      registry.clear<DelayedEnableMarker>();
   }

   void Scene::EntityDisableImmediate(Entity& entity) {
      ASSERT(!(entity.HasAny<DisableMarker, DelayedEnableMarker, DelayedDisableMarker>()));
      entity.Add<DisableMarker>();
   }

   void Scene::DuplicateHier(Entity& dst, const Entity& src, bool copyUUID) {
      ASSERT(dst.GetScene() == this);
      // if copyUUID == true, dst must be in another scene
      ASSERT(!copyUUID || dst.GetScene() != src.GetScene());

      struct DuplicateContext {
         entt::entity enttEntity{ entt::null };
         bool enabled = false;
      };

      std::unordered_map<UUID, DuplicateContext> hierEntitiesMap;

      auto AddCopiedEntityToMap = [&](Entity& duplicated, const Entity& src) {
         hierEntitiesMap[src.GetUUID()] = DuplicateContext{ duplicated.GetEntityID(), src.Enabled() };
         // while duplicate entities, disable them
         EntityDisableImmediate(duplicated);
         };

      AddCopiedEntityToMap(dst, src);

      SceneHier::ApplyFuncForChildren(src, [&](Entity& child) {
         Entity duplicated = CreateWithUUID(copyUUID ? child.GetUUID() : UUID{}, NullEntity, child.GetName());
         AddCopiedEntityToMap(duplicated, child);
         }, false);

      SceneHier::ApplyFuncForChildren(src, [&hierEntitiesMap, dstScene = dst.GetScene()](Entity& src) {
         Entity dst = { hierEntitiesMap[src.GetUUID()].enttEntity, dstScene };

         auto& srcTrans = src.GetTransform();
         auto& dstTrans = dst.GetTransform();

         if (srcTrans.parent) {
            Entity dstParent = { hierEntitiesMap[srcTrans.parent.GetUUID()].enttEntity, dst.GetScene() };
            dstTrans.SetParent(dstParent, -1, true);
         }
         dstTrans.Local() = srcTrans.Local();
         dstTrans.UpdatePrevTransform();

         const auto& typer = Typer::Get();

         // todo: use entt for iterate components
         for (const auto& ci : typer.components) {
            auto* pSrc = ci.tryGetConst(src);
            if (!pSrc) {
               continue;
            }

            auto pDst = ci.copyCtor(dst, pSrc);

            const auto& ti = typer.GetTypeInfo(ci.typeID);
            if (!ti.hasEntityRef) {
               continue;
            }

            for (const auto& field : ti.fields) {
               auto& filedTypeInfo = typer.GetTypeInfo(field.typeID);

               if (filedTypeInfo.hasEntityRef) {
                  // todo: while dont support nested entity ref
                  ASSERT(filedTypeInfo.IsSimpleType());

                  // it like from src, because it was copied by value in 'copyCtor'
                  auto pDstEntity = (Entity*)((u8*)pDst + field.offset);
                  if (!*pDstEntity) {
                     continue;
                  }

                  auto uuid = pDstEntity->GetUUID();

                  auto it = hierEntitiesMap.find(uuid);
                  if (it != hierEntitiesMap.end()) {
                     *pDstEntity = Entity{ it->second.enttEntity, dst.GetScene() };
                  }
               }
            }
         }
         });

      // todo: mb create set with enabled entities?
      // todo: not fastest solution, find better way to implement copy scene, duplicate logic
      for (auto [_, context] : hierEntitiesMap) {
         EntityEnable(context.enttEntity, context.enabled, false);
      }

      ProcessDelayedEnable();
   }

   static string gAssetsPath = "../../assets/";

   string GetAssetsPath(string_view path) {
      return gAssetsPath + path.data();
   }

   void SceneSerialize(std::string_view path, Scene& scene) {
      Serializer ser;

      {
         SERIALIZER_MAP(ser);
         {
            // ser.KeyValue("sceneName", "test_scene");

            ser.Key("entities");
            SERIALIZER_SEQ(ser);
            {
               std::vector<u64> entitiesUuids;

               for (auto [_, uuid] : scene.ViewAll<UUIDComponent>().each()) {
                  entitiesUuids.emplace_back((u64)uuid.uuid);
               }

               std::ranges::sort(entitiesUuids);

               for (auto uuid : entitiesUuids) {
                  Entity entity = scene.GetEntity(uuid);
                  EntitySerialize(ser, entity);
               }
            }
         }
      }

      // todo:
      // GetAssetsPath(path)
      ser.SaveToFile(path);
   }

   Own<Scene> SceneDeserialize(std::string_view path) {
      INFO("Deserialize scene '{}'", path);

      if (!fs::exists(path)) {
         WARN("Cant find file '{}'", path);
         return {};
      }

      Own<Scene> scene = std::make_unique<Scene>(false);

      gCurrentDeserializedScene = scene.get();

      // todo:
      // GetAssetsPath(path);
      auto deser = Deserializer::FromFile(path);

      // auto sceneName = deser["sceneName"].As<string>();

      auto entitiesNode = deser["entities"];

      // on first iteration create all entities
      for (int i = 0; i < entitiesNode.Size(); ++i) {
         auto it = entitiesNode[i];

         auto uuid = it["uuid"].As<u64>();

         string entityTag;
         if (it["tag"]) {
            entityTag = it["tag"].As<string>();
         }

         Entity entity = scene->CreateWithUUID(UUID{ uuid }, Entity{}, entityTag);
         scene->EntityDisableImmediate(entity);
      }

      // on second iteration create all components
      for (int i = 0; i < entitiesNode.Size(); ++i) {
         auto it = entitiesNode[i];
         EntityDeserialize(it, *scene);
      }

      entt::entity rootEntityId = entt::null;
      for (auto [e, trans] : scene->ViewAll<SceneTransformComponent>().each()) {
         if (trans.parent) {
            continue;
         }

         if (rootEntityId != entt::null) {
            WARN("Scene has several roots. Create new root and parenting to it all entities without parent");
            auto newRoot = scene->CreateWithUUID(UUID{}, Entity{}, "Scene");
            rootEntityId = newRoot.GetEntityID();

            for (auto [e, trans] : scene->ViewAll<SceneTransformComponent>().each()) {
               if (!trans.parent) {
                  trans.SetParent(newRoot);
               }
            }

            break;
         }

         rootEntityId = e;
         // todo: remove after all scenes will have root entity
         // break;
      }

      Entity rootEntity = { rootEntityId, scene.get() };
      scene->SetRootEntity(rootEntity);

      scene->ProcessDelayedEnable();

      gCurrentDeserializedScene = nullptr;

      return scene;
   }

   void EntitySerialize(Serializer& ser, const Entity& entity) {
      SERIALIZER_MAP(ser);
      {
         auto uuid = (u64)entity.Get<UUIDComponent>().uuid;

         ser.KeyValue("uuid", uuid);

         if (const auto* c = entity.TryGet<TagComponent>()) {
            ser.KeyValue("tag", c->tag);
         }

         if (!entity.Enabled()) {
            // todo: without value. As I understend, yaml doesnt support key without value
            ser.KeyValue("disabled", true);
         }

         ser.Ser("SceneTransformComponent", entity.GetTransform());

         const auto& typer = Typer::Get();

         for (const auto& ci : typer.components) {
            const auto& ti = typer.GetTypeInfo(ci.typeID);

            auto* ptr = (u8*)ci.tryGetConst(entity);
            if (ptr) {
               const char* name = ti.name.data();
               ser.Ser(name, ci.typeID, ptr);
            }
         }
      }
   }

   void EntityDeserialize(const Deserializer& deser, Scene& scene) {
      auto uuid = deser["uuid"].As<u64>();

      Entity entity = scene.GetEntity(uuid);
      if (entity) {
         // todo: ����� �� ��� �������, ������ ����?
         // entity.RemoveAll<UUIDComponent, TagComponent, SceneTransformComponent>();
      } else {
         ASSERT(false);
         entity = scene.CreateWithUUID(uuid, Entity{});
      }

      ASSERT(!entity.Enabled());

      string name = std::invoke([&] {
         if (deser["tag"]) {
            return deser["tag"].As<string>();
         }
         return string{};
      });

      entity.GetOrAdd<TagComponent>().tag = name;

      bool enabled = !deser["disabled"];

      deser.Deser("SceneTransformComponent", entity.GetTransform());

      const auto& typer = Typer::Get();

      // std::array<u8, 128> data; // todo:

      for (const auto& ci : typer.components) {
         const auto& ti = typer.GetTypeInfo(ci.typeID);

         // ASSERT(ti.typeSizeOf < data.size());

         const char* name = ti.name.data();

         if (auto node = deser[name]) {
            // todo: use move ctor
            auto* ptr = (u8*)ci.getOrAdd(entity);
            deser.Deser(name, ci.typeID, ptr);
         }
      }

      if (enabled) {
         entity.GetScene()->EntityEnable(entity.GetEntityID(), true, false);
      }
   }
}
