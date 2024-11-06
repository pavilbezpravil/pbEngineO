#include "pch.h"
#include "Shader.h"
#include "Device.h"
#include "core/Log.h"
#include "Texture2D.h"
#include "Buffer.h"
#include "CommandList.h"
#include "core/Assert.h"
#include "fs/FileSystem.h"

#include <dxcapi.h>
#include <d3d12shader.h>

#include "core/CVar.h"
#include "core/Profiler.h"
#include "gui/Gui.h"
#include "fs/FileWatch.h"
#include "utils/Hash.h"
#include "utils/String.h"

using namespace pbe;

// todo: any integer
template<>
struct std::hash<std::vector<int>> {
   std::size_t operator()(const std::vector<int>& v) const {
      std::size_t res = v.size();
      for (auto& i : v) {
         res ^= i + 0x9e3779b9 + (res << 6) + (res >> 2);
      }
      return res;
   }
};

template<typename T>
struct std::hash<std::vector<T>> {
   std::size_t operator()(const std::vector<T>& v) const {
      std::size_t res = v.size();
      for (auto& i : v) {
         HashCombine(res, i);
      }
      return res;
   }
};

template<>
struct std::hash<pbe::ShaderDesc> {
   std::size_t operator()(const pbe::ShaderDesc& v) const {
      std::size_t res = 0;
      HashCombine(res, v.entryPoint);
      HashCombine(res, v.path);
      HashCombine(res, v.type);
      for (auto& define : v.defines) {
         HashCombine(res, define);
      }

      HashCombine(res, v.externalBytecode);
      HashCombine(res, v.externalSize);

      return res;
   }
};

template<>
struct std::hash<pbe::ProgramDesc> {
   std::size_t operator()(const pbe::ProgramDesc& v) const {
      std::size_t res = 0;
      HashCombine(res, v.vs);
      HashCombine(res, v.hs);
      HashCombine(res, v.ds);
      HashCombine(res, v.gs);
      HashCombine(res, v.ps);

      HashCombine(res, v.cs);
      return res;
   }
};

namespace pbe {

   CVarValue<bool> cvGenerateShaderPDB{ "shaders/generate PDB", false};
   CVarValue<bool> cShaderReloadOnAnyChange{ "shaders/reload on any change", true};
   CVarValue<bool> cShaderUseCache{ "shaders/use cache", false}; // todo:

   // todo:
   static std::wstring ToWstr(std::string_view str) {
      return std::wstring(str.begin(), str.end());
   }

   static string gEngineSourcePath = "../../";
   static string gShadersSourcePath = "../../core/shaders";
   static string gShadersCacheFolder = "shader_cache";
   static std::wstring gShadersPdbFolder = L".\\shader_pdbs\\";

   string GetShadersPath(string_view path) {
      return gShadersSourcePath + '/' + path.data();
   }

   static ComPtr<IDxcResult> CompileDxcShader(ShaderDesc& desc) {
      static const wchar_t* gShaderProfile[] = {
         L"as_6_6",
         L"ms_6_6",
         L"vs_6_6",
         L"hs_6_6",
         L"ds_6_6",
         L"gs_6_6",
         L"ps_6_6",
         L"cs_6_6",
      };

      auto profile = gShaderProfile[(int)desc.type];

      auto path = GetShadersPath(desc.path);
      auto pathW = ConvertToWString(path);

      if (!fs::exists(path)) {
         WARN("Cant find file '{}' for compilation", desc.path);
      }

      auto shaderPath = ToWstr(path);

      // todo: dont need to create for each shader.
      ComPtr<IDxcUtils> pUtils;
      ThrowIfFailed(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(pUtils.GetAddressOf())));

      ComPtr<IDxcCompiler3> compiler;
      ThrowIfFailed(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf())));

      ComPtr<IDxcIncludeHandler> includeHandler;
      ThrowIfFailed(pUtils->CreateDefaultIncludeHandler(includeHandler.GetAddressOf()));

      auto entryPoint = ConvertToWString(desc.entryPoint);
      auto includePathW = ConvertToWString(gShadersSourcePath);

      std::vector<LPCWSTR> arguments
      {
          shaderPath.c_str(),
          L"-E", entryPoint.c_str(),
          L"-T", profile,
          L"/Fd", gShadersPdbFolder.c_str(),
          L"-I",
          // L"C:\\dev\\pbEngine\\core\\shaders",
          includePathW.c_str(),
          // DXC_ARG_PACK_MATRIX_ROW_MAJOR,
          DXC_ARG_WARNINGS_ARE_ERRORS,
          DXC_ARG_ALL_RESOURCES_BOUND,
      };

      // std::vector<LPWSTR> arguments;

      // todo:
      // Strip reflection data and pdbs (see later)
      // Remove from DXC_OUT_OBJECT data DXC_OUT_PDB and DXC_OUT_REFLECTION
      // arguments.push_back(L"-Qstrip_debug");
      // arguments.push_back(L"-Qstrip_reflect");

      arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS); //-WX
