#include "Frustum.h"

#include <glm/geometric.hpp>
#include <array>

Frustum Frustum::fromMatrix(const glm::mat4& m)
{
    Frustum f;

    const std::array<glm::vec4, 6> rawPlanes = {
        glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]),
        glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]),
        glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]),
        glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]),
        glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]),
        glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]),
    };

    for (size_t i = 0; i < rawPlanes.size(); i++)
    {
        glm::vec3 n(rawPlanes[i].x, rawPlanes[i].y, rawPlanes[i].z);
        float len = glm::length(n);
        if (len > 0.0f)
        {
            f.planes[i].normal = n / len;
            f.planes[i].distance = rawPlanes[i].w / len;
        }
        else 
        {
            f.planes[i].normal = glm::vec3(0.0f, 0.0f, 0.0f);
            f.planes[i].distance = 0.0f;
        }
    }

    return f;
}

bool Frustum::intersectsAABB(const glm::vec3& minPoint, const glm::vec3& maxPoint) const
{
    for (const Plane& plane : planes)
    {
        glm::vec3 p = minPoint;

        if (plane.normal.x >= 0.0f) p.x = maxPoint.x;
        if (plane.normal.y >= 0.0f) p.y = maxPoint.y;
        if (plane.normal.z >= 0.0f) p.z = maxPoint.z;

        if (glm::dot(plane.normal, p) + plane.distance < 0.0f)
            return false;
    }

    return true;
}