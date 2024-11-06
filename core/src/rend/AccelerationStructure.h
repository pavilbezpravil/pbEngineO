#pragma once
#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "core/Ref.h"
#include "math/Common.h"
#include "math/Transform.h"
#include "utils/Memory.h"


namespace pbe {

   class AccelerationStructure : public RefCounted {
   public:
      enum Type {
         TLAS,
         BLAS,
         Unknown,
      };

      struct AABB {
         float minX;
         float minY;
         float minZ;
         float maxX;
         float maxY;
         float maxZ;
      };

      struct GeomDesc {
         enum class Type {
            Triangles,
            ProceduralPrimitiveAABBs,
         };

         enum class Flags {
            None = 0,
            Opaque = 0x1,
         };

         Type type;
         Flags flags = Flags::None;

         Ref<Buffer> vertexes;
         Ref<Buffer> indexes;

         Ref<Buffer> aabbs;
      };

      struct Instance {
         Transform transform;
         Ref<AccelerationStructure> blas;

         u32 instanceID : 24 = 0;
         u32 instanceMask : 8 = 0xFF;
      };

      void AddAABBs(Ref<Buffer> aabb) {
         ASSERT(!IsTLAS());
         type = BLAS;

         geoms.push_back({});

         GeomDesc& geom = geoms.back();
         geom.type = GeomDesc::Type::ProceduralPrimitiveAABBs;
         geom.aabbs = aabb;
      }

      void AddTriangles(Ref<Buffer> vertexes, Ref<Buffer> indexes) {
         ASSERT(!IsTLAS());
         type = BLAS;

         geoms.push_back({});

         GeomDesc& geom = geoms.back();
         geom.type = GeomDesc::Type::Triangles;
         geom.vertexes = vertexes;
         geom.indexes = indexes;
      }

      void SetInstances(Ref<Buffer> instancesBuffer) {
         ASSERT(!IsBLAS());
         type = TLAS;

         instances = instancesBuffer;
      }

