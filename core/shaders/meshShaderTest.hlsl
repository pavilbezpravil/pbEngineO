#include "commonResources.hlsli"
#include "common.hlsli"
#include "random.hlsli"

#include "NRDEncoding.hlsli"
#include "NRD.hlsli"

struct Vertex {
   float4 posH : SV_POSITION;
   float3 posW : POS_W;
};

static const uint GROUP_SIZE = 64;
static const uint VERTEX_SIZE = 256;
static const uint TRIS_SIZE = VERTEX_SIZE / 8 * 6;

#define OUTPUT_GEOM

#ifdef OUTPUT_GEOM
RWStructuredBuffer<float3> gVertices : register(u0);
RWStructuredBuffer<uint> gIndexes : register(u1);
RWStructuredBuffer<uint> gCounter : register(u2);

groupshared uint vertWriteId;
groupshared uint indexWriteId;
#endif

float3 GetWindOffset(float3 posW, float3 windDirection) {
   float time = GetAnimationTime();
   float posOnSinWave = dot(posW, windDirection);

   float t = posOnSinWave + time * 2;
   float3 windOffset = float3(
         sin(t * 0.5),
         0,
         cos(t)
      );

   return windOffset;
}

[NumThreads(GROUP_SIZE, 1, 1)]
[OutputTopology("triangle")]
void MS(
    uint gtid : SV_GroupThreadID,
    uint2 gid : SV_GroupID,
    out indices uint3 tris[TRIS_SIZE],
    out vertices Vertex verts[VERTEX_SIZE]
) {
   static const int verticesPerBladeEdge = 4;
   static const int verticesPerBlade = 2 * verticesPerBladeEdge;
   static const int trianglesPerBlade = 6;
   static const int maxBladeCount = 32;

   uint patchSeed = RandomToSeed(gid);

   float3 patchCenterW = 0;
   patchCenterW.xz += gid + 0.5 - 50.0;

   static const float GRASS_END_DISTANCE = 75;

   float distanceToCamera = distance(patchCenterW, GetCullCamera().position);
   float bladeCountF = lerp(float(maxBladeCount), 2., pow(saturate(distanceToCamera / (GRASS_END_DISTANCE * 1.05)), 0.75));
    
   int bladeCount = ceil(bladeCountF);

   // bladeCount = maxBladeCount;

   int vertCount = bladeCount * 8;
   int trisCount = bladeCount * 6;

   SetMeshOutputCounts(vertCount, trisCount);

#ifdef OUTPUT_GEOM
   bool emitGeom = distanceToCamera < 10;

   if (gtid == 0 && emitGeom) {
      InterlockedAdd(gCounter[0], vertCount, vertWriteId);
      InterlockedAdd(gCounter[1], trisCount * 3, indexWriteId);
   }
   GroupMemoryBarrierWithGroupSync();
#endif

   for (int i = 0; i < VERTEX_SIZE / GROUP_SIZE + 1; ++i) {
      int vertId = gtid + GROUP_SIZE * i;
      if (vertId >= vertCount) {
         break;
      }
   
      int bladeId = vertId / 8;
      int vertIdLocal = vertId % 8;
   
      int lvl = vertIdLocal / 2;

      float tHight = lvl / 3.0;

      float2 bladeOffset = RandomFloat2Seed(patchSeed + bladeId) - 0.5;

      float bladeAngle = PI_2 * RandomFloatSeed(patchSeed + bladeId + 23423);
      float3 bladeRight = float3(cos(bladeAngle), 0, sin(bladeAngle));
      float3 bladeForward = float3(bladeRight.x, 0, -bladeRight.y);

      float bladeScale = lerp(0.2, 1, RandomFloatSeed(patchSeed + bladeId + 98239));
      float bladeHeight = 0.4 * bladeScale;

      float bladeWidth = 0.03 * bladeScale;
      bladeWidth *= maxBladeCount / bladeCountF;
      if (bladeId == (bladeCount - 1)) {
         bladeWidth *= frac(bladeCountF);
      }

      float3 posW = patchCenterW;
      posW.xz += bladeOffset;
      posW += float3(0, 1, 0) * tHight * bladeHeight;
      posW += bladeRight * select(vertIdLocal & 1, -1, 1) * lerp(1, 0.1, tHight) * bladeWidth;
      posW += bladeForward * pow(tHight, 2) * 0.2 * bladeScale;

      posW += GetWindOffset(posW, normalize(float3(1, 0, 1))) * 0.1 * tHight;

      verts[vertId].posW = posW;
      verts[vertId].posH = mul(GetCamera().viewProjectionJitter, float4(posW, 1));

#ifdef OUTPUT_GEOM
      if (emitGeom) {
         gVertices[vertWriteId + vertId] = posW;
      }
#endif
   }

   for (uint i = 0; i < TRIS_SIZE / GROUP_SIZE + 1; ++i) {
      int triId = gtid + GROUP_SIZE * i;

      if (triId >= trisCount)
         break;

      int bladeId = triId / 6;
      int triIdLocal = triId % 6;

      int offset = bladeId * verticesPerBlade + 2 * (triIdLocal / 2);

      uint3 triangleIndices = (triIdLocal & 1) == 0 ? uint3(0, 1, 2) :
                                                      uint3(3, 2, 1);

      triangleIndices += offset;
      tris[triId] = triangleIndices;

#ifdef OUTPUT_GEOM
      if (emitGeom) {
         gIndexes[indexWriteId + triId * 3 + 0] = vertWriteId + triangleIndices.x;
         gIndexes[indexWriteId + triId * 3 + 1] = vertWriteId + triangleIndices.y;
         gIndexes[indexWriteId + triId * 3 + 2] = vertWriteId + triangleIndices.z;
      }
#endif
   }
}

#define GBUFFER

#ifdef GBUFFER
   struct PsOut {
      float4 emissive  : SV_Target0;
      float4 baseColor : SV_Target1;
      float4 normal    : SV_Target2;
      float4 motion    : SV_Target3;
      float  viewz     : SV_Target4;
   };

   float3 DdxPixelNormal(float3 posW) {
      return normalize(cross(ddx(posW), ddy(posW)));
   }

   uint2 PSGetPixelId(float4 psInputPosH) {
      return psInputPosH.xy;
   }
   
   [earlydepthstencil]
   PsOut PS(Vertex input) {
      // uint2 pixelIdx = PSGetPixelId(input.posH);
      // float2 screenUV = float2(pixelIdx) / gCamera.rtSize;

      float3 normalW = DdxPixelNormal(input.posW);

      SMaterial material = (SMaterial)0;
      material.baseColor = float3(0.1, 0.3, 0.1);
      material.emissivePower = 0;
      material.metallic = 0;
      material.roughness = 0.3;

      PsOut output;
      output.emissive = float4(material.baseColor * material.emissivePower, 1); // todo: 1?
      output.baseColor = float4(material.baseColor, material.metallic);
      output.normal = NRD_FrontEnd_PackNormalAndRoughness(normalW, material.roughness);
      output.motion = 0; // todo:

      output.viewz = mul(gCamera.view, float4(input.posW, 1)).z;

      // todo:
      #if defined(EDITOR)
         // SetEntityUnderCursor(pixelIdx, instance.entityID);
      #endif

      return output;
   }

#else
   float4 PS(Vertex input) : SV_Target0 {
      float4 color = float4(0, 0.5, 0, 1);
      return color;
   }
#endif
