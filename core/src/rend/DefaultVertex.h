#pragma once

#include "core/Core.h"
#include "math/Types.h"

namespace pbe {
   class Mesh;

   struct VertexPos {
      vec3 position;

      static inline std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   struct VertexPosNormal {
      vec3 position;
      vec3 normal;

      static inline std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   struct VertexPosColor {
      vec3 position;
      vec4 color;

      static inline std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   struct CORE_API VertexPosUintColor {
      vec3 position;
      u32 uintValue;
      vec4 color;

      static inline std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc;
   };
}
