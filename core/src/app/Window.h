#pragma once

#include "math/Types.h"
#include "core/Core.h"
#include <functional>

namespace pbe {

   struct Event;

   class CORE_API Window {
   public:
      struct Desc {
         uint2 size;
         string title = "pbEngine";
      };

      Window(const Desc& desc);
      ~Window();

      void Update();

      const Desc& GetDesc() const { return desc; }

      void SetTitle(string_view title);

      int2 GetClientPos() const;
      int2 GetClientSize() const;

      int2 GetMousePosition() const;
      int2 GetAbsolutePosition(int2 pos) const;

      void ClipMouse(int2 pos, int2 size);
      void UnclipMouse();

      WNDCLASSEX wc{};
      HWND hwnd{};

      using EventCallbackFn = std::function<void(Event&)>;
      EventCallbackFn eventCallback;
   private:
      Desc desc;
   };

   extern CORE_API Window* sWindow;

}
