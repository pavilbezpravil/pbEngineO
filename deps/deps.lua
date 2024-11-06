libsinfo.glm = {}
libsinfo.glm.includepath = os.getcwd().."/glm"
libsinfo.glm.natvis = os.getcwd().."/glm/util/*.natvis"

libsinfo.spdlog = {}
libsinfo.spdlog.includepath = os.getcwd().."/spdlog/include"

libsinfo.entt = {}
libsinfo.entt.includepath = os.getcwd().."/entt/src"
libsinfo.entt.natvis = os.getcwd().."/entt/natvis/*.natvis"

libsinfo.physx = {}
libsinfo.physx.includepath = os.getcwd().."/physx/include"
libsinfo.physx.libDir = os.getcwd().."/physx/bin/%{cfg.buildcfg}" -- todo:

libsinfo.blast = {}
libsinfo.blast.includepath = os.getcwd().."/blast/include"
libsinfo.blast.libDir = os.getcwd().."/blast/bin/%{cfg.buildcfg}" -- todo:

libsinfo.nrd = {}
libsinfo.nrd.includepath = os.getcwd().."/NRD/include"
libsinfo.nrd.libDir = os.getcwd().."/NRD/bin/%{cfg.buildcfg}"

libsinfo.winPixEventRuntime = {}
libsinfo.winPixEventRuntime.includepath = os.getcwd().."/WinPixEventRuntime/include"
libsinfo.winPixEventRuntime.libDir = os.getcwd().."/WinPixEventRuntime/bin"

libsinfo.dxcompiler = {}
libsinfo.dxcompiler.includepath = os.getcwd().."/dxcompiler/inc"
libsinfo.dxcompiler.libDir = os.getcwd().."/dxcompiler/lib/x64"
libsinfo.dxcompiler.binDir = os.getcwd().."/dxcompiler/bin/x64"

libsinfo.dx12 = {}
libsinfo.dx12.binDir = os.getcwd().."/D3D12"

libsinfo.fsr3 = {}
libsinfo.fsr3.includepath = os.getcwd().."/fsr3/include"
libsinfo.fsr3.libDir = os.getcwd().."/fsr3/bin"
