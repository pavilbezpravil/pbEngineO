#ifndef LIGHTING_HEADER
#define LIGHTING_HEADER

#include "commonResources.hlsli"
#include "pbr.hlsli"
#include "noise.hlsli"

StructuredBuffer<SLight> gLights : DECLARE_REGISTER(t, SRV_SLOT_LIGHTS, REGISTER_SPACE_COMMON);
Texture2D<float> gShadowMap : DECLARE_REGISTER(t, SRV_SLOT_SHADOWMAP, REGISTER_SPACE_COMMON);

float3 LightGetL(SLight light, float3 posW) { // L
  if (light.type == SLIGHT_TYPE_DIRECT) {
    return -light.direction;
  } else if (light.type == SLIGHT_TYPE_POINT) {
    float3 L = normalize(light.position - posW);
    return L;
  }
  return float3(0, 0, 0);
}

float SunShadowAttenuation(float3 posW, float2 jitter = 0) {
  if (0) {
    jitter = (rand3dTo2d(posW) - 0.5) * 0.001;
  }

  float3 shadowUVZ = mul(gScene.toShadowSpace, float4(posW, 1)).xyz;
  if (shadowUVZ.z >= 1) {
      return 1;
  }

  float2 shadowUV = shadowUVZ.xy + jitter;
  float z = shadowUVZ.z;

  float bias = 0.003;

  if (0) {
    return gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias);
  } else {
    float sum = 0;

    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, float2(0, 0)));

    // int offset = 2;
    // // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(offset, offset)));
    // // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(offset, -offset)));
    // // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(-offset, offset)));
    // // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(-offset, -offset)));

    // // offset = 3;
    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(-offset, 0)));
    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(offset, 0)));
    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(0, -offset)));
    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(0, offset)));
    // return sum / (4 * 5);

    sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, float2(0, 0));

    int offset = 1;

    sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, int2(-offset, 0));
    sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, int2(offset, 0));
    sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, int2(0, -offset));
    sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, int2(0, offset));

    return sum / (5);
  }
}

float LightAttenuation(SLight light, float3 posW) {
  float attenuation = 1;

  if (light.type == SLIGHT_TYPE_DIRECT) {
    attenuation = SunShadowAttenuation(posW);
  } else if (light.type == SLIGHT_TYPE_POINT) {
    float distance = length(light.position - posW);
    // attenuation = 1 / (distance * distance);
    attenuation = smoothstep(1, 0, distance / light.radius);
  }

  return attenuation;
}

struct Surface {
  float3 posW;
  float3 normalW;
  float roughness;
  float metalness;
  float3 baseColor;
};

void ConvertBaseColorMetalnessToAlbedoRf0( float3 baseColor, float metalness, out float3 albedo, out float3 Rf0, float reflectance = 0.5 ) {
  albedo = baseColor * saturate( 1.0 - metalness );
  Rf0 = lerp( 0.16 * reflectance * reflectance, baseColor, metalness );
}

void ConvertBaseColorMetalnessToAlbedoRf0( Surface surface, out float3 albedo, out float3 Rf0, float reflectance = 0.5 ) {
  ConvertBaseColorMetalnessToAlbedoRf0( surface.baseColor, surface.metalness, albedo, Rf0, reflectance );
}

float3 LightRadiance(SLight light, float3 posW) {
  float attenuation = LightAttenuation(light, posW);
  float3 radiance = light.color * attenuation;
  return radiance;
}

float3 LightShadeLo(Surface surface, float3 V, float3 radiance, float3 L) {
  float3 albedo, Rf0;
  ConvertBaseColorMetalnessToAlbedoRf0(surface.baseColor, surface.metalness, albedo, Rf0 );

  float3 Cdiff, Cspec;
  STL::BRDF::DirectLighting(surface.normalW, L, V, Rf0, surface.roughness, Cdiff, Cspec);
  return (Cdiff * albedo + Cspec) * radiance;
}

float3 LightShadeLo(SLight light, Surface surface, float3 V) {
  float attenuation = 1 ; // LightAttenuation(light, surface.posW);
  if (attenuation <= 0.0001) {
    return 0;
  }
  float3 radiance = light.color * attenuation;
  float3 L = LightGetL(light, surface.posW);

  return LightShadeLo(surface, V, radiance, L);
}

float3 Shade(Surface surface, float3 V) {
  float3 Lo = 0;

  Lo += LightShadeLo(gScene.directLight, surface, V);

  for(int i = 0; i < gScene.nLights; ++i) {
      Lo += LightShadeLo(gLights[i], surface, V);
  }

  return Lo;
}

#endif
