#pragma once

#include <array>
#include <glm/glm.hpp>

struct Plane
{
    glm::vec3 normal;
    float distance;
};

struct Frustum 
{
    std::array<Plane, 6> planes;

    static Frustum fromMatrix(const glm::mat4& viewProjection);
    bool intersectsAABB(const glm::vec3& minPoint, const glm::vec3& maxPoint) const;
};