#if defined(DEBUG)
      arguments.push_back(DXC_ARG_DEBUG); //-Zi
      arguments.push_back(L"-Qembed_debug"); // embed pbd
#endif

      // todo:
      for (const std::wstring& define : desc.defines) {
         arguments.push_back(L"-D");
         arguments.push_back(define.c_str());
      }

      ComPtr<IDxcBlobEncoding> sourceBlob{};
      pUtils->LoadFile(shaderPath.data(), nullptr, &sourceBlob);

      DxcBuffer sourceBuffer
      {
          .Ptr = sourceBlob->GetBufferPointer(),
          .Size = sourceBlob->GetBufferSize(),
          .Encoding = 0u,
      };

      CpuTimer timer;

      // Compile the shader.
      ComPtr<IDxcResult> compiledShaderBuffer{};
      const HRESULT hr = compiler->Compile(
         &sourceBuffer,
         arguments.data(),
         (u32)arguments.size(),
         includeHandler.Get(),
         IID_PPV_ARGS(&compiledShaderBuffer));
      if (FAILED(hr)) {
         ASSERT(false);
      }

      ComPtr<IDxcBlobUtf8> errors;
      compiledShaderBuffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
      if (errors && errors->GetStringLength() > 0) {
         const LPCSTR errorMessage = errors->GetStringPointer();
         OutputDebugStringA(errorMessage);
         WARN("[Shader] '{}' with entry point '{}' error\n{}", desc.path, desc.entryPoint, errorMessage);
      }

      HRESULT hrStatus;
      compiledShaderBuffer->GetStatus(&hrStatus);
      if (FAILED(hrStatus)) {
         WARN("Compilation Failed");
         return {};
      }

      INFO("Compiled shader '{}' entryPoint: '{}'. Compile time {} ms.", desc.path, desc.entryPoint, timer.ElapsedMs());

      return compiledShaderBuffer;
   }

   std::unordered_map<size_t, Shader*> sShadersMap;

   void ReloadShaders() {
      INFO("Reload shaders!");
      for (auto [_, shader] : sShadersMap) {
         shader->Compile(true);
      }
   }

   Shader::Shader(const ShaderDesc& desc) : desc(desc) {}

   static Ref<Shader> ShaderCompile(const ShaderDesc& desc) {
      if (desc.path.empty()) {
         return {};
      }

      std::hash<ShaderDesc> h;
      auto shaderDescHash = h(desc);

      auto it = sShadersMap.find(shaderDescHash);
      if (it != sShadersMap.end()) {
         return it->second;
      }

      Ref shader{ new Shader(desc) };
      shader->Compile();

      sShadersMap[shaderDescHash] = shader;

      return shader;
   }

   bool Shader::Compile(bool force) {
      if (desc.IsExternal()) {
         // cant recompile external shader
         return true;
      }

      reflection.clear();

      bytecode.clear();
      // todo: change ptr to byte code for trigger pso recreation
      bytecode.shrink_to_fit();

      auto shaderDescHash = std::hash<ShaderDesc>()(desc);

      auto shaderCachePath = std::format("{}/{}", gShadersCacheFolder, shaderDescHash);
      auto wShaderCachePath = ToWstr(shaderCachePath);

      ID3D10Blob* shaderBuffer{};

      if (cShaderUseCache && !force) {
         // D3DReadFileToBlob(wShaderCachePath.c_str(), &shaderBuffer);
      }

      ComPtr<IDxcResult> compiledShaderBuffer;

      if (!shaderBuffer) {
         compiledShaderBuffer = CompileDxcShader(desc);
         if (!compiledShaderBuffer) {
            return false;
         }

         if (cShaderUseCache) {
            UNIMPLEMENTED();
            fs::create_directory(gShadersCacheFolder);
            // if (D3DWriteBlobToFile(shaderBuffer, wShaderCachePath.c_str(), true) != S_OK) {
            //    WARN("Cant save shader in cache");
            // }
         }
      }

      // todo
      {
         ComPtr<IDxcBlob> compiledShaderBlob{ nullptr };

         //
         // Save shader binary.
         //
         ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
         ThrowIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), &pShaderName));
         // if (compiledShaderBlob != nullptr) {
         //    FILE* fp = NULL;
         //    _wfopen_s(&fp, pShaderName->GetStringPointer(), L"wb");
         //    fwrite(compiledShaderBlob->GetBufferPointer(), compiledShaderBlob->GetBufferSize(), 1, fp);
         //    fclose(fp);
         // }
         if (!compiledShaderBlob) {
            return false;
         }

         bytecode.resize(compiledShaderBlob->GetBufferSize());
         memcpy(bytecode.data(), compiledShaderBlob->GetBufferPointer(), compiledShaderBlob->GetBufferSize());

         if (cvGenerateShaderPDB) {
            ComPtr<IDxcBlob> pPDB = nullptr;
            ComPtr<IDxcBlobUtf16> pPDBName = nullptr;
            compiledShaderBuffer->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName);
            if (pPDB && pPDB->GetBufferSize() && pPDB->GetBufferPointer()) {
               FILE* fp = NULL;

               std::wstring saveFilePath = gShadersPdbFolder + pPDBName->GetStringPointer();

               fs::create_directory(gShadersPdbFolder);

               // Note that if you don't specify -Fd, a pdb name will be automatically generated.
               // Use this file name to save the pdb so that PIX can find it quickly.
               // _wfopen_s(&fp, pPDBName->GetStringPointer(), L"wb");
               _wfopen_s(&fp, saveFilePath.c_str(), L"wb");
               fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
               fclose(fp);
            }
         }

         // todo:
         ComPtr<IDxcBlob> pHash;
         if (SUCCEEDED(compiledShaderBuffer->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(pHash.GetAddressOf()), nullptr))) {
            DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
         }

         {
            ComPtr<IDxcBlob> reflectionBlob{};
            ThrowIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr));

            const DxcBuffer reflectionBuffer
            {
                .Ptr = reflectionBlob->GetBufferPointer(),
                .Size = reflectionBlob->GetBufferSize(),
                .Encoding = 0,
            };

            // todo: second init for this shader compile
            ComPtr<IDxcUtils> pUtils;
            ThrowIfFailed(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(pUtils.GetAddressOf())));

            ComPtr<ID3D12ShaderReflection> shaderReflection{};
            pUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));
            D3D12_SHADER_DESC shaderDesc{};
            shaderReflection->GetDesc(&shaderDesc);

            // D3D12_SHADER_INPUT_BIND_DESC desc;
            // shaderReflection->GetResourceBindingDesc(0, &desc);

            for (const uint32_t i : std::views::iota(0u, shaderDesc.BoundResources)) {
               D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc{};
               ThrowIfFailed(shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc));

               // INFO("\tName: {} Type: {} BindPoint: {}", bindDesc.Name, bindDesc.Type, bindDesc.BindPoint);

               size_t id = StrHash(shaderInputBindDesc.Name);
               reflection[id] = shaderInputBindDesc;
