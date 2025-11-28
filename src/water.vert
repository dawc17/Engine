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
out float FragDepth;
out vec3 WorldPos;

uniform mat4 transform;
uniform mat4 model;
uniform float time;

void main()
{
   vec4 worldPosition = model * vec4(aPos, 1.0);
   
   float waveHeight = sin(worldPosition.x * 0.8 + time * 2.0) * 0.04 +
                      sin(worldPosition.z * 0.6 + time * 1.5) * 0.03;
   
   vec3 modifiedPos = aPos;
   modifiedPos.y += waveHeight - 0.1;
   
   worldPosition.y += waveHeight - 0.1;
   
   gl_Position = transform * vec4(modifiedPos, 1.0);
   LocalUV = aLocalUV;
   TileIndex = aTileIndex;
   SkyLight = aSkyLight;
   FaceShade = aFaceShade;
   FragDepth = gl_Position.z;
   WorldPos = worldPosition.xyz;
}
