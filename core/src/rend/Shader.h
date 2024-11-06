#pragma once
#include <d3d12shader.h>
#include <dxcapi.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "BindPoint.h"
#include "Common.h"
#include "core/Core.h"
#include "core/Ref.h"
#include "math/Types.h"
#include "utils/String.h"

struct IDxcBlob;

namespace pbe {
   class GpuResource;
   class Buffer;

   class Texture2D;
   class CommandList;

   void CORE_API ReloadShaders();

   // struct ShaderDefine {
   //    std::string define;
   //    std::string value;
   // };

   struct ShaderDefines : std::vector<std::wstring> {
      void AddDefine(string_view name, string_view value = {}) {
         std::wstring define;

         auto nameW = ConvertToWString(name.data());

         if (value.empty()) {
            define = std::format(L"{}", nameW);
         } else {
            auto valueW = ConvertToWString(name.data());
            define = std::format(L"{}={}", nameW, valueW);
         }
         
         push_back(define);
      }

      // todo: use vector operator==
      friend bool operator==(const ShaderDefines& lhs, const ShaderDefines& rhs) {
         if (lhs.size() != rhs.size()) {
            return false;
         }

         for (int i = 0; i < (int)lhs.size(); ++i) {
            auto& l = lhs[i];
            auto& r = rhs[i];

            if (l != r) {
               return false;
            }
         }

         return true;
      }

      // todo: remove
      int NDefines() const {
         return (int)size();
      }
   };

   enum class ShaderType {
      Amplification,
      Mesh,
      Vertex,
      Hull,
      Domain,
      Geometry,
      Pixel,
      Compute,
      Unknown
   };

   struct ShaderDesc {
      std::string path;
      std::string entryPoint;
      ShaderType type = ShaderType::Unknown;
      ShaderDefines defines;

      const void* externalBytecode = nullptr;
      u64 externalSize = 0;

      ShaderDesc() = default;
      ShaderDesc(const string_view path, const string_view& entry_point, ShaderType type) : path(path),
         entryPoint(entry_point), type(type) {}

      friend bool operator==(ShaderDesc const& lhs, ShaderDesc const& rhs) = default;

      bool IsExternal() const { return externalBytecode != nullptr; }
   };

   class Shader : public RefCounted {
   public:
      Shader(const ShaderDesc& desc);

      // todo: IsValid
      bool Valid() const { return desc.IsExternal() ? true : !bytecode.empty(); }

      ShaderDesc desc;

      D3D12_SHADER_BYTECODE GetShaderByteCode() const {
         if (desc.IsExternal()) { {}
         return D3D12_SHADER_BYTECODE{ desc.externalBytecode, desc.externalSize };
         }
         return D3D12_SHADER_BYTECODE{ bytecode.data(), bytecode.size() };
      }

      std::vector<u8> bytecode;

      std::unordered_map<size_t, D3D12_SHADER_INPUT_BIND_DESC> reflection;

      bool Compile(bool force = false);
   };

   struct ProgramDesc {
      ShaderDesc as;
      ShaderDesc ms;
      ShaderDesc vs;
      ShaderDesc hs;
      ShaderDesc ds;
      ShaderDesc gs;
      ShaderDesc ps;

      ShaderDesc cs;

      friend bool operator==(ProgramDesc const& lhs, ProgramDesc const& rhs) = default;

      // todo:
      // ShaderDefines defines;

      static ProgramDesc Cs(std::string_view path, std::string_view entry,
         const void* externalBytecode = nullptr, u64 externalSize = 0) {
         ProgramDesc desc;
         desc.cs = { path.data(), entry.data(), ShaderType::Compute,  };
         desc.cs.externalBytecode = externalBytecode;
         desc.cs.externalSize = externalSize;
         return desc;
      }

      static ProgramDesc VsPs(std::string_view path, std::string_view vsEntry, std::string_view psEntry = {}) {
         ProgramDesc desc;
         desc.vs = { path.data(), vsEntry.data(), ShaderType::Vertex };
         if (!psEntry.empty()) {
            desc.ps = { path.data(), psEntry.data(), ShaderType::Pixel };
         }
         return desc;
      }

