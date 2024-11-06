#include "pch.h"
#include "Texture2D.h"
#include "Device.h"
#include "core/Assert.h"

namespace pbe {
   static bool IsTypelessFormat(DXGI_FORMAT format) {
      // todo:
      return format == DXGI_FORMAT_R24G8_TYPELESS;
   }

   static DXGI_FORMAT FormatToDepth(DXGI_FORMAT format) {
      if (format == DXGI_FORMAT_R24G8_TYPELESS) {
         return DXGI_FORMAT_D24_UNORM_S8_UINT;
      }
      if (format == DXGI_FORMAT_R16_TYPELESS) {
         return DXGI_FORMAT_D16_UNORM;
      }
      return format;
   }

   static DXGI_FORMAT FormatToSrv(DXGI_FORMAT format) {
      if (format == DXGI_FORMAT_R24G8_TYPELESS) {
         return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
      }
      if (format == DXGI_FORMAT_R16_TYPELESS) {
         return DXGI_FORMAT_R16_UNORM;
      }
      return format;
   }

   template class Ref<Texture2D>;

   Ref<Texture2D> Texture2D::Create(ComPtr<ID3D12Resource> pRes) {
      return Ref<Texture2D>::Create(pRes);
   }

   Ref<Texture2D> Texture2D::Create(const Desc& desc) {
      return Ref<Texture2D>::Create(desc);
   }

   const Texture2D::Desc& Texture2D::GetDesc() const {
      return desc;
   }

   Texture2D::Texture2D(ComPtr<ID3D12Resource> pRes) : GpuResource(pRes) {
      CD3DX12_RESOURCE_DESC dx12Desc(pResource->GetDesc());

      desc.format = dx12Desc.Format;
      desc.size = {dx12Desc.Width, dx12Desc.Height};

      // todo: expect this func calls only for spaw chain texs
      desc.heapType = D3D12_HEAP_TYPE_DEFAULT;
      desc.mips = 1;
      desc.allowRenderTarget = true;

      CreateViews();
   }

   Texture2D::Texture2D(const Desc& _desc) : GpuResource(nullptr), desc(_desc) {
      // todo: calc Mips
      if (desc.mips == 0) {
         int2 size = desc.size;
         int nMips = 0;

         while (size.x > 0 || size.y > 0) {
            size /= 2;
            nMips++;
         }

         desc.mips = nMips;
      }

      auto& device = *sDevice;

      D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
      if (desc.allowRenderTarget) {
         resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
      }
      if (desc.allowDepthStencil) {
         resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
      }
      if (desc.allowUAV) {
         resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
         state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; // todo: strange. Driver decided it as first resource state
      }

      auto heapProps = CD3DX12_HEAP_PROPERTIES(desc.heapType);
      auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(desc.format, desc.size.x, desc.size.y,
         1, desc.mips, 1, 0,
         resourceFlags, D3D12_TEXTURE_LAYOUT_UNKNOWN, desc.alignment);

      auto clarValue = desc.useOptimizedClearValue ? &desc.optimizedClearValue : nullptr;

      if (desc.aliasable) {
         D3D12_RESOURCE_DESC resourceDescs[] = { resDesc };
         auto allocationInfo = device.g_Device->GetResourceAllocationInfo(0, _countof(resourceDescs), resourceDescs);

         D3D12_HEAP_DESC heapDesc = {};
         heapDesc.SizeInBytes = allocationInfo.SizeInBytes;
         heapDesc.Alignment = allocationInfo.Alignment;
         // heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
         heapDesc.Flags = D3D12_HEAP_FLAG_NONE;
         heapDesc.Properties = heapProps;

         ThrowIfFailed(sDevice->g_Device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)));

