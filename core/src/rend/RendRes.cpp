#include "pch.h"
#include "RendRes.h"

#include "CommandQueue.h"
#include "d3dx12.h"
#include "Device.h"
#include "Buffer.h"
#include "DefaultVertex.h"
#include "DescriptorAllocation.h"
#include "mesh/Mesh.h"
#include "shared/hlslCppShared.hlsli"

namespace pbe {
   Ref<Mesh> rendres::pCubeMesh;

   void rendres::Init() {
      VertexPos::inputElementDesc = {
         {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      };

      VertexPosNormal::inputElementDesc = {
         {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
         {
            "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
         },
      };

      VertexPosColor::inputElementDesc = {
         {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
         {
            "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
         },
      };

      VertexPosUintColor::inputElementDesc = {
         {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
         {
            "UINT", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
         },
         {
            "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
         },
      };

      /// rasterizer state
      rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

      rasterizerStateWireframe = rasterizerState;
      rasterizerStateWireframe.FillMode = D3D12_FILL_MODE_WIREFRAME;

      // Depth stencil state
      depthStencilStateDepthReadWrite = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEFAULT);

      depthStencilStateDepthReadNoWrite = depthStencilStateDepthReadWrite;
      depthStencilStateDepthReadNoWrite.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

      depthStencilStateEqual = depthStencilStateDepthReadWrite;
      depthStencilStateEqual.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;

      depthStencilStateNo = depthStencilStateDepthReadWrite;
      depthStencilStateNo.DepthEnable = false;

      /// Blend state
      blendStateDefaultRGBA = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

      blendStateDefaultRGB = blendStateDefaultRGB;
      blendStateDefaultRGB.RenderTarget[0].RenderTargetWriteMask =
         D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;

      blendStateTransparencyRGB.RenderTarget[0].BlendEnable = true;
      blendStateTransparencyRGB.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      blendStateTransparencyRGB.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      blendStateTransparencyRGB.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      blendStateTransparencyRGB.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
      blendStateTransparencyRGB.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
      blendStateTransparencyRGB.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
      blendStateTransparencyRGB.RenderTarget[0].RenderTargetWriteMask =
         D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;

      // # Default root signature

      // ## Static samplers
      CD3DX12_STATIC_SAMPLER_DESC templateSampler{0};
      templateSampler.AddressU = templateSampler.AddressV = templateSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      templateSampler.RegisterSpace = REGISTER_SPACE_COMMON;

      CD3DX12_STATIC_SAMPLER_DESC samplers[4];

      samplers[0] = templateSampler;
      samplers[0].ShaderRegister = SAMPLER_SLOT_WRAP_POINT;
      samplers[0].Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;

      samplers[1] = templateSampler;
      samplers[1].ShaderRegister = SAMPLER_SLOT_WRAP_LINEAR;
      samplers[1].Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;

      samplers[2] = templateSampler;
      samplers[2].ShaderRegister = SAMPLER_SLOT_CLAMP_POINT;
      samplers[2].Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
      samplers[2].AddressU = templateSampler.AddressV = templateSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

      samplers[3] = templateSampler;
      samplers[3].ShaderRegister = SAMPLER_SLOT_SHADOW;
      samplers[3].Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;
      samplers[3].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
      samplers[3].AddressU = templateSampler.AddressV = templateSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
      samplers[3].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;

      /// ## Dynamic samplers

      auto CreateDescriptor = [](DescriptorAllocation& samplerAllocation, const D3D12_SAMPLER_DESC& desc) {
         samplerAllocation = sDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1);
         sDevice->g_Device->CreateSampler(&desc, samplerAllocation.GetDescriptorHandle());
         };

      D3D12_SAMPLER_DESC dynSamplerTemplate = {};
      dynSamplerTemplate.MipLODBias = 0.f;
      dynSamplerTemplate.MinLOD = 0.f;
      dynSamplerTemplate.MaxLOD = D3D12_FLOAT32_MAX;
      dynSamplerTemplate.MaxAnisotropy = 16;
      dynSamplerTemplate.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

      dynSamplerTemplate.AddressU = dynSamplerTemplate.AddressV = dynSamplerTemplate.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

      dynSamplerTemplate.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
      CreateDescriptor(samplerPointClamp, dynSamplerTemplate);
      dynSamplerTemplate.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      CreateDescriptor(samplerLinearClamp, dynSamplerTemplate);

      dynSamplerTemplate.AddressU = dynSamplerTemplate.AddressV = dynSamplerTemplate.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

      dynSamplerTemplate.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
      CreateDescriptor(samplerPointWrap, dynSamplerTemplate);
      dynSamplerTemplate.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      CreateDescriptor(samplerLinearWrap, dynSamplerTemplate);

      dynSamplerTemplate.AddressU = dynSamplerTemplate.AddressV = dynSamplerTemplate.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;

      dynSamplerTemplate.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
      CreateDescriptor(samplerPointMirror, dynSamplerTemplate);
      dynSamplerTemplate.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      CreateDescriptor(samplerLinearMirror, dynSamplerTemplate);

      dynSamplerTemplate.AddressU = dynSamplerTemplate.AddressV = dynSamplerTemplate.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;

      dynSamplerTemplate.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
      dynSamplerTemplate.BorderColor[0] = dynSamplerTemplate.BorderColor[1]
         = dynSamplerTemplate.BorderColor[2] = dynSamplerTemplate.BorderColor[3] = 0;
      CreateDescriptor(samplerPointClampBoarderZero, dynSamplerTemplate);

      // ## Root parameters
      std::vector<CD3DX12_ROOT_PARAMETER1> rootParamets;

      CD3DX12_ROOT_PARAMETER1 templateParameter;

      templateParameter.InitAsConstantBufferView(0, 0);
      rootParamets.push_back(templateParameter);

      {
         static CD3DX12_DESCRIPTOR_RANGE1 ranges[2]; // todo: static
         ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 20, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
         ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 10, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

         templateParameter.InitAsDescriptorTable(_countof(ranges), ranges);
         rootParamets.push_back(templateParameter);
      }

      {
         static CD3DX12_DESCRIPTOR_RANGE1 ranges[1]; // todo: static
         ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 10, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

         templateParameter.InitAsDescriptorTable(_countof(ranges), ranges);
         rootParamets.push_back(templateParameter);
      }

      templateParameter.InitAsConstantBufferView(CB_SLOT_SCENE, REGISTER_SPACE_COMMON);
      rootParamets.push_back(templateParameter);

      templateParameter.InitAsConstantBufferView(CB_SLOT_CAMERA, REGISTER_SPACE_COMMON);
      rootParamets.push_back(templateParameter);

      templateParameter.InitAsConstantBufferView(CB_SLOT_CULL_CAMERA, REGISTER_SPACE_COMMON);
      rootParamets.push_back(templateParameter);

      templateParameter.InitAsConstantBufferView(CB_SLOT_EDITOR, REGISTER_SPACE_COMMON);
      rootParamets.push_back(templateParameter);

      {
         static CD3DX12_DESCRIPTOR_RANGE1 ranges[2]; // todo: static
         ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, REGISTER_SPACE_COMMON, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
         ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, REGISTER_SPACE_COMMON, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

         templateParameter.InitAsDescriptorTable(_countof(ranges), ranges);
         rootParamets.push_back(templateParameter);
      }

      D3D12_ROOT_SIGNATURE_DESC1 rsDesc;
      rsDesc.NumParameters = (u32)rootParamets.size();
      rsDesc.pParameters = rootParamets.data();
      rsDesc.NumStaticSamplers = _countof(samplers);
      rsDesc.pStaticSamplers = samplers;
      rsDesc.Flags =
         D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
         | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
         | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
         ;

      pDefaultRootSignature = Ref<RootSignature>::Create(rsDesc);

      sDevice->GetCommandQueue().ExecuteImmediate(
         [](CommandList& cmd) {
            pCubeMesh = Mesh::Create(MeshGeomCube(), cmd);
         });
   }

   void rendres::Term() {
      pDefaultRootSignature.Reset();
      pCubeMesh.Reset();

      samplerPointClamp.Free();
      samplerLinearClamp.Free();
      samplerPointWrap.Free();
      samplerLinearWrap.Free();
      samplerPointMirror.Free();
      samplerLinearMirror.Free();

      samplerPointClampBoarderZero.Free();
   }

   Mesh& rendres::CubeMesh() {
      return *pCubeMesh;
   }
}
