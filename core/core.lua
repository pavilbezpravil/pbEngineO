project "core"
   sharedCppLib()

   libsinfo.shaders = {}
   libsinfo.shaders.includepath = os.getcwd().."/shaders"

   libsinfo.core = {}
   libsinfo.core.includepath = os.getcwd().."/src"
   libsinfo.core.includedirs = {
      libsinfo.core.includepath,
      libsinfo.imgui.includepath,
      libsinfo.glm.includepath,
      libsinfo.spdlog.includepath,
      libsinfo.yaml.includepath,
      libsinfo.optick.includepath,
      libsinfo.entt.includepath,
      libsinfo.shaders.includepath, -- todo: remove
      libsinfo.physx.includepath,

      -- todo: not in release
      libsinfo.winPixEventRuntime.includepath,
   }
   libsinfo.core.natvis = os.getcwd().."/natvis/*.natvis"

   pchheader "pch.h"
   pchsource "src/pch.cpp"

   includedirs {
      libsinfo.core.includedirs,
      "%{libsinfo.blast.includepath}/shared/NvFoundation",
      "%{libsinfo.blast.includepath}/globals",
      "%{libsinfo.blast.includepath}/lowlevel",
      "%{libsinfo.blast.includepath}/toolkit",
      "%{libsinfo.blast.includepath}/extensions/shaders",
      "%{libsinfo.fsr3.includepath}", -- todo: optional
      -- todo: compile option. It must be easyly disabled
      libsinfo.nrd.includepath,
      libsinfo.dxcompiler.includepath,
   }

   libdirs {
      libsinfo.physx.libDir,
      libsinfo.blast.libDir,
      libsinfo.nrd.libDir,
      libsinfo.winPixEventRuntime.libDir,
      libsinfo.dxcompiler.libDir,
      libsinfo.fsr3.libDir,
   }

   links {
       "imgui", "yaml", "optick", 
       "d3d12", "dxcompiler", "dxguid", "dxgi",
       "PhysX_64",
       "PhysXCommon_64",
       "PhysXExtensions_static_64",
       "PhysXFoundation_64",
       "PhysXPvdSDK_static_64",
       "NvBlastTk",
       "NvBlastExtShaders",
       "NRD",
       "WinPixEventRuntime",
   }

   filter "configurations:Debug"
      links {
         "ffx_fsr3upscaler_x64d",
         "ffx_backend_dx12_x64d",
      }

   filter "configurations:Release"
      links {
         "ffx_fsr3upscaler_x64",
         "ffx_backend_dx12_x64",
      }

   filter {}
   
   postbuildcommands {
      '{COPY} "%{libsinfo.physx.libDir}/*.dll" "%{cfg.targetdir}"',
      '{COPY} "%{libsinfo.blast.libDir}/*.dll" "%{cfg.targetdir}"',
      '{COPY} "%{libsinfo.nrd.libDir}/*.dll" "%{cfg.targetdir}"',
      '{COPY} "%{libsinfo.winPixEventRuntime.libDir}/*.dll" "%{cfg.targetdir}"',
      '{COPY} "%{libsinfo.dxcompiler.binDir}/*.dll" "%{cfg.targetdir}"',
      '{COPY} "%{libsinfo.dx12.binDir}/*.dll" "%{cfg.targetdir}/D3D12"',
      '{COPY} "%{libsinfo.dx12.binDir}/*.pdb" "%{cfg.targetdir}/D3D12"',
   }

   defines { "CORE_API_EXPORT" }
   files { "src/**.h", "src/**.cpp", "shaders/**", libsinfo.core.natvis, libsinfo.glm.natvis, libsinfo.entt.natvis }

   filter "files:shaders/**"
      buildaction "None"
