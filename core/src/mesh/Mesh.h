#pragma once
#include "core/Common.h"
#include "core/Core.h"
#include "core/Ref.h"
#include "math/Types.h"


namespace pbe {
   class CommandList;
   class Buffer;


   class MeshGeom {
   public:
      std::vector<u8> vertexes;
      std::vector<u16> indexes;

      u32 nVertexByteSize = 0;

      MeshGeom() = default;
      MeshGeom(int nVertexByteSize) : nVertexByteSize(nVertexByteSize) {}

      int VertexesBytes() const { return (int)vertexes.size(); }
      int IndexesBytes() const { return IndexCount() * sizeof(u16); }
      int VertexCount() const { return int(vertexes.size() / nVertexByteSize); }
      int IndexCount() const { return (int)indexes.size(); }

      template<typename T>
      void AddVertex(const T& v) {
         auto prevSize = vertexes.size();
         vertexes.resize(prevSize + sizeof(T));
         memcpy(&vertexes[prevSize], &v, sizeof(T));
      }

      void AddTriangle(u16 a, u16 b, u16 c) {
         indexes.push_back(a);
         indexes.push_back(b);
         indexes.push_back(c);
      }

      template<typename T>
      std::vector<T>& VertexesAs() {
         return *(std::vector<T>*) & vertexes;
      }
   };

   MeshGeom MeshGeomCube();


   class Mesh : public RefCounted {
      NON_COPYABLE(Mesh);
   public:
      Mesh() = default;
      Mesh(MeshGeom&& geom) : geom(std::move(geom)) {}

      Ref<Buffer> vertexBuffer;
      Ref<Buffer> indexBuffer;

      MeshGeom geom;

      static Ref<Mesh> Create(MeshGeom&& geom, CommandList& cmd);
   };

}
