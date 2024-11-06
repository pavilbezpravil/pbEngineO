#include <cstdint>
#include <string>

#include <vector>
#include <stack>
#include <deque>
#include <unordered_map>

#include <algorithm>
#include <functional>
#include <random>
#include <filesystem>

#include <fstream>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
// todo:
template<typename OStream>
OStream& operator<<(OStream& os, const ImVec2& v)
{
   return os << "(" << v.x << ", " << v.y << ")";
}

// todo:
#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

// todo:
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <entt/entt.hpp>

#include "PxPhysicsAPI.h"
