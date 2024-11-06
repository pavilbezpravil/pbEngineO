#pragma once

#include <limits>

#include "Core.h"
#include "Assert.h"

namespace pbe {

   class CORE_API UUID {
   public:
      UUID();
      constexpr UUID(u64 uuid) : uuid(uuid) {}

      bool operator==(const UUID& rhs) const { return uuid == rhs.uuid; }
      bool operator!=(const UUID& rhs) const { return !(*this == rhs); }

      operator const u64() const { return uuid; }

      bool Valid() const;

   private:
      u64 uuid;
   };

   SASSERT(std::is_move_constructible_v<UUID>);
   SASSERT(std::is_move_assignable_v<UUID>);
   SASSERT(std::is_move_assignable_v<int>);

   constexpr UUID UUID_INVALID = std::numeric_limits<u64>::max();

}

namespace std {

   template <>
   struct hash<pbe::UUID> {
      std::size_t operator()(const pbe::UUID& uuid) const {
         return hash<pbe::u64>()(uuid);
      }
   };

}
