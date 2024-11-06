#include "pch.h"
#include "Mesh.h"
#include "rend/Buffer.h"
#include "rend/CommandList.h"
#include "rend/DefaultVertex.h"


namespace pbe {

   MeshGeom MeshGeomCube() {
      MeshGeom cube{ sizeof(VertexPosNormal) };

      cube.AddVertex(VertexPosNormal{ { -0.5f, 0.5f, -0.5f}, {0, 0, 255}, });
      cube.AddVertex(VertexPosNormal{ { 0.5f, 0.5f, -0.5f}, {0, 255, 0}, });
      cube.AddVertex(VertexPosNormal{ { -0.5f, -0.5f, -0.5f}, {255, 0, 0}, });
      cube.AddVertex(VertexPosNormal{ { 0.5f, -0.5f, -0.5f}, {0, 255, 255}, });
      cube.AddVertex(VertexPosNormal{ { -0.5f, 0.5f, 0.5f}, {0, 0, 255}, });
      cube.AddVertex(VertexPosNormal{ { 0.5f, 0.5f, 0.5f}, {255, 0, 0}, });
      cube.AddVertex(VertexPosNormal{ { -0.5f, -0.5f, 0.5f}, {0, 255, 0}, });
      cube.AddVertex(VertexPosNormal{ { 0.5f, -0.5f, 0.5f}, {0, 255, 255}, });

      cube.indexes = {
         0, 1, 2,    // side 1
         2, 1, 3,
         4, 0, 6,    // side 2
         6, 0, 2,
         7, 5, 6,    // side 3
         6, 5, 4,
         3, 1, 7,    // side 4
         7, 1, 5,
         4, 5, 0,    // side 5
         0, 5, 1,
         3, 7, 2,    // side 6
         2, 7, 6,
      };

      return cube;
   }

   Ref<Mesh> Mesh::Create(MeshGeom&& geom, CommandList& cmd) {
      Ref<Mesh> mesh = Ref<Mesh>::Create(std::move(geom));

      mesh->vertexBuffer = cmd.CreateBuffer(mesh->geom.vertexes.data(), mesh->geom.VertexesBytes());
      mesh->indexBuffer = cmd.CreateBuffer(mesh->geom.indexes.data(), mesh->geom.IndexesBytes());

      return mesh;
   }

}
