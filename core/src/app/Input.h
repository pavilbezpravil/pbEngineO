#pragma once
#include "KeyCode.h"
#include "core/Core.h"
#include "math/Types.h"


namespace pbe {
   struct Event;


   // todo: imgui suppresed some key events

   class CORE_API Input {
   public:
      Input(); // todo:
      // static void Init();

      static int2 GetMousePosition();
      static int2 GetMouseDelta();

      static void SetMousePosition(int2 pos);

      static void LockMousePos(bool lock);
      static void HideMouse(bool lock = false);
      static void ShowMouse(bool unlock = false);

      static bool IsKeyDown(KeyCode keyCode);
      static bool IsKeyPressing(KeyCode keyCode);
      static bool IsKeyUp(KeyCode keyCode);

      static void OnEvent(Event& event);
      static void ClearKeys();
      static void NextFrame();

      static void SetZeroOutput(bool set);

   private:
      bool zeroOutput = false;

      Array<bool> keyDown;
      Array<bool> keyPressing;
      Array<bool> keyUp;

      int2 mousePos{};
      int2 mouseDelta{};
      bool mouseLocked = false;
   };

}
