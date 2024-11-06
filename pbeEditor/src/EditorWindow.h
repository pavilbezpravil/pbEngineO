#pragma once
#include <string>

#include "core/Common.h"
#include "math/Types.h"

namespace pbe {

   class EditorWindow {
      NON_COPYABLE(EditorWindow);
   public:
      EditorWindow(std::string_view name) : name(name) { }
      virtual ~EditorWindow() = default;

      virtual void OnBefore() {}
      void OnPoolWindowState();
      virtual void OnWindowUI() = 0;
      virtual void OnAfter() {}

      virtual void OnUpdate(float dt) {}

      virtual void OnFocus() {}
      virtual void OnLostFocus() {}

      std::string name{ "EditorWindow" };
      bool show = true;
      bool focused = false;

      int2 windowPosition{};
      int2 windowSize{};
   };

}
