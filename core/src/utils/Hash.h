#pragma once
#include "core/Core.h"

namespace pbe {

   // https://stackoverflow.com/questions/19195183/how-to-properly-hash-the-custom-struct
   template <class T>
   void HashCombine(std::size_t& seed, const T& v) {
      std::hash<T> hasher;
      seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
   }

   template <class T>
   void HashCombineMemoryInternal(std::size_t& seed, const T* data, u32& dataSizeInBytes) {
      constexpr int stepSize = sizeof(T);

      while (dataSizeInBytes >= stepSize) {
         HashCombine(seed, *(T*)data);
         data = data + 1;
         dataSizeInBytes -= stepSize;
      }
   }

   void HashCombineMemory(std::size_t& seed, const void* data, u32 dataSizeInBytes);

   template <class T>
   void HashCombineMemory(std::size_t& seed, const T& data) {
      HashCombineMemory(seed, &data, sizeof(T));
   }
   
}