#if 0
               if (shaderInputBindDesc.Type == D3D_SIT_CBUFFER) {
                  rootParameterIndexMap[stringToWString(shaderInputBindDesc.Name)] = static_cast<uint32_t>(rootParameters.size());
                  ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
                  D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
                  shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

                  const D3D12_ROOT_PARAMETER1 rootParameter
                  {
                      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                      .Descriptor{
                          .ShaderRegister = shaderInputBindDesc.BindPoint,
                          .RegisterSpace = shaderInputBindDesc.Space,
                          .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                      },
                  };

                  rootParameters.push_back(rootParameter);
               }

               if (shaderInputBindDesc.Type == D3D_SIT_TEXTURE) {
                  // For now, each individual texture belongs in its own descriptor table. This can cause the root signature to quickly exceed the 64WORD size limit.
                  rootParameterIndexMap[stringToWString(shaderInputBindDesc.Name)] = static_cast<uint32_t>(rootParameters.size());
                  const CD3DX12_DESCRIPTOR_RANGE1 srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                     1u,
                     shaderInputBindDesc.BindPoint,
                     shaderInputBindDesc.Space,
                     D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

                  descriptorRanges.push_back(srvRange);

                  const D3D12_ROOT_PARAMETER1 rootParameter
                  {
                      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                      .DescriptorTable =
                      {
                          .NumDescriptorRanges = 1u,
                          .pDescriptorRanges = &descriptorRanges.back(),
                      },
                      .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
                  };

                  rootParameters.push_back(rootParameter);
               }
