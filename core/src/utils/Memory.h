#pragma once
#include "core/Core.h"

namespace pbe {
#define KB (1024)
#define MB (KB * 1024)
#define GB (MB * 1024)

   template <typename T>
   void Memset(T& data, int value) {
      memset(&data, value, sizeof(T));
   }

   template <typename T>
   void MemsetZero(T& data) {
      Memset(data, 0);
   }
}
