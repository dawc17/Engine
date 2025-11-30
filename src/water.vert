#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aLocalUV;
layout (location = 2) in float aTileIndex;
layout (location = 3) in float aSkyLight;
layout (location = 4) in float aFaceShade;

out vec2 LocalUV;
flat out float TileIndex;
out float SkyLight;
out float FaceShade;
out vec3 WorldPos;

uniform mat4 transform;
uniform mat4 model;

void main()
{
   vec4 worldPosition = model * vec4(aPos, 1.0);
   WorldPos = worldPosition.xyz;
   
   gl_Position = transform * vec4(aPos, 1.0);
   LocalUV = aLocalUV;
   TileIndex = aTileIndex;
   SkyLight = aSkyLight;
   FaceShade = aFaceShade;
}
