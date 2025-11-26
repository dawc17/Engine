#pragma once
#include <glm/glm.hpp>

struct Camera
{
  glm::vec3 position;
  float yaw;
  float pitch;
  float fov;
};

glm::vec3 CameraForward(const Camera &camera);
