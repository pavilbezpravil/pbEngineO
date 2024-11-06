#include "pch.h"

#include "NRDIntegration.h"
#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "Texture2D.h"
#include "Shader.h"
#include "RendRes.h"
// #include "core/Profiler.h"


constexpr std::array<DXGI_FORMAT, (size_t)nrd::Format::MAX_NUM> g_NRD_NrdToNriFormat2 =
{
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R32_UINT,
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32G32_UINT,
    DXGI_FORMAT_R32G32_SINT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32B32_UINT,
    DXGI_FORMAT_R32G32B32_SINT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_UINT,
    DXGI_FORMAT_R32G32B32A32_SINT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
};

static DXGI_FORMAT NRD_GetNriFormat(nrd::Format format) {
    return g_NRD_NrdToNriFormat2[(uint32_t)format];
}

bool NrdIntegration::Initialize(const nrd::InstanceCreationDesc& instanceCreationDesc) {
    const nrd::LibraryDesc& libraryDesc = nrd::GetLibraryDesc();
    if (libraryDesc.versionMajor != NRD_VERSION_MAJOR || libraryDesc.versionMinor != NRD_VERSION_MINOR) {
        ASSERT_MESSAGE(false, "NRD version mismatch detected!");
        return false;
    }

    if (nrd::CreateInstance(instanceCreationDesc, m_Instance) != nrd::Result::SUCCESS)
        return false;

    CreatePipelines();
    CreateResources();

    return true;
}

void NrdIntegration::CreatePipelines() {
   const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);

    // Assuming that the device is in IDLE state
    m_Pipelines2.clear();

    // Pipelines
   for (uint32_t i = 0; i < instanceDesc.pipelinesNum; i++) {
      const nrd::PipelineDesc& nrdPipelineDesc = instanceDesc.pipelines[i];

      const nrd::ComputeShaderDesc& nrdComputeShader = nrdPipelineDesc.computeShaderDXBC;

      auto compute = GetGpuProgram(ProgramDesc::Cs(nrdPipelineDesc.shaderFileName, nrdPipelineDesc.shaderEntryPointName,
         nrdComputeShader.bytecode, nrdComputeShader.size));

      m_Pipelines2.push_back(compute);
   }
}

void NrdIntegration::CreateResources() {
    const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);
    const uint poolSize = instanceDesc.permanentPoolSize + instanceDesc.transientPoolSize;

    m_TexturePool2.resize(poolSize);

    for (uint i = 0; i < poolSize; i++) {
        const nrd::TextureDesc& nrdTextureDesc = (i < instanceDesc.permanentPoolSize) ? instanceDesc.permanentPool[i] : instanceDesc.transientPool[i - instanceDesc.permanentPoolSize];

        ASSERT(nrdTextureDesc.mipNum == 1);

        m_TexturePool2[i] = Texture2D::Create(Texture2D::Desc::Default(NRD_GetNriFormat(nrdTextureDesc.format),
           { nrdTextureDesc.width, nrdTextureDesc.height }, true).Mips(nrdTextureDesc.mipNum));
    }
}

bool NrdIntegration::SetCommonSettings(const nrd::CommonSettings& commonSettings) {
    nrd::Result result = nrd::SetCommonSettings(*m_Instance, commonSettings);
    ASSERT(result == nrd::Result::SUCCESS);

    return result == nrd::Result::SUCCESS;
}

bool NrdIntegration::SetDenoiserSettings(nrd::Identifier denoiser, const void* denoiserSettings) {
    nrd::Result result = nrd::SetDenoiserSettings(*m_Instance, denoiser, denoiserSettings);
    ASSERT(result == nrd::Result::SUCCESS);

    return result == nrd::Result::SUCCESS;
}

void NrdIntegration::Denoise(const nrd::Identifier* denoisers, uint32_t denoisersNum, CommandList& cmd, const NrdUserPool& userPool) {
    const nrd::DispatchDesc* dispatchDescs = nullptr;
    uint32_t dispatchDescsNum = 0;
    nrd::GetComputeDispatches(*m_Instance, denoisers, denoisersNum, dispatchDescs, dispatchDescsNum);

    for (uint32_t i = 0; i < dispatchDescsNum; i++) {
        const nrd::DispatchDesc& dispatchDesc = dispatchDescs[i];

        COMMAND_LIST_SCOPE(cmd, dispatchDesc.name);
        // PROFILE_GPU(dispatchDesc.name);

        Dispatch(cmd, dispatchDesc, userPool);
    }
}

