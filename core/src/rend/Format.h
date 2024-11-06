#pragma once
#include "core/Assert.h"

namespace pbe {
   using Format = DXGI_FORMAT;

   bool HasAlpha(Format format);
   u32 BitsPerPixel(Format format);

   bool IsUAVCompatibleFormat(Format format);
   bool IsSRGBFormat(Format format);
   bool IsBGRFormat(Format format);
   bool IsDepthFormat(Format format);

   Format GetTypelessFormat(Format format);
   Format GetSRGBFormat(Format format);
   Format GetUAVCompatableFormat(Format format);

   Format GetSRVFormat(Format defaultFormat);
   Format GetUAVFormat(Format defaultFormat);

   Format GetDSVFormat(Format defaultFormat);

   Format GetDepthFormat(Format defaultFormat);

   DXGI_FORMAT GetStencilFormat(DXGI_FORMAT defaultFormat);
}
