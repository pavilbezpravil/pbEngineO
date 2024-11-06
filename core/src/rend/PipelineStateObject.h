#pragma once

#include "Common.h"
#include "d3dx12.h"
#include "core/Ref.h"

namespace pbe {
   class PipelineStateObject : public RefCounted {
   public:
      PipelineStateObject(const D3D12_PIPELINE_STATE_STREAM_DESC& desc);
      PipelineStateObject(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc);
      PipelineStateObject(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
      PipelineStateObject(const CD3DX12_PIPELINE_STATE_STREAM2& stream);
      virtual ~PipelineStateObject() = default;

      ComPtr<ID3D12PipelineState> GetD3D12PipelineState() const {
         return m_d3d12PipelineState;
      }

      bool IsCompute() const { return isComputePSO; };
      bool IsGraphics() const { return !IsCompute(); };

      // todo: remove
      static u64 PSODescHash(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc);
      static u64 PSODescHash(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

      static u64 PSODescHash(const CD3DX12_PIPELINE_STATE_STREAM2& desc);

   private:
      ComPtr<ID3D12PipelineState> m_d3d12PipelineState;
      bool isComputePSO = false;
   };
}