void NrdIntegration::Dispatch(CommandList& cmd, const nrd::DispatchDesc& dispatchDesc, const NrdUserPool& userPool) {
    const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);
    const nrd::PipelineDesc& pipelineDesc = instanceDesc.pipelines[dispatchDesc.pipelineIndex];

    uint nSrv = 0;
    uint nUav = 0;

    uint32_t n = 0;
    for (uint32_t i = 0; i < pipelineDesc.resourceRangesNum; i++) {
        const nrd::ResourceRangeDesc& resourceRange = pipelineDesc.resourceRanges[i];
        const bool isUav = resourceRange.descriptorType == nrd::DescriptorType::STORAGE_TEXTURE;

        for (uint32_t j = 0; j < resourceRange.descriptorsNum; j++) {
           const nrd::ResourceDesc& nrdResource = dispatchDesc.resources[n];

           ASSERT(nrdResource.mipNum == 1);
           ASSERT(nrdResource.mipOffset == 0);

           Texture2D* nrdTex = nullptr;
           if (nrdResource.type == nrd::ResourceType::TRANSIENT_POOL) {
               nrdTex = m_TexturePool2[nrdResource.indexInPool + instanceDesc.permanentPoolSize];
            } else if (nrdResource.type == nrd::ResourceType::PERMANENT_POOL) {
               nrdTex = m_TexturePool2[nrdResource.indexInPool];
            } else {
               nrdTex = userPool[(uint32_t)nrdResource.type];
               ASSERT_MESSAGE(nrdTex, "'userPool' entry can't be NULL if it's in use!");
            }


           if (!isUav) {
              cmd.SetSRV(BindPoint{ resourceRange.baseRegisterIndex + nSrv++ }, nrdTex);
           } else {
              cmd.SetUAV(BindPoint{ resourceRange.baseRegisterIndex + nUav++ }, nrdTex);
           }

            n++;
        }
    }

    ASSERT(instanceDesc.constantBufferSpaceIndex == 0);
    ASSERT(instanceDesc.samplersSpaceIndex == 0);
    ASSERT(instanceDesc.resourcesSpaceIndex == 0);

    // Updating constants
    if (dispatchDesc.constantBufferDataSize) {
       ASSERT(pipelineDesc.hasConstantData);
       // todo: I think all cb in 0 slot
       cmd.AllocAndSetCB({ 0 }, dispatchDesc.constantBufferData, dispatchDesc.constantBufferDataSize);
    } else {
       ASSERT(!pipelineDesc.hasConstantData);
    }

    for (uint i = 0; i < instanceDesc.samplersNum; i++) {
       nrd::Sampler nrdSampler = instanceDesc.samplers[i];
       switch (nrdSampler) {
       case nrd::Sampler::LINEAR_CLAMP:
          cmd.SetSampler({ instanceDesc.samplersBaseRegisterIndex + i}, rendres::samplerLinearClamp);
          break;
       case nrd::Sampler::LINEAR_MIRRORED_REPEAT:
          cmd.SetSampler({ instanceDesc.samplersBaseRegisterIndex + i }, rendres::samplerLinearMirror);
          break;
       case nrd::Sampler::NEAREST_CLAMP:
          cmd.SetSampler({ instanceDesc.samplersBaseRegisterIndex + i }, rendres::samplerPointClamp);
          break;
       case nrd::Sampler::NEAREST_MIRRORED_REPEAT:
          cmd.SetSampler({ instanceDesc.samplersBaseRegisterIndex + i }, rendres::samplerPointMirror);
          break;
       default:
            UNIMPLEMENTED();
       }
    }

    auto computeShader = m_Pipelines2[dispatchDesc.pipelineIndex];
    cmd.SetComputeProgram(computeShader);

    cmd.Dispatch2D({ dispatchDesc.gridWidth, dispatchDesc.gridHeight });
}

void NrdIntegration::Destroy() {
   m_TexturePool2.clear();
   m_Pipelines2.clear();
   m_Samplers2.clear();

    nrd::DestroyInstance(*m_Instance);
    m_Instance = nullptr;
}
