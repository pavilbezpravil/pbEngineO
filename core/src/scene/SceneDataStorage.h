#pragma once

#include <any>
#include "core/Core.h"
#include "core/Type.h"

namespace pbe {
   struct CORE_API SceneDataStorage {
      template <typename T>
      T& GetSceneData() {
         return std::any_cast<T&>(datas[GetTypeID<T>()]);
      }

      template <typename T>
      const T& GetSceneData() const {
         return std::any_cast<T&>(datas[GetTypeID<T>()]);
      }

      template <typename T>
      T* GetSceneDataPtr() {
         auto it = datas.find(GetTypeID<T>());
         return it != datas.end() ? &std::any_cast<T&>(it->second) : nullptr;
      }

      template <typename T>
      const T* GetSceneDataPtr() const {
         auto it = datas.find(GetTypeID<T>());
         return it != datas.end() ? &std::any_cast<T&>(it->second) : nullptr;
      }

      template <typename T>
      bool HasSceneData() const {
         return datas.find(GetTypeID<T>()) != datas.end();
      }

      template <typename T, typename... Args>
      void AddSceneData(Args&&... args) {
         datas[GetTypeID<T>()] = T(std::forward<Args>(args)...);
      }

      template <typename T>
      void RemoveSceneData() {
         datas.erase(GetTypeID<T>());
      }

   private:
      HashMap<TypeID, std::any> datas;
   };
}
