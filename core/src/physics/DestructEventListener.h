#pragma once
#include <NvBlastTkEvent.h>

#include "core/Core.h"
#include "math/Types.h"


namespace Nv {
   namespace Blast {
      class TkGroup;
   }
}

namespace pbe {

   class PhysicsScene;

   class DestructEventListener : public Nv::Blast::TkEventListener {
   public:
      DestructEventListener(PhysicsScene& scene) : scene(scene) {}

      void receive(const Nv::Blast::TkEvent* events, uint32_t eventCount) override;
   private:
      PhysicsScene& scene;

      std::vector<u32> visibleChunkIndices;
   };

}
