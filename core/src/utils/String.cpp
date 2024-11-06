#include "pch.h"

#include "String.h"
#include <codecvt>

namespace pbe {
   std::wstring ConvertToWString(const std::string& string) {
      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
      return converter.from_bytes(string);
   }

   std::string ConvertToString(const std::wstring& wstring) {
      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
      return converter.to_bytes(wstring);
   }

   size_t StrHash(std::string_view str) {
      return std::hash<std::string_view>()(str);
   }
}
