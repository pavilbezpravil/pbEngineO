#include "pch.h"
#include "ShaderToyWindow.h"

#include "app/Input.h"
#include "core/Profiler.h"
#include "rend/RendRes.h"
#include "math/Types.h"
#include "typer/Serialize.h"
#include "typer/Registration.h"


namespace pbe {

   constexpr char savePath[] = "sahdertoy.yaml";

   STRUCT_BEGIN(ShaderToySettings)
      STRUCT_FIELD(path)
   STRUCT_END()

   ShaderToyWindow::ShaderToyWindow(): EditorWindow("ShaderToy") {
      Deserialize(savePath, settings);
   }

   ShaderToyWindow::~ShaderToyWindow() {
      Serialize(savePath, settings);
   }

   void ShaderToyWindow::OnWindowUI() {
      UNIMPLEMENTED();
#if 0
      CommandList cmd{ sDevice->g_pd3dDeviceContext };

      auto imSize = ImGui::GetContentRegionAvail();
      int2 size = { imSize.x, imSize.y };

      if (all(size > 1)) {
         if (!texture || texture->GetDesc().size != size) {
            Texture2D::Desc texDesc {
               .size = size,
               .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
               .bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
               .name = "shader toy texture",
            };
            texture = Texture2D::Create(texDesc);
         }

         vec2 mousePos = { ImGui::GetMousePos().x, ImGui::GetMousePos().y };
         vec2 cursorPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

         int2 cursorPixelIdx{ mousePos - cursorPos};

         cmd.SetCommonSamplers();

         COMMAND_LIST_SCOPE("ShaderToy");
         PROFILE_GPU("ShaderToy");

         cmd.SetRenderTarget();

         auto tonemapPass = GetGpuProgram(ProgramDesc::Cs("shaderToy.hlsl", "main"));
         tonemapPass->Activate(cmd);

         tonemapPass->SetUAV(cmd, "gOut", texture);
         cmd.Dispatch2D(texture->GetDesc().size, int2{ 8 });

         cmd.pContext->ClearState();

         auto srv = texture->srv.Get();
         ImGui::Image(srv, imSize);
      }
#endif
   }

   void ShaderToyWindow::OnUpdate(float dt) {
      time += dt;
   }

}
