#ifndef COMMON_RESOURCES_HEADER
#define COMMON_RESOURCES_HEADER

#include "samplers.hlsli"

cbuffer gSceneCB : DECLARE_REGISTER(b, CB_SLOT_SCENE, REGISTER_SPACE_COMMON) {
  SSceneCB gScene;
}

cbuffer gCameraCB : DECLARE_REGISTER(b, CB_SLOT_CAMERA, REGISTER_SPACE_COMMON) {
  SCameraCB gCamera;
}

cbuffer gCullCameraCB : DECLARE_REGISTER(b, CB_SLOT_CULL_CAMERA, REGISTER_SPACE_COMMON) {
  SCameraCB gCullCamera;
}

SCameraCB GetCullCamera() {
  // return gCamera;
  return gCullCamera;
}

SCameraCB GetCamera() {
  return gCamera;
}

float3 GetCameraPosition() {
  return GetCamera().position;
}

float3 GetCameraForward() {
  return GetCamera().forward;
}

float GetAnimationTime() {
   return gScene.animationTime;
}

// from NRD source
// orthoMode = { 0 - perspective, -1 - right handed ortho, 1 - left handed ortho }
float3 ReconstructViewPosition(float2 uv, float4 frustumLeftUp_Size, float viewZ = 1.0, float orthoMode = 0.0) {
   float3 p;
   p.xy = uv * frustumLeftUp_Size.zw * float2(2, -2) + frustumLeftUp_Size.xy;
   p.xy *= viewZ * ( 1.0 - abs( orthoMode ) ) + orthoMode;
   p.z = viewZ;

   return p;
}

float3 ReconstructWorldPosition(float3 viewPos) {
    return mul(GetCamera().invView, float4(viewPos, 1)).xyz;
}

bool ViewZDiscard(float viewZ) {
    return viewZ > 1e4; // todo: constant
}

#endif
