#pragma once

#include "imgui.h"
#include "app/Layer.h"
#include "core/Ref.h"

namespace pbe {
   class Texture2D;
   class CommandList;

   class ImGuiLayer : public Layer {
   public:
      void OnAttach() override;
      void OnDetach() override;
      void OnImGuiRender() override;
      void OnEvent(Event& event) override;

      void NewFrame();
      void EndFrame();
      void Render(CommandList& cmd);
   private:
      Ref<Texture2D> fontTex;
   };

}
