#pragma once
#include "core/Core.h"

namespace pbe {
   CORE_API std::wstring ConvertToWString(const std::string& string);
   CORE_API std::string ConvertToString(const std::wstring& wstring);

   CORE_API size_t StrHash(std::string_view str);
}
