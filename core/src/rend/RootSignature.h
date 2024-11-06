#pragma once

#include "BindPoint.h"
#include "Common.h"
#include "core/Ref.h"

namespace pbe {
   class Device;

   class RootSignature : public RefCounted {
   public:
      RootSignature(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc);
      virtual ~RootSignature();

      ComPtr<ID3D12RootSignature> GetD3D12RootSignature() const {
         return m_RootSignature;
      }

      const D3D12_ROOT_SIGNATURE_DESC1& GetRootSignatureDesc() const {
         return m_RootSignatureDesc;
      }

      uint32_t GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const;
      uint32_t GetNumDescriptors(uint32_t rootIndex) const;

      CmdBindPoint GetCmdBindPoint(BindType bindType, BindPoint bindPoint);

   private:
      void Destroy();
      void SetRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc);

      D3D12_ROOT_SIGNATURE_DESC1 m_RootSignatureDesc{};
      ComPtr<ID3D12RootSignature> m_RootSignature;

      // Need to know the number of descriptors per descriptor table.
      // A maximum of 32 descriptor tables are supported (since a 32-bit
      // mask is used to represent the descriptor tables in the root signature.
      uint32_t m_NumDescriptorsPerTable[32]{0};

      // A bit mask that represents the root parameter indices that are
      // descriptor tables for Samplers.
      uint32_t m_SamplerTableBitMask = 0;
      // A bit mask that represents the root parameter indices that are
      // CBV, UAV, and SRV descriptor tables.
      uint32_t m_DescriptorTableBitMask = 0;

      std::unordered_map<u32, CmdBindPoint> cmdBindPoints;

      // todo:
      u32 GetLinearIndex(BindType type, u32 slot, u32 space);
   };
}
