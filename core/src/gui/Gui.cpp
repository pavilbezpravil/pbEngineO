#include "pch.h"
#include "Gui.h"
#include "typer/Typer.h"

namespace pbe {

   vec2 ConvertToPBE(const ImVec2& v) {
      return { v.x, v.y };
   }

   ImGuiContext* GetImGuiContext() {
      return ImGui::GetCurrentContext();
   }

   bool EditorUI(std::string_view lable, TypeID typeID, u8* value) {
      return Typer::Get().UI(lable, typeID, value);
   }

   bool EditorUI(TypeID typeID, u8* value) {
      return EditorUI("", typeID, value);
   }

   ImGuiTreeNodeFlags DefaultTreeNodeFlags() {
      return ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap;
   }

   bool UIColorEdit3(const char* name, u8* value) {
      return ImGui::ColorEdit3(name, (float*)value);
   }

   bool UIColorPicker3(const char* name, u8* value) {
      return ImGui::ColorPicker3(name, (float*)value);
   }

}
