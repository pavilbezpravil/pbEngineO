#include "pch.h"
#include "Common.h"
#include "Device.h"
#include "core/Assert.h"
#include "utils/String.h"
#include "core/Log.h"

namespace pbe {

   // void ThrowIfFailed(HRESULT hr) {
   //    if (FAILED(hr)) {
   //       // todo: Log error
   //       ERROR("D3D12 Failed!");
   //       ASSERT(false);
   //    }
   // }

   void SetDbgName(ID3D12Object* obj, std::string_view name) {
      if (!name.empty()) {
         auto nameW = ConvertToWString(name.data());
         obj->SetName(nameW.data());
      }
   }

}
