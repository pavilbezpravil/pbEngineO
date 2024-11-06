#pragma once

#include <cstdint>
#include <string>
#include <vector>

// warning C4251: 'SomeClass::member': class 'OtherClass' needs to have dll-interface
// to be used by clients of class 'SomeClass'
#pragma warning( disable : 4251 )

#define EXTERN_C extern "C"

#ifdef CORE_API_EXPORT
   #define CORE_API  __declspec(dllexport)
#else
   #define CORE_API  __declspec(dllimport)
#endif

namespace pbe {

   using i8 = int8_t;
   using u8 = uint8_t;
   using i16 = int16_t;
   using u16 = uint16_t;
   using i32 = int32_t;
   using u32 = uint32_t;
   using i64 = int64_t;
   using u64 = uint64_t;

   using std::string;
   using std::string_view;

   using std::vector; // todo: remove from src
   template<typename T>
   using Array = std::vector<T>;

   template<typename Key, typename Value>
   using HashMap = std::unordered_map<Key, Value>;

   template<typename T>
   using DataView = std::span<T>;

   template<typename T>
   using Optional = std::optional<T>;

#define STRINGIFY(x) #x
#define CONCAT(a, b) a ## b

#define BIT(x) 1 << x

#define CALL_N_TIMES(f, times) \
   { \
      static int executed = times; \
      if (executed > 0) { \
            executed--; \
            f(); \
      } \
   }

#define CALL_ONCE(f) \
   CALL_N_TIMES(f, 1)

}
