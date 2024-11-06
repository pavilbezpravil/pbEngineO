#include "pch.h"
#include "Hash.h"

namespace pbe {
   void HashCombineMemory(std::size_t& seed, const void* data, u32 dataSizeInBytes) {
      HashCombineMemoryInternal(seed, (const u64*)data, dataSizeInBytes);
      HashCombineMemoryInternal(seed, (const u32*)data, dataSizeInBytes);
      HashCombineMemoryInternal(seed, (const u8*)data, dataSizeInBytes);
   }
}
