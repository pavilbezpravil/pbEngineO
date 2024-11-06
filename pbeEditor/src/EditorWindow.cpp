#include "pch.h"
#include "EditorWindow.h"

#include "gui/Gui.h"

namespace pbe {

   void EditorWindow::OnPoolWindowState() {
      windowPosition = ConvertToPBE(ImGui::GetWindowPos());
      windowSize = ConvertToPBE(ImGui::GetWindowSize());

      // todo:
      if (ImGui::IsWindowHovered() && (!ImGui::IsAnyItemActive() || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
         ImGui::SetWindowFocus();
      }

      // if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      //    ImGui::SetWindowFocus();
      // }

      bool nextFocused = ImGui::IsWindowFocused();
      if (focused != nextFocused) {
         focused = nextFocused;

         if (focused) {
            OnFocus();
         } else {
            OnLostFocus();
         }
      }
   }

}
