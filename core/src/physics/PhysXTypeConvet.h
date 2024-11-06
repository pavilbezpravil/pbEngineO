#pragma once

#include "PxPhysicsAPI.h"
#include "math/Transform.h"
#include "math/Types.h"

namespace pbe {
   struct Geom;
   struct Transform;
   using namespace physx;

	vec2 ConvertToPBE(const PxVec2& v);
	vec3 ConvertToPBE(const PxVec3& v);
	vec4 ConvertToPBE(const PxVec4& v);
	quat ConvertToPBE(const PxQuat& q);
   Transform ConvertToPBE(const PxTransform& trans, const vec3& scale = vec3_One);

	PxVec2 ConvertToPx(const vec2& v);
	PxVec3 ConvertToPx(const vec3& v);
	PxVec4 ConvertToPx(const vec4& v);
	PxQuat ConvertToPx(const quat& q);
   PxTransform ConvertToPx(const Transform& trans);
   PxGeometryHolder ConvertToPx(const Geom& geom);
}

