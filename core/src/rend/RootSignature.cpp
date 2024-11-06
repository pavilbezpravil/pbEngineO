#include "pch.h"

#include "RootSignature.h"
#include "Device.h"
#include "core/Assert.h"

using namespace pbe;

RootSignature::RootSignature(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc) {
   SetRootSignatureDesc(rootSignatureDesc);
}

RootSignature::~RootSignature() {
   Destroy();
}

CmdBindPoint RootSignature::GetCmdBindPoint(BindType bindType, BindPoint bindPoint) {
   u32 linearIndex = GetLinearIndex(bindType, bindPoint.slot, bindPoint.space);
   ASSERT(cmdBindPoints.contains(linearIndex));
   return cmdBindPoints[linearIndex];
}

void RootSignature::Destroy() {
   for (UINT i = 0; i < m_RootSignatureDesc.NumParameters; ++i) {
      const D3D12_ROOT_PARAMETER1& rootParameter = m_RootSignatureDesc.pParameters[i];
      if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
         delete[] rootParameter.DescriptorTable.pDescriptorRanges;
      }
   }

   delete[] m_RootSignatureDesc.pParameters;
   m_RootSignatureDesc.pParameters = nullptr;
   m_RootSignatureDesc.NumParameters = 0;

   delete[] m_RootSignatureDesc.pStaticSamplers;
   m_RootSignatureDesc.pStaticSamplers = nullptr;
   m_RootSignatureDesc.NumStaticSamplers = 0;

   m_DescriptorTableBitMask = 0;
   m_SamplerTableBitMask = 0;

   memset(m_NumDescriptorsPerTable, 0, sizeof(m_NumDescriptorsPerTable));
}

void RootSignature::SetRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc) {
   // Make sure any previously allocated root signature description is cleaned
   // up first.
   Destroy();

   UINT numParameters = rootSignatureDesc.NumParameters;
   D3D12_ROOT_PARAMETER1* pParameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER1[numParameters] : nullptr;

   for (UINT i = 0; i < numParameters; ++i) {
      const D3D12_ROOT_PARAMETER1& rootParameter = rootSignatureDesc.pParameters[i];
      pParameters[i] = rootParameter;

      if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
         UINT numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
         D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges =
            numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges] : nullptr;

         memcpy(pDescriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges,
                sizeof(D3D12_DESCRIPTOR_RANGE1) * numDescriptorRanges);

         pParameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
         pParameters[i].DescriptorTable.pDescriptorRanges = pDescriptorRanges;

         // Set the bit mask depending on the type of descriptor table.
         if (numDescriptorRanges > 0) {
            switch (pDescriptorRanges[0].RangeType) {
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
               m_DescriptorTableBitMask |= (1 << i);
               break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
               m_SamplerTableBitMask |= (1 << i);
               break;
            }
         }

         // Count the number of descriptors in the descriptor table.
         for (UINT j = 0; j < numDescriptorRanges; ++j) {
            u32 rangeTableOffset = m_NumDescriptorsPerTable[i];
            m_NumDescriptorsPerTable[i] += pDescriptorRanges[j].NumDescriptors;

            auto& range = pDescriptorRanges[j];
            u32 offset = -1;
            switch (range.RangeType) {
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
               offset = 0; break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
               offset = 1; break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
               offset = 2; break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
               offset = 3; break;
            default:
               UNIMPLEMENTED();
            }

            for (u32 iRegister = 0; iRegister < range.NumDescriptors; ++iRegister){
               u32 linearIndex = GetLinearIndex((BindType)offset, range.BaseShaderRegister + iRegister, range.RegisterSpace);
               cmdBindPoints[linearIndex] = {
                  .isRoot = false,
                  .rootParameterIndex = i,
                  .descriptorOffset = rangeTableOffset + iRegister,
               };
            }
         }
      } else {
         u32 linearIndex = GetLinearIndex((BindType)(rootParameter.ParameterType - 2), rootParameter.Descriptor.ShaderRegister, rootParameter.Descriptor.RegisterSpace);
         cmdBindPoints[linearIndex] = {
            .isRoot = true,
            .rootParameterIndex = i,
            .descriptorOffset = 0,
         };
      }
   }

   m_RootSignatureDesc.NumParameters = numParameters;
   m_RootSignatureDesc.pParameters = pParameters;

   UINT numStaticSamplers = rootSignatureDesc.NumStaticSamplers;
   D3D12_STATIC_SAMPLER_DESC* pStaticSamplers =
      numStaticSamplers > 0 ? new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers] : nullptr;

   if (pStaticSamplers) {
      memcpy(pStaticSamplers, rootSignatureDesc.pStaticSamplers,
             sizeof(D3D12_STATIC_SAMPLER_DESC) * numStaticSamplers);
   }

   m_RootSignatureDesc.NumStaticSamplers = numStaticSamplers;
   m_RootSignatureDesc.pStaticSamplers = pStaticSamplers;

   D3D12_ROOT_SIGNATURE_FLAGS flags = rootSignatureDesc.Flags;
   m_RootSignatureDesc.Flags = flags;

   CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc;
   versionRootSignatureDesc.Init_1_1(numParameters, pParameters, numStaticSamplers, pStaticSamplers, flags);

   D3D_ROOT_SIGNATURE_VERSION highestVersion = sDevice->GetFeatures().highestRootSignatureVersion;

   // Serialize the root signature.
   ComPtr<ID3DBlob> rootSignatureBlob;
   ComPtr<ID3DBlob> errorBlob;
   ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&versionRootSignatureDesc, highestVersion,
                                                       &rootSignatureBlob, &errorBlob));

   if (errorBlob) {
      auto errorMsg = (const char*)errorBlob->GetBufferPointer();
      ERROR("RootSignature error: {}", errorMsg);
   }

   auto d3d12Device = GetD3D12Device();

   // Create the root signature.
   ThrowIfFailed(d3d12Device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
                                                  rootSignatureBlob->GetBufferSize(),
                                                  IID_PPV_ARGS(&m_RootSignature)));
}

uint RootSignature::GetLinearIndex(BindType type, u32 slot, u32 space) {
   return space * 10000 + (u32)type * 1000 + slot;
}

uint32_t RootSignature::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const {
   uint32_t descriptorTableBitMask = 0;
   switch (descriptorHeapType) {
   case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
      descriptorTableBitMask = m_DescriptorTableBitMask;
      break;
   case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
      descriptorTableBitMask = m_SamplerTableBitMask;
      break;
   }

   return descriptorTableBitMask;
}

uint32_t RootSignature::GetNumDescriptors(uint32_t rootIndex) const {
   assert(rootIndex < 32);
   return m_NumDescriptorsPerTable[rootIndex];
}
