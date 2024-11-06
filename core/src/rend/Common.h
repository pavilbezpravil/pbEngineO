#pragma once

#include <string_view>
#include <wrl/client.h>
#include "core/Assert.h"

struct ID3D11DeviceChild;

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace pbe {

   // todo: rename
   // void ThrowIfFailed(HRESULT hr);
#define ThrowIfFailed(hRes) \
      if (FAILED(hRes)) { \
         ERROR("D3D12 Failed!"); \
         ASSERT(false); \
      }

   void SetDbgName(ID3D12Object* obj, std::string_view name);

}
