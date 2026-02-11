#version 460 core
layout (location = 0) in vec2 aQuadPos;
layout (location = 1) in vec3 aInstancePos;
layout (location = 2) in float aSize;
layout (location = 3) in float aTileIndex;
layout (location = 4) in vec4 aColor;
layout (location = 5) in float aLife;
layout (location = 6) in float aSkyLight;

out vec2 TexCoord;
flat out float TileIndex;
out vec4 ParticleColor;
out float Lifetime;
out float SkyLight;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraRight;
uniform vec3 cameraUp;

void main()
{
    vec3 vertexPos = aInstancePos
        + cameraRight * aQuadPos.x * aSize
        + cameraUp * aQuadPos.y * aSize;

    gl_Position = projection * view * vec4(vertexPos, 1.0);
    TexCoord = vec2(aQuadPos.x + 0.5, 0.5 - aQuadPos.y);
    TileIndex = aTileIndex;
    ParticleColor = aColor;
    Lifetime = aLife;
    SkyLight = aSkyLight;
}
