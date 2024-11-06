#pragma once
#include <d3d12.h>

#include "DescriptorAllocation.h"
#include "Format.h"
#include "GpuResource.h"
#include "GpuResource.h"
#include "core/Ref.h"
#include "math/Types.h"

namespace pbe {
   class CORE_API Texture2D : public GpuResource {
   public:
      struct CORE_API Desc : ResourceDesc, ResourceDescBuilder<Desc> {
         Format format = DXGI_FORMAT_UNKNOWN;
         uint2 size = {};
         int mips = 1;
         bool allowRenderTarget = false;
         bool allowDepthStencil = false;
         bool useOptimizedClearValue = false;
         D3D12_CLEAR_VALUE optimizedClearValue; // todo: optimized clear value

         bool aliasable = false; // todo:

         static Desc Default(Format format, uint2 size, bool allowUAV = false) {
            Desc desc{};
            desc.format = format;
            desc.size = size;
            desc.allowUAV = allowUAV;
            return desc;
         }

         static Desc RenderTarget(Format format, uint2 size) {
            Desc desc{};
            desc.format = format;
            desc.size = size;
            desc.allowRenderTarget = true;
            return desc;
         }

         static Desc DepthStencil(Format format, uint2 size) {
            Desc desc{};
            desc.format = format;
            desc.size = size;
            desc.allowDepthStencil = true;
            return desc;
         }

         Desc Format(Format format) {
            this->format = format;
            return *this;
         }

         Desc Mips(int mips = 0) {
            this->mips = mips;
            return *this;
         }

         Desc OptimizedClearValue(D3D12_CLEAR_VALUE optimizedClearValue) {
            this->optimizedClearValue = optimizedClearValue;
            this->useOptimizedClearValue = true;
            return *this;
         }

         Desc Aliasable(bool aliasable = true) {
            this->aliasable = aliasable;
            return *this;
         }
      };

      static Ref<Texture2D> Create(ComPtr<ID3D12Resource> pRes);
      static Ref<Texture2D> Create(const Desc& desc);

      const Desc& GetDesc() const;
      const ResourceDesc& GetResourceDesc() const override { return desc; }

      D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const {
         return m_RenderTargetView.GetDescriptorHandle();
      }

      D3D12_CPU_DESCRIPTOR_HANDLE GetDSVReadWrite() const {
         return m_DepthStencilViewReadWrite.GetDescriptorHandle();
      }

      D3D12_CPU_DESCRIPTOR_HANDLE GetDSVReadOnly() const {
         return m_DepthStencilViewReadOnly.GetDescriptorHandle();
      }

   private:
      friend Ref<Texture2D>;

      Texture2D(ComPtr<ID3D12Resource> pRes);
      Texture2D(const Desc& desc);

      void CreateViews();

      DescriptorAllocation m_RenderTargetView;
      DescriptorAllocation m_DepthStencilViewReadWrite;
      DescriptorAllocation m_DepthStencilViewReadOnly;

      Desc desc;

      ResourceDesc& GetResourceDesc() override { return desc; }

      ComPtr<ID3D12Heap> heap;
   };

   extern template class Ref<Texture2D>;
}