         ThrowIfFailed(device.g_Device->CreatePlacedResource(
            heap.Get(), 0,
            &resDesc,
            state,
            clarValue,
            IID_PPV_ARGS(&pResource)));
      } else {
         ThrowIfFailed(device.g_Device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            state,
            clarValue,
            IID_PPV_ARGS(&pResource)));
      }

      if (!pResource) {
         WARN("Cant create texture {}!", desc.name);
         return;
      }

      SetName(desc.name);

      CreateViews();
   }

   // Get a UAV description that matches the resource description.
   static D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(const D3D12_RESOURCE_DESC& resDesc, UINT mipSlice,
                                                      UINT arraySlice = 0, UINT planeSlice = 0) {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
      uavDesc.Format = resDesc.Format;

      switch (resDesc.Dimension) {
      case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
         if (resDesc.DepthOrArraySize > 1) {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            uavDesc.Texture1DArray.ArraySize = resDesc.DepthOrArraySize - arraySlice;
            uavDesc.Texture1DArray.FirstArraySlice = arraySlice;
            uavDesc.Texture1DArray.MipSlice = mipSlice;
         } else {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            uavDesc.Texture1D.MipSlice = mipSlice;
         }
         break;
      case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
         if (resDesc.DepthOrArraySize > 1) {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.ArraySize = resDesc.DepthOrArraySize - arraySlice;
            uavDesc.Texture2DArray.FirstArraySlice = arraySlice;
            uavDesc.Texture2DArray.PlaneSlice = planeSlice;
            uavDesc.Texture2DArray.MipSlice = mipSlice;
         } else {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.PlaneSlice = planeSlice;
            uavDesc.Texture2D.MipSlice = mipSlice;
         }
         break;
      case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
         uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
         uavDesc.Texture3D.WSize = resDesc.DepthOrArraySize - arraySlice;
         uavDesc.Texture3D.FirstWSlice = arraySlice;
         uavDesc.Texture3D.MipSlice = mipSlice;
         break;
      default:
         UNIMPLEMENTED();
      }

      return uavDesc;
   }

   void Texture2D::CreateViews() {
      auto& device = *sDevice;
      auto d3d12Device = GetD3D12Device();

      CD3DX12_RESOURCE_DESC desc(pResource->GetDesc());

      // Create RTV
      if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0) {
         m_RenderTargetView = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
         d3d12Device->CreateRenderTargetView(pResource.Get(), nullptr,
                                             m_RenderTargetView.GetDescriptorHandle());
      }
      // Create DSV
      if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0) {
         D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
         dsvDesc.Format = GetDSVFormat(desc.Format);
         dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
         dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
         dsvDesc.Texture2D.MipSlice = 0;

         m_DepthStencilViewReadOnly = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
         d3d12Device->CreateDepthStencilView(pResource.Get(), &dsvDesc,
            m_DepthStencilViewReadOnly.GetDescriptorHandle());

         m_DepthStencilViewReadWrite = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
         dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
         d3d12Device->CreateDepthStencilView(pResource.Get(), &dsvDesc,
            m_DepthStencilViewReadWrite.GetDescriptorHandle());
      }
      // Create SRV
      if ((desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0) {
         m_ShaderResourceView = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

         D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
         srvDesc.Format = GetSRVFormat(desc.Format);
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
         srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
         srvDesc.Texture2D.MipLevels = desc.MipLevels;

         d3d12Device->CreateShaderResourceView(pResource.Get(), &srvDesc,
                                               m_ShaderResourceView.GetDescriptorHandle());
      }
      // Create UAV for each mip (only supported for 1D and 2D textures).
      if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0 && desc.DepthOrArraySize == 1) {
         m_UnorderedAccessView = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, desc.MipLevels);
         for (int i = 0; i < desc.MipLevels; ++i) {
            auto uavDesc = GetUAVDesc(desc, i);
            d3d12Device->CreateUnorderedAccessView(pResource.Get(), nullptr, &uavDesc,
                                                   m_UnorderedAccessView.GetDescriptorHandle(i));
         }
      }
   }
}