#endif
            }
         }
      }

      return true;
   }

   Ref<GpuProgram> GpuProgram::Create(const ProgramDesc& desc) {
      return Ref<GpuProgram>::Create(desc);
   }

   void GpuProgram::Activate(CommandList& cmd) {
      bool graphics = vs || ps;
      if (graphics) {
         cmd.SetGraphicsProgram(this);
      } else {
         cmd.SetComputeProgram(this);
      }
   }

   BindPoint GpuProgram::GetBindPoint(std::string_view name) const {
      size_t id = StrHash(name);

      auto getSlot = [&](const Ref<Shader>& shader) -> BindPoint {
         if (shader) {
            const auto reflection = shader->reflection;

            auto iter = reflection.find(id);
            if (iter != reflection.end()) {
               return BindPoint{ iter->second.BindPoint, iter->second.Space };
            }
         }

         return BindPoint{};
      };

      BindPoint bindPoints[8] = { getSlot(as), getSlot(ms), getSlot(vs), getSlot(hs), getSlot(ds), getSlot(gs), getSlot(ps), getSlot(cs) };
      BindPoint bindPoint = {UINT32_MAX, 0};
      for (int i = 0; i < _countof(bindPoints); i++) {
         BindPoint checkBindPoint = bindPoints[i];
         if (checkBindPoint.slot != -1) {
            ASSERT(bindPoint.slot == UINT32_MAX || bindPoint == checkBindPoint);
            bindPoint = checkBindPoint;
         }
      }

      return bindPoint;
   }

   void GpuProgram::SetCB(CommandList& cmd, std::string_view name, Buffer& buffer, u32 offsetInBytes) {
      cmd.SetCB(GetBindPoint(name), &buffer, offsetInBytes);
   }

   void GpuProgram::SetSRV(CommandList& cmd, std::string_view name, GpuResource* resource) {
      cmd.SetSRV(GetBindPoint(name), resource);
   }

   void GpuProgram::SetSRV(CommandList& cmd, std::string_view name, GpuResource& resource) {
      SetSRV(cmd, name, &resource);
   }

   void GpuProgram::SetUAV(CommandList& cmd, std::string_view name, GpuResource* resource) {
      cmd.SetUAV(GetBindPoint(name), resource);
   }

   void GpuProgram::SetUAV(CommandList& cmd, std::string_view name, GpuResource& resource) {
      SetUAV(cmd, name, &resource);
   }

   void GpuProgram::DrawInstanced(CommandList& cmd, u32 vertCount, u32 instCount, u32 startVert) {
      cmd.DrawInstanced(vertCount, instCount, startVert, 0);
   }

   void GpuProgram::DrawIndexedInstanced(CommandList& cmd, u32 indexCount, u32 instCount, u32 indexStart, u32 startVert) {
      cmd.DrawIndexedInstanced(indexCount, instCount, indexStart, startVert, 0);
   }

   void GpuProgram::DrawIndexedInstancedIndirect(CommandList& cmd, Buffer& args, u32 offset) {
      cmd.DrawIndexedInstancedIndirect(args, offset);
   }

   bool GpuProgram::IsCompute() const {
      return cs;
   }

   bool GpuProgram::IsMeshShader() const {
      return ms;
   }

   bool GpuProgram::Valid() const {
      return (!vs || vs->Valid()) && (!hs || hs->Valid()) && (!ds || ds->Valid()) && (!gs || gs->Valid()) && (!ps || ps->Valid()) && (!cs || cs->Valid());
   }

   GpuProgram::GpuProgram(const ProgramDesc& desc) : desc(desc) {
      as = ShaderCompile(desc.as);
      ms = ShaderCompile(desc.ms);
      vs = ShaderCompile(desc.vs);
      hs = ShaderCompile(desc.hs);
      ds = ShaderCompile(desc.ds);
      gs = ShaderCompile(desc.gs);
      ps = ShaderCompile(desc.ps);
      cs = ShaderCompile(desc.cs);
   }

   static std::unordered_map<ProgramDesc, Ref<GpuProgram>> sGpuPrograms;

   GpuProgram* GetGpuProgram(const ProgramDesc& desc) {
      auto it = sGpuPrograms.find(desc);
      if (it == sGpuPrograms.end()) {
         auto program = GpuProgram::Create(desc);
         sGpuPrograms[desc] = program;
         return program.Raw();
      }

      return it->second.Raw();
   }

   void TermGpuPrograms() {
      sGpuPrograms.clear();
   }

   struct ShaderSrcDesc {
      string path;
      string source;
      i64 hash;

      std::unordered_set<string> includes;
   };

   struct ShaderMng {
      std::unordered_map<string, ShaderSrcDesc> shaderSrcMap;

      void AddSrc(string_view path)
      {
         auto iter = shaderSrcMap.find(path.data());
         if (iter == shaderSrcMap.end()) {
            ShaderSrcDesc desc;
            desc.path = path;

            INFO("ShaderSrcPath = {}", desc.path);

            auto absPath = GetShadersPath(desc.path);

            desc.source = ReadFileAsString(absPath);
            const auto& src = desc.source;

            size_t pos = 0;
            while (pos != std::string::npos) {
               pos = src.find("#include """, pos);

               if (pos != std::string::npos) {
                  pos = src.find('"', pos + 1);
                  if (pos != std::string::npos) {
                     auto startIdx = pos + 1;
                     pos = src.find('"', pos + 1);

                     if (pos != std::string::npos) {
                        string_view includePath{ src.data() + startIdx, pos - startIdx };
                        INFO("   IncludePath = {}", includePath);

                        desc.includes.insert(includePath.data()); // todo:

                        continue;
                     }
                  }

                  WARN("Shader src code error");
               }
            }

            // desc.hash = ;

            shaderSrcMap[path.data()] = desc;
         }
      }
      
   };

   static ShaderMng sShaderMng;

   void OpenVSCodeEngineSource() {
      std::string cmd = std::format("code {}", gEngineSourcePath);
      system(cmd.c_str());
   }

   void ShadersWindow() {
      if (ImGui::Button("Reload")) {
         ReloadShaders();
      }

      if (ImGui::Button("Clear cache")) {
         fs::remove_all(gShadersCacheFolder);
      }

      if (ImGui::Button("Open cache folder")) {
         OpenFileExplorer(gShadersCacheFolder);
      }

      // todo:
      if (ImGui::Button("Open vs code engine folder")) {
         OpenVSCodeEngineSource();
      }

      // todo:
      if (ImGui::Button("Open explorer engine folder")) {
         OpenFileExplorer(".\\..\\..\\");
      }

      // CALL_ONCE([] {
      //       i64 mask = 1 << 7 | 1 << 13 | 1 << 25;
      //
      //       INFO("Mask {}", mask);
      //
      //       unsigned long index;
      //       while (BitScanForward64(&index, mask)) {
      //          mask &= ~(1 << index);
      //          INFO("{}", index);
      //       }
      // });

      ImGui::Text("Shaders:");

      for (auto [_, shader] : sShadersMap) {
         const auto& desc = shader->desc;
         std::string shaderName = desc.path + " " + desc.entryPoint;

         UI_PUSH_ID(&desc);

         if (UI_TREE_NODE(shaderName.c_str())) {
            ImGui::Text("%s type %d", desc.entryPoint.c_str(), desc.type);

            if (ImGui::Button("Edit")) {
               OpenVSCodeEngineSource();

               // open shader file
               auto cmd = std::format("code {}/{}", gShadersSourcePath, desc.path);
               system(cmd.c_str());
            }

            if (UI_TREE_NODE("Defines")) {
               for (auto& define : desc.defines) {
                  string defineStr = ConvertToString(define);
                  ImGui::Text("%s", defineStr.c_str());
               }
            }

            if (UI_TREE_NODE("Reflection")) {
               for (auto [id, bindDesc] : shader->reflection) {
                  ImGui::Text("%s %d, type %d", bindDesc.Name, bindDesc.BindPoint, bindDesc.Type);
               }
            }
         }
      }
   }

   void ShadersSrcWatcherUpdate()
   {
      static bool anySrcChanged = false; // todo: atomic

      CALL_ONCE([] {
         // todo:
         static filewatch::FileWatch<std::string> watch(
            gShadersSourcePath,
            [](const std::string& path, const filewatch::Event change_type) {
               std::cout << path << " : ";
               switch (change_type) {
               case filewatch::Event::added:
                  // std::cout << "The file was added to the directory." << '\n';
                  break;
               case filewatch::Event::removed:
                  // std::cout << "The file was removed from the directory." << '\n';
                  break;
               case filewatch::Event::modified:
                  anySrcChanged = true;
                  std::cout << "The file was modified. This can be a change in the time stamp or attributes." << '\n';
                  break;
               case filewatch::Event::renamed_old:
                  // std::cout << "The file was renamed and this is the old name." << '\n';
                  break;
               case filewatch::Event::renamed_new:
                  // std::cout << "The file was renamed and this is the new name." << '\n';
                  break;
               }
            }
         );
      });

      if (anySrcChanged && cShaderReloadOnAnyChange) {
         ReloadShaders();
         anySrcChanged = false;
      }
   }
}