      static ProgramDesc VsGsPs(std::string_view path, std::string_view vsEntry, std::string_view gsEntry, std::string_view psEntry = {}) {
         ProgramDesc desc;
         desc.vs = { path.data(), vsEntry.data(), ShaderType::Vertex };
         desc.gs = { path.data(), gsEntry.data(), ShaderType::Geometry };
         if (!psEntry.empty()) {
            desc.ps = { path.data(), psEntry.data(), ShaderType::Pixel };
         }
         return desc;
      }

      static ProgramDesc VsHsDsPs(std::string_view path, std::string_view vsEntry, std::string_view hsEntry, std::string_view dsEntry, std::string_view psEntry = {}) {
         ProgramDesc desc;
         desc.vs = { path.data(), vsEntry.data(), ShaderType::Vertex };
         desc.hs = { path.data(), hsEntry.data(), ShaderType::Hull };
         desc.ds = { path.data(), dsEntry.data(), ShaderType::Domain };
         if (!psEntry.empty()) {
            desc.ps = { path.data(), psEntry.data(), ShaderType::Pixel };
         }
         return desc;
      }

      static ProgramDesc MsPs(std::string_view path, std::string_view msEntry, std::string_view psEntry = {}) {
         ProgramDesc desc;
         desc.ms = { path.data(), msEntry.data(), ShaderType::Mesh };
         if (!psEntry.empty()) {
            desc.ps = { path.data(), psEntry.data(), ShaderType::Pixel };
         }
         return desc;
      }

      void AddDefine(string_view name, string_view value = {}) {
         as.defines.AddDefine(name, value);
         ms.defines.AddDefine(name, value);
         vs.defines.AddDefine(name, value);
         hs.defines.AddDefine(name, value);
         ds.defines.AddDefine(name, value);
         gs.defines.AddDefine(name, value);
         ps.defines.AddDefine(name, value);

         cs.defines.AddDefine(name, value);
      }
   };

   class CORE_API GpuProgram : public RefCounted {
   public:
      static Ref<GpuProgram> Create(const ProgramDesc& desc);

      void Activate(CommandList& cmd);

      // BindPoint GetBindPoint(u64 strID) const;
      BindPoint GetBindPoint(std::string_view name) const;

      // todo: remove
      template<typename T>
      void SetCB(CommandList& cmd, std::string_view name, Buffer& buffer, u32 offsetInBytes = 0) {
         SetCB(cmd, name, buffer, offsetInBytes);
      }
      void SetCB(CommandList& cmd, std::string_view name, Buffer& buffer, u32 offsetInBytes);

      void SetSRV(CommandList& cmd, std::string_view name, GpuResource* resource);
      void SetSRV(CommandList& cmd, std::string_view name, GpuResource& resource);

      void SetUAV(CommandList& cmd, std::string_view name, GpuResource* resource = nullptr);
      void SetUAV(CommandList& cmd, std::string_view name, GpuResource& resource); // todo: remove

      void DrawInstanced(CommandList& cmd, u32 vertCount, u32 instCount = 1, u32 startVert = 0);
      void DrawIndexedInstanced(CommandList& cmd, u32 indexCount, u32 instCount = 1, u32 indexStart = 0, u32 startVert = 0);
      void DrawIndexedInstancedIndirect(CommandList& cmd, Buffer& args, u32 offset);

      bool IsCompute() const;
      bool IsMeshShader() const;

      bool Valid() const;

      Ref<Shader> as;
      Ref<Shader> ms;
      Ref<Shader> vs;
      Ref<Shader> hs;
      Ref<Shader> ds;
      Ref<Shader> gs;
      Ref<Shader> ps;

      Ref<Shader> cs;

      ProgramDesc desc;

   private:
      friend Ref<GpuProgram>;

      GpuProgram(const ProgramDesc& desc);
   };

   // class GraphicsProgram : public GpuProgram {
   // public:
   // };
   //
   // class ComputeProgram : public GpuProgram {
   // public:
   // };

   CORE_API GpuProgram* GetGpuProgram(const ProgramDesc& desc);
   void TermGpuPrograms();

   extern CORE_API std::vector<Shader*> sShaders;

   void CORE_API ShadersWindow();
   void CORE_API ShadersSrcWatcherUpdate();

}
