#include "pch.h"
#include "PipelineStateObject.h"

#include "Device.h"
#include "core/Assert.h"
#include "utils/Hash.h"

using namespace pbe;

PipelineStateObject::PipelineStateObject(const D3D12_PIPELINE_STATE_STREAM_DESC& desc) {
   auto d3d12Device = GetD3D12Device();
   ThrowIfFailed(d3d12Device->CreatePipelineState(&desc, IID_PPV_ARGS(&m_d3d12PipelineState)));
}

PipelineStateObject::PipelineStateObject(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc) {
   auto d3d12Device = GetD3D12Device();
   ThrowIfFailed(d3d12Device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_d3d12PipelineState)));
}

PipelineStateObject::PipelineStateObject(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
   auto d3d12Device = GetD3D12Device();
   ThrowIfFailed(d3d12Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_d3d12PipelineState)));
}

PipelineStateObject::PipelineStateObject(const CD3DX12_PIPELINE_STATE_STREAM2& stream) {
   auto d3d12Device = GetD3D12Device();
   auto pStream = (void*)const_cast<CD3DX12_PIPELINE_STATE_STREAM2*>(&stream);

   D3D12_PIPELINE_STATE_STREAM_DESC desc = {
         .SizeInBytes = sizeof(stream),
         .pPipelineStateSubobjectStream = pStream,
   };
   ThrowIfFailed(d3d12Device->CreatePipelineState(&desc, IID_PPV_ARGS(&m_d3d12PipelineState)));
}

u64 PipelineStateObject::PSODescHash(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc) {
   u64 hash = 0;
   HashCombine(hash, desc.pRootSignature);
   HashCombine(hash, desc.CS.pShaderBytecode);
   HashCombine(hash, desc.NodeMask);
   HashCombine(hash, desc.Flags);

   return hash;
}

u64 PipelineStateObject::PSODescHash(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
   u64 hash = 0;
   HashCombine(hash, desc.pRootSignature);
   HashCombine(hash, desc.VS.pShaderBytecode);
   HashCombine(hash, desc.HS.pShaderBytecode);
   HashCombine(hash, desc.DS.pShaderBytecode);
   HashCombine(hash, desc.GS.pShaderBytecode);
   HashCombine(hash, desc.PS.pShaderBytecode);

   HashCombine(hash, desc.BlendState.AlphaToCoverageEnable);
   HashCombine(hash, desc.BlendState.IndependentBlendEnable);
   HashCombineMemory(hash, desc.BlendState.RenderTarget[0]);

   HashCombineMemory(hash, desc.RasterizerState);
   HashCombineMemory(hash, desc.DepthStencilState);
   HashCombineMemory(hash, desc.InputLayout.pInputElementDescs, sizeof(D3D12_INPUT_ELEMENT_DESC) * desc.InputLayout.NumElements);
   HashCombine(hash, desc.PrimitiveTopologyType);
   HashCombineMemory(hash, desc.RTVFormats, sizeof(DXGI_FORMAT) * desc.NumRenderTargets);
   HashCombine(hash, desc.DSVFormat);
   HashCombine(hash, desc.Flags);

   return hash;
}

u64 PipelineStateObject::PSODescHash(const CD3DX12_PIPELINE_STATE_STREAM2& desc) {
   bool isCompute = desc.CS.Get().BytecodeLength > 0;
   bool isGraphics = desc.VS.Get().BytecodeLength > 0;
   bool isGraphicsMesh = desc.MS.Get().BytecodeLength > 0;
   ASSERT(int(isCompute) + int(isGraphics) + int(isGraphicsMesh) == 1);

   u64 hash = 0;

   if (isCompute) {
      HashCombine(hash, desc.pRootSignature.Get());

      HashCombine(hash, desc.CS.Get().pShaderBytecode);

      HashCombine(hash, desc.NodeMask.Get());
      HashCombine(hash, desc.Flags.Get());
   } else {
      HashCombine(hash, desc.pRootSignature.Get());

      if (isGraphics) {
         HashCombine(hash, desc.VS.Get().pShaderBytecode);
         HashCombine(hash, desc.HS.Get().pShaderBytecode);
         HashCombine(hash, desc.DS.Get().pShaderBytecode);
         HashCombine(hash, desc.GS.Get().pShaderBytecode);
      } else {
         HashCombine(hash, desc.AS.Get().pShaderBytecode);
         HashCombine(hash, desc.MS.Get().pShaderBytecode);
      }

      HashCombine(hash, desc.PS.Get().pShaderBytecode);

      HashCombine(hash, desc.BlendState.Get().AlphaToCoverageEnable);
      HashCombine(hash, desc.BlendState.Get().IndependentBlendEnable);
      HashCombineMemory(hash, desc.BlendState.Get().RenderTarget[0]);

      HashCombineMemory(hash, desc.RasterizerState.Get());
      HashCombineMemory(hash, desc.DepthStencilState.Get());
      HashCombineMemory(hash, desc.InputLayout.Get().pInputElementDescs,
         sizeof(D3D12_INPUT_ELEMENT_DESC) * desc.InputLayout.Get().NumElements);
      HashCombine(hash, desc.PrimitiveTopologyType.Get());
      HashCombineMemory(hash, desc.RTVFormats.Get().RTFormats,
         sizeof(DXGI_FORMAT) * desc.RTVFormats.Get().NumRenderTargets);
      HashCombine(hash, desc.DSVFormat.Get());
      HashCombine(hash, desc.Flags.Get());
   }

   return hash;
}
