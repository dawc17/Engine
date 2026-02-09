#include "Camera.h"
#include <cmath>

glm::vec3 CameraForward(const Camera &camera)
{
  glm::vec3 forward;
  forward.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
  forward.y = sin(glm::radians(camera.pitch));
  forward.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
  return glm::normalize(forward);
}