      void Build(CommandList& cmd) {
         ASSERT(type != Unknown);

         D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
         ASInputs.Type = (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE)type;
         ASInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
         ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

         Array<D3D12_RAYTRACING_GEOMETRY_DESC> geometries{ geoms.size() };

         if (type == BLAS) {
            for (u32 i = 0; i < geoms.size(); ++i) {
               const GeomDesc& geom = geoms[i];
               D3D12_RAYTRACING_GEOMETRY_DESC& geometry = geometries[i];

               geometry.Flags = (D3D12_RAYTRACING_GEOMETRY_FLAGS)geom.flags;

               if (geom.type == GeomDesc::Type::Triangles) {
                  geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                  geometry.Triangles.Transform3x4 = 0; // todo:

                  geometry.Triangles.VertexBuffer.StartAddress = geom.vertexes->GetGPUVirtualAddress();
                  geometry.Triangles.VertexBuffer.StrideInBytes = geom.vertexes->StructureByteStride();
                  geometry.Triangles.VertexCount = (u32)geom.vertexes->NumElements();
                  geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT; // todo:
                  // geometry.Triangles.VertexFormat = geom.vertexes->GetDesc().format;

                  geometry.Triangles.IndexBuffer = geom.indexes->GetGPUVirtualAddress();
                  geometry.Triangles.IndexCount = (u32)geom.indexes->NumElements();
                  geometry.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT; // todo:
                  // geometry.Triangles.IndexFormat = geom.indexes->GetDesc().format;

                  cmd.TransitionBarrier(*geom.vertexes, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                  cmd.TransitionBarrier(*geom.indexes, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
               }
               else {
                  geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;

                  geometry.AABBs.AABBCount = geom.aabbs->NumElements();
                  geometry.AABBs.AABBs = {
                     .StartAddress = geom.aabbs->GetGPUVirtualAddress(),
                     .StrideInBytes = geom.aabbs->StructureByteStride(),
                  };

                  cmd.TransitionBarrier(*geom.aabbs, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
               }
            }

            ASInputs.pGeometryDescs = geometries.data();
            ASInputs.NumDescs = (u32)geometries.size();
         }
         else {
            ASInputs.InstanceDescs = instances->GetGPUVirtualAddress();
            ASInputs.NumDescs = (u32)instances->NumElements();

            cmd.TransitionBarrier(*instances, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
         }

         cmd.FlushResourceBarriers();

         D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASBuildInfo = {};
         sDevice->g_Device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASBuildInfo);

         u64 scratchDataSizeInBytes = AlignUp(ASBuildInfo.ScratchDataSizeInBytes,
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
         u64 resultDataMaxSizeInBytes = AlignUp(ASBuildInfo.ResultDataMaxSizeInBytes,
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

         // todo: reuse already created buffers
         scratch = Buffer::Create(Buffer::Desc::Default(scratchDataSizeInBytes)
            .AllowUAV().Name("AS scratch"));
         result = Buffer::Create(Buffer::Desc::AccelerationStructure(resultDataMaxSizeInBytes)
            .Name("AS result"));

         cmd.TrackResource(*scratch);
         cmd.TrackResource(*result);

         // Describe the bottom-level acceleration structure.
         D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
         desc.Inputs = ASInputs;
         desc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
         desc.DestAccelerationStructureData = result->GetGPUVirtualAddress();

         // todo: not direct access to d3dCommandList
         cmd.GetD3D12CommandList()->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

         cmd.UAVBarrier(*result);
      }

      Ref<Buffer> GetASBuffer() const { return result; }

      bool IsBuilt() const { return result; }

      Type GetType() const { return type; }
      bool IsBLAS() const { return type == BLAS; }
      bool IsTLAS() const { return type == TLAS; }

      static Ref<Buffer> CreateAABBs(CommandList& cmd, DataView<AABB> aabbData) {
         u32 aabbCount = (u32)aabbData.size();
         if (aabbCount == 0) {
            return {};
         }

         // todo: mb not necessary
         u64 aabbGpuBufferSizeInBytes = AlignUp(aabbCount * sizeof(AABB),
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

         auto aabbBuffer = cmd.CreateBuffer(Buffer::Desc::StructuredWithSize<AABB>(aabbGpuBufferSizeInBytes, aabbCount),
            aabbData);
         aabbBuffer->SetName("BLAS aabbs");

         return aabbBuffer;
      }

      static Ref<Buffer> CreateInstances(CommandList& cmd, DataView<Instance> insts) {
         u32 instCount = (u32)insts.size();
         if (instCount == 0) {
            return {};
         }

         Array<D3D12_RAYTRACING_INSTANCE_DESC> instancesDesc;

         for (u32 i = 0; i < instCount; ++i) {
            const Instance& instance = insts[i];
            ASSERT(instance.blas->IsBuilt());

            D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
            // todo:
            MemsetZero(instanceDesc.Transform);
            instanceDesc.Transform[0][0]
               = instanceDesc.Transform[1][1]
               = instanceDesc.Transform[2][2] = 1;

            instanceDesc.InstanceID = instance.instanceID;
            instanceDesc.InstanceMask = instance.instanceMask;
            instanceDesc.InstanceContributionToHitGroupIndex = 0;
            instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            instanceDesc.AccelerationStructure = instance.blas->result->GetGPUVirtualAddress();

            instancesDesc.push_back(instanceDesc);
         }

         u32 sizeOfInstanceDesc = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) - sizeof(D3D12_GPU_VIRTUAL_ADDRESS);

         u64 instancesGpuBufferSizeInBytes = AlignUp(instancesDesc.size() * sizeOfInstanceDesc,
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

         auto instancesGpuBuffer = cmd.CreateBuffer(
            Buffer::Desc::StructuredWithSize(instancesGpuBufferSizeInBytes,
               instancesDesc.size(), sizeOfInstanceDesc), DataView{ instancesDesc });

         instancesGpuBuffer->SetName("TLAS instances");

         return instancesGpuBuffer;
      }

   private:
      Type type = Type::Unknown;
      Array<GeomDesc> geoms;
      Ref<Buffer> instances;

      // todo: update
      Ref<Buffer> scratch;
      Ref<Buffer> result;
   };

}